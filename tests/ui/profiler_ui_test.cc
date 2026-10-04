#include <catch2/catch_test_macros.hpp>
#include <profiler/profiler.hpp>
#include <type_traits>
#include <ui/profiler_ui.hpp>

namespace {
using hemlok::profiler::Profiler;
using hemlok::ui::ProfilerUi;

static_assert(!std::is_copy_constructible_v<ProfilerUi>);
static_assert(!std::is_copy_assignable_v<ProfilerUi>);
static_assert(!std::is_move_constructible_v<ProfilerUi>);
static_assert(!std::is_move_assignable_v<ProfilerUi>);

TEST_CASE("constructing the profiler UI does not change profiler state",
          "[ui]") {
    Profiler profiler;

    REQUIRE_NOTHROW(ProfilerUi{profiler});
    REQUIRE(profiler.GetTargets().empty());
}
}  // namespace
