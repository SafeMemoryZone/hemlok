#ifndef HEMLOK_PROCESS_PROCESS_HPP_
#define HEMLOK_PROCESS_PROCESS_HPP_

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
    kStatusQueryFailed,
};

// A thin wrapper around Boost.Process
class Process {
public:
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) noexcept = default;
    Process& operator=(Process&&) noexcept = default;

    static std::expected<Process, ProcessError> CreateFromExecutable(
        std::filesystem::path path);

    std::expected<bool, ProcessError> IsRunning();
    std::expected<void, ProcessError> Terminate();

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

#endif  // HEMLOK_PROCESS_PROCESS_HPP_
