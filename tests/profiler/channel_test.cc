#include <catch2/catch_test_macros.hpp>
#include <future>
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

TEST_CASE("queued messages satisfy a channel wait immediately",
          "[profiler][channel]") {
    hemlok::profiler::Channel<int> channel;
    channel.Send(7);
    channel.Wait(std::stop_token{});
    const auto messages = channel.TakeAll();
    REQUIRE(messages == std::vector<int>{7});
}

TEST_CASE("sending wakes an idle or timed channel wait",
          "[profiler][channel]") {
    hemlok::profiler::Channel<int> channel;
    std::optional<std::chrono::milliseconds> timeout;
    SECTION("idle wait") {}
    SECTION("timed wait") { timeout = std::chrono::seconds{30}; }
    std::promise<void> finished;
    auto result = finished.get_future();
    std::jthread waiter([&](std::stop_token stop) {
        channel.Wait(stop, timeout);
        finished.set_value();
    });
    REQUIRE(result.wait_for(std::chrono::milliseconds{20}) ==
            std::future_status::timeout);
    channel.Send(7);
    REQUIRE(result.wait_for(std::chrono::seconds{5}) ==
            std::future_status::ready);
    REQUIRE(channel.TakeAll() == std::vector<int>{7});
}

TEST_CASE("shutdown interrupts idle and timed channel waits",
          "[profiler][channel]") {
    hemlok::profiler::Channel<int> channel;
    std::optional<std::chrono::milliseconds> timeout;
    SECTION("idle wait") {}
    SECTION("timed wait") { timeout = std::chrono::seconds{30}; }
    std::promise<void> finished;
    auto result = finished.get_future();
    std::jthread waiter([&](std::stop_token stop) {
        channel.Wait(stop, timeout);
        finished.set_value();
    });
    REQUIRE(result.wait_for(std::chrono::milliseconds{20}) ==
            std::future_status::timeout);
    waiter.request_stop();
    REQUIRE(result.wait_for(std::chrono::seconds{5}) ==
            std::future_status::ready);
    REQUIRE(channel.TakeAll().empty());
}

TEST_CASE("an already stopped channel wait preserves queued messages",
          "[profiler][channel]") {
    hemlok::profiler::Channel<int> channel;
    std::stop_source stop;
    stop.request_stop();
    std::vector<int> expected;
    SECTION("empty queue") {}
    SECTION("queued messages") {
        channel.Send(1);
        channel.Send(2);
        expected = {1, 2};
    }
    // CTest also bounds the whole case if a broken wait ignores this token.
    channel.Wait(stop.get_token());
    channel.Wait(stop.get_token(), std::chrono::seconds{30});
    REQUIRE(channel.TakeAll() == expected);
    REQUIRE(channel.TakeAll().empty());
}

TEST_CASE("timed channel waits return without a message",
          "[profiler][channel]") {
    hemlok::profiler::Channel<int> channel;
    const auto before = std::chrono::steady_clock::now();
    channel.Wait(std::stop_token{}, std::chrono::milliseconds{20});
    REQUIRE(std::chrono::steady_clock::now() - before >=
            std::chrono::milliseconds{20});
    REQUIRE(channel.TakeAll().empty());
}
}  // namespace
