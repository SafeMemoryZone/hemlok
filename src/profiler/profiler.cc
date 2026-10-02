#include <profiler/profiler.hpp>
#include <utility>

namespace hemlok::profiler {
void Profiler::AddProfilingTarget(ProfilingTarget target) {
    targets_.emplace(next_target_id_++, std::move(target));
}

const std::unordered_map<TargetId, ProfilingTarget>& Profiler::GetTargets()
    const {
    return targets_;
}
}  // namespace hemlok::profiler
