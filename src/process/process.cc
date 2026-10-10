#include <process/process.hpp>
#include <process/process_errors.hpp>
#include <vector>

#if defined(__APPLE__)
#include <boost/asio/signal_set.hpp>
#include <csignal>
#endif

namespace hemlok::process {
std::expected<Process, ProcessError> Process::CreateFromExecutable(
    std::filesystem::path path) {
#if defined(__APPLE__)
    // Boost.Process uses Asio's SIGCHLD service on macOS. Keep its handler and
    // shared pipe alive across per-process contexts: a handler on another
    // thread may still be running when the last process unregisters it and
    // closes the pipe. Both statics outlive processes created by this factory.
    static boost::asio::io_context signal_context;
    static boost::asio::signal_set signal_service(signal_context, SIGCHLD);
#endif
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
