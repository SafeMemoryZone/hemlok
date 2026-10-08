#ifndef HEMLOK_TESTS_SUPPORT_TEMPORARY_DIRECTORY_HPP_
#define HEMLOK_TESTS_SUPPORT_TEMPORARY_DIRECTORY_HPP_

#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

namespace hemlok::testing {
class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const std::filesystem::path root{HEMLOK_TEST_TEMP_DIR};
        std::filesystem::create_directories(root);
        std::random_device random;
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = root / ("case-" + std::to_string(random()));
            if (std::filesystem::create_directory(path_)) return;
        }
        throw std::runtime_error("Could not create a unique test directory");
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    const std::filesystem::path& GetPath() const { return path_; }

private:
    std::filesystem::path path_;
};
}  // namespace hemlok::testing

#endif  // HEMLOK_TESTS_SUPPORT_TEMPORARY_DIRECTORY_HPP_
