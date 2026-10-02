#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <process/process.hpp>
#include <utility>

#ifndef HEMLOK_PROCESS_FIXTURE_PATH
#error "HEMLOK_PROCESS_FIXTURE_PATH must identify the process test fixture"
#endif

namespace {
using hemlok::process::Process;

TEST_CASE("starting a missing executable throws", "[process]") {
    const auto missing_executable =
        std::filesystem::current_path() /
        "hemlok-process-test-executable-that-does-not-exist";
    REQUIRE_FALSE(std::filesystem::exists(missing_executable));

    REQUIRE_THROWS(Process::startProcessFromExecutable(missing_executable));
}

TEST_CASE("a running process can be terminated", "[process]") {
    auto process = Process::startProcessFromExecutable(
        std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});

    SECTION("terminate the process") { REQUIRE_NOTHROW(process.terminate()); }
    SECTION("move the process value") {
        Process owned_process = std::move(process);
        REQUIRE_NOTHROW(owned_process.terminate());
    }
    SECTION("move-assign over another running process") {
        auto other = Process::startProcessFromExecutable(
            std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});
        other = std::move(process);
        REQUIRE_NOTHROW(other.terminate());
    }
}
}  // namespace
