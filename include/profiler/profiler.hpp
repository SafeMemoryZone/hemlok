#ifndef HEMLOK_PROFILER_PROFILER_HPP_
#define HEMLOK_PROFILER_PROFILER_HPP_

#include <expected>
#include <process/process.hpp>
#include <profiler/sampling_worker.hpp>
#include <profiler/target.hpp>
#include <unordered_map>

namespace hemlok::profiler {
// Main-thread facade. Process operations and their errors arrive via Poll().
class Profiler {
public:
    Profiler();
    TargetId AddTarget(Target target);
    // Success means the command was queued, not that the process has launched.
    std::expected<RunId, process::ProcessError> StartRun(TargetId target);
    // Termination failures are reported in the run overview after Poll().
    std::expected<void, process::ProcessError> TerminateRun(RunId run);
    // Drains the publication queue; never queries or controls processes.
    void Poll();
    const std::unordered_map<TargetId, Target>& GetTargets() const;
    const std::unordered_map<RunId, RunOverview>& GetRunOverviews() const;
    const std::unordered_map<RunId, std::shared_ptr<const CompletedRun>>&
    GetCompletedRuns() const;
    // The ID must exist. Returns null while the run is ongoing.
    std::shared_ptr<const CompletedRun> GetCompletedRun(RunId run) const;

private:
    static TargetId GenerateTargetId();
    static RunId GenerateRunId();
    void ApplyPublication(WorkerPublication publication);
    std::unordered_map<TargetId, Target> targets_;
    std::unordered_map<RunId, std::shared_ptr<const CompletedRun>>
        completed_runs_;
    std::unordered_map<RunId, RunOverview> run_overviews_;
    CommandChannel commands_;
    PublicationChannel publications_;
    SamplingWorker worker_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_PROFILER_HPP_
