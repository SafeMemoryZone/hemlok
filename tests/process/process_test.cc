#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <process/process.hpp>
#include <utility>

#ifndef HEMLOK_PROCESS_FIXTURE_PATH
#error "HEMLOK_PROCESS_FIXTURE_PATH must identify the process test fixture"
#endif

namespace {
using hemlok::process::Process;
using hemlok::process::ProcessError;

TEST_CASE("starting a missing executable returns a file-not-found error",
          "[process]") {
    std::filesystem::path missing_executable;
    SECTION("missing file") {
        missing_executable =
            std::filesystem::current_path() /
            "hemlok-process-test-executable-that-does-not-exist";
        REQUIRE_FALSE(std::filesystem::exists(missing_executable));
    }
    SECTION("empty path") { missing_executable = ""; }
    SECTION("a parent component is a file") {
        missing_executable =
            std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH} / "child";
    }

    const auto result = Process::StartProcessFromExecutable(missing_executable);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ProcessError::kFileNotFound);
}

TEST_CASE("a running process can be terminated", "[process]") {
    auto process = Process::StartProcessFromExecutable(
        std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});

    REQUIRE(process.has_value());
    SECTION("terminate through the expected result") {
        REQUIRE(process->Terminate().has_value());
    }
    SECTION("move the process value") {
        Process owned_process = std::move(*process);
        REQUIRE(owned_process.Terminate().has_value());
    }
    SECTION("move-assign over another running process") {
        auto other = Process::StartProcessFromExecutable(
            std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});
        REQUIRE(other.has_value());
        *other = std::move(*process);
        REQUIRE(other->Terminate().has_value());
    }
}
}  // namespace
