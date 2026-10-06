#include <cassert>
#include <process/process.hpp>
#include <profiler/profiler.hpp>
#include <utility>

namespace hemlok::profiler {
Profiler::Profiler() : worker_(commands_, publications_) {}

TargetId Profiler::GenerateTargetId() {
    static TargetId next_target_id = 0;
    return next_target_id++;
}

RunId Profiler::GenerateRunId() {
    static RunId next_run_id = 0;
    return next_run_id++;
}

TargetId Profiler::AddTarget(Target target) {
    const auto id = GenerateTargetId();
    targets_.emplace(id, std::move(target));
    return id;
}

std::expected<RunId, process::ProcessError> Profiler::StartRun(
    TargetId target) {
    const auto entry = targets_.find(target);
    assert(entry != targets_.end());
    const auto id = GenerateRunId();
    run_overviews_.emplace(
        id, RunOverview{id, target, RunStatus::kStarting, std::nullopt,
                        std::nullopt, std::nullopt});
    commands_.Send(
        StartRunCommand{id, target, entry->second.GetExecutablePath()});
    return id;
}

std::expected<void, process::ProcessError> Profiler::TerminateRun(RunId run) {
    assert(run_overviews_.contains(run));
    if (!completed_runs_.contains(run)) {
        commands_.Send(TerminateRunCommand{run});
    }
    return {};
}

void Profiler::Poll() {
    for (auto& publication : publications_.TakeAll()) {
        ApplyPublication(std::move(publication));
    }
}

void Profiler::ApplyPublication(WorkerPublication publication) {
    for (auto& overview : publication.changed_overviews) {
        assert(run_overviews_.contains(overview.run_id));
        run_overviews_.insert_or_assign(overview.run_id, std::move(overview));
    }
    for (auto& run : publication.completed_runs) {
        const auto id = run->run_id;
        completed_runs_.insert_or_assign(id, std::move(run));
    }
}

const std::unordered_map<TargetId, Target>& Profiler::GetTargets() const {
    return targets_;
}

const std::unordered_map<RunId, RunOverview>& Profiler::GetRunOverviews()
    const {
    return run_overviews_;
}

const std::unordered_map<RunId, std::shared_ptr<const CompletedRun>>&
Profiler::GetCompletedRuns() const {
    return completed_runs_;
}

std::shared_ptr<const CompletedRun> Profiler::GetCompletedRun(RunId run) const {
    assert(run_overviews_.contains(run));
    const auto entry = completed_runs_.find(run);
    return entry == completed_runs_.end() ? nullptr : entry->second;
}
}  // namespace hemlok::profiler
