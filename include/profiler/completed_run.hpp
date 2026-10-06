#ifndef HEMLOK_PROFILER_COMPLETED_RUN_HPP_
#define HEMLOK_PROFILER_COMPLETED_RUN_HPP_

#include <optional>
#include <profiler/run_types.hpp>
#include <profiler/sampling_results.hpp>

namespace hemlok::profiler {
struct CompletedRun {
    RunId run_id;
    TargetId target_id;
    RunOutcome outcome;
    std::optional<Error> error;
    std::optional<TimePoint> start_time;
    TimePoint finish_time;
    SamplingResults sampling_results;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_COMPLETED_RUN_HPP_
