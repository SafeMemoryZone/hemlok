#include <profiler/profiler.hpp>
#include <utility>

namespace hemlok::profiler {
TargetId Profiler::AddProfilingTarget(ProfilingTarget target) {
    const TargetId id = next_target_id_;
    targets_.emplace(id, std::move(target));
    ++next_target_id_;
    return id;
}

const std::unordered_map<TargetId, ProfilingTarget>& Profiler::GetTargets()
    const {
    return targets_;
}
}  // namespace hemlok::profiler
