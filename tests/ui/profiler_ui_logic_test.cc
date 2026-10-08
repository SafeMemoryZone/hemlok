#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <string_view>
#include <support/temporary_directory.hpp>
#include <ui/profiler_ui_logic.hpp>

namespace {
using namespace hemlok::profiler;
using namespace hemlok::ui::detail;
using namespace std::chrono_literals;

std::filesystem::path MakeFile(const hemlok::testing::TemporaryDirectory& dir) {
    const auto path = dir.GetPath() / "program with spaces";
    std::ofstream file(path);
    file << "Regular file; executability is checked at launch.";
    file.close();
    REQUIRE(file.good());
    return path;
}

TEST_CASE("UI input rejects empty missing and directory paths", "[ui]") {
    hemlok::testing::TemporaryDirectory directory;
    std::filesystem::path path;
    std::string_view expected;
    SECTION("empty path") { expected = "Enter an executable path."; }
    SECTION("missing file") {
        path = directory.GetPath() / "missing";
        expected = "Path must refer to an existing regular file.";
    }
    SECTION("directory") {
        path = directory.GetPath();
        expected = "Path must refer to an existing regular file.";
    }
    const auto result = ValidateTargetInput("worker", path);
    REQUIRE_FALSE(result);
    REQUIRE(std::string_view{result.error()} == expected);
}

TEST_CASE("UI input accepts regular files and resolves relative paths",
          "[ui]") {
    hemlok::testing::TemporaryDirectory directory;
    const auto path = MakeFile(directory);
    auto input = path;
    SECTION("absolute path containing spaces") {}
    SECTION("relative path") {
        input =
            std::filesystem::relative(path, std::filesystem::current_path());
        REQUIRE(input.is_relative());
    }
    const auto result = ValidateTargetInput(" worker ", input);
    REQUIRE(result);
    REQUIRE(result->GetName() == " worker ");
    REQUIRE(result->GetExecutablePath().is_absolute());
    REQUIRE(result->GetExecutablePath().lexically_normal() ==
            path.lexically_normal());
}

TEST_CASE("UI input distinguishes absent names from whitespace names", "[ui]") {
    hemlok::testing::TemporaryDirectory directory;
    const auto path = MakeFile(directory);
    const auto unnamed = ValidateTargetInput("", path);
    REQUIRE(unnamed);
    REQUIRE_FALSE(unnamed->GetName());
    for (const auto* name : {" ", "\t\n\r\f\v"}) {
        CAPTURE(name);
        const auto result = ValidateTargetInput(name, path);
        REQUIRE_FALSE(result);
        REQUIRE(std::string_view{result.error()} ==
                "Name must contain a non-whitespace character.");
    }
}

TEST_CASE("UI submission adds only valid targets and returns the selected ID",
          "[ui]") {
    hemlok::testing::TemporaryDirectory directory;
    const auto path = MakeFile(directory);
    Profiler profiler;
    const auto first = SubmitTarget(profiler, "first", path);
    REQUIRE(first);
    REQUIRE(profiler.GetTargets().size() == 1);
    REQUIRE(profiler.GetTargets().at(*first).GetName() == "first");
    REQUIRE(profiler.GetTargets().at(*first).GetExecutablePath() == path);

    const auto invalid_name = SubmitTarget(profiler, " ", path);
    REQUIRE_FALSE(invalid_name);
    const auto invalid_path = SubmitTarget(profiler, "second", {});
    REQUIRE_FALSE(invalid_path);
    REQUIRE(profiler.GetTargets().size() == 1);
    REQUIRE(profiler.GetTargets().at(*first).GetName() == "first");

    const auto second = SubmitTarget(profiler, "", path);
    REQUIRE(second);
    REQUIRE(*second != *first);
    REQUIRE(profiler.GetTargets().size() == 2);
    REQUIRE_FALSE(profiler.GetTargets().at(*second).GetName());
    REQUIRE(profiler.GetRunOverviews().empty());
    REQUIRE(profiler.GetCompletedRuns().empty());
}

TEST_CASE("UI run lists filter targets and sort by creation ID", "[ui]") {
    std::unordered_map<RunId, RunOverview> runs;
    REQUIRE(GetTargetRunIds(runs, 10).empty());
    // Global IDs can be interleaved across targets and exceed 32 bits.
    constexpr RunId kLargeId = RunId{1} << 40;
    for (const auto& [id, target] : std::vector<std::pair<RunId, TargetId>>{
             {5, 10}, {2, 20}, {1, 10}, {kLargeId, 10}, {7, 20}}) {
        runs.emplace(id, RunOverview{id, target, RunStatus::kStarting,
                                     std::nullopt, std::nullopt, std::nullopt});
    }
    REQUIRE(GetTargetRunIds(runs, 10) == std::vector<RunId>{kLargeId, 5, 1});
    REQUIRE(GetTargetRunIds(runs, 20) == std::vector<RunId>{7, 2});
    REQUIRE(GetTargetRunIds(runs, 99).empty());
    REQUIRE(runs.size() == 5);
}

TEST_CASE("UI permits termination only for starting or running runs", "[ui]") {
    REQUIRE(CanTerminate(RunStatus::kStarting));
    REQUIRE(CanTerminate(RunStatus::kRunning));
    REQUIRE_FALSE(CanTerminate(RunStatus::kFinished));
    REQUIRE_FALSE(CanTerminate(RunStatus::kTerminated));
    REQUIRE_FALSE(CanTerminate(RunStatus::kFailed));
}

TEST_CASE(
    "UI duration uses explicit timestamps and clamps negative elapsed time",
    "[ui]") {
    const TimePoint start{100s};
    RunOverview run{
        1, 10, RunStatus::kStarting, std::nullopt, std::nullopt, std::nullopt};
    REQUIRE_FALSE(GetRunDurationSeconds(run, start));
    run.finish_time = start + 5s;
    REQUIRE_FALSE(GetRunDurationSeconds(run, start));

    run.status = RunStatus::kRunning;
    run.start_time = start;
    run.finish_time.reset();
    REQUIRE(GetRunDurationSeconds(run, start) == 0.0);
    REQUIRE(GetRunDurationSeconds(run, start + 1250ms) == 1.25);
    REQUIRE(GetRunDurationSeconds(run, start - 1s) == 0.0);

    run.status = RunStatus::kFinished;
    run.finish_time = start + 2500ms;
    REQUIRE(GetRunDurationSeconds(run, start + 10s) == 2.5);
    REQUIRE(GetRunDurationSeconds(run, start - 10s) == 2.5);
    run.finish_time = start - 1s;
    REQUIRE(GetRunDurationSeconds(run, start + 10s) == 0.0);
}
}  // namespace
