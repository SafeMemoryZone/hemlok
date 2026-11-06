#ifndef HEMLOK_SAMPLING_LINUX_SYMBOLIZER_HPP_
#define HEMLOK_SAMPLING_LINUX_SYMBOLIZER_HPP_

#include <cstdint>
#include <optional>
#include <sampling/linux/elf_image.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hemlok::sampling::detail {
// An executable, file-backed region of the sampled process's address space.
struct MemoryMapping {
    std::uint64_t start;
    std::uint64_t end;
    std::uint64_t file_offset;
    std::string path;
};

// Parses one /proc/<pid>/maps line. Returns nullopt unless the line describes
// an executable mapping of a file.
std::optional<MemoryMapping> ParseMapsLine(std::string_view line);
// Names the function containing a runtime address inside the mapping.
std::optional<std::string> ResolveAddress(const MemoryMapping& mapping,
                                          const ElfImage& image,
                                          std::uint64_t address);

// Turns runtime addresses into function names. ELF files are read lazily, on
// the first address that falls inside one of their mappings.
class Symbolizer {
public:
    void AddMapping(MemoryMapping mapping);
    std::optional<std::string> Resolve(std::uint64_t address);

private:
    const ElfImage* GetImage(const std::string& path);
    std::vector<MemoryMapping> mappings_;
    // nullopt records a file that could not be read, so it is not retried.
    std::unordered_map<std::string, std::optional<ElfImage>> images_;
};
}  // namespace hemlok::sampling::detail

#endif  // HEMLOK_SAMPLING_LINUX_SYMBOLIZER_HPP_
