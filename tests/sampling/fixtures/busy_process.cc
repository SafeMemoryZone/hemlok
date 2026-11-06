#include <chrono>

// Keep this function out of line so samples can be attributed to it by name.
// Most time goes to arithmetic rather than to reading the clock.
__attribute__((noinline)) void HemlokBusyLoop() {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::minutes{1};
    volatile unsigned long counter = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        for (int i = 0; i < 1'000'000; ++i) {
            counter = counter + 1;
        }
    }
}

int main() { HemlokBusyLoop(); }
