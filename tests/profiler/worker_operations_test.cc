#include <catch2/catch_test_macros.hpp>
#include <profiler/worker_operations.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
using namespace hemlok::profiler;
using namespace hemlok::profiler::detail;
using hemlok::process::ProcessError;

RunOverview AddRun(ActiveRunMap& runs, RunId id, TargetId target = 7) {
    auto process = hemlok::process::Process::CreateFromExecutable(
        HEMLOK_PROCESS_FIXTURE_PATH);
    REQUIRE(process);
    auto [entry, inserted] =
        runs.emplace(id, ActiveRun::Create(id, target, std::move(*process)));
    REQUIRE(inserted);
    return entry->second.GetOverview();
}

void CheckCompletion(const WorkerPublication& publication,
                     const RunOverview& initial, RunOutcome outcome,
                     RunStatus status,
                     std::optional<ProcessError> error = std::nullopt) {
    REQUIRE(publication.completed_runs.size() == 1);
    REQUIRE(publication.changed_overviews.size() == 1);
    const auto& completed = publication.completed_runs.front();
    const auto& overview = publication.changed_overviews.front();
    REQUIRE(completed.run_id == initial.run_id);
    REQUIRE(completed.target_id == initial.target_id);
    REQUIRE(completed.start_time == initial.start_time);
    REQUIRE(completed.outcome == outcome);
    REQUIRE(completed.error == error);
    REQUIRE(overview.run_id == completed.run_id);
    REQUIRE(overview.target_id == completed.target_id);
    REQUIRE(overview.status == status);
    REQUIRE(overview.start_time == completed.start_time);
    REQUIRE(overview.finish_time == completed.finish_time);
    REQUIRE(overview.error == completed.error);
}

TEST_CASE("termination observes natural exit without sending a kill",
          "[profiler][worker]") {
    ActiveRunMap runs;
    PublicationChannel publications;
    const auto initial = AddRun(runs, 42);
    int queries = 0;
    int terminations = 0;
    TerminateActiveRun(
        runs, publications, 42,
        [&](ActiveRun& run) -> std::expected<bool, ProcessError> {
            REQUIRE(run.GetRunId() == 42);
            ++queries;
            return false;
        },
        [&](ActiveRun&) -> std::expected<void, ProcessError> {
            ++terminations;
            return {};
        });
    REQUIRE(queries == 1);
    REQUIRE(terminations == 0);
    REQUIRE(runs.empty());
    const auto messages = publications.TakeAll();
    REQUIRE(messages.size() == 1);
    CheckCompletion(messages.front(), initial, RunOutcome::kFinished,
                    RunStatus::kFinished);
    TerminateActiveRun(runs, publications, 42);
    REQUIRE(publications.TakeAll().empty());
}

TEST_CASE("explicit termination proceeds after a failed status query",
          "[profiler][worker]") {
    ActiveRunMap runs;
    PublicationChannel publications;
    const auto initial = AddRun(runs, 42);
    std::vector<std::string> calls;
    TerminateActiveRun(
        runs, publications, 42,
        [&](ActiveRun&) -> std::expected<bool, ProcessError> {
            calls.push_back("query");
            return std::unexpected(ProcessError::kStatusQueryFailed);
        },
        [&](ActiveRun& run) {
            calls.push_back("terminate");
            return run.TerminateProcess();
        });
    REQUIRE(calls == std::vector<std::string>{"query", "terminate"});
    REQUIRE(runs.empty());
    const auto messages = publications.TakeAll();
    REQUIRE(messages.size() == 1);
    CheckCompletion(messages.front(), initial, RunOutcome::kTerminated,
                    RunStatus::kTerminated);
}

TEST_CASE("failed termination retains the run and permits a successful retry",
          "[profiler][worker]") {
    ActiveRunMap runs;
    PublicationChannel publications;
    const auto initial = AddRun(runs, 42);
    ProcessError error = ProcessError::kTerminationFailed;
    SECTION("generic termination error") {}
    SECTION("permission denied") { error = ProcessError::kPermissionDenied; }
    int terminations = 0;
    const auto terminate =
        [&](ActiveRun& run) -> std::expected<void, ProcessError> {
        ++terminations;
        if (terminations == 1) return std::unexpected(error);
        return run.TerminateProcess();
    };
    TerminateActiveRun(runs, publications, 42, &ActiveRun::IsProcessRunning,
                       terminate);
    REQUIRE(runs.size() == 1);
    REQUIRE(runs.contains(42));
    const auto failure = publications.TakeAll();
    REQUIRE(failure.size() == 1);
    REQUIRE(failure.front().completed_runs.empty());
    REQUIRE(failure.front().changed_overviews.size() == 1);
    const auto& overview = failure.front().changed_overviews.front();
    REQUIRE(overview.run_id == initial.run_id);
    REQUIRE(overview.target_id == initial.target_id);
    REQUIRE(overview.start_time == initial.start_time);
    REQUIRE_FALSE(overview.finish_time);
    REQUIRE(overview.status == RunStatus::kRunning);
    REQUIRE(overview.error == error);

    TerminateActiveRun(runs, publications, 42, &ActiveRun::IsProcessRunning,
                       terminate);
    REQUIRE(terminations == 2);
    REQUIRE(runs.empty());
    const auto success = publications.TakeAll();
    REQUIRE(success.size() == 1);
    CheckCompletion(success.front(), initial, RunOutcome::kTerminated,
                    RunStatus::kTerminated);
}

