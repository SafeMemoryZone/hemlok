#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <profiler/channel.hpp>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {
TEST_CASE("channels drain move-only messages in order", "[profiler][channel]") {
    hemlok::profiler::Channel<std::unique_ptr<int>> channel;
    REQUIRE(channel.TakeAll().empty());
    channel.Send(std::make_unique<int>(1));
    channel.Send(std::make_unique<int>(2));
    const auto messages = channel.TakeAll();
    REQUIRE(messages.size() == 2);
    REQUIRE(*messages[0] == 1);
    REQUIRE(*messages[1] == 2);
    REQUIRE(channel.TakeAll().empty());
}
TEST_CASE("concurrent sends and drains preserve every message",
          "[profiler][channel]") {
    hemlok::profiler::Channel<int> channel;
    std::vector<std::jthread> senders;
    for (int sender = 0; sender < 4; ++sender) {
        senders.emplace_back([&, sender] {
            for (int i = 0; i < 1000; ++i) channel.Send(sender * 1000 + i);
        });
    }
    std::unordered_set<int> received;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (received.size() < 4000 &&
           std::chrono::steady_clock::now() < deadline) {
        for (const auto value : channel.TakeAll())
            REQUIRE(received.insert(value).second);
        std::this_thread::yield();
    }
    REQUIRE(received.size() == 4000);
}
}  // namespace
