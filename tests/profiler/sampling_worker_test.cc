#include <catch2/catch_test_macros.hpp>
#include <profiler/sampling_worker.hpp>
#include <thread>

namespace {
TEST_CASE("worker shutdown completes active processes and is repeatable",
          "[profiler][worker]") {
    using namespace hemlok::profiler;
    CommandChannel commands;
    PublicationChannel publications;
    SamplingWorker worker(commands, publications);
    commands.Send(StartRunCommand{4, 2, HEMLOK_PROCESS_FIXTURE_PATH});
    bool started = false;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!started && std::chrono::steady_clock::now() < deadline) {
        for (const auto& publication : publications.TakeAll()) {
            for (const auto& overview : publication.changed_overviews) {
                if (overview.status == RunStatus::kRunning) started = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    REQUIRE(started);
    worker.Shutdown();
    worker.Shutdown();
    std::shared_ptr<const CompletedRun> completed;
    for (const auto& publication : publications.TakeAll()) {
        for (const auto& run : publication.completed_runs) completed = run;
    }
    REQUIRE(completed);
    REQUIRE(completed->run_id == 4);
    REQUIRE(completed->outcome == RunOutcome::kTerminated);
    REQUIRE_FALSE(completed->error);
    REQUIRE(publications.TakeAll().empty());
}

TEST_CASE("an idle worker shuts down without a command", "[profiler][worker]") {
    using namespace hemlok::profiler;
    CommandChannel commands;
    PublicationChannel publications;
    SamplingWorker worker(commands, publications);
    worker.Shutdown();
    worker.Shutdown();
    REQUIRE(publications.TakeAll().empty());
}
}  // namespace
