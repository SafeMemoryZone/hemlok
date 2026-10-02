#ifndef HEMLOK_PROFILER_PROFILER_HPP_
#define HEMLOK_PROFILER_PROFILER_HPP_

#include <cstdint>
#include <profiler/profiling_target.hpp>
#include <unordered_map>

namespace hemlok::profiler {
using TargetId = std::uint64_t;

class Profiler {
public:
    Profiler() = default;

    void AddProfilingTarget(ProfilingTarget target);
    const std::unordered_map<TargetId, ProfilingTarget>& GetTargets() const;

private:
    TargetId next_target_id_ = 0;
    std::unordered_map<TargetId, ProfilingTarget> targets_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_PROFILER_HPP_
