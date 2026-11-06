#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <profiler/active_run.hpp>
#include <thread>
#include <type_traits>
#include <utility>

namespace {
using namespace hemlok::profiler;
using hemlok::sampling::SamplingSession;

ActiveRun LaunchRun(RunId run, TargetId target, const char* executable) {
    auto process = hemlok::process::Process::CreateFromExecutable(executable);
    REQUIRE(process);
    auto session = SamplingSession::Attach(process->GetPid());
    REQUIRE(session);
    return ActiveRun::Create(run, target, std::move(*process),
                             std::move(*session));
}

static_assert(std::is_invocable_v<decltype(&ActiveRun::Complete), ActiveRun&&,
                                  RunOutcome, std::optional<RunError>>);
static_assert(!std::is_invocable_v<decltype(&ActiveRun::Complete), ActiveRun&,
                                   RunOutcome, std::optional<RunError>>);

TEST_CASE("active runs expose metadata and move results into completion",
          "[profiler]") {
    auto run = LaunchRun(7, 3, HEMLOK_PROCESS_FIXTURE_PATH);
    REQUIRE(run.GetRunId() == 7);
    REQUIRE(run.GetTargetId() == 3);
    const auto overview = run.GetOverview();
    REQUIRE(overview.status == RunStatus::kRunning);
    REQUIRE(overview.start_time);
    REQUIRE_FALSE(overview.finish_time);
    REQUIRE_FALSE(overview.error);
    const auto running = run.IsProcessRunning();
    REQUIRE(running);
    REQUIRE(*running);
    run.CollectSamples();
    REQUIRE(run.TerminateProcess());
    const auto stopped = run.IsProcessRunning();
    REQUIRE(stopped);
    REQUIRE_FALSE(*stopped);
    const auto completed =
        std::move(run).Complete(RunOutcome::kTerminated, std::nullopt);
    REQUIRE(completed.run_id == 7);
    REQUIRE(completed.target_id == 3);
    REQUIRE(completed.start_time == overview.start_time);
    REQUIRE(completed.finish_time >= *completed.start_time);
    REQUIRE(completed.outcome == RunOutcome::kTerminated);
}

TEST_CASE("failed completion retains the supplied error and run metadata",
          "[profiler]") {
    auto run = LaunchRun(42, 7, HEMLOK_PROCESS_FIXTURE_PATH);
    const auto initial = run.GetOverview();
    REQUIRE(run.TerminateProcess());
    const auto error = hemlok::process::ProcessError::kTerminationFailed;
    const auto completed = std::move(run).Complete(RunOutcome::kFailed, error);
    REQUIRE(completed.run_id == initial.run_id);
    REQUIRE(completed.target_id == initial.target_id);
    REQUIRE(completed.start_time == initial.start_time);
    REQUIRE(completed.outcome == RunOutcome::kFailed);
    REQUIRE(completed.error == RunError{error});
}

TEST_CASE("completion keeps samples collected before and after the last drain",
          "[profiler]") {
    auto run = LaunchRun(1, 1, HEMLOK_BUSY_PROCESS_FIXTURE_PATH);
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    run.CollectSamples();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    REQUIRE(run.TerminateProcess());
    const auto completed =
        std::move(run).Complete(RunOutcome::kTerminated, std::nullopt);
    // At 1 kHz, 100 ms of busy work yields far more than 50 samples.
    REQUIRE(completed.sampling_data.samples.size() > 50);
}
}  // namespace
