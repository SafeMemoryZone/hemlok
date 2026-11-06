#include <chrono>
#include <thread>

int main() {
    std::thread worker(
        [] { std::this_thread::sleep_for(std::chrono::minutes{1}); });
    worker.join();
}
