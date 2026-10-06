#ifndef HEMLOK_PROFILER_CHANNEL_HPP_
#define HEMLOK_PROFILER_CHANNEL_HPP_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace hemlok::profiler {
template <typename T>
class Channel {
public:
    void Send(T message) {
        {
            std::lock_guard lock(mutex_);
            pending_.push_back(std::move(message));
        }
        ready_.notify_one();
    }
    // Waits for pending messages, shutdown, or an optional timeout.
    // Messages remain queued until TakeAll().
    void Wait(std::stop_token stop,
              std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        std::unique_lock lock(mutex_);
        const auto has_messages = [this] { return !pending_.empty(); };
        if (timeout) {
            ready_.wait_for(lock, stop, *timeout, has_messages);
        } else {
            ready_.wait(lock, stop, has_messages);
        }
    }
    std::vector<T> TakeAll() {
        std::lock_guard lock(mutex_);
        std::vector<T> messages;
        messages.swap(pending_);
        return messages;
    }

private:
    std::mutex mutex_;
    std::condition_variable_any ready_;
    std::vector<T> pending_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_CHANNEL_HPP_
