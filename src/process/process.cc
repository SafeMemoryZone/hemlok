#include <process/process.hpp>
#include <process/process_errors.hpp>
#include <vector>

namespace hemlok::process {
std::expected<Process, ProcessError> Process::CreateFromExecutable(
    std::filesystem::path path) {
    boost::system::error_code error;
    Process process(path, error);
    if (error) {
        return std::unexpected(detail::MapLaunchError(error));
    }
    return process;
}

Process::Process(std::filesystem::path path, boost::system::error_code& error)
    : state_(std::make_unique<State>()) {
    state_->process = boost::process::v2::default_process_launcher{}(
        state_->io_context.get_executor(), error, path,
        std::vector<std::string>{});
}

std::expected<bool, ProcessError> Process::IsRunning() {
    boost::system::error_code error;
    const bool running = state_->process.running(error);
    if (error) {
        return std::unexpected(detail::MapStatusError(error));
    }
    return running;
}

std::expected<void, ProcessError> Process::Terminate() {
    boost::system::error_code error;
    state_->process.terminate(error);
    if (error) {
        return std::unexpected(detail::MapTerminationError(error));
    }
    return {};
}
}  // namespace hemlok::process
