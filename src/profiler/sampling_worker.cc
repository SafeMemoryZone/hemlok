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

void SamplingWorker::Publish(WorkerPublication publication) {
    publications_.Send(std::move(publication));
}

void SamplingWorker::PublishOverview(RunOverview overview) {
    Publish({{std::move(overview)}, {}});
}

void SamplingWorker::PublishCompletedRun(CompletedRun run) {
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
    auto completed_run = std::make_shared<const CompletedRun>(std::move(run));
    Publish({{{completed_run->run_id, completed_run->target_id, status,
               completed_run->start_time, completed_run->finish_time,
               completed_run->error}},
             {std::move(completed_run)}});
}

void SamplingWorker::HandleCommand(WorkerCommand command) {
    if (const auto* start = std::get_if<StartRunCommand>(&command)) {
        auto process =
            process::Process::CreateFromExecutable(start->executable_path);
        if (!process) {
            PublishCompletedRun({start->run_id,
                                 start->target_id,
                                 RunOutcome::kFailed,
                                 process.error(),
                                 std::nullopt,
                                 std::chrono::system_clock::now(),
                                 {}});
            return;
        }
        auto run = ActiveRun::Create(start->run_id, start->target_id,
                                     std::move(*process));
        PublishOverview(run.GetOverview());
        active_runs_.emplace(start->run_id, std::move(run));
        return;
    }
    const auto id = std::get<TerminateRunCommand>(command).run_id;
    const auto entry = active_runs_.find(id);
    // A queued termination can arrive after a run completed, or be repeated.
    if (entry == active_runs_.end()) {
        return;
    }
    auto& run = entry->second;
    const auto running = run.IsProcessRunning();
    if (running && !*running) {
        PublishCompletedRun(
            std::move(run).Complete(RunOutcome::kFinished, std::nullopt));
        active_runs_.erase(entry);
        return;
    }
    const auto result = run.TerminateProcess();
    if (!result) {
        auto overview = run.GetOverview();
        overview.error = result.error();
        PublishOverview(std::move(overview));
        return;
    }
    PublishCompletedRun(
        std::move(run).Complete(RunOutcome::kTerminated, std::nullopt));
    active_runs_.erase(entry);
}

void SamplingWorker::RunLoop(std::stop_token stop) {
    while (!stop.stop_requested()) {
        commands_.Wait(stop,
                       active_runs_.empty()
                           ? std::nullopt
                           : std::optional{std::chrono::milliseconds{10}});
        if (stop.stop_requested()) {
            break;
        }
        for (auto& command : commands_.TakeAll()) {
            HandleCommand(std::move(command));
        }
        for (auto entry = active_runs_.begin(); entry != active_runs_.end();) {
            auto& run = entry->second;
            const auto running = run.IsProcessRunning();
            if (running && !*running) {
                PublishCompletedRun(std::move(run).Complete(
                    RunOutcome::kFinished, std::nullopt));
                entry = active_runs_.erase(entry);
                continue;
            }
            auto overview = run.GetOverview();
            if (!running) {
                overview.error = running.error();
            }
            PublishOverview(std::move(overview));
            ++entry;
        }
    }
    // Destroy and stop processes on their owning thread, before channels die.
    for (auto& [id, run] : active_runs_) {
        const auto result = run.TerminateProcess();
        PublishCompletedRun(std::move(run).Complete(
            result ? RunOutcome::kTerminated : RunOutcome::kFailed,
            result ? std::nullopt
                   : std::optional<process::ProcessError>{result.error()}));
    }
    active_runs_.clear();
}
}  // namespace hemlok::profiler
