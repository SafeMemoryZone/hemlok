#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <boost/scope/scope_exit.hpp>
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
    constexpr auto kGlslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    constexpr auto kGlslVersion = "#version 130";
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
    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init(kGlslVersion)) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
        return false;
    }
    return true;
}

int ProfilerUi::Start() {
    if (!Init()) {
        return EXIT_FAILURE;
    }
    auto cleanup = boost::scope::scope_exit([this] { Shutdown(); });

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
        if (ImGui::InputText("Name", target_name_.data(),
                             target_name_.size())) {
            target_error_ = nullptr;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Optional");
        }
        if (ImGui::InputText("Executable path", executable_path_.data(),
                             executable_path_.size())) {
            target_error_ = nullptr;
        }
        if (target_error_ != nullptr) {
            ImGui::TextColored(ImVec4(1.0F, 0.3F, 0.3F, 1.0F), "%s",
                               target_error_);
        }

        if (ImGui::Button("Add") && AddTarget()) {
            target_name_.fill('\0');
            executable_path_.fill('\0');
            target_error_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            target_error_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::BeginChild("Targets", ImVec2(240.0F, 0.0F), true);
    for (const auto& [target_id, target] : profiler_.GetTargets()) {
        const bool is_selected = selected_target_ == target_id;
        const std::string label =
            target.GetName().value_or(target.GetExecutablePath().string());

        ImGui::PushID(static_cast<int>(target_id));
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected_target_ = target_id;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (selected_target_.has_value()) {
        const auto& targets = profiler_.GetTargets();
        const auto target = targets.find(*selected_target_);
        if (target != targets.end()) {
            ImGui::SameLine();
            if (ImGui::BeginChild("Target details")) {
                DrawTargetDetails(target->second);
            }
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

bool ProfilerUi::AddTarget() {
    std::filesystem::path executable_path(executable_path_.data());
    if (executable_path.empty()) {
        target_error_ = "Enter an executable path.";
        return false;
    }

    std::error_code error;
    executable_path = std::filesystem::absolute(executable_path, error);
    if (error || !std::filesystem::is_regular_file(executable_path, error)) {
        target_error_ = "Path must refer to an existing regular file.";
        return false;
    }

    std::optional<std::string> name;
    if (target_name_[0] != '\0') {
        name.emplace(target_name_.data());
    }
    auto target = profiler::ProfilingTarget::Create(std::move(name),
                                                    std::move(executable_path));
    if (!target) {
        switch (target.error()) {
            case profiler::TargetError::kBlankName:
                target_error_ = "Name must contain a non-whitespace character.";
                break;
            case profiler::TargetError::kEmptyPath:
                target_error_ = "Enter an executable path.";
                break;
            case profiler::TargetError::kRelativePath:
                target_error_ = "Executable path must be absolute.";
                break;
        }
        return false;
    }

    selected_target_ = profiler_.AddProfilingTarget(std::move(*target));
    return true;
}

void ProfilerUi::DrawTargetDetails(const profiler::ProfilingTarget& target) {
    const ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushTextWrapPos(0.0F);
    if (target.GetName().has_value()) {
        ImGui::TextDisabled("Name");
        ImGui::TextUnformatted(target.GetName()->c_str());
        ImGui::Spacing();
    }
    ImGui::TextDisabled("Executable");
    ImGui::TextUnformatted(target.GetExecutablePath().string().c_str());
    ImGui::PopTextWrapPos();

    ImGui::Separator();
    ImGui::Spacing();

    constexpr const char* kButtonLabel = "Start profiling";
    constexpr const char* kHelpText =
        "Profile this target to start collecting performance data.";
    const ImVec2 origin = ImGui::GetCursorPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float width = std::max(1.0F, available.x);
    const float wrap_width = std::min(width, ImGui::GetFontSize() * 26.0F);
    const ImVec2 help_size =
        ImGui::CalcTextSize(kHelpText, nullptr, false, wrap_width);
    const ImVec2 button_size(
        std::min(width, ImGui::CalcTextSize(kButtonLabel).x +
                            style.FramePadding.x * 6.0F),
        ImGui::GetFrameHeight() + style.FramePadding.y * 2.0F);
    const float content_height =
        button_size.y + style.ItemSpacing.y * 2.0F + help_size.y;
    const float content_top =
        origin.y + std::max(0.0F, (available.y - content_height) * 0.5F);

    ImGui::SetCursorPos(
        ImVec2(origin.x + (width - button_size.x) * 0.5F, content_top));
    // Profiling will be wired up when the backend supports starting a session.
    ImGui::Button(kButtonLabel, button_size);

    ImGui::SetCursorPos(
        ImVec2(origin.x + (width - help_size.x) * 0.5F,
               content_top + button_size.y + style.ItemSpacing.y * 2.0F));
    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
    ImGui::TextUnformatted(kHelpText);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
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
