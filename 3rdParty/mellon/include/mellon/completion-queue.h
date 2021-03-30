#ifndef FUTURES_COMPLETION_QUEUE_H
#define FUTURES_COMPLETION_QUEUE_H
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include "futures.h"
#include "traits.h"

namespace mellon {

template <typename Fut>
struct completion_context : std::enable_shared_from_this<completion_context<Fut>> {
  using trait = future_trait<Fut>;
  using value_type = typename trait::value_type;

  template <typename T, std::enable_if_t<is_future_like_v<T>, int> = 0>
  void register_future(T&& f) {
    static_assert(std::is_same_v<value_type, typename T::value_type>);
    pending_futures.fetch_add(1, std::memory_order_relaxed);
    std::move(f).finally([self = this->shared_from_this()](value_type&& v) noexcept {
      self->on_completion(std::move(v));
    });
  }

  auto await_all() noexcept -> value_type {
    std::unique_lock guard(mutex);
    cv.wait(guard, [&] { return !completed_values.empty(); });
    value_type v = std::move(completed_values.front());
    completed_values.pop();
    return v;
  }

  auto await() noexcept -> std::optional<value_type> {
    std::unique_lock guard(mutex);
    while (true) {
      if (!completed_values.empty()) {
        value_type v = std::move(completed_values.front());
        completed_values.pop();
        return v;
      }

      if (pending_futures.load(std::memory_order_relaxed) == 0) {
        return std::nullopt;
      }
      consumer_waiting = true;
      cv.wait(guard);
    }
  }

 private:
  void on_completion(value_type&& v) noexcept {
    bool consumer_was_waiting = false;
    {
      std::unique_lock guard(mutex);
      completed_values.push(std::move(v));  // TODO this invocation is not noexcept
      std::swap(consumer_was_waiting, consumer_waiting);
      pending_futures.fetch_sub(1, std::memory_order_relaxed);
    }

    if (consumer_was_waiting) {
      cv.notify_one();
    }
  }

  bool consumer_waiting = false;
  std::atomic<std::size_t> pending_futures = 0;
  std::queue<value_type> completed_values;
  std::mutex mutex;
  std::condition_variable cv;
};

}  // namespace mellon

#endif  // FUTURES_COMPLETION_QUEUE_H
