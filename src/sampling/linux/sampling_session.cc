#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <sampling/linux/perf_records.hpp>
#include <sampling/linux/symbolizer.hpp>
#include <sampling/sampling_session.hpp>
#include <string>
#include <system_error>
#include <utility>

namespace hemlok::sampling {
namespace {
constexpr std::uint64_t kSamplingFrequencyHz = 1000;
// The perf data ring must be a power-of-two number of pages.
constexpr std::size_t kDataPageCount = 64;

std::expected<std::size_t, SamplingError> CountThreads(int pid) {
    std::error_code error;
    const std::filesystem::directory_iterator tasks(
        "/proc/" + std::to_string(pid) + "/task", error);
    if (error) return std::unexpected(SamplingError::kProcessNotFound);
    std::size_t count = 0;
    for (auto task = tasks; task != std::filesystem::directory_iterator();
         task.increment(error)) {
        if (error) return std::unexpected(SamplingError::kProcessNotFound);
        ++count;
    }
    return count;
}

std::vector<detail::MemoryMapping> ReadMemoryMappings(int pid) {
    std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
    std::vector<detail::MemoryMapping> mappings;
    std::string line;
    while (std::getline(maps, line)) {
        auto mapping = detail::ParseMapsLine(line);
        if (mapping) mappings.push_back(std::move(*mapping));
    }
    return mappings;
}

std::expected<int, SamplingError> OpenPerfEvent(int pid) {
    perf_event_attr attributes{};
    attributes.size = sizeof(attributes);
    // Task clock advances only while the process runs on a CPU.
    attributes.type = PERF_TYPE_SOFTWARE;
    attributes.config = PERF_COUNT_SW_TASK_CLOCK;
    attributes.freq = 1;
    attributes.sample_freq = kSamplingFrequencyHz;
    attributes.sample_type = PERF_SAMPLE_IP;
    // Unprivileged users may only sample user space.
    attributes.exclude_kernel = 1;
    attributes.exclude_hv = 1;
    // Report executable mappings created after attaching, such as shared
    // libraries loaded by the dynamic linker.
    attributes.mmap = 1;
    const auto fd = static_cast<int>(syscall(
        SYS_perf_event_open, &attributes, pid, -1, -1, PERF_FLAG_FD_CLOEXEC));
    if (fd >= 0) return fd;
    switch (errno) {
        case EACCES:
        case EPERM:
            return std::unexpected(SamplingError::kPermissionDenied);
        case ESRCH:
            return std::unexpected(SamplingError::kProcessNotFound);
        default:
            return std::unexpected(SamplingError::kAttachFailed);
    }
}
}  // namespace

// Owns the perf event file descriptor and its memory-mapped ring buffer.
struct SamplingSession::Impl {
    int fd;
    void* buffer;
    std::size_t buffer_size;
    std::size_t page_size;
    detail::Symbolizer symbolizer;

    Impl(int fd, void* buffer, std::size_t buffer_size, std::size_t page_size)
        : fd(fd),
          buffer(buffer),
          buffer_size(buffer_size),
          page_size(page_size) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() {
        munmap(buffer, buffer_size);
        close(fd);
    }

    void Detach() { ioctl(fd, PERF_EVENT_IOC_DISABLE, 0); }

    std::vector<Sample> DrainSamples() {
        // The first page holds metadata; the data ring follows it.
        auto* metadata = static_cast<perf_event_mmap_page*>(buffer);
        const std::span<const std::byte> ring(
            static_cast<const std::byte*>(buffer) + page_size,
            buffer_size - page_size);
        // Acquire pairs with the kernel's write of data_head. Releasing
        // data_tail hands the consumed bytes back to the kernel.
        const auto head =
            __atomic_load_n(&metadata->data_head, __ATOMIC_ACQUIRE);
        const auto tail = metadata->data_tail;
        const auto bytes = detail::CopyRingData(ring, tail, head);
        __atomic_store_n(&metadata->data_tail, head, __ATOMIC_RELEASE);

        std::vector<Sample> samples;
        for (auto& record : detail::ParsePerfRecords(bytes)) {
            if (auto* mapping = std::get_if<detail::MemoryMapping>(&record)) {
                symbolizer.AddMapping(std::move(*mapping));
                continue;
            }
            const auto address =
                std::get<detail::IpSample>(record).instruction_pointer;
            samples.push_back({address, symbolizer.Resolve(address)});
        }
        return samples;
    }
};

SamplingSession::SamplingSession(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
SamplingSession::SamplingSession(SamplingSession&&) noexcept = default;
SamplingSession& SamplingSession::operator=(SamplingSession&&) noexcept =
    default;
SamplingSession::~SamplingSession() = default;

std::expected<SamplingSession, SamplingError> SamplingSession::Attach(int pid) {
    const auto thread_count = CountThreads(pid);
    if (!thread_count) return std::unexpected(thread_count.error());
    if (*thread_count > 1) {
        return std::unexpected(SamplingError::kMultiThreaded);
    }

    const auto fd = OpenPerfEvent(pid);
    if (!fd) return std::unexpected(fd.error());
    const auto page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    const auto buffer_size = (1 + kDataPageCount) * page_size;
    void* buffer =
        mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (buffer == MAP_FAILED) {
        close(*fd);
        return std::unexpected(SamplingError::kAttachFailed);
    }
    auto impl = std::make_unique<Impl>(*fd, buffer, buffer_size, page_size);

    // Read existing mappings after sampling starts, so that a mapping created
    // in between is reported by perf rather than lost.
    for (auto& mapping : ReadMemoryMappings(pid)) {
        impl->symbolizer.AddMapping(std::move(mapping));
    }
    return SamplingSession(std::move(impl));
}

void SamplingSession::Detach() {
    assert(impl_);
    impl_->Detach();
}

std::vector<Sample> SamplingSession::DrainSamples() {
    assert(impl_);
    return impl_->DrainSamples();
}
}  // namespace hemlok::sampling
