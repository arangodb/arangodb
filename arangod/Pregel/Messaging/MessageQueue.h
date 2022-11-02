#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

namespace arangodb::pregel {

// The pop-fct of this queue blocks if the queue is empty and returns a value as
// soon as a new value is pushed into the queue.

template<typename T>
struct MessageQueue {
 public:
  auto push(T message) -> void {
    {
      std::lock_guard lk(m);
      _queue.emplace_back(message);
      pushed = true;
    }
    cv.notify_one();
  }
  auto pop() -> T {
    std::unique_lock lk(m);
    if (_queue.empty()) {
      do {
        cv.wait(lk, [&] { return pushed; });
      } while (!pushed);
    }
    pushed = false;
    auto message = _queue.front();
    _queue.pop_front();
    return message;
  }

 private:
  std::deque<T> _queue = {};
  std::mutex m;
  std::condition_variable cv;
  bool pushed = false;
};
}  // namespace arangodb::pregel
