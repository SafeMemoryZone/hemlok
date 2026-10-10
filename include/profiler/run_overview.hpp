#ifndef HEMLOK_PROFILER_RUN_OVERVIEW_HPP_
#define HEMLOK_PROFILER_RUN_OVERVIEW_HPP_

#include <optional>
#include <process/process.hpp>
#include <profiler/run_types.hpp>

namespace hemlok::profiler {
struct RunOverview {
    RunId run_id;
    TargetId target_id;
    RunStatus status;
    std::optional<TimePoint> start_time;
    std::optional<TimePoint> finish_time;
    std::optional<process::ProcessError> error;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_RUN_OVERVIEW_HPP_
