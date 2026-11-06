#ifndef HEMLOK_PROFILER_COMPLETED_RUN_HPP_
#define HEMLOK_PROFILER_COMPLETED_RUN_HPP_

#include <optional>
#include <process/process.hpp>
#include <profiler/run_types.hpp>
#include <profiler/sampling_data.hpp>

namespace hemlok::profiler {
// Published once by the worker; consumers read it through const references.
struct CompletedRun {
    RunId run_id;
    TargetId target_id;
    RunOutcome outcome;
    std::optional<RunError> error;
    std::optional<TimePoint> start_time;
    TimePoint finish_time;
    SamplingData sampling_data;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_COMPLETED_RUN_HPP_
