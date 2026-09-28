#ifndef HEMLOK_UI_PROFILER_UI_HPP_
#define HEMLOK_UI_PROFILER_UI_HPP_

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace hemlok::ui {

class ProfilerUi {
 public:
  void Draw();

 private:
  static constexpr std::size_t kPathCapacity = 1024;

  std::array<char, kPathCapacity> executable_path_{};
  std::vector<std::string> targets_;
  std::optional<std::size_t> selected_target_;
};

}  // namespace hemlok::ui

#endif  // HEMLOK_UI_PROFILER_UI_HPP_
