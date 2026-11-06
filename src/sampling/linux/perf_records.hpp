#ifndef HEMLOK_SAMPLING_LINUX_PERF_RECORDS_HPP_
#define HEMLOK_SAMPLING_LINUX_PERF_RECORDS_HPP_

#include <cstddef>
#include <cstdint>
#include <sampling/linux/symbolizer.hpp>
#include <span>
#include <variant>
#include <vector>

namespace hemlok::sampling::detail {
// A PERF_RECORD_SAMPLE opened with sample_type == PERF_SAMPLE_IP.
struct IpSample {
    std::uint64_t instruction_pointer;
};
// IpSample, or a PERF_RECORD_MMAP for a newly mapped executable file.
using PerfRecord = std::variant<IpSample, MemoryMapping>;

// Copies the unread bytes [tail, head) out of the perf data ring. head and
// tail are free-running byte counters; ring.size() must be a power of two.
std::vector<std::byte> CopyRingData(std::span<const std::byte> ring,
                                    std::uint64_t tail, std::uint64_t head);
// Parses contiguous perf records. Other record types are skipped, and parsing
// stops at the first truncated record.
std::vector<PerfRecord> ParsePerfRecords(std::span<const std::byte> bytes);
}  // namespace hemlok::sampling::detail

#endif  // HEMLOK_SAMPLING_LINUX_PERF_RECORDS_HPP_
