#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <optional>
#include <profiler/profiler.hpp>
#include <string>
#include <thread>
#include <utility>

namespace {
using hemlok::profiler::Profiler;
using hemlok::profiler::ProfilingTarget;

ProfilingTarget MakeTarget(std::optional<std::string> name,
                           std::filesystem::path path) {
    auto target = ProfilingTarget::Create(std::move(name), std::move(path));
    REQUIRE(target.has_value());
    return std::move(*target);
}

TEST_CASE("a new profiler has no targets", "[profiler]") {
    const Profiler profiler;

    REQUIRE(profiler.GetTargets().empty());
}

TEST_CASE("a profiler stores a named target", "[profiler]") {
    Profiler profiler;
    const std::filesystem::path executable_path = "/usr/bin/example";

    const auto id = profiler.AddTarget(MakeTarget("example", executable_path));
    REQUIRE(id == 0);

    const auto& targets = profiler.GetTargets();
    REQUIRE(targets.size() == 1);
    REQUIRE(targets.contains(0));
    REQUIRE(targets.at(0).GetName() == std::optional<std::string>{"example"});
    REQUIRE(targets.at(0).GetExecutablePath() == executable_path);
}

TEST_CASE("a profiler stores a target without a name", "[profiler]") {
    Profiler profiler;

    const auto id =
        profiler.AddTarget(MakeTarget(std::nullopt, "/usr/bin/unnamed"));
    REQUIRE(id == 0);

    const auto& target = profiler.GetTargets().at(0);
    REQUIRE_FALSE(target.GetName().has_value());
    REQUIRE(target.GetExecutablePath() == "/usr/bin/unnamed");
}

TEST_CASE("a profiler assigns consecutive target IDs", "[profiler]") {
    Profiler profiler;

    const auto first = profiler.AddTarget(MakeTarget("first", "/bin/first"));
    REQUIRE(first == 0);
    const auto second = profiler.AddTarget(MakeTarget("second", "/bin/second"));
    REQUIRE(second == 1);
    const auto third = profiler.AddTarget(MakeTarget("third", "/bin/third"));
    REQUIRE(third == 2);

    const auto& targets = profiler.GetTargets();
    REQUIRE(targets.size() == 3);
    REQUIRE(targets.at(0).GetName() == std::optional<std::string>{"first"});
    REQUIRE(targets.at(1).GetName() == std::optional<std::string>{"second"});
    REQUIRE(targets.at(2).GetName() == std::optional<std::string>{"third"});
}

TEST_CASE("existing target references observe newly added targets",
          "[profiler]") {
    Profiler profiler;
    const auto& targets = profiler.GetTargets();

    REQUIRE(profiler.AddTarget(MakeTarget("later", "/bin/later")) == 0);

    REQUIRE(targets.size() == 1);
    REQUIRE(targets.at(0).GetName() == std::optional<std::string>{"later"});
}

TEST_CASE("duplicate target configurations receive distinct IDs",
          "[profiler]") {
    Profiler profiler;
    const auto target = MakeTarget("worker", "/bin/worker");
    const auto first = profiler.AddTarget(target);
    const auto second = profiler.AddTarget(target);
    REQUIRE(first != second);
    REQUIRE(profiler.GetTargets().size() == 2);
}
TEST_CASE("profiling starts independent runs and retains terminated history",
          "[profiler]") {
    Profiler profiler;
    REQUIRE(profiler.GetRuns().empty());
    const auto target =
        profiler.AddTarget(MakeTarget("worker", HEMLOK_PROCESS_FIXTURE_PATH));
    const auto before = std::chrono::system_clock::now();
    const auto first = profiler.StartRun(target);
    const auto second = profiler.StartRun(target);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(*first == 0);
    REQUIRE(*second == 1);
    const auto& entry = profiler.GetRuns().at(*first);
    REQUIRE(entry.id == target);
    REQUIRE(entry.profiling_run.GetStartTime() >= before);
    REQUIRE(entry.profiling_run.GetStartTime() <=
            std::chrono::system_clock::now());
    REQUIRE(entry.profiling_run.IsRunning());
    REQUIRE_FALSE(entry.profiling_run.GetFinishTime());
    profiler.Poll();
    for (const auto& [id, entry] : profiler.GetRuns()) {
        REQUIRE_FALSE(entry.profiling_run.GetPollError().has_value());
    }
    REQUIRE(entry.profiling_run.IsRunning());
    REQUIRE_FALSE(entry.profiling_run.GetFinishTime());
    REQUIRE(profiler.TerminateRun(*first).has_value());
    const auto finish = entry.profiling_run.GetFinishTime();
    REQUIRE(finish.has_value());
    REQUIRE_FALSE(entry.profiling_run.IsRunning());
    REQUIRE(*finish >= entry.profiling_run.GetStartTime());
    REQUIRE(*finish <= std::chrono::system_clock::now());
    REQUIRE(profiler.TerminateRun(*first).has_value());
    profiler.Poll();
    for (const auto& [id, entry] : profiler.GetRuns()) {
        REQUIRE_FALSE(entry.profiling_run.GetPollError().has_value());
    }
    REQUIRE(entry.profiling_run.GetFinishTime() == finish);
    REQUIRE_FALSE(profiler.GetRuns().at(*second).profiling_run.GetFinishTime());
    REQUIRE(profiler.TerminateRun(*second).has_value());
    REQUIRE(profiler.GetRuns().size() == 2);
}

TEST_CASE("failed launches do not create runs or consume run IDs",
          "[profiler]") {
    Profiler profiler;
    const auto missing =
        std::filesystem::current_path() / "hemlok-missing-run-executable";
    REQUIRE_FALSE(std::filesystem::exists(missing));
    const auto target = profiler.AddTarget(MakeTarget("missing", missing));
    const auto result = profiler.StartRun(target);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() ==
            hemlok::profiler::ProfilerError::kProcessStartFailed);
    REQUIRE(profiler.GetRuns().empty());
    const auto valid =
        profiler.AddTarget(MakeTarget("worker", HEMLOK_PROCESS_FIXTURE_PATH));
    const auto run = profiler.StartRun(valid);
    REQUIRE(run.has_value());
    REQUIRE(*run == 0);
    REQUIRE(profiler.TerminateRun(*run).has_value());
}

