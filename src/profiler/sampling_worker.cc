#include <process/process.hpp>
#include <profiler/sampling_worker.hpp>
#include <utility>

namespace hemlok::profiler {
SamplingWorker::SamplingWorker(CommandChannel& commands,
                               PublicationChannel& publications)
    : commands_(commands),
      publications_(publications),
      thread_([this](std::stop_token stop) { RunLoop(stop); }) {}

SamplingWorker::~SamplingWorker() { Shutdown(); }

void SamplingWorker::Shutdown() {
    thread_.request_stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void SamplingWorker::FillCompletedRunOverviews(WorkerPublication& publication) {
    for (const auto& run : publication.completed_runs) {
        RunStatus status = RunStatus::kFailed;
        switch (run->outcome) {
            case RunOutcome::kFinished:
                status = RunStatus::kFinished;
                break;
            case RunOutcome::kTerminated:
                status = RunStatus::kTerminated;
                break;
            case RunOutcome::kFailed:
                break;
        }
        publication.changed_overviews.push_back({run->run_id, run->target_id,
                                                 status, run->start_time,
                                                 run->finish_time, run->error});
    }
}

void SamplingWorker::Publish(WorkerPublication publication) {
    publications_.Send(std::move(publication));
}

void SamplingWorker::HandleCommand(WorkerCommand command) {
    // Start commands launch a new process; the remaining command type
    // terminates a run.
    const auto* start = std::get_if<StartRunCommand>(&command);
    if (start != nullptr) {
        auto process =
            process::Process::CreateFromExecutable(start->executable_path);
        if (!process.has_value()) {
            // Launch failed. Retain the error as a completed run with no start
            // time.
            CompletedRun completed_run{
                .run_id = start->run_id,
                .target_id = start->target_id,
                .outcome = RunOutcome::kFailed,
                .error = process.error(),
                .start_time = std::nullopt,
                .finish_time = std::chrono::system_clock::now(),
                .sampling_results = {}};
            WorkerPublication publication{
                .completed_runs = {std::make_shared<const CompletedRun>(
                    std::move(completed_run))}};
            FillCompletedRunOverviews(publication);
            Publish(std::move(publication));
            return;
        }

        // Launch succeeded. Publish the running overview and keep the process
        // here.
        auto run = ActiveRun::Create(start->run_id, start->target_id,
                                     std::move(process.value()));
        Publish({{run.GetOverview()}, {}});
        active_runs_.emplace(start->run_id, std::move(run));
        return;
    }

    const auto id = std::get<TerminateRunCommand>(command).run_id;
    const auto entry = active_runs_.find(id);
    if (entry == active_runs_.end()) {
        // The run already completed, or a previous termination command handled
        // it.
        return;
    }

    auto& run = entry->second;
    const auto process_status = run.IsProcessRunning();
    if (process_status.has_value()) {
        if (!process_status.value()) {
            // The status check succeeded and the process has already exited.
            // Record a natural finish rather than trying to terminate it.
            auto completed_run =
                std::move(run).Complete(RunOutcome::kFinished, std::nullopt);
            WorkerPublication publication{
                .completed_runs = {std::make_shared<const CompletedRun>(
                    std::move(completed_run))}};
            FillCompletedRunOverviews(publication);
            Publish(std::move(publication));
            active_runs_.erase(entry);
            return;
        }
    }

    // Either the process is still running or its status could not be checked.
    // In both cases, honor the explicit request to terminate it.
    const auto termination = run.TerminateProcess();
    if (!termination.has_value()) {
        // Termination failed. Keep checking the process and allow another
        // termination request.
        auto overview = run.GetOverview();
        overview.error = termination.error();
        Publish({{std::move(overview)}, {}});
        return;
    }

    // Termination succeeded. Publish the final result, then release the active
    // run.
    auto completed_run =
        std::move(run).Complete(RunOutcome::kTerminated, std::nullopt);
    WorkerPublication publication{
        .completed_runs = {
            std::make_shared<const CompletedRun>(std::move(completed_run))}};
    FillCompletedRunOverviews(publication);
    Publish(std::move(publication));
    active_runs_.erase(entry);
}

void SamplingWorker::RunLoop(std::stop_token stop) {
    while (!stop.stop_requested()) {
        if (active_runs_.empty()) {
            // With no active processes, wait until a command arrives or
            // shutdown starts.
            commands_.Wait(stop);
        } else {
            // Active processes need periodic status checks. Commands can wake
            // us sooner.
            commands_.Wait(stop, std::chrono::milliseconds{10});
        }
        if (stop.stop_requested()) {
            // A shutdown request also wakes Wait(); proceed to process cleanup
            // below.
            break;
        }

        // Handle queued commands in order before checking the remaining
        // processes.
        for (auto& command : commands_.TakeAll()) {
            HandleCommand(std::move(command));
        }
        for (auto entry = active_runs_.begin(); entry != active_runs_.end();) {
            auto& run = entry->second;
            const auto process_status = run.IsProcessRunning();
            if (!process_status.has_value()) {
                // The status check failed. We do not know whether the process
                // exited. Report the error, keep the run active, and check
                // again next iteration.
                auto overview = run.GetOverview();
                overview.error = process_status.error();
                Publish({{std::move(overview)}, {}});
                ++entry;
                continue;
            }

            if (!process_status.value()) {
                // The status check succeeded and the process has exited
                // naturally.
                auto completed_run = std::move(run).Complete(
                    RunOutcome::kFinished, std::nullopt);
                WorkerPublication publication{
                    .completed_runs = {std::make_shared<const CompletedRun>(
                        std::move(completed_run))}};
                FillCompletedRunOverviews(publication);
                Publish(std::move(publication));
                // erase() returns the next iterator; do not increment it again.
                entry = active_runs_.erase(entry);
                continue;
            }

            // The status check succeeded and the process is still running.
            // A fresh overview also clears any error from a previous failed
            // check.
            Publish({{run.GetOverview()}, {}});
            ++entry;
        }
    }

    // On shutdown, stop and release all remaining processes on this worker
    // thread.
    for (auto& [id, run] : active_runs_) {
        const auto termination = run.TerminateProcess();
        RunOutcome outcome = RunOutcome::kTerminated;
        std::optional<process::ProcessError> error;
        if (!termination.has_value()) {
            // Cleanup could not terminate the process; retain the failure in
            // its result.
            outcome = RunOutcome::kFailed;
            error = termination.error();
        }
        auto completed_run = std::move(run).Complete(outcome, error);
        WorkerPublication publication{
            .completed_runs = {std::make_shared<const CompletedRun>(
                std::move(completed_run))}};
        FillCompletedRunOverviews(publication);
        Publish(std::move(publication));
    }
    active_runs_.clear();
}
}  // namespace hemlok::profiler
