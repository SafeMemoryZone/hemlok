#ifndef HEMLOK_PROFILER_PROFILING_TARGET_HPP_
#define HEMLOK_PROFILER_PROFILING_TARGET_HPP_

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace hemlok::profiler {
enum class TargetError {
    kBlankName,
    kEmptyPath,
    kRelativePath,
};

// Owns a nonblank optional name and a nonempty absolute path. Filesystem
// existence and executability must still be checked when launching.
// Moved-from targets must be reassigned before reuse.
class ProfilingTarget {
public:
    static std::expected<ProfilingTarget, TargetError> Create(
        std::optional<std::string> name,
        std::filesystem::path executable_path) {
        if (name &&
            name->find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
            return std::unexpected(TargetError::kBlankName);
        }
        if (executable_path.empty()) {
            return std::unexpected(TargetError::kEmptyPath);
        }
        if (!executable_path.is_absolute()) {
            return std::unexpected(TargetError::kRelativePath);
        }
        return ProfilingTarget(std::move(name), std::move(executable_path));
    }

    const std::optional<std::string>& GetName() const { return name_; }
    const std::filesystem::path& GetExecutablePath() const {
        return executable_path_;
    }

private:
    ProfilingTarget(std::optional<std::string> name,
                    std::filesystem::path executable_path)
        : name_(std::move(name)),
          executable_path_(std::move(executable_path)) {}

    std::optional<std::string> name_;
    std::filesystem::path executable_path_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_PROFILING_TARGET_HPP_
