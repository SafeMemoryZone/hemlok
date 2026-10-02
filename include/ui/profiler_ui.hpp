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
    explicit ProfilerUi(profiler::Profiler& profiler);

    int Start();

private:
    static constexpr std::size_t kNameCapacity = 256;
    static constexpr std::size_t kPathCapacity = 1024;

    bool Init();
    void Draw();
    void Shutdown();

    // UI state
    std::array<char, kNameCapacity> target_name_{};
    std::array<char, kPathCapacity> executable_path_{};
    bool invalid_path_ = false;
    std::optional<profiler::TargetId> selected_target_;

    // App state
    profiler::Profiler& profiler_;
    GLFWwindow* window_ = nullptr;
};

}  // namespace hemlok::ui

#endif  // HEMLOK_UI_PROFILER_UI_HPP_
