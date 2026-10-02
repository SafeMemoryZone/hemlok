#include <catch2/catch_test_macros.hpp>
#include <profiler/profiler.hpp>
#include <ui/profiler_ui.hpp>

namespace {
using hemlok::profiler::Profiler;
using hemlok::ui::ProfilerUi;

TEST_CASE("constructing the profiler UI does not change profiler state",
          "[ui]") {
    Profiler profiler;

    REQUIRE_NOTHROW(ProfilerUi{profiler});
    REQUIRE(profiler.getTargets().empty());
}
}  // namespace
