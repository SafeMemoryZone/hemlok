#ifndef HEMLOK_UI_PROFILER_UI_HPP_
#define HEMLOK_UI_PROFILER_UI_HPP_

#include <array>
#include <cstddef>
#include <optional>
#include <profiler/profiler.hpp>

struct GLFWwindow;

namespace hemlok::ui {
class ProfilerUi {
public:
    // The profiler must outlive this UI.
    explicit ProfilerUi(profiler::Profiler& profiler);
    ProfilerUi(const ProfilerUi&) = delete;
    ProfilerUi& operator=(const ProfilerUi&) = delete;
    ProfilerUi(ProfilerUi&&) = delete;
    ProfilerUi& operator=(ProfilerUi&&) = delete;

    int Start();

private:
    static constexpr std::size_t kNameCapacity = 256;
    static constexpr std::size_t kPathCapacity = 1024;

    bool Init();
    void Draw();
    bool AddTarget();
    void DrawTargetDetails(profiler::TargetId id,
                           const profiler::Target& target);
    void DrawRuns(profiler::TargetId target);
    void Shutdown();

    // UI state
    std::array<char, kNameCapacity> target_name_{};
    std::array<char, kPathCapacity> executable_path_{};
    const char* target_error_ = nullptr;
    const char* run_error_ = nullptr;
    std::optional<profiler::TargetId> selected_target_;

    // App state
    profiler::Profiler& profiler_;
    GLFWwindow* window_ = nullptr;
};

}  // namespace hemlok::ui

#endif  // HEMLOK_UI_PROFILER_UI_HPP_
