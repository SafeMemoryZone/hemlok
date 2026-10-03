#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>
#include <profiler/profiler.hpp>
#include <string>
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

    const auto id =
        profiler.AddProfilingTarget(MakeTarget("example", executable_path));
    REQUIRE(id == 0);

    const auto& targets = profiler.GetTargets();
    REQUIRE(targets.size() == 1);
    REQUIRE(targets.contains(0));
    REQUIRE(targets.at(0).GetName() == std::optional<std::string>{"example"});
    REQUIRE(targets.at(0).GetExecutablePath() == executable_path);
}

TEST_CASE("a profiler stores a target without a name", "[profiler]") {
    Profiler profiler;

    const auto id = profiler.AddProfilingTarget(
        MakeTarget(std::nullopt, "/usr/bin/unnamed"));
    REQUIRE(id == 0);

    const auto& target = profiler.GetTargets().at(0);
    REQUIRE_FALSE(target.GetName().has_value());
    REQUIRE(target.GetExecutablePath() == "/usr/bin/unnamed");
}

TEST_CASE("a profiler assigns consecutive target IDs", "[profiler]") {
    Profiler profiler;

    const auto first =
        profiler.AddProfilingTarget(MakeTarget("first", "/bin/first"));
    REQUIRE(first == 0);
    const auto second =
        profiler.AddProfilingTarget(MakeTarget("second", "/bin/second"));
    REQUIRE(second == 1);
    const auto third =
        profiler.AddProfilingTarget(MakeTarget("third", "/bin/third"));
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

    REQUIRE(profiler.AddProfilingTarget(MakeTarget("later", "/bin/later")) ==
            0);

    REQUIRE(targets.size() == 1);
    REQUIRE(targets.at(0).GetName() == std::optional<std::string>{"later"});
}

TEST_CASE("duplicate target configurations receive distinct IDs",
          "[profiler]") {
    Profiler profiler;
    const auto target = MakeTarget("worker", "/bin/worker");
    const auto first = profiler.AddProfilingTarget(target);
    const auto second = profiler.AddProfilingTarget(target);
    REQUIRE(first != second);
    REQUIRE(profiler.GetTargets().size() == 2);
}
}  // namespace
