#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <profiler/sampling_worker.hpp>
#include <support/temporary_directory.hpp>
#include <vector>

namespace {
using namespace hemlok::profiler;

class Publications {
public:
    void Drain(PublicationChannel& channel) {
        for (auto& publication : channel.TakeAll()) {
            messages_.push_back(std::move(publication));
        }
    }
    template <typename Predicate>
    void WaitFor(PublicationChannel& channel, Predicate ready) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (!ready() && std::chrono::steady_clock::now() < deadline) {
            channel.Wait({}, std::chrono::milliseconds{10});
            Drain(channel);
        }
        REQUIRE(ready());
    }
    bool HasStatus(RunId id, RunStatus status) const {
        for (const auto& publication : messages_) {
            for (const auto& overview : publication.changed_overviews) {
                if (overview.run_id == id && overview.status == status)
                    return true;
            }
        }
        return false;
    }
    void CheckCompletion(RunId id, TargetId target, RunOutcome outcome,
                         std::optional<hemlok::process::ProcessError> error =
                             std::nullopt) const {
        int count = 0;
        for (const auto& publication : messages_) {
            for (const auto& run : publication.completed_runs) {
                if (run.run_id != id) continue;
                ++count;
                REQUIRE(run.target_id == target);
                REQUIRE(run.outcome == outcome);
                REQUIRE(run.error == error);
                REQUIRE(run.start_time.has_value() ==
                        (outcome != RunOutcome::kFailed));
                const auto overview = std::find_if(
                    publication.changed_overviews.begin(),
                    publication.changed_overviews.end(),
                    [id](const auto& value) { return value.run_id == id; });
                REQUIRE(overview != publication.changed_overviews.end());
                REQUIRE(overview->target_id == target);
                const auto status = outcome == RunOutcome::kFinished
                                        ? RunStatus::kFinished
                                    : outcome == RunOutcome::kTerminated
                                        ? RunStatus::kTerminated
                                        : RunStatus::kFailed;
                REQUIRE(overview->status == status);
                REQUIRE(overview->error == error);
                REQUIRE(overview->start_time == run.start_time);
                REQUIRE(overview->finish_time == run.finish_time);
            }
        }
        REQUIRE(count == 1);
    }

private:
    std::vector<WorkerPublication> messages_;
};

TEST_CASE("worker shutdown completes every active run exactly once",
          "[profiler][worker]") {
    CommandChannel commands;
    PublicationChannel publications;
    Publications observed;
    {
        SamplingWorker worker(commands, publications);
        commands.Send(StartRunCommand{1, 10, HEMLOK_PROCESS_FIXTURE_PATH});
        commands.Send(StartRunCommand{2, 20, HEMLOK_PROCESS_FIXTURE_PATH});
        observed.WaitFor(publications, [&] {
            return observed.HasStatus(1, RunStatus::kRunning) &&
                   observed.HasStatus(2, RunStatus::kRunning);
        });
        SECTION("explicit shutdown is repeatable") {
            worker.Shutdown();
            worker.Shutdown();
        }
        SECTION("destruction shuts down active runs") {}
    }
    observed.Drain(publications);
    observed.CheckCompletion(1, 10, RunOutcome::kTerminated);
    observed.CheckCompletion(2, 20, RunOutcome::kTerminated);
    REQUIRE(publications.TakeAll().empty());
}

TEST_CASE("worker handles launch failure and duplicate termination in order",
          "[profiler][worker]") {
    hemlok::testing::TemporaryDirectory directory;
    CommandChannel commands;
    PublicationChannel publications;
    Publications observed;
    // Queue the entire batch before starting the worker so shutdown cannot
    // interrupt the test between the two termination commands.
    commands.Send(StartRunCommand{1, 10, directory.GetPath() / "missing"});
    commands.Send(TerminateRunCommand{1});
    commands.Send(StartRunCommand{2, 20, HEMLOK_PROCESS_FIXTURE_PATH});
    commands.Send(TerminateRunCommand{2});
    commands.Send(TerminateRunCommand{2});
    commands.Send(StartRunCommand{3, 30, HEMLOK_EXITING_PROCESS_FIXTURE_PATH});
    SamplingWorker worker(commands, publications);
    observed.WaitFor(publications, [&] {
        return observed.HasStatus(1, RunStatus::kFailed) &&
               observed.HasStatus(2, RunStatus::kTerminated) &&
               observed.HasStatus(3, RunStatus::kFinished);
    });
    worker.Shutdown();
    observed.Drain(publications);
    observed.CheckCompletion(1, 10, RunOutcome::kFailed,
                             hemlok::process::ProcessError::kFileNotFound);
    observed.CheckCompletion(2, 20, RunOutcome::kTerminated);
    observed.CheckCompletion(3, 30, RunOutcome::kFinished);
}
}  // namespace
