#include <process/process.hpp>
#include <string>
#include <vector>

namespace hemlok::process {
std::expected<Process, ProcessError> Process::startProcessFromExecutable(
    std::filesystem::path path) {
    boost::system::error_code error;
    Process process(path, error);
    if (error) {
        if (error == boost::system::errc::no_such_file_or_directory ||
            error == boost::system::errc::not_a_directory) {
            return std::unexpected(ProcessError::kFileNotFound);
        }
        if (error == boost::system::errc::permission_denied ||
            error == boost::system::errc::operation_not_permitted) {
            return std::unexpected(ProcessError::kPermissionDenied);
        }
        if (error == boost::system::errc::executable_format_error ||
            error == boost::system::errc::is_a_directory) {
            return std::unexpected(ProcessError::kInvalidExecutable);
        }
        return std::unexpected(ProcessError::kStartFailed);
    }
    return process;
}

Process::Process(std::filesystem::path path, boost::system::error_code& error)
    : state_(std::make_unique<State>()) {
    state_->process = boost::process::v2::default_process_launcher{}(
        state_->io_context.get_executor(), error, path,
        std::vector<std::string>{});
}

std::expected<void, ProcessError> Process::terminate() {
    boost::system::error_code error;
    state_->process.terminate(error);
    if (error) {
        if (error == boost::system::errc::permission_denied ||
            error == boost::system::errc::operation_not_permitted) {
            return std::unexpected(ProcessError::kPermissionDenied);
        }
        return std::unexpected(ProcessError::kTerminationFailed);
    }
    return {};
}
}  // namespace hemlok::process
