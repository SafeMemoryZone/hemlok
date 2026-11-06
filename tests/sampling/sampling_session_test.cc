#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <limits>
#include <process/process.hpp>
#include <sampling/sampling_session.hpp>
#include <thread>

namespace {
using hemlok::process::Process;
using hemlok::sampling::SamplingError;
using hemlok::sampling::SamplingSession;

TEST_CASE("a session attributes samples to the busy function", "[sampling]") {
    auto process =
        Process::CreateFromExecutable(HEMLOK_BUSY_PROCESS_FIXTURE_PATH);
    REQUIRE(process);
    auto session = SamplingSession::Attach(process->GetPid());
    REQUIRE(session);
    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    const auto samples = session->DrainSamples();
    REQUIRE(process->Terminate());

    std::size_t busy_samples = 0;
    for (const auto& sample : samples) {
        if (sample.function_name == "HemlokBusyLoop()") ++busy_samples;
    }
    // The loop is not the only code that runs, so require a majority.
    REQUIRE(busy_samples * 2 > samples.size());
}

TEST_CASE("detaching stops sampling but keeps the process running",
          "[sampling]") {
    auto process =
        Process::CreateFromExecutable(HEMLOK_BUSY_PROCESS_FIXTURE_PATH);
    REQUIRE(process);
    auto session = SamplingSession::Attach(process->GetPid());
    REQUIRE(session);
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    session->Detach();
    session->Detach();
    REQUIRE_FALSE(session->DrainSamples().empty());
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    REQUIRE(session->DrainSamples().empty());
    const auto running = process->IsRunning();
    REQUIRE(running);
    REQUIRE(*running);
    REQUIRE(process->Terminate());
}

TEST_CASE("attaching to a missing process fails", "[sampling]") {
    const auto session =
        SamplingSession::Attach(std::numeric_limits<int>::max());
    REQUIRE_FALSE(session);
    REQUIRE(session.error() == SamplingError::kProcessNotFound);
}

TEST_CASE("attaching to a multi-threaded process fails", "[sampling]") {
    auto process =
        Process::CreateFromExecutable(HEMLOK_THREADED_PROCESS_FIXTURE_PATH);
    REQUIRE(process);
    // The fixture starts its second thread shortly after launch.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    auto session = SamplingSession::Attach(process->GetPid());
    while (session && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        session = SamplingSession::Attach(process->GetPid());
    }
    REQUIRE(process->Terminate());
    REQUIRE_FALSE(session);
    REQUIRE(session.error() == SamplingError::kMultiThreaded);
}
}  // namespace
