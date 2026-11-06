#include <iterator>
#include <process/process.hpp>
#include <profiler/active_run.hpp>
#include <utility>

namespace hemlok::profiler {
ActiveRun::ActiveRun(RunId run, TargetId target, process::Process process,
                     sampling::SamplingSession session)
    : run_id_(run),
      target_id_(target),
      process_(std::move(process)),
      start_time_(std::chrono::system_clock::now()),
      session_(std::move(session)) {}

ActiveRun ActiveRun::Create(RunId run, TargetId target,
                            process::Process process,
                            sampling::SamplingSession session) {
    return ActiveRun(run, target, std::move(process), std::move(session));
}

RunId ActiveRun::GetRunId() const { return run_id_; }

TargetId ActiveRun::GetTargetId() const { return target_id_; }

std::expected<bool, process::ProcessError> ActiveRun::IsProcessRunning() {
    return process_.IsRunning();
}

std::expected<void, process::ProcessError> ActiveRun::TerminateProcess() {
    return process_.Terminate();
}

void ActiveRun::CollectSamples() {
    auto samples = session_.DrainSamples();
    sampling_data_.samples.insert(sampling_data_.samples.end(),
                                  std::make_move_iterator(samples.begin()),
                                  std::make_move_iterator(samples.end()));
}

RunOverview ActiveRun::GetOverview() const {
    return {run_id_,     target_id_,   RunStatus::kRunning,
            start_time_, std::nullopt, std::nullopt};
}

CompletedRun ActiveRun::Complete(RunOutcome outcome,
                                 std::optional<RunError> error) && {
    // Keep the samples recorded before sampling stopped.
    session_.Detach();
    CollectSamples();
    return {run_id_,
            target_id_,
            outcome,
            error,
            start_time_,
            std::chrono::system_clock::now(),
            std::move(sampling_data_)};
}
}  // namespace hemlok::profiler
