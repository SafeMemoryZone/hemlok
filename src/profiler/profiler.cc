#include <cassert>
#include <profiler/profiler.hpp>
#include <utility>

namespace hemlok::profiler {
TargetId Profiler::AddTarget(ProfilingTarget target) {
    const TargetId id = next_target_id_;
    targets_.emplace(id, std::move(target));
    ++next_target_id_;
    return id;
}

std::expected<RunId, ProfilerError> Profiler::StartRun(TargetId target) {
    const auto entry = targets_.find(target);
    assert(entry != targets_.end());
    auto process = process::Process::CreateFromExecutable(
        entry->second.GetExecutablePath());
    if (!process) {
        return std::unexpected(ProfilerError::kProcessStartFailed);
    }
    auto run = ProfilingRun::Create(std::move(*process));
    if (!run) {
        return std::unexpected(run.error() ==
                                       ProfilingRunError::kProcessTerminated
                                   ? ProfilerError::kProcessTerminated
                                   : ProfilerError::kStatusQueryFailed);
    }
    const RunId id = next_run_id_;
    profiling_runs_.emplace(id, RunEntry{target, std::move(*run)});
    next_run_id_++;
    return id;
}

std::expected<void, ProfilerError> Profiler::TerminateRun(RunId run) {
    const auto entry = profiling_runs_.find(run);
    assert(entry != profiling_runs_.end());
    if (!entry->second.profiling_run.Terminate()) {
        return std::unexpected(ProfilerError::kTerminationFailed);
    }
    return {};
}

void Profiler::Poll() {
    for (auto& [id, entry] : profiling_runs_) {
        entry.profiling_run.Poll();
    }
}

const std::unordered_map<RunId, RunEntry>& Profiler::GetRuns() const {
    return profiling_runs_;
}

const std::unordered_map<TargetId, ProfilingTarget>& Profiler::GetTargets()
    const {
    return targets_;
}
}  // namespace hemlok::profiler
