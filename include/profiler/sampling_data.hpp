#ifndef HEMLOK_PROFILER_SAMPLING_DATA_HPP_
#define HEMLOK_PROFILER_SAMPLING_DATA_HPP_

#include <sampling/sample.hpp>
#include <vector>

namespace hemlok::profiler {
// Samples collected over a run, oldest first.
struct SamplingData {
    std::vector<sampling::Sample> samples;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_SAMPLING_DATA_HPP_
