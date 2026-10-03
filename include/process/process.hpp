#ifndef HEMLOK_PROCESS_PROCESS_HPP_
#define HEMLOK_PROCESS_PROCESS_HPP_

#include <boost/process/v2/process.hpp>
#include <expected>
#include <filesystem>
#include <memory>

#include "boost/asio/io_context.hpp"

namespace hemlok::process {
enum class ProcessError {
    kNoProcess,
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

    [[nodiscard]] static std::expected<Process, ProcessError>
    StartProcessFromExecutable(std::filesystem::path path);

    // Returns a snapshot of whether the child is running, or kNoProcess after
    // ownership has been moved. Observing exit updates the cached process
    // state.
    [[nodiscard]] std::expected<bool, ProcessError> IsRunning();

    // Returns kNoProcess if ownership has been moved to another Process.
    [[nodiscard]] std::expected<void, ProcessError> Terminate();

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
