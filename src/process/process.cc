#include <process/process.hpp>

namespace hemlok::process {
Process Process::startProcessFromExecutable(std::filesystem::path path) {
    return Process(path);
}

Process::Process(std::filesystem::path path)
    : state_(std::make_unique<State>()) {
    state_->process = boost::process::v2::process(
        state_->io_context.get_executor(), path, {});
}

void Process::terminate() { state_->process.terminate(); }
}  // namespace hemlok::process
