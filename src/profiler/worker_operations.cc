#include <profiler/worker_operations.hpp>
#include <utility>

namespace hemlok::profiler::detail {
void FillCompletedRunOverviews(WorkerPublication& publication) {
    for (const auto& run : publication.completed_runs) {
        RunStatus status = RunStatus::kFailed;
        switch (run.outcome) {
            case RunOutcome::kFinished:
                status = RunStatus::kFinished;
                break;
            case RunOutcome::kTerminated:
                status = RunStatus::kTerminated;
                break;
            case RunOutcome::kFailed:
                break;
        }
        publication.changed_overviews.push_back({run.run_id, run.target_id,
                                                 status, run.start_time,
                                                 run.finish_time, run.error});
    }
}

CompletedRun CompleteUnsampledRun(RunId run, TargetId target,
                                  TimePoint start_time,
                                  process::Process& process,
                                  sampling::SamplingError error,
                                  const LaunchedProcessQuery& query,
                                  const LaunchedProcessTermination& terminate) {
    CompletedRun completed_run{.run_id = run,
                               .target_id = target,
                               .outcome = RunOutcome::kFailed,
                               .error = error,
                               .start_time = start_time,
                               .finish_time = {},
                               .sampling_data = {}};
    const auto process_status = query(process);
    if (process_status.has_value() && !process_status.value()) {
        // The process exited before sampling could attach. That is a natural
        // finish without samples, not a sampling failure.
        completed_run.outcome = RunOutcome::kFinished;
        completed_run.error = std::nullopt;
    } else {
        const auto termination = terminate(process);
        if (!termination.has_value()) {
            // The process may still be running unsampled; report that
            // instead of the sampling error.
            completed_run.error = termination.error();
        }
    }
    completed_run.finish_time = std::chrono::system_clock::now();
    return completed_run;
}

void TerminateActiveRun(ActiveRunMap& active_runs,
                        PublicationChannel& publications, RunId id,
                        const ProcessStatusQuery& query,
                        const ProcessTermination& terminate) {
    const auto entry = active_runs.find(id);
    if (entry == active_runs.end()) {
        // The run already completed, or a previous termination command handled
        // it.
        return;
    }

    auto& run = entry->second;
    const auto process_status = query(run);
    if (process_status.has_value()) {
        if (!process_status.value()) {
            // The status check succeeded and the process has already exited.
            // Record a natural finish rather than trying to terminate it.
            auto completed_run =
                std::move(run).Complete(RunOutcome::kFinished, std::nullopt);
            WorkerPublication publication;
            publication.completed_runs.push_back(std::move(completed_run));
            FillCompletedRunOverviews(publication);
            publications.Send(std::move(publication));
            active_runs.erase(entry);
            return;
        }
    }

    // Either the process is still running or its status could not be checked.
    // In both cases, honor the explicit request to terminate it.
    const auto termination = terminate(run);
    if (!termination.has_value()) {
        // Termination failed. Keep checking the process and allow another
        // termination request.
        auto overview = run.GetOverview();
        overview.error = termination.error();
        publications.Send({{std::move(overview)}, {}});
        return;
    }

    // Termination succeeded. Publish the final result, then release the active
    // run.
    auto completed_run =
        std::move(run).Complete(RunOutcome::kTerminated, std::nullopt);
    WorkerPublication publication;
    publication.completed_runs.push_back(std::move(completed_run));
    FillCompletedRunOverviews(publication);
    publications.Send(std::move(publication));
    active_runs.erase(entry);
}

void PollActiveRuns(ActiveRunMap& active_runs, PublicationChannel& publications,
                    const ProcessStatusQuery& query) {
    for (auto entry = active_runs.begin(); entry != active_runs.end();) {
        auto& run = entry->second;
        const auto process_status = query(run);
        if (!process_status.has_value()) {
            // The status check failed. We do not know whether the process
            // exited. Report the error, keep the run active, and check
            // again next iteration.
            auto overview = run.GetOverview();
            overview.error = process_status.error();
            publications.Send({{std::move(overview)}, {}});
            ++entry;
            continue;
        }

        if (!process_status.value()) {
            // The status check succeeded and the process has exited
            // naturally.
            auto completed_run =
                std::move(run).Complete(RunOutcome::kFinished, std::nullopt);
            WorkerPublication publication;
            publication.completed_runs.push_back(std::move(completed_run));
            FillCompletedRunOverviews(publication);
            publications.Send(std::move(publication));
            // erase() returns the next iterator; do not increment it again.
            entry = active_runs.erase(entry);
            continue;
        }

        // The status check succeeded and the process is still running.
        // A fresh overview also clears any error from a previous failed
        // check.
        publications.Send({{run.GetOverview()}, {}});
        ++entry;
    }
}

void CompleteActiveRuns(ActiveRunMap& active_runs,
                        PublicationChannel& publications,
                        const ProcessTermination& terminate) {
    for (auto& [id, run] : active_runs) {
        const auto termination = terminate(run);
        RunOutcome outcome = RunOutcome::kTerminated;
        std::optional<RunError> error;
        if (!termination.has_value()) {
            // Cleanup could not terminate the process; retain the failure in
            // its result.
            outcome = RunOutcome::kFailed;
            error = termination.error();
        }
        auto completed_run = std::move(run).Complete(outcome, error);
        WorkerPublication publication;
        publication.completed_runs.push_back(std::move(completed_run));
        FillCompletedRunOverviews(publication);
        publications.Send(std::move(publication));
    }
    active_runs.clear();
}
}  // namespace hemlok::profiler::detail
