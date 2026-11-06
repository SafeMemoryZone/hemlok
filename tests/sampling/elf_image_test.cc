#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <sampling/linux/elf_image.hpp>
#include <support/temporary_directory.hpp>
#include <vector>

namespace {
using namespace hemlok::sampling::detail;

TEST_CASE("file offsets map through the containing load segment",
          "[sampling]") {
    const ElfImage image{{{0x0, 0x1000, 0x400000}, {0x2000, 0x500, 0x602000}},
                         {}};
    REQUIRE(FileOffsetToVirtualAddress(image, 0x10) == 0x400010);
    REQUIRE(FileOffsetToVirtualAddress(image, 0x2100) == 0x602100);
    REQUIRE_FALSE(FileOffsetToVirtualAddress(image, 0x1000));
    REQUIRE_FALSE(FileOffsetToVirtualAddress(image, 0x2500));
}

TEST_CASE("symbol lookup respects symbol bounds", "[sampling]") {
    const ElfImage image{{}, {{0x100, 0x10, "first"}, {0x200, 0x20, "second"}}};
    REQUIRE(FindSymbol(image, 0x100)->name == "first");
    REQUIRE(FindSymbol(image, 0x10f)->name == "first");
    REQUIRE(FindSymbol(image, 0x21f)->name == "second");
    REQUIRE(FindSymbol(image, 0x0ff) == nullptr);
    REQUIRE(FindSymbol(image, 0x110) == nullptr);
    REQUIRE(FindSymbol(image, 0x220) == nullptr);
}

TEST_CASE("non-ELF input is rejected", "[sampling]") {
    const std::vector<std::byte> empty;
    REQUIRE_FALSE(ParseElfImage(empty));
    const std::vector<std::byte> garbage(128, std::byte{0x42});
    REQUIRE_FALSE(ParseElfImage(garbage));
    hemlok::testing::TemporaryDirectory directory;
    REQUIRE_FALSE(ReadElfImage(directory.GetPath() / "missing"));
}

TEST_CASE("a compiled executable yields demangled function symbols",
          "[sampling]") {
    const auto image = ReadElfImage(HEMLOK_BUSY_PROCESS_FIXTURE_PATH);
    REQUIRE(image);
    REQUIRE_FALSE(image->segments.empty());
    bool found = false;
    for (const auto& symbol : image->symbols) {
        if (symbol.name == "HemlokBusyLoop()") {
            found = true;
            REQUIRE(FindSymbol(*image, symbol.address) == &symbol);
        }
    }
    REQUIRE(found);
}
}  // namespace
