#ifndef HEMLOK_PROFILER_SAMPLING_WORKER_HPP_
#define HEMLOK_PROFILER_SAMPLING_WORKER_HPP_

#include <profiler/active_run.hpp>
#include <profiler/worker_command.hpp>
#include <profiler/worker_publication.hpp>
#include <thread>
#include <unordered_map>

namespace hemlok::profiler {
using ActiveRunMap = std::unordered_map<RunId, ActiveRun>;
class SamplingWorker {
public:
    // Both channels must outlive the worker.
    SamplingWorker(CommandChannel& commands, PublicationChannel& publications);
    ~SamplingWorker();
    void Shutdown();

private:
    void RunLoop(std::stop_token stop);
    void HandleCommand(WorkerCommand command);
    void Publish(WorkerPublication publication);
    void PublishOverview(RunOverview overview);
    void PublishCompletedRun(CompletedRun run);
    ActiveRunMap active_runs_;
    CommandChannel& commands_;
    PublicationChannel& publications_;
    // Start only after all worker state is initialized.
    std::jthread thread_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_SAMPLING_WORKER_HPP_
