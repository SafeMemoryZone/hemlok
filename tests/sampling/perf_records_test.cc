#include <linux/perf_event.h>

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <sampling/linux/perf_records.hpp>
#include <string>
#include <vector>

namespace {
using namespace hemlok::sampling::detail;

template <typename T>
void Append(std::vector<std::byte>& bytes, const T& value) {
    const auto* begin = reinterpret_cast<const std::byte*>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(T));
}

void AppendHeader(std::vector<std::byte>& bytes, std::uint32_t type,
                  std::uint16_t size) {
    Append(bytes, perf_event_header{type, 0, size});
}

TEST_CASE("ring data is copied across the wrap point", "[sampling]") {
    const std::vector<std::byte> ring{std::byte{0}, std::byte{1}, std::byte{2},
                                      std::byte{3}};
    // Free-running counters: positions 6 and 7 wrap to indices 2 and 3.
    REQUIRE(CopyRingData(ring, 6, 10) ==
            std::vector<std::byte>{std::byte{2}, std::byte{3}, std::byte{0},
                                   std::byte{1}});
    REQUIRE(CopyRingData(ring, 5, 5).empty());
}

TEST_CASE("sample and mmap records are parsed in order", "[sampling]") {
    std::vector<std::byte> bytes;
    AppendHeader(bytes, PERF_RECORD_SAMPLE, 16);
    Append(bytes, std::uint64_t{0x1234});

    // pid, tid, address, length, offset, then an 8-byte padded file name.
    AppendHeader(bytes, PERF_RECORD_MMAP, 8 + 32 + 8);
    Append(bytes, std::uint32_t{1});
    Append(bytes, std::uint32_t{1});
    Append(bytes, std::uint64_t{0x7000});
    Append(bytes, std::uint64_t{0x1000});
    Append(bytes, std::uint64_t{0x2000});
    const char name[8] = "/lib/a";
    Append(bytes, name);

    // Unknown record types are skipped.
    AppendHeader(bytes, PERF_RECORD_LOST, 24);
    Append(bytes, std::uint64_t{0});
    Append(bytes, std::uint64_t{0});

    AppendHeader(bytes, PERF_RECORD_SAMPLE, 16);
    Append(bytes, std::uint64_t{0x5678});

    const auto records = ParsePerfRecords(bytes);
    REQUIRE(records.size() == 3);
    REQUIRE(std::get<IpSample>(records[0]).instruction_pointer == 0x1234);
    const auto& mapping = std::get<MemoryMapping>(records[1]);
    REQUIRE(mapping.start == 0x7000);
    REQUIRE(mapping.end == 0x8000);
    REQUIRE(mapping.file_offset == 0x2000);
    REQUIRE(mapping.path == "/lib/a");
    REQUIRE(std::get<IpSample>(records[2]).instruction_pointer == 0x5678);
}

TEST_CASE("parsing stops at a truncated record", "[sampling]") {
    std::vector<std::byte> bytes;
    AppendHeader(bytes, PERF_RECORD_SAMPLE, 16);
    Append(bytes, std::uint64_t{0x1234});
    AppendHeader(bytes, PERF_RECORD_SAMPLE, 16);
    REQUIRE(ParsePerfRecords(bytes).size() == 1);
}
}  // namespace
