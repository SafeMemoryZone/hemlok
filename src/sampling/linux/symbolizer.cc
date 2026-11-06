#include <algorithm>
#include <charconv>
#include <sampling/linux/symbolizer.hpp>
#include <utility>

namespace hemlok::sampling::detail {
namespace {
// Splits off the next space-separated field and advances `text` past it.
std::string_view NextField(std::string_view& text) {
    const auto begin = text.find_first_not_of(' ');
    if (begin == std::string_view::npos) {
        text = {};
        return {};
    }
    text.remove_prefix(begin);
    const auto end = std::min(text.find(' '), text.size());
    const auto field = text.substr(0, end);
    text.remove_prefix(end);
    return field;
}

std::optional<std::uint64_t> ParseHex(std::string_view text) {
    std::uint64_t value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}
}  // namespace

std::optional<MemoryMapping> ParseMapsLine(std::string_view line) {
    // Format: start-end perms offset dev inode [path]
    auto rest = line;
    const auto range = NextField(rest);
    const auto permissions = NextField(rest);
    const auto offset = NextField(rest);
    NextField(rest);  // device
    NextField(rest);  // inode
    const auto path_begin = rest.find_first_not_of(' ');
    if (path_begin == std::string_view::npos) return std::nullopt;
    const auto path = rest.substr(path_begin);

    const auto dash = range.find('-');
    if (dash == std::string_view::npos) return std::nullopt;
    const auto start = ParseHex(range.substr(0, dash));
    const auto end = ParseHex(range.substr(dash + 1));
    const auto file_offset = ParseHex(offset);
    if (!start || !end || !file_offset) return std::nullopt;
    // Skip non-executable regions and pseudo-paths such as [vdso].
    if (permissions.size() < 3 || permissions[2] != 'x') return std::nullopt;
    if (!path.starts_with('/')) return std::nullopt;
    return MemoryMapping{*start, *end, *file_offset, std::string(path)};
}

std::optional<std::string> ResolveAddress(const MemoryMapping& mapping,
                                          const ElfImage& image,
                                          std::uint64_t address) {
    if (address < mapping.start || address >= mapping.end) return std::nullopt;
    const auto file_offset = address - mapping.start + mapping.file_offset;
    const auto virtual_address = FileOffsetToVirtualAddress(image, file_offset);
    if (!virtual_address) return std::nullopt;
    const auto* symbol = FindSymbol(image, *virtual_address);
    if (symbol == nullptr) return std::nullopt;
    return symbol->name;
}

void Symbolizer::AddMapping(MemoryMapping mapping) {
    mappings_.push_back(std::move(mapping));
}

std::optional<std::string> Symbolizer::Resolve(std::uint64_t address) {
    // Search newest first: a later mapping replaces an older one at the same
    // address.
    for (auto mapping = mappings_.rbegin(); mapping != mappings_.rend();
         ++mapping) {
        if (address < mapping->start || address >= mapping->end) continue;
        const auto* image = GetImage(mapping->path);
        if (image == nullptr) return std::nullopt;
        return ResolveAddress(*mapping, *image, address);
    }
    return std::nullopt;
}

const ElfImage* Symbolizer::GetImage(const std::string& path) {
    auto entry = images_.find(path);
    if (entry == images_.end()) {
        entry = images_.emplace(path, ReadElfImage(path)).first;
    }
    if (!entry->second) return nullptr;
    return &*entry->second;
}
}  // namespace hemlok::sampling::detail
