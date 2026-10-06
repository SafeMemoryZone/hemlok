#include <profiler/active_run.hpp>
#include <utility>

namespace hemlok::profiler {
ActiveRun::ActiveRun(RunId run, TargetId target, process::Process process)
    : run_id_(run),
      target_id_(target),
      process_(std::move(process)),
      start_time_(std::chrono::system_clock::now()) {}

ActiveRun ActiveRun::Create(RunId run, TargetId target,
                            process::Process process) {
    return ActiveRun(run, target, std::move(process));
}

RunId ActiveRun::GetRunId() const { return run_id_; }

TargetId ActiveRun::GetTargetId() const { return target_id_; }

std::expected<bool, Error> ActiveRun::IsProcessRunning() {
    return process_.IsRunning();
}

std::expected<void, Error> ActiveRun::TerminateProcess() {
    return process_.Terminate();
}

std::expected<void, Error> ActiveRun::CollectSamples() { return {}; }

RunOverview ActiveRun::GetOverview() const {
    return {run_id_,     target_id_,   RunStatus::kRunning,
            start_time_, std::nullopt, std::nullopt};
}

CompletedRun ActiveRun::Complete(RunOutcome outcome,
                                 std::optional<Error> error) {
    return {run_id_,
            target_id_,
            outcome,
            error,
            start_time_,
            std::chrono::system_clock::now(),
            std::move(sampling_results_)};
}
}  // namespace hemlok::profiler
