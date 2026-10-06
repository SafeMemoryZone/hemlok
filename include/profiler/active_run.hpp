#ifndef HEMLOK_PROFILER_ACTIVE_RUN_HPP_
#define HEMLOK_PROFILER_ACTIVE_RUN_HPP_

#include <expected>
#include <process/process.hpp>
#include <profiler/completed_run.hpp>
#include <profiler/run_overview.hpp>

namespace hemlok::profiler {
class ActiveRun {
public:
    static ActiveRun Create(RunId run, TargetId target,
                            process::Process process);
    RunId GetRunId() const;
    TargetId GetTargetId() const;
    std::expected<bool, process::ProcessError> IsProcessRunning();
    std::expected<void, process::ProcessError> TerminateProcess();
    // Placeholder until sampling is implemented.
    std::expected<void, process::ProcessError> CollectSamples();
    RunOverview GetOverview() const;
    // Consumes results; the worker removes this run after completion.
    CompletedRun Complete(RunOutcome outcome,
                          std::optional<process::ProcessError> error);

private:
    ActiveRun(RunId run, TargetId target, process::Process process);
    RunId run_id_;
    TargetId target_id_;
    process::Process process_;
    TimePoint start_time_;
    SamplingResults sampling_results_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_ACTIVE_RUN_HPP_