TEST_CASE(
    "status errors retain runs without blocking other runs and then clear",
    "[profiler][worker]") {
    ActiveRunMap runs;
    PublicationChannel publications;
    const auto first = AddRun(runs, 1, 10);
    const auto second = AddRun(runs, 2, 20);
    AddRun(runs, 3, 30);
    std::unordered_map<RunId, int> queries;
    PollActiveRuns(
        runs, publications,
        [&](ActiveRun& run) -> std::expected<bool, ProcessError> {
            ++queries[run.GetRunId()];
            if (run.GetRunId() == 1)
                return std::unexpected(ProcessError::kStatusQueryFailed);
            return run.GetRunId() != 2;
        });
    REQUIRE(queries == std::unordered_map<RunId, int>{{1, 1}, {2, 1}, {3, 1}});
    REQUIRE(runs.size() == 2);
    REQUIRE(runs.contains(1));
    REQUIRE(runs.contains(3));
    const auto messages = publications.TakeAll();
    REQUIRE(messages.size() == 3);
    for (const auto& message : messages) {
        REQUIRE(message.changed_overviews.size() == 1);
        const auto& overview = message.changed_overviews.front();
        if (overview.run_id == 2) {
            CheckCompletion(message, second, RunOutcome::kFinished,
                            RunStatus::kFinished);
        } else {
            REQUIRE(message.completed_runs.empty());
            REQUIRE(overview.status == RunStatus::kRunning);
            REQUIRE_FALSE(overview.finish_time);
            if (overview.run_id == 1) {
                REQUIRE(overview.error == ProcessError::kStatusQueryFailed);
                REQUIRE(overview.start_time == first.start_time);
            } else {
                REQUIRE_FALSE(overview.error);
            }
        }
    }

    PollActiveRuns(runs, publications);
    const auto recovery = publications.TakeAll();
    REQUIRE(recovery.size() == 2);
    for (const auto& message : recovery) {
        REQUIRE(message.completed_runs.empty());
        REQUIRE(message.changed_overviews.size() == 1);
        const auto& overview = message.changed_overviews.front();
        REQUIRE(overview.status == RunStatus::kRunning);
        REQUIRE_FALSE(overview.error);
        REQUIRE_FALSE(overview.finish_time);
    }
    REQUIRE(runs.size() == 2);

    // Removing several finished runs in one pass must not skip an iterator.
    PollActiveRuns(
        runs, publications,
        [](ActiveRun&) -> std::expected<bool, ProcessError> { return false; });
    REQUIRE(runs.empty());
    const auto finished = publications.TakeAll();
    REQUIRE(finished.size() == 2);
    for (const auto& message : finished) {
        REQUIRE(message.completed_runs.size() == 1);
        REQUIRE(message.completed_runs.front().outcome ==
                RunOutcome::kFinished);
    }
    PollActiveRuns(runs, publications);
    REQUIRE(publications.TakeAll().empty());
}

TEST_CASE("shutdown retains cleanup errors and still completes other runs",
          "[profiler][worker]") {
    ActiveRunMap runs;
    PublicationChannel publications;
    const auto first = AddRun(runs, 1, 10);
    const auto second = AddRun(runs, 2, 20);
    std::unordered_map<RunId, int> terminations;
    CompleteActiveRuns(
        runs, publications,
        [&](ActiveRun& run) -> std::expected<void, ProcessError> {
            ++terminations[run.GetRunId()];
            if (run.GetRunId() == 1)
                return std::unexpected(ProcessError::kTerminationFailed);
            return run.TerminateProcess();
        });
    REQUIRE(terminations == std::unordered_map<RunId, int>{{1, 1}, {2, 1}});
    REQUIRE(runs.empty());
    const auto messages = publications.TakeAll();
    REQUIRE(messages.size() == 2);
    for (const auto& message : messages) {
        REQUIRE(message.completed_runs.size() == 1);
        if (message.completed_runs.front().run_id == 1) {
            CheckCompletion(message, first, RunOutcome::kFailed,
                            RunStatus::kFailed,
                            ProcessError::kTerminationFailed);
        } else {
            CheckCompletion(message, second, RunOutcome::kTerminated,
                            RunStatus::kTerminated);
        }
    }
    CompleteActiveRuns(runs, publications);
    REQUIRE(publications.TakeAll().empty());
}
}  // namespace
