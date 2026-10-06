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
using hemlok::profiler::Target;

Target MakeTarget(std::optional<std::string> name, std::filesystem::path path) {
    auto target = Target::Create(std::move(name), std::move(path));
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

    const auto& targets = profiler.GetTargets();
    REQUIRE(targets.size() == 1);
    REQUIRE(targets.contains(id));
    REQUIRE(targets.at(id).GetName() == std::optional<std::string>{"example"});
    REQUIRE(targets.at(id).GetExecutablePath() == executable_path);
}

TEST_CASE("a profiler stores a target without a name", "[profiler]") {
    Profiler profiler;

    const auto id =
        profiler.AddTarget(MakeTarget(std::nullopt, "/usr/bin/unnamed"));

    const auto& target = profiler.GetTargets().at(id);
    REQUIRE_FALSE(target.GetName().has_value());
    REQUIRE(target.GetExecutablePath() == "/usr/bin/unnamed");
}

TEST_CASE("a profiler assigns consecutive target IDs", "[profiler]") {
    Profiler profiler;

    const auto first = profiler.AddTarget(MakeTarget("first", "/bin/first"));
    const auto second = profiler.AddTarget(MakeTarget("second", "/bin/second"));
    REQUIRE(second == first + 1);
    const auto third = profiler.AddTarget(MakeTarget("third", "/bin/third"));
    REQUIRE(third == second + 1);

    const auto& targets = profiler.GetTargets();
    REQUIRE(targets.size() == 3);
    REQUIRE(targets.at(first).GetName() == std::optional<std::string>{"first"});
    REQUIRE(targets.at(second).GetName() ==
            std::optional<std::string>{"second"});
    REQUIRE(targets.at(third).GetName() == std::optional<std::string>{"third"});
}

TEST_CASE("existing target references observe newly added targets",
          "[profiler]") {
    Profiler profiler;
    const auto& targets = profiler.GetTargets();

    const auto id = profiler.AddTarget(MakeTarget("later", "/bin/later"));

    REQUIRE(targets.size() == 1);
    REQUIRE(targets.at(id).GetName() == std::optional<std::string>{"later"});
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
using hemlok::profiler::RunOutcome;
using hemlok::profiler::RunStatus;

template <typename Predicate>
void WaitFor(Profiler& profiler, Predicate predicate) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        profiler.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    REQUIRE(predicate());
}

TEST_CASE("independent runs retain immutable terminated history",
          "[profiler]") {
    Profiler profiler;
    REQUIRE(profiler.GetRunOverviews().empty());
    REQUIRE(profiler.GetCompletedRuns().empty());
    const auto target =
        profiler.AddTarget(MakeTarget("worker", HEMLOK_PROCESS_FIXTURE_PATH));
    const auto first = profiler.StartRun(target);
    const auto second = profiler.StartRun(target);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(*second == *first + 1);
    REQUIRE(profiler.GetRunOverviews().at(*first).status ==
            RunStatus::kStarting);
    REQUIRE_FALSE(profiler.GetCompletedRun(*first));
    WaitFor(profiler, [&] {
        return profiler.GetRunOverviews().at(*first).status ==
                   RunStatus::kRunning &&
               profiler.GetRunOverviews().at(*second).status ==
                   RunStatus::kRunning;
    });
    const auto& overview = profiler.GetRunOverviews().at(*first);
    REQUIRE(overview.run_id == *first);
    REQUIRE(overview.target_id == target);
    REQUIRE(overview.start_time);
    REQUIRE_FALSE(overview.finish_time);
    REQUIRE(profiler.TerminateRun(*first));
    REQUIRE(profiler.TerminateRun(*first));
    WaitFor(profiler, [&] { return bool(profiler.GetCompletedRun(*first)); });
    const auto completed = profiler.GetCompletedRun(*first);
    REQUIRE(completed->outcome == RunOutcome::kTerminated);
    REQUIRE(completed->run_id == *first);
    REQUIRE(completed->target_id == target);
    REQUIRE_FALSE(completed->error);
    REQUIRE(completed->finish_time >= *completed->start_time);
    REQUIRE(overview.status == RunStatus::kTerminated);
    REQUIRE(overview.finish_time == completed->finish_time);
    REQUIRE(profiler.GetRunOverviews().at(*second).status ==
            RunStatus::kRunning);
    REQUIRE(profiler.TerminateRun(*first));
    REQUIRE(profiler.TerminateRun(*second));
    WaitFor(profiler, [&] { return profiler.GetCompletedRuns().size() == 2; });
    REQUIRE(profiler.GetCompletedRun(*first) == completed);
    REQUIRE(profiler.GetRunOverviews().size() == 2);
}

