#include <cassert>
#include <process/process_errors.hpp>

namespace hemlok::process::detail {
namespace {
bool IsPermissionError(const boost::system::error_code& error) {
    return error == boost::system::errc::permission_denied ||
           error == boost::system::errc::operation_not_permitted;
}
}  // namespace

ProcessError MapLaunchError(const boost::system::error_code& error) {
    assert(error);
    if (error == boost::system::errc::no_such_file_or_directory ||
        error == boost::system::errc::not_a_directory) {
        return ProcessError::kFileNotFound;
    }
    if (IsPermissionError(error)) return ProcessError::kPermissionDenied;
    if (error == boost::system::errc::executable_format_error ||
        error == boost::system::errc::is_a_directory) {
        return ProcessError::kInvalidExecutable;
    }
    return ProcessError::kStartFailed;
}

ProcessError MapStatusError(const boost::system::error_code& error) {
    assert(error);
    return IsPermissionError(error) ? ProcessError::kPermissionDenied
                                    : ProcessError::kStatusQueryFailed;
}

ProcessError MapTerminationError(const boost::system::error_code& error) {
    assert(error);
    return IsPermissionError(error) ? ProcessError::kPermissionDenied
                                    : ProcessError::kTerminationFailed;
}
}  // namespace hemlok::process::detail
