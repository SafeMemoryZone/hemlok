#include <profiler/profiling_run.hpp>
#include <utility>

namespace hemlok::profiler {
ProfilingRun::ProfilingRun(process::Process process)
    : process_(std::move(process)),
      start_time_(std::chrono::system_clock::now()) {}

std::expected<ProfilingRun, ProfilingRunError> ProfilingRun::StartProfiling(
    process::Process process) {
    const auto running = process.IsRunning();
    if (!running) {
        return std::unexpected(ProfilingRunError::kStatusQueryFailed);
    }
    if (!*running) {
        return std::unexpected(ProfilingRunError::kProcessTerminated);
    }
    return ProfilingRun(std::move(process));
}

void ProfilingRun::Poll() {
    if (!IsRunning()) {
        return;
    }
    const auto running = process_.IsRunning();
    if (!running.has_value()) {
        poll_error_ = running.error();
        return;
    }
    poll_error_.reset();
    if (!*running) {
        finish_time_ = std::chrono::system_clock::now();
    }
}

bool ProfilingRun::IsRunning() const { return !finish_time_.has_value(); }

std::optional<process::ProcessError> ProfilingRun::GetPollError() const {
    return poll_error_;
}

std::expected<void, process::ProcessError> ProfilingRun::Terminate() {
    Poll();
    if (!IsRunning()) {
        return {};
    }
    auto termination = process_.Terminate();
    if (termination.has_value()) {
        finish_time_ = std::chrono::system_clock::now();
        poll_error_.reset();
    }
    return termination;
}

std::chrono::system_clock::time_point ProfilingRun::GetStartTime() const {
    return start_time_;
}

std::optional<std::chrono::system_clock::time_point>
ProfilingRun::GetFinishTime() const {
    return finish_time_;
}
}  // namespace hemlok::profiler
