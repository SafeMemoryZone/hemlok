#ifndef HEMLOK_PROFILER_ACTIVE_RUN_HPP_
#define HEMLOK_PROFILER_ACTIVE_RUN_HPP_

#include <expected>
#include <process/process.hpp>
#include <profiler/completed_run.hpp>
#include <profiler/run_overview.hpp>
#include <profiler/sampling_data.hpp>
#include <sampling/sampling_session.hpp>

namespace hemlok::profiler {
class ActiveRun {
public:
    // The session must already be attached to the process.
    static ActiveRun Create(RunId run, TargetId target,
                            process::Process process,
                            sampling::SamplingSession session);
    RunId GetRunId() const;
    TargetId GetTargetId() const;
    std::expected<bool, process::ProcessError> IsProcessRunning();
    std::expected<void, process::ProcessError> TerminateProcess();
    // Moves samples recorded since the previous call into the run's data.
    void CollectSamples();
    RunOverview GetOverview() const;
    // Stops sampling and consumes the results; the worker removes this run
    // after completion.
    CompletedRun Complete(RunOutcome outcome, std::optional<RunError> error) &&;

private:
    ActiveRun(RunId run, TargetId target, process::Process process,
              sampling::SamplingSession session);
    RunId run_id_;
    TargetId target_id_;
    process::Process process_;
    TimePoint start_time_;
    sampling::SamplingSession session_;
    SamplingData sampling_data_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_ACTIVE_RUN_HPP_
