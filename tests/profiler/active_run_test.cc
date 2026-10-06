#include <catch2/catch_test_macros.hpp>
#include <profiler/active_run.hpp>
#include <type_traits>
#include <utility>

namespace {
using namespace hemlok::profiler;

static_assert(
    std::is_invocable_v<decltype(&ActiveRun::Complete), ActiveRun&&, RunOutcome,
                        std::optional<hemlok::process::ProcessError>>);
static_assert(
    !std::is_invocable_v<decltype(&ActiveRun::Complete), ActiveRun&, RunOutcome,
                         std::optional<hemlok::process::ProcessError>>);

TEST_CASE("active runs expose metadata and move results into completion",
          "[profiler]") {
    auto process = hemlok::process::Process::CreateFromExecutable(
        HEMLOK_PROCESS_FIXTURE_PATH);
    REQUIRE(process);
    auto run = ActiveRun::Create(7, 3, std::move(*process));
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
    REQUIRE(run.CollectSamples());
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
}  // namespace
