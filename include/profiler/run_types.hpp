#ifndef HEMLOK_PROFILER_RUN_TYPES_HPP_
#define HEMLOK_PROFILER_RUN_TYPES_HPP_

#include <chrono>
#include <cstdint>

namespace hemlok::profiler {
using TargetId = std::uint64_t;
using RunId = std::uint64_t;
using TimePoint = std::chrono::system_clock::time_point;

enum class RunStatus { kStarting, kRunning, kFinished, kTerminated, kFailed };
enum class RunOutcome { kFinished, kTerminated, kFailed };
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_RUN_TYPES_HPP_
