file(GLOB_RECURSE HEMLOK_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc"
)

add_executable(hemlok_process_fixture
    process/fixtures/waiting_process.cc
)

add_executable(hemlok_tests ${HEMLOK_TEST_SOURCES})
target_link_libraries(hemlok_tests PRIVATE
    hemlok_lib
    Catch2::Catch2WithMain
)
target_compile_definitions(hemlok_tests PRIVATE
    HEMLOK_PROCESS_FIXTURE_PATH="$<TARGET_FILE:hemlok_process_fixture>"
)
add_dependencies(hemlok_tests hemlok_process_fixture)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(hemlok_tests)
