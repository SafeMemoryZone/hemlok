#ifndef HEMLOK_PROFILER_WORKER_OPERATIONS_HPP_
#define HEMLOK_PROFILER_WORKER_OPERATIONS_HPP_

#include <functional>
#include <profiler/sampling_worker.hpp>

namespace hemlok::profiler::detail {
using ProcessStatusQuery =
    std::function<std::expected<bool, process::ProcessError>(ActiveRun&)>;
using ProcessTermination =
    std::function<std::expected<void, process::ProcessError>(ActiveRun&)>;

void FillCompletedRunOverviews(WorkerPublication& publication);

// These synchronous operations run on the worker thread. Callables are used
// only for the duration of the call; ownership stays in active_runs.
void TerminateActiveRun(
    ActiveRunMap& active_runs, PublicationChannel& publications, RunId id,
    const ProcessStatusQuery& query = &ActiveRun::IsProcessRunning,
    const ProcessTermination& terminate = &ActiveRun::TerminateProcess);
void PollActiveRuns(
    ActiveRunMap& active_runs, PublicationChannel& publications,
    const ProcessStatusQuery& query = &ActiveRun::IsProcessRunning);
void CompleteActiveRuns(
    ActiveRunMap& active_runs, PublicationChannel& publications,
    const ProcessTermination& terminate = &ActiveRun::TerminateProcess);
}  // namespace hemlok::profiler::detail

#endif  // HEMLOK_PROFILER_WORKER_OPERATIONS_HPP_
