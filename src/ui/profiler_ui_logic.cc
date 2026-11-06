#include <algorithm>
#include <chrono>
#include <map>
#include <system_error>
#include <ui/profiler_ui_logic.hpp>
#include <utility>

namespace hemlok::ui::detail {
std::expected<profiler::Target, const char*> ValidateTargetInput(
    std::string name, std::filesystem::path path) {
    if (path.empty()) return std::unexpected("Enter an executable path.");

    std::error_code error;
    path = std::filesystem::absolute(path, error);
    if (error || !std::filesystem::is_regular_file(path, error)) {
        return std::unexpected("Path must refer to an existing regular file.");
    }

    std::optional<std::string> optional_name;
    if (!name.empty()) optional_name = std::move(name);
    auto target =
        profiler::Target::Create(std::move(optional_name), std::move(path));
    if (!target) {
        switch (target.error()) {
            case profiler::TargetError::kBlankName:
                return std::unexpected(
                    "Name must contain a non-whitespace character.");
            case profiler::TargetError::kEmptyPath:
                return std::unexpected("Enter an executable path.");
            case profiler::TargetError::kRelativePath:
                return std::unexpected("Executable path must be absolute.");
        }
    }
    return std::move(*target);
}

std::expected<profiler::TargetId, const char*> SubmitTarget(
    profiler::Profiler& profiler, std::string name,
    std::filesystem::path path) {
    auto target = ValidateTargetInput(std::move(name), std::move(path));
    if (!target) return std::unexpected(target.error());
    return profiler.AddTarget(std::move(*target));
}

std::vector<profiler::RunId> GetTargetRunIds(
    const std::unordered_map<profiler::RunId, profiler::RunOverview>& runs,
    profiler::TargetId target) {
    std::vector<profiler::RunId> ids;
    for (const auto& [id, run] : runs) {
        if (run.target_id == target) ids.push_back(id);
    }
    std::sort(ids.rbegin(), ids.rend());
    return ids;
}

bool CanTerminate(profiler::RunStatus status) {
    return status == profiler::RunStatus::kStarting ||
           status == profiler::RunStatus::kRunning;
}

std::optional<double> GetRunDurationSeconds(const profiler::RunOverview& run,
                                            profiler::TimePoint now) {
    if (!run.start_time) return std::nullopt;
    const auto end = run.finish_time.value_or(now);
    const double seconds =
        std::chrono::duration<double>(end - *run.start_time).count();
    return std::max(0.0, seconds);
}

std::vector<FunctionShare> ComputeFunctionShares(
    const std::vector<sampling::Sample>& samples) {
    std::map<std::string, std::size_t> counts;
    for (const auto& sample : samples) {
        ++counts[sample.function_name.value_or("[unknown]")];
    }
    std::vector<FunctionShare> shares;
    for (const auto& [name, count] : counts) {
        shares.push_back(
            {name, count,
             static_cast<double>(count) / static_cast<double>(samples.size())});
    }
    // counts is ordered by name, so a stable sort keeps ties in name order.
    std::stable_sort(shares.begin(), shares.end(),
                     [](const FunctionShare& left, const FunctionShare& right) {
                         return left.sample_count > right.sample_count;
                     });
    return shares;
}
}  // namespace hemlok::ui::detail
