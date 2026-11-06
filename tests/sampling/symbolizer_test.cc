#include <catch2/catch_test_macros.hpp>
#include <sampling/linux/symbolizer.hpp>

namespace {
using namespace hemlok::sampling::detail;

TEST_CASE("maps lines yield executable file mappings", "[sampling]") {
    const auto mapping = ParseMapsLine(
        "5581c000-5581e000 r-xp 00002000 103:02 1234   /usr/bin/my app");
    REQUIRE(mapping);
    REQUIRE(mapping->start == 0x5581c000);
    REQUIRE(mapping->end == 0x5581e000);
    REQUIRE(mapping->file_offset == 0x2000);
    REQUIRE(mapping->path == "/usr/bin/my app");
}

TEST_CASE("maps lines without an executable file are skipped", "[sampling]") {
    REQUIRE_FALSE(ParseMapsLine(
        "5581c000-5581e000 r--p 00000000 103:02 1234 /usr/bin/a"));
    REQUIRE_FALSE(
        ParseMapsLine("7ffd000-7ffe000 r-xp 00000000 00:00 0 [vdso]"));
    REQUIRE_FALSE(ParseMapsLine("7ffd000-7ffe000 rwxp 00000000 00:00 0"));
    REQUIRE_FALSE(ParseMapsLine("not a maps line"));
    REQUIRE_FALSE(ParseMapsLine(""));
}

TEST_CASE("runtime addresses resolve through mapping and ELF image",
          "[sampling]") {
    // The text segment is at file offset 0x1000 and linked at 0x1000. It is
    // mapped at runtime address 0x7000.
    const MemoryMapping mapping{0x7000, 0x8000, 0x1000, "/bin/test"};
    const ElfImage image{{{0x1000, 0x1000, 0x1000}},
                         {{0x1100, 0x40, "Compute()"}}};
    REQUIRE(ResolveAddress(mapping, image, 0x7120) == "Compute()");
    REQUIRE_FALSE(ResolveAddress(mapping, image, 0x7000));
    REQUIRE_FALSE(ResolveAddress(mapping, image, 0x8000));
}

TEST_CASE("the symbolizer reads executables named by mappings", "[sampling]") {
    const auto image = ReadElfImage(HEMLOK_BUSY_PROCESS_FIXTURE_PATH);
    REQUIRE(image);
    const ElfSymbol* busy_loop = nullptr;
    for (const auto& symbol : image->symbols) {
        if (symbol.name == "HemlokBusyLoop()") busy_loop = &symbol;
    }
    REQUIRE(busy_loop != nullptr);
    LoadSegment text{};
    for (const auto& segment : image->segments) {
        if (busy_loop->address >= segment.virtual_address &&
            busy_loop->address - segment.virtual_address < segment.file_size) {
            text = segment;
        }
    }
    // Pretend the segment containing the function is mapped at 0x10000.
    constexpr std::uint64_t kBase = 0x10000;
    Symbolizer symbolizer;
    symbolizer.AddMapping({kBase, kBase + text.file_size, text.file_offset,
                           HEMLOK_BUSY_PROCESS_FIXTURE_PATH});
    const auto address = kBase + (busy_loop->address - text.virtual_address);
    REQUIRE(symbolizer.Resolve(address) == "HemlokBusyLoop()");
    REQUIRE_FALSE(symbolizer.Resolve(kBase - 1));
}
}  // namespace
