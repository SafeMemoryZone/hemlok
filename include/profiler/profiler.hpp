#ifndef HEMLOK_PROFILER_PROFILER_HPP_
#define HEMLOK_PROFILER_PROFILER_HPP_

#include <cstdint>
#include <expected>
#include <profiler/profiling_run.hpp>
#include <profiler/profiling_target.hpp>
#include <unordered_map>

namespace hemlok::profiler {
using TargetId = std::uint64_t;
using RunId = std::uint64_t;

enum class ProfilerError {
    kProcessStartFailed,
    kProcessTerminated,
    kStatusQueryFailed,
    kTerminationFailed,
};

struct RunEntry {
    TargetId id;
    ProfilingRun profiling_run;
};

class Profiler {
public:
    Profiler() = default;

    TargetId AddTarget(ProfilingTarget target);
    std::expected<RunId, ProfilerError> StartRun(TargetId target);
    std::expected<void, ProfilerError> TerminateRun(RunId run);
    // Each run retains its own polling error.
    void Poll();
    const std::unordered_map<TargetId, ProfilingTarget>& GetTargets() const;
    const std::unordered_map<RunId, RunEntry>& GetRuns() const;

private:
    TargetId next_target_id_ = 0;
    RunId next_run_id_ = 0;

    std::unordered_map<TargetId, ProfilingTarget> targets_;
    std::unordered_map<RunId, RunEntry> profiling_runs_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_PROFILER_HPP_