TEST_CASE("failed launches retain errors and consume run IDs", "[profiler]") {
    Profiler profiler;
    const auto missing =
        std::filesystem::current_path() / "hemlok-missing-run-executable";
    REQUIRE_FALSE(std::filesystem::exists(missing));
    const auto target = profiler.AddTarget(MakeTarget("missing", missing));
    const auto result = profiler.StartRun(target);
    REQUIRE(result);
    WaitFor(profiler, [&] { return bool(profiler.GetCompletedRun(*result)); });
    const auto completed = profiler.GetCompletedRun(*result);
    REQUIRE(completed->outcome == RunOutcome::kFailed);
    REQUIRE(completed->error == hemlok::process::ProcessError::kFileNotFound);
    REQUIRE_FALSE(completed->start_time);
    const auto& overview = profiler.GetRunOverviews().at(*result);
    REQUIRE(overview.status == RunStatus::kFailed);
    REQUIRE(overview.error == completed->error);
    REQUIRE(overview.finish_time == completed->finish_time);
    const auto valid =
        profiler.AddTarget(MakeTarget("worker", HEMLOK_PROCESS_FIXTURE_PATH));
    const auto run = profiler.StartRun(valid);
    REQUIRE(run);
    REQUIRE(*run == *result + 1);
    // Termination queued immediately after launch must work before any Poll.
    REQUIRE(profiler.TerminateRun(*run));
    WaitFor(profiler, [&] { return bool(profiler.GetCompletedRun(*run)); });
    REQUIRE(profiler.GetCompletedRun(*run)->outcome == RunOutcome::kTerminated);
}

TEST_CASE("worker observes natural exit without changing UI state until Poll",
          "[profiler]") {
    Profiler profiler;
    const auto target = profiler.AddTarget(
        MakeTarget("short lived", HEMLOK_EXITING_PROCESS_FIXTURE_PATH));
    const auto result = profiler.StartRun(target);
    REQUIRE(result);
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    REQUIRE(profiler.GetRunOverviews().at(*result).status ==
            RunStatus::kStarting);
    REQUIRE(profiler.GetCompletedRuns().empty());
    WaitFor(profiler, [&] { return bool(profiler.GetCompletedRun(*result)); });
    const auto run = profiler.GetCompletedRun(*result);
    REQUIRE(run->outcome == RunOutcome::kFinished);
    REQUIRE(run->start_time);
    REQUIRE(run->finish_time >= *run->start_time);
    REQUIRE_FALSE(run->error);
    REQUIRE(profiler.TerminateRun(*result));
    profiler.Poll();
    REQUIRE(profiler.GetCompletedRun(*result) == run);
}

TEST_CASE("completed results outlive their profiler", "[profiler]") {
    std::shared_ptr<const hemlok::profiler::CompletedRun> result;
    {
        Profiler profiler;
        const auto target = profiler.AddTarget(
            MakeTarget("worker", HEMLOK_PROCESS_FIXTURE_PATH));
        const auto run = profiler.StartRun(target);
        REQUIRE(run);
        REQUIRE(profiler.TerminateRun(*run));
        WaitFor(profiler, [&] { return bool(profiler.GetCompletedRun(*run)); });
        result = profiler.GetCompletedRun(*run);
    }
    REQUIRE(result->outcome == RunOutcome::kTerminated);
    REQUIRE(result->finish_time >= *result->start_time);
}

TEST_CASE("generated IDs continue across profiler instances", "[profiler]") {
    hemlok::profiler::TargetId first_target;
    hemlok::profiler::RunId first_run;
    const auto missing =
        std::filesystem::current_path() / "hemlok-missing-run-executable";
    REQUIRE_FALSE(std::filesystem::exists(missing));
    {
        Profiler profiler;
        first_target = profiler.AddTarget(MakeTarget("first", missing));
        const auto run = profiler.StartRun(first_target);
        REQUIRE(run);
        first_run = *run;
        WaitFor(profiler,
                [&] { return bool(profiler.GetCompletedRun(first_run)); });
    }
    Profiler profiler;
    const auto target = profiler.AddTarget(MakeTarget("second", missing));
    REQUIRE(target == first_target + 1);
    const auto run = profiler.StartRun(target);
    REQUIRE(run);
    REQUIRE(*run == first_run + 1);
    WaitFor(profiler, [&] { return bool(profiler.GetCompletedRun(*run)); });
}
}  // namespace
