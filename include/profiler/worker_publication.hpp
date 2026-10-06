#ifndef HEMLOK_PROFILER_WORKER_PUBLICATION_HPP_
#define HEMLOK_PROFILER_WORKER_PUBLICATION_HPP_

#include <memory>
#include <profiler/channel.hpp>
#include <profiler/completed_run.hpp>
#include <profiler/run_overview.hpp>
#include <vector>

namespace hemlok::profiler {
struct WorkerPublication {
    std::vector<RunOverview> changed_overviews;
    std::vector<std::shared_ptr<const CompletedRun>> completed_runs;
};
using PublicationChannel = Channel<WorkerPublication>;
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_WORKER_PUBLICATION_HPP_
