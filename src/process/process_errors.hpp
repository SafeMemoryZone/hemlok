#ifndef HEMLOK_PROCESS_PROCESS_ERRORS_HPP_
#define HEMLOK_PROCESS_PROCESS_ERRORS_HPP_

#include <boost/system/error_code.hpp>
#include <process/process.hpp>

namespace hemlok::process::detail {
// Call only for nonzero errors returned by the corresponding operation.
ProcessError MapLaunchError(const boost::system::error_code& error);
ProcessError MapStatusError(const boost::system::error_code& error);
ProcessError MapTerminationError(const boost::system::error_code& error);
}  // namespace hemlok::process::detail

#endif  // HEMLOK_PROCESS_PROCESS_ERRORS_HPP_
