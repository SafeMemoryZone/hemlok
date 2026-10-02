#ifndef HEMLOK_PROCESS_PROCESS_
#define HEMLOK_PROCESS_PROCESS_

#include <boost/process/v2/process.hpp>
#include <expected>
#include <filesystem>
#include <memory>

#include "boost/asio/io_context.hpp"

namespace hemlok::process {
enum class ProcessError {
    kFileNotFound,
    kPermissionDenied,
    kInvalidExecutable,
    kStartFailed,
    kTerminationFailed,
};

// A thin wrapper around Boost.Process
class Process {
public:
    [[nodiscard]] static std::expected<Process, ProcessError>
    startProcessFromExecutable(std::filesystem::path path);

    [[nodiscard]] std::expected<void, ProcessError> terminate();

private:
    explicit Process(std::filesystem::path path,
                     boost::system::error_code& error);

    // Keep the context alive until after the process is destroyed, including
    // when Process is moved or move-assigned.
    struct State {
        boost::asio::io_context io_context;
        boost::process::v2::process process{io_context.get_executor()};
    };

    std::unique_ptr<State> state_;
};
}  // namespace hemlok::process

#endif  // HEMLOK_PROCESS_PROCESS_
