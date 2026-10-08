#ifndef HEMLOK_UI_PROFILER_UI_LOGIC_HPP_
#define HEMLOK_UI_PROFILER_UI_LOGIC_HPP_

#include <expected>
#include <filesystem>
#include <optional>
#include <profiler/profiler.hpp>
#include <string>
#include <vector>

namespace hemlok::ui::detail {
// Errors point to static messages that can be retained by ProfilerUi.
std::expected<profiler::Target, const char*> ValidateTargetInput(
    std::string name, std::filesystem::path path);
std::expected<profiler::TargetId, const char*> SubmitTarget(
    profiler::Profiler& profiler, std::string name, std::filesystem::path path);
std::vector<profiler::RunId> GetTargetRunIds(
    const std::unordered_map<profiler::RunId, profiler::RunOverview>& runs,
    profiler::TargetId target);
bool CanTerminate(profiler::RunStatus status);
std::optional<double> GetRunDurationSeconds(const profiler::RunOverview& run,
                                            profiler::TimePoint now);
}  // namespace hemlok::ui::detail

#endif  // HEMLOK_UI_PROFILER_UI_LOGIC_HPP_
