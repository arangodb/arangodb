#ifndef FUTURES_LOCKS_H
#define FUTURES_LOCKS_H
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include "futures.h"

namespace mellon {

struct immediate_executor {
  template <typename F>
  void operator()(F&& f) const noexcept {
    static_assert(std::is_nothrow_invocable_r_v<void, F>);
    std::invoke(std::forward<F>(f));
  }
};

template <typename Tag, typename Executor = immediate_executor>
struct future_mutex {
  using lock_guard = std::unique_lock<future_mutex<Tag, Executor>>;
  using future_type = future<lock_guard, Tag>;

  explicit future_mutex(Executor ex = Executor()) : _executor(std::move(ex)) {}

  void lock() {
    async_lock().await().release();
  }

  void unlock() {
    std::unique_lock guard(_mutex);
    // assert(_is_locked == true);
    if (!_queue.empty()) {
      auto p = std::move(_queue.front());
      _queue.pop_front();
      guard.unlock();

      _executor([this, p = std::move(p)]() mutable noexcept {
        std::move(p).fulfill(*this, std::adopt_lock);
      });
    } else {
      _is_locked = false;
    }
  }

  auto async_lock() -> future_type {
    std::unique_lock guard(_mutex);
    if (_is_locked) {
      auto&& [f, p] = make_promise<lock_guard, Tag>();
      _queue.emplace_back(std::move(p));
      return std::move(f);
    } else {
      _is_locked = true;
      return future_type{std::in_place, *this, std::adopt_lock};
    }
  }

  [[nodiscard]] auto is_locked() const noexcept -> bool { return _is_locked; }
  [[nodiscard]] auto queue_length() const noexcept -> std::size_t {
    std::unique_lock guard(_mutex);
    return _queue.size();
  }

 private:
  std::atomic<bool> _is_locked = false;
  mutable std::mutex _mutex;
  std::deque<promise<lock_guard, Tag>> _queue;

  Executor _executor;
};

}  // namespace mellon

#endif  // FUTURES_LOCKS_H
