#include <process/process.hpp>

namespace hemlok::process {
Process Process::startProcessFromExecutable(std::filesystem::path path) {
    return Process(path);
}

Process::Process(std::filesystem::path path)
    : io_context_{}, process_(io_context_.get_executor(), path, {}) {}

void Process::terminate() { process_.terminate(); }
}  // namespace hemlok::process
