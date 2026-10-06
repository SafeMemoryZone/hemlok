#ifndef HEMLOK_PROFILER_PROFILER_WORKER_HPP_
#define HEMLOK_PROFILER_PROFILER_WORKER_HPP_

#include <profiler/active_run.hpp>
#include <profiler/worker_command.hpp>
#include <profiler/worker_publication.hpp>
#include <thread>
#include <unordered_map>

namespace hemlok::profiler {
using ActiveRunMap = std::unordered_map<RunId, ActiveRun>;
class ProfilerWorker {
public:
    // Both channels must outlive the worker.
    ProfilerWorker(CommandChannel& commands, PublicationChannel& publications);
    ~ProfilerWorker();
    void Shutdown();

private:
    void RunLoop(std::stop_token stop);
    void HandleCommand(WorkerCommand command);
    void Publish(WorkerPublication publication);
    ActiveRunMap active_runs_;
    CommandChannel& commands_;
    PublicationChannel& publications_;
    // Start only after all worker state is initialized.
    std::jthread thread_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_PROFILER_WORKER_HPP_
