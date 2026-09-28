#include "ui/profiler_ui.hpp"

#include <imgui.h>

namespace hemlok::ui {
    void ProfilerUi::Draw() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Hemlok", nullptr, ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::InputText("Executable path", executable_path_.data(),
                executable_path_.size());
        ImGui::SameLine();
        if (ImGui::Button("Add target") && executable_path_[0] != '\0') {
            targets_.emplace_back(executable_path_.data());
            executable_path_.fill('\0');
        }

        ImGui::Separator();
        ImGui::BeginChild("Targets", ImVec2(240.0F, 0.0F), true);
        for (std::size_t i = 0; i < targets_.size(); i++) {
            const bool is_selected = selected_target_ == i;
            if (ImGui::Selectable(targets_[i].c_str(), is_selected)) {
                selected_target_ = i;
            }
        }
        ImGui::EndChild();

        if (selected_target_.has_value()) {
            ImGui::SameLine();
            ImGui::TextUnformatted("Profiler details will appear here.");
        }

        ImGui::End();
    }

}  // namespace hemlok::ui
