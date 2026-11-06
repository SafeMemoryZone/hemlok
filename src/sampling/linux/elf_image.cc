#include <cxxabi.h>
#include <elf.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sampling/linux/elf_image.hpp>
#include <string_view>

namespace hemlok::sampling::detail {
namespace {
// Copies a T from bytes[offset]. Fails instead of reading out of bounds.
template <typename T>
std::optional<T> ReadStruct(std::span<const std::byte> bytes,
                            std::uint64_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
        return std::nullopt;
    }
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

std::optional<Elf64_Shdr> ReadSectionHeader(std::span<const std::byte> bytes,
                                            const Elf64_Ehdr& header,
                                            std::uint64_t index) {
    if (index >= header.e_shnum) return std::nullopt;
    return ReadStruct<Elf64_Shdr>(bytes,
                                  header.e_shoff + index * sizeof(Elf64_Shdr));
}

// Reads a NUL-terminated string from a string table section.
std::optional<std::string_view> ReadString(std::span<const std::byte> bytes,
                                           const Elf64_Shdr& table,
                                           std::uint64_t offset) {
    if (table.sh_offset > bytes.size() ||
        table.sh_size > bytes.size() - table.sh_offset ||
        offset >= table.sh_size) {
        return std::nullopt;
    }
    const auto* begin =
        reinterpret_cast<const char*>(bytes.data() + table.sh_offset + offset);
    const auto length = strnlen(begin, table.sh_size - offset);
    if (length == table.sh_size - offset) return std::nullopt;
    return std::string_view(begin, length);
}

std::string Demangle(std::string_view name) {
    const std::string mangled(name);
    int status = 0;
    char* demangled =
        abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if (status != 0 || demangled == nullptr) return mangled;
    std::string result(demangled);
    std::free(demangled);
    return result;
}

std::vector<LoadSegment> ReadLoadSegments(std::span<const std::byte> bytes,
                                          const Elf64_Ehdr& header) {
    std::vector<LoadSegment> segments;
    for (std::uint64_t i = 0; i < header.e_phnum; ++i) {
        const auto program = ReadStruct<Elf64_Phdr>(
            bytes, header.e_phoff + i * sizeof(Elf64_Phdr));
        if (!program) break;
        if (program->p_type != PT_LOAD) continue;
        segments.push_back(
            {program->p_offset, program->p_filesz, program->p_vaddr});
    }
    return segments;
}

// Finds the first section of the given type, such as SHT_SYMTAB.
std::optional<Elf64_Shdr> FindSection(std::span<const std::byte> bytes,
                                      const Elf64_Ehdr& header,
                                      std::uint32_t type) {
    for (std::uint64_t i = 0; i < header.e_shnum; ++i) {
        const auto section = ReadSectionHeader(bytes, header, i);
        if (!section) return std::nullopt;
        if (section->sh_type == type) return section;
    }
    return std::nullopt;
}

std::vector<ElfSymbol> ReadFunctionSymbols(std::span<const std::byte> bytes,
                                           const Elf64_Ehdr& header) {
    auto table = FindSection(bytes, header, SHT_SYMTAB);
    if (!table) table = FindSection(bytes, header, SHT_DYNSYM);
    if (!table) return {};
    // A symbol table's sh_link names its string table.
    const auto names = ReadSectionHeader(bytes, header, table->sh_link);
    if (!names) return {};

    std::vector<ElfSymbol> symbols;
    const auto count = table->sh_size / sizeof(Elf64_Sym);
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto symbol = ReadStruct<Elf64_Sym>(
            bytes, table->sh_offset + i * sizeof(Elf64_Sym));
        if (!symbol) break;
        if (ELF64_ST_TYPE(symbol->st_info) != STT_FUNC) continue;
        if (symbol->st_shndx == SHN_UNDEF || symbol->st_size == 0) continue;
        const auto name = ReadString(bytes, *names, symbol->st_name);
        if (!name || name->empty()) continue;
        symbols.push_back({symbol->st_value, symbol->st_size, Demangle(*name)});
    }
    std::sort(symbols.begin(), symbols.end(),
              [](const ElfSymbol& left, const ElfSymbol& right) {
                  return left.address < right.address;
              });
    return symbols;
}
}  // namespace

std::optional<ElfImage> ParseElfImage(std::span<const std::byte> bytes) {
    const auto header = ReadStruct<Elf64_Ehdr>(bytes, 0);
    if (!header) return std::nullopt;
    if (std::memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 ||
        header->e_ident[EI_CLASS] != ELFCLASS64) {
        return std::nullopt;
    }
    return ElfImage{ReadLoadSegments(bytes, *header),
                    ReadFunctionSymbols(bytes, *header)};
}

std::optional<ElfImage> ReadElfImage(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    const std::vector<char> contents{std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>()};
    return ParseElfImage(std::as_bytes(std::span(contents)));
}

std::optional<std::uint64_t> FileOffsetToVirtualAddress(
    const ElfImage& image, std::uint64_t file_offset) {
    for (const auto& segment : image.segments) {
        if (file_offset >= segment.file_offset &&
            file_offset - segment.file_offset < segment.file_size) {
            return segment.virtual_address +
                   (file_offset - segment.file_offset);
        }
    }
    return std::nullopt;
}

const ElfSymbol* FindSymbol(const ElfImage& image,
                            std::uint64_t virtual_address) {
    // Find the last symbol starting at or before the address.
    const auto after = std::upper_bound(
        image.symbols.begin(), image.symbols.end(), virtual_address,
        [](std::uint64_t address, const ElfSymbol& symbol) {
            return address < symbol.address;
        });
    if (after == image.symbols.begin()) return nullptr;
    const auto& symbol = *std::prev(after);
    if (virtual_address - symbol.address >= symbol.size) return nullptr;
    return &symbol;
}
}  // namespace hemlok::sampling::detail
