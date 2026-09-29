#include <profiler/profiler.hpp>
#include <ui/profiler_ui.hpp>

int main() {
    hemlok::profiler::Profiler profiler;
    hemlok::ui::ProfilerUi profiler_ui(profiler);
    return profiler_ui.Start();
}
