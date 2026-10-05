#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <process/process.hpp>
#include <thread>
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

    const auto result = Process::CreateFromExecutable(missing_executable);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ProcessError::kFileNotFound);
}

TEST_CASE("a running process can be terminated", "[process]") {
    auto process = Process::CreateFromExecutable(
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
        auto other = Process::CreateFromExecutable(
            std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});
        REQUIRE(other.has_value());
        *other = std::move(*process);
        REQUIRE(other->Terminate().has_value());
    }
}
TEST_CASE("process status reports running and terminated children",
          "[process]") {
    auto process = Process::CreateFromExecutable(
        std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});
    REQUIRE(process.has_value());

    const auto running = process->IsRunning();
    REQUIRE(running.has_value());
    REQUIRE(*running);

    REQUIRE(process->Terminate().has_value());
    for (int query = 0; query < 2; ++query) {
        const auto stopped = process->IsRunning();
        REQUIRE(stopped.has_value());
        REQUIRE_FALSE(*stopped);
    }
}

TEST_CASE("process status observes natural exit", "[process]") {
    auto process = Process::CreateFromExecutable(
        std::filesystem::path{HEMLOK_EXITING_PROCESS_FIXTURE_PATH});
    REQUIRE(process.has_value());

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    auto running = process->IsRunning();
    REQUIRE(running.has_value());
    while (*running && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        running = process->IsRunning();
        REQUIRE(running.has_value());
    }
    REQUIRE_FALSE(*running);
    const auto repeated = process->IsRunning();
    REQUIRE(repeated.has_value());
    REQUIRE_FALSE(*repeated);
}

TEST_CASE("process status works after ownership is moved", "[process]") {
    auto process = Process::CreateFromExecutable(
        std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});
    REQUIRE(process.has_value());

    SECTION("move construction") {
        Process owner = std::move(*process);
        const auto running = owner.IsRunning();
        REQUIRE(running.has_value());
        REQUIRE(*running);
    }
    SECTION("move assignment") {
        auto owner = Process::CreateFromExecutable(
            std::filesystem::path{HEMLOK_PROCESS_FIXTURE_PATH});
        REQUIRE(owner.has_value());
        *owner = std::move(*process);
        const auto running = owner->IsRunning();
        REQUIRE(running.has_value());
        REQUIRE(*running);
    }
}
}  // namespace
