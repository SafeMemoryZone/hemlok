#ifndef HEMLOK_PROFILER_WORKER_COMMAND_HPP_
#define HEMLOK_PROFILER_WORKER_COMMAND_HPP_

#include <filesystem>
#include <profiler/channel.hpp>
#include <profiler/run_types.hpp>
#include <variant>

namespace hemlok::profiler {
struct StartRunCommand {
    RunId run_id;
    TargetId target_id;
    std::filesystem::path executable_path;
};
struct TerminateRunCommand {
    RunId run_id;
};
using WorkerCommand = std::variant<StartRunCommand, TerminateRunCommand>;
using CommandChannel = Channel<WorkerCommand>;
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_WORKER_COMMAND_HPP_
