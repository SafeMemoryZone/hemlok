#ifndef HEMLOK_PROFILER_CHANNEL_HPP_
#define HEMLOK_PROFILER_CHANNEL_HPP_

#include <mutex>
#include <utility>
#include <vector>

namespace hemlok::profiler {
template <typename T>
class Channel {
public:
    void Send(T message) {
        std::lock_guard lock(mutex_);
        pending_.push_back(std::move(message));
    }
    std::vector<T> TakeAll() {
        std::lock_guard lock(mutex_);
        std::vector<T> messages;
        messages.swap(pending_);
        return messages;
    }

private:
    std::mutex mutex_;
    std::vector<T> pending_;
};
}  // namespace hemlok::profiler

#endif  // HEMLOK_PROFILER_CHANNEL_HPP_
