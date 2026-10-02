#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>
#include <profiler/profiler.hpp>
#include <string>

namespace {
using hemlok::profiler::Profiler;
using hemlok::profiler::ProfilingTarget;

TEST_CASE("a new profiler has no targets", "[profiler]") {
    const Profiler profiler;

    REQUIRE(profiler.GetTargets().empty());
}

TEST_CASE("a profiler stores a named target", "[profiler]") {
    Profiler profiler;
    const std::filesystem::path executable_path = "/usr/bin/example";

    profiler.AddProfilingTarget(ProfilingTarget{"example", executable_path});

    const auto& targets = profiler.GetTargets();
    REQUIRE(targets.size() == 1);
    REQUIRE(targets.contains(0));
    REQUIRE(targets.at(0).name == std::optional<std::string>{"example"});
    REQUIRE(targets.at(0).executable_path == executable_path);
}

TEST_CASE("a profiler stores a target without a name", "[profiler]") {
    Profiler profiler;

    profiler.AddProfilingTarget(
        ProfilingTarget{std::nullopt, "/usr/bin/unnamed"});

    const auto& target = profiler.GetTargets().at(0);
    REQUIRE_FALSE(target.name.has_value());
    REQUIRE(target.executable_path == "/usr/bin/unnamed");
}

TEST_CASE("a profiler assigns consecutive target IDs", "[profiler]") {
    Profiler profiler;

    profiler.AddProfilingTarget(ProfilingTarget{"first", "/bin/first"});
    profiler.AddProfilingTarget(ProfilingTarget{"second", "/bin/second"});
    profiler.AddProfilingTarget(ProfilingTarget{"third", "/bin/third"});

    const auto& targets = profiler.GetTargets();
    REQUIRE(targets.size() == 3);
    REQUIRE(targets.at(0).name == std::optional<std::string>{"first"});
    REQUIRE(targets.at(1).name == std::optional<std::string>{"second"});
    REQUIRE(targets.at(2).name == std::optional<std::string>{"third"});
}

TEST_CASE("existing target references observe newly added targets",
          "[profiler]") {
    Profiler profiler;
    const auto& targets = profiler.GetTargets();

    profiler.AddProfilingTarget(ProfilingTarget{"later", "/bin/later"});

    REQUIRE(targets.size() == 1);
    REQUIRE(targets.at(0).name == std::optional<std::string>{"later"});
}
}  // namespace
