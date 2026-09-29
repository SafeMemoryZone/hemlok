#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <profiler/profiling_target.hpp>
#include <string>
#include <system_error>
#include <ui/profiler_ui.hpp>
#include <utility>

namespace hemlok::ui {

ProfilerUi::ProfilerUi(profiler::Profiler& profiler) : profiler_(profiler) {}

bool ProfilerUi::Init() {
    if (!glfwInit()) {
        return false;
    }

#if defined(__APPLE__)
    constexpr auto glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    constexpr auto glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    window_ = glfwCreateWindow(1280, 720, "Hemlok", nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    return true;
}

int ProfilerUi::Start() {
    if (!Init()) {
        return EXIT_FAILURE;
    }

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        Draw();

        ImGui::Render();
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window_, &framebuffer_width,
                               &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.10F, 0.10F, 0.10F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }

    Shutdown();
    return EXIT_SUCCESS;
}

void ProfilerUi::Draw() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Hemlok", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::Button("Add target", ImVec2(160.0F, 40.0F))) {
        ImGui::OpenPopup("Add target");
    }

    if (ImGui::BeginPopupModal("Add target", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", target_name_.data(), target_name_.size());
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Optional");
        }
        if (ImGui::InputText("Executable path", executable_path_.data(),
                             executable_path_.size())) {
            invalid_path_ = false;
        }
        if (invalid_path_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0F, 0.3F, 0.3F, 1.0F), "Invalid path");
        }

        if (ImGui::Button("Add")) {
            std::filesystem::path executable_path(executable_path_.data());
            std::error_code error;
            if (!std::filesystem::is_regular_file(executable_path, error)) {
                invalid_path_ = true;
            } else {
                std::optional<std::string> name;
                if (target_name_[0] != '\0') {
                    name.emplace(target_name_.data());
                }
                profiler_.addProfilingTarget(profiler::ProfilingTarget{
                    std::move(name), std::move(executable_path)});
                target_name_.fill('\0');
                executable_path_.fill('\0');
                invalid_path_ = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            invalid_path_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::BeginChild("Targets", ImVec2(240.0F, 0.0F), true);
    for (const auto& [target_id, target] : profiler_.getTargets()) {
        const bool is_selected = selected_target_ == target_id;
        const std::string label =
            target.name.value_or(target.executable_path.string());

        ImGui::PushID(static_cast<int>(target_id));
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected_target_ = target_id;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (selected_target_.has_value()) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Profiler details will appear here.");
    }

    ImGui::End();
}

void ProfilerUi::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
}

}  // namespace hemlok::ui
