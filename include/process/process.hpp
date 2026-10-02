#ifndef HEMLOK_PROCESS_PROCESS_
#define HEMLOK_PROCESS_PROCESS_

#include <boost/process/v2/process.hpp>
#include <filesystem>

#include "boost/asio/io_context.hpp"

namespace hemlok::process {
// A thin wrapper around Boost.Process
class Process {
public:
    static Process startProcessFromExecutable(std::filesystem::path path);

    void terminate();

private:
    explicit Process(std::filesystem::path path);

    boost::asio::io_context io_context_;
    boost::process::v2::process process_;
};
}  // namespace hemlok::process

#endif  // HEMLOK_PROCESS_PROCESS_
