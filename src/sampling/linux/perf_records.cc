#include <linux/perf_event.h>

#include <cstring>
#include <sampling/linux/perf_records.hpp>
#include <string>

namespace hemlok::sampling::detail {
namespace {
// Layout of PERF_RECORD_MMAP when sample_id_all is not set. The NUL-padded
// file name follows the fixed fields.
struct MmapRecordFields {
    std::uint32_t pid;
    std::uint32_t tid;
    std::uint64_t address;
    std::uint64_t length;
    std::uint64_t file_offset;
};

std::optional<MemoryMapping> ParseMmapRecord(std::span<const std::byte> body) {
    if (body.size() < sizeof(MmapRecordFields)) return std::nullopt;
    MmapRecordFields fields;
    std::memcpy(&fields, body.data(), sizeof(fields));
    const auto name = body.subspan(sizeof(fields));
    const auto* name_begin = reinterpret_cast<const char*>(name.data());
    std::string path(name_begin, strnlen(name_begin, name.size()));
    // Anonymous executable memory, such as JIT code, has no file to read.
    if (!path.starts_with('/')) return std::nullopt;
    return MemoryMapping{fields.address, fields.address + fields.length,
                         fields.file_offset, std::move(path)};
}
}  // namespace

std::vector<std::byte> CopyRingData(std::span<const std::byte> ring,
                                    std::uint64_t tail, std::uint64_t head) {
    std::vector<std::byte> bytes;
    bytes.reserve(head - tail);
    for (auto position = tail; position != head; ++position) {
        bytes.push_back(ring[position & (ring.size() - 1)]);
    }
    return bytes;
}

std::vector<PerfRecord> ParsePerfRecords(std::span<const std::byte> bytes) {
    std::vector<PerfRecord> records;
    while (bytes.size() >= sizeof(perf_event_header)) {
        perf_event_header header;
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (header.size < sizeof(header) || header.size > bytes.size()) break;
        const auto body =
            bytes.subspan(sizeof(header), header.size - sizeof(header));
        if (header.type == PERF_RECORD_SAMPLE &&
            body.size() >= sizeof(std::uint64_t)) {
            IpSample sample;
            std::memcpy(&sample.instruction_pointer, body.data(),
                        sizeof(sample.instruction_pointer));
            records.emplace_back(sample);
        } else if (header.type == PERF_RECORD_MMAP) {
            auto mapping = ParseMmapRecord(body);
            if (mapping) records.emplace_back(std::move(*mapping));
        }
        bytes = bytes.subspan(header.size);
    }
    return records;
}
}  // namespace hemlok::sampling::detail
