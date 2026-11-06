#ifndef HEMLOK_SAMPLING_SAMPLE_HPP_
#define HEMLOK_SAMPLING_SAMPLE_HPP_

#include <cstdint>
#include <optional>
#include <string>

namespace hemlok::sampling {
// One observation of where the sampled thread was executing.
struct Sample {
    std::uint64_t instruction_pointer;
    // Empty when no symbol covers the instruction pointer.
    std::optional<std::string> function_name;
};
}  // namespace hemlok::sampling

#endif  // HEMLOK_SAMPLING_SAMPLE_HPP_
