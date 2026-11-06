#ifndef HEMLOK_SAMPLING_SAMPLING_SESSION_HPP_
#define HEMLOK_SAMPLING_SAMPLING_SESSION_HPP_

#include <expected>
#include <memory>
#include <sampling/sample.hpp>
#include <vector>

namespace hemlok::sampling {
enum class SamplingError {
    kProcessNotFound,
    kPermissionDenied,
    kMultiThreaded,
    kAttachFailed,
};

// Samples one process. The implementation is selected per platform at build
// time; each platform defines Impl in its own translation unit.
class SamplingSession {
public:
    SamplingSession(const SamplingSession&) = delete;
    SamplingSession& operator=(const SamplingSession&) = delete;
    SamplingSession(SamplingSession&&) noexcept;
    SamplingSession& operator=(SamplingSession&&) noexcept;
    ~SamplingSession();

    // Returns a session that is already sampling the process. Only
    // single-threaded processes are supported.
    static std::expected<SamplingSession, SamplingError> Attach(int pid);
    // Stops sampling without affecting the process. Samples recorded before
    // detaching remain available to DrainSamples(). Repeated calls do nothing.
    void Detach();
    // Returns the samples recorded since the previous drain, oldest first.
    std::vector<Sample> DrainSamples();

private:
    struct Impl;
    explicit SamplingSession(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
}  // namespace hemlok::sampling

#endif  // HEMLOK_SAMPLING_SAMPLING_SESSION_HPP_
