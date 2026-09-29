#ifndef HEMLOK_PROFILER_PROFILING_TARGET_HPP_
#define HEMLOK_PROFILER_PROFILING_TARGET_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace hemlok::profiler {
    struct ProfilingTarget {
        ProfilingTarget(std::optional<std::string> name,
                std::filesystem::path executable_path)
            : name(std::move(name)), executable_path(std::move(executable_path)) {}

        std::optional<std::string> name;
        std::filesystem::path executable_path;
    };
} // namespace hemlok::profiler

#endif // HEMLOK_PROFILER_PROFILING_TARGET_HPP_
