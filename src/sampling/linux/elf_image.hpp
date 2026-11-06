#ifndef HEMLOK_SAMPLING_LINUX_ELF_IMAGE_HPP_
#define HEMLOK_SAMPLING_LINUX_ELF_IMAGE_HPP_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace hemlok::sampling::detail {
// A PT_LOAD program header: file bytes that are mapped at a virtual address.
struct LoadSegment {
    std::uint64_t file_offset;
    std::uint64_t file_size;
    std::uint64_t virtual_address;
};

struct ElfSymbol {
    std::uint64_t address;
    std::uint64_t size;
    std::string name;
};

// The parts of a 64-bit ELF file needed to name sampled addresses.
struct ElfImage {
    std::vector<LoadSegment> segments;
    // Function symbols with a nonzero size, sorted by address.
    std::vector<ElfSymbol> symbols;
};

// Uses .symtab when present, otherwise .dynsym. C++ names are demangled.
// Returns nullopt for anything that is not a well-formed 64-bit ELF file.
std::optional<ElfImage> ParseElfImage(std::span<const std::byte> bytes);
std::optional<ElfImage> ReadElfImage(const std::filesystem::path& path);

std::optional<std::uint64_t> FileOffsetToVirtualAddress(
    const ElfImage& image, std::uint64_t file_offset);
const ElfSymbol* FindSymbol(const ElfImage& image,
                            std::uint64_t virtual_address);
}  // namespace hemlok::sampling::detail

#endif  // HEMLOK_SAMPLING_LINUX_ELF_IMAGE_HPP_
