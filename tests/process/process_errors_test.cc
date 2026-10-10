#include <catch2/catch_test_macros.hpp>
#include <process/process_errors.hpp>

namespace {
using boost::system::errc::errc_t;
using boost::system::errc::make_error_code;
using hemlok::process::ProcessError;
using namespace hemlok::process::detail;

TEST_CASE("launch errors retain their operational classification",
          "[process]") {
    struct Case {
        errc_t input;
        ProcessError expected;
    };
    const Case cases[] = {
        {errc_t::no_such_file_or_directory, ProcessError::kFileNotFound},
        {errc_t::not_a_directory, ProcessError::kFileNotFound},
        {errc_t::permission_denied, ProcessError::kPermissionDenied},
        {errc_t::operation_not_permitted, ProcessError::kPermissionDenied},
        {errc_t::executable_format_error, ProcessError::kInvalidExecutable},
        {errc_t::is_a_directory, ProcessError::kInvalidExecutable},
        {errc_t::io_error, ProcessError::kStartFailed},
    };
    for (const auto& test : cases) {
        CAPTURE(test.input);
        REQUIRE(MapLaunchError(make_error_code(test.input)) == test.expected);
    }
}

TEST_CASE("process query and termination errors preserve permission failures",
          "[process]") {
    for (const auto code :
         {errc_t::permission_denied, errc_t::operation_not_permitted}) {
        CAPTURE(code);
        const auto error = make_error_code(code);
        REQUIRE(MapStatusError(error) == ProcessError::kPermissionDenied);
        REQUIRE(MapTerminationError(error) == ProcessError::kPermissionDenied);
    }
    const auto other = make_error_code(errc_t::io_error);
    REQUIRE(MapStatusError(other) == ProcessError::kStatusQueryFailed);
    REQUIRE(MapTerminationError(other) == ProcessError::kTerminationFailed);
}
}  // namespace