TEST_CASE("polling records natural process exit", "[profiler]") {
    Profiler profiler;
    const auto target = profiler.AddTarget(
        MakeTarget("short lived", HEMLOK_EXITING_PROCESS_FIXTURE_PATH));
    const auto result = profiler.StartRun(target);
    // A short-lived child may exit before Create observes it.
    if (!result) {
        REQUIRE(result.error() ==
                hemlok::profiler::ProfilerError::kProcessTerminated);
        REQUIRE(profiler.GetRuns().empty());
        return;
    }
    const auto& run = profiler.GetRuns().at(*result).profiling_run;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    REQUIRE(run.IsRunning());
    while (run.IsRunning() && std::chrono::steady_clock::now() < deadline) {
        profiler.Poll();
        for (const auto& [id, entry] : profiler.GetRuns()) {
            REQUIRE_FALSE(entry.profiling_run.GetPollError().has_value());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    REQUIRE_FALSE(run.IsRunning());
    REQUIRE(run.GetFinishTime().has_value());
    REQUIRE(*run.GetFinishTime() >= run.GetStartTime());
    const auto finish = run.GetFinishTime();
    REQUIRE(profiler.TerminateRun(*result).has_value());
    profiler.Poll();
    for (const auto& [id, entry] : profiler.GetRuns()) {
        REQUIRE_FALSE(entry.profiling_run.GetPollError().has_value());
    }
    REQUIRE(run.GetFinishTime() == finish);
}

TEST_CASE("profiling rejects an already terminated process", "[profiler]") {
    auto process = hemlok::process::Process::CreateFromExecutable(
        HEMLOK_PROCESS_FIXTURE_PATH);
    REQUIRE(process.has_value());
    REQUIRE(process->Terminate().has_value());
    const auto run =
        hemlok::profiler::ProfilingRun::Create(std::move(*process));
    REQUIRE_FALSE(run.has_value());
    REQUIRE(run.error() ==
            hemlok::profiler::ProfilingRunError::kProcessTerminated);
}
TEST_CASE("polling a run preserves running state and completed timestamps",
          "[profiler]") {
    auto process = hemlok::process::Process::CreateFromExecutable(
        HEMLOK_PROCESS_FIXTURE_PATH);
    REQUIRE(process.has_value());
    auto run = hemlok::profiler::ProfilingRun::Create(std::move(*process));
    REQUIRE(run.has_value());
    const auto& snapshot = *run;
    const auto start = snapshot.GetStartTime();

    REQUIRE(snapshot.IsRunning());
    run->Poll();
    REQUIRE_FALSE(snapshot.GetPollError().has_value());
    REQUIRE(snapshot.IsRunning());
    REQUIRE_FALSE(snapshot.GetFinishTime().has_value());
    REQUIRE(snapshot.GetStartTime() == start);

    REQUIRE(run->Terminate().has_value());
    REQUIRE_FALSE(snapshot.IsRunning());
    const auto finish = snapshot.GetFinishTime();
    REQUIRE(finish.has_value());
    for (int poll = 0; poll < 2; ++poll) {
        run->Poll();
        REQUIRE_FALSE(snapshot.GetPollError().has_value());
        REQUIRE_FALSE(snapshot.IsRunning());
        REQUIRE(snapshot.GetStartTime() == start);
        REQUIRE(snapshot.GetFinishTime() == finish);
    }
}
}  // namespace
