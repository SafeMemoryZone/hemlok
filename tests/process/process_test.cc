#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <process/process.hpp>

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

    REQUIRE_NOTHROW(process.terminate());
}
}  // namespace
