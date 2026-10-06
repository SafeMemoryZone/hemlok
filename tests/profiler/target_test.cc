#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <optional>
#include <profiler/target.hpp>
#include <string>

namespace {
using hemlok::profiler::Target;
using hemlok::profiler::TargetError;

TEST_CASE("a profiling target retains its name and executable path",
          "[profiling-target]") {
    const std::filesystem::path path = "/path containing spaces/program";
    const auto target = Target::Create("worker", path);
    REQUIRE(target.has_value());

    REQUIRE(target->GetName() == std::optional<std::string>{"worker"});
    REQUIRE(target->GetExecutablePath() == path);
}

TEST_CASE("a profiling target can be unnamed", "[profiling-target]") {
    const auto target = Target::Create(std::nullopt, "/bin/program");
    REQUIRE(target.has_value());

    REQUIRE_FALSE(target->GetName().has_value());
    REQUIRE(target->GetExecutablePath() == "/bin/program");
}

TEST_CASE("a profiling target rejects blank supplied names",
          "[profiling-target]") {
    for (const std::string name : {"", " ", "\t\n\r\f\v"}) {
        const auto target = Target::Create(name, "/bin/program");
        REQUIRE_FALSE(target.has_value());
        REQUIRE(target.error() == TargetError::kBlankName);
    }
}

TEST_CASE("a profiling target preserves nonblank names", "[profiling-target]") {
    const auto target = Target::Create(" worker ", "/bin/program");
    REQUIRE(target.has_value());
    REQUIRE(target->GetName() == " worker ");
}

TEST_CASE("a profiling target rejects empty and relative paths",
          "[profiling-target]") {
    SECTION("empty path") {
        const auto target = Target::Create(std::nullopt, "");
        REQUIRE_FALSE(target.has_value());
        REQUIRE(target.error() == TargetError::kEmptyPath);
    }
    SECTION("relative path") {
        const auto target = Target::Create(std::nullopt, "bin/program");
        REQUIRE_FALSE(target.has_value());
        REQUIRE(target.error() == TargetError::kRelativePath);
    }
}

TEST_CASE("target configuration does not require an existing executable",
          "[profiling-target]") {
    const auto path = std::filesystem::current_path() /
                      "hemlok-target-test-executable-that-does-not-exist";
    REQUIRE_FALSE(std::filesystem::exists(path));
    const auto target = Target::Create(std::nullopt, path);
    REQUIRE(target.has_value());
    REQUIRE(target->GetExecutablePath() == path);
}
}  // namespace
