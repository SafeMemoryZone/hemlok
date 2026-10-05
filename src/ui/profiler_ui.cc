#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <boost/scope/scope_exit.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <optional>
#include <profiler/profiling_target.hpp>
#include <string>
#include <system_error>
#include <ui/profiler_ui.hpp>
#include <utility>
#include <vector>

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
    profiler_
        .Poll();  // NOTE: this will eventually be done by the sampling worker
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
            run_error_ = nullptr;
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
                DrawTargetDetails(target->first, target->second);
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

    selected_target_ = profiler_.AddTarget(std::move(*target));
    return true;
}

void ProfilerUi::DrawTargetDetails(profiler::TargetId id,
                                   const profiler::ProfilingTarget& target) {
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

    if (ImGui::Button("Start profiling")) {
        const auto result = profiler_.StartRun(id);
        run_error_ = nullptr;
        if (!result) {
            switch (result.error()) {
                case profiler::ProfilerError::kProcessStartFailed:
                    run_error_ =
                        "Could not start the executable. Check its path and "
                        "permissions.";
                    break;
                case profiler::ProfilerError::kProcessTerminated:
                    run_error_ =
                        "The process exited before the run could start.";
                    break;
                case profiler::ProfilerError::kStatusQueryFailed:
                    run_error_ = "Could not query the process status.";
                    break;
                case profiler::ProfilerError::kTerminationFailed:
                    run_error_ = "Could not terminate the process.";
                    break;
            }
        }
    }
    DrawRuns(id);
    if (run_error_ != nullptr) {
        ImGui::TextColored(ImVec4(1.0F, 0.3F, 0.3F, 1.0F), "%s", run_error_);
    }
}

void ProfilerUi::DrawRuns(profiler::TargetId target) {
    ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 1.0F);
    ImGui::SeparatorText("Runs");
    ImGui::PopStyleVar();
    std::vector<profiler::RunId> run_ids;
    for (const auto& [id, entry] : profiler_.GetRuns()) {
        if (entry.id == target) {
            run_ids.push_back(id);
        }
    }
    std::sort(run_ids.rbegin(), run_ids.rend());
    if (run_ids.empty()) {
        ImGui::TextDisabled("No runs yet.");
        return;
    }
    if (!ImGui::BeginTable("Runs", 6,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                               ImGuiTableFlags_ScrollX)) {
        return;
    }
    for (const char* label :
         {"Run", "Status", "Started", "Finished", "Duration", "Action"}) {
        ImGui::TableSetupColumn(label);
    }
    ImGui::TableHeadersRow();
    const auto draw_time = [](std::chrono::system_clock::time_point time) {
        const std::time_t value = std::chrono::system_clock::to_time_t(time);
        const std::tm* local = std::localtime(&value);
        char text[32]{};
        if (local &&
            std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", local)) {
            ImGui::TextUnformatted(text);
        } else {
            ImGui::TextUnformatted("Unavailable");
        }
    };
    // Number runs in creation order within this target, newest first.
    std::size_t run_number = run_ids.size();
    for (const auto id : run_ids) {
        const auto& run = profiler_.GetRuns().at(id).profiling_run;
        const std::string widget_id = std::to_string(id);
        const std::string label = "#" + std::to_string(run_number--);
        ImGui::PushID(widget_id.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label.c_str());
        ImGui::TableNextColumn();
        if (const auto error = run.GetPollError()) {
            ImGui::TextColored(ImVec4(1.0F, 0.7F, 0.2F, 1.0F), "Status stale");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    *error == process::ProcessError::kPermissionDenied
                        ? "Permission denied while checking process status. "
                          "Retrying."
                        : "Could not check process status. Retrying.");
            }
        } else {
            ImGui::TextUnformatted(run.IsRunning() ? "Running" : "Finished");
        }
        ImGui::TableNextColumn();
        draw_time(run.GetStartTime());
        ImGui::TableNextColumn();
        if (run.GetFinishTime()) {
            draw_time(*run.GetFinishTime());
        } else {
            ImGui::TextUnformatted("--");
        }
        ImGui::TableNextColumn();
        const auto end =
            run.GetFinishTime().value_or(std::chrono::system_clock::now());
        const double seconds =
            std::chrono::duration<double>(end - run.GetStartTime()).count();
        ImGui::Text("%.2f s", std::max(0.0, seconds));
        ImGui::TableNextColumn();
        if (run.IsRunning() && ImGui::Button("Terminate")) {
            run_error_ = profiler_.TerminateRun(id)
                             ? nullptr
                             : "Could not terminate the process.";
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
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
