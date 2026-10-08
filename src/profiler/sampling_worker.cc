#include <process/process.hpp>
#include <profiler/sampling_worker.hpp>
#include <profiler/worker_operations.hpp>
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
            WorkerPublication publication;
            publication.completed_runs.push_back(std::move(completed_run));
            detail::FillCompletedRunOverviews(publication);
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

    detail::TerminateActiveRun(active_runs_, publications_,
                               std::get<TerminateRunCommand>(command).run_id);
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
        detail::PollActiveRuns(active_runs_, publications_);
    }

    detail::CompleteActiveRuns(active_runs_, publications_);
}
}  // namespace hemlok::profiler
