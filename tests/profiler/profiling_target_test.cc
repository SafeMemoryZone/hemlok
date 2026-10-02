#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>
#include <profiler/profiling_target.hpp>
#include <string>

namespace {
using hemlok::profiler::ProfilingTarget;

TEST_CASE("a profiling target retains its name and executable path",
          "[profiling-target]") {
    const std::filesystem::path path = "/path containing spaces/program";
    const ProfilingTarget target{"worker", path};

    REQUIRE(target.name == std::optional<std::string>{"worker"});
    REQUIRE(target.executable_path == path);
}

TEST_CASE("a profiling target can be unnamed", "[profiling-target]") {
    const ProfilingTarget target{std::nullopt, "/bin/program"};

    REQUIRE_FALSE(target.name.has_value());
    REQUIRE(target.executable_path == "/bin/program");
}
}  // namespace
