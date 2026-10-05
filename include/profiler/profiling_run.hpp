#ifndef HEMLOK_PROFILER_PROFILING_RUN_HPP_
#define HEMLOK_PROFILER_PROFILING_RUN_HPP_
#include <chrono>
#include <expected>
#include <optional>
#include <process/process.hpp>

namespace hemlok::profiler {
enum ProfilingRunError {
    kProcessTerminated,
    kStatusQueryFailed,
};

class ProfilingRun {
public:
    static std::expected<ProfilingRun, ProfilingRunError> Create(
        process::Process process);

    std::expected<void, process::ProcessError> Terminate();
    // Refreshes process state and records finish time when exit is observed.
    // Query failures preserve cached state and are exposed by GetPollError().
    // Successful polls clear the error; finished runs are left unchanged.
    void Poll();

    // Reads cached state without querying the process.
    bool IsRunning() const;
    std::optional<process::ProcessError> GetPollError() const;
    std::chrono::system_clock::time_point GetStartTime() const;
    std::optional<std::chrono::system_clock::time_point> GetFinishTime() const;

private:
    explicit ProfilingRun(process::Process process);

    process::Process process_;
    std::chrono::system_clock::time_point start_time_;
    std::optional<std::chrono::system_clock::time_point> finish_time_;
    std::optional<process::ProcessError> poll_error_;
};
}  // namespace hemlok::profiler
#endif  // HEMLOK_PROFILER_PROFILING_RUN_HPP_
