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

  void register_future(Fut&& f) {
    pending_futures.fetch_add(1, std::memory_order_release);
    std::move(f).finally([self = this->shared_from_this()](value_type&& v) noexcept {
      self->on_completion(std::move(v));
    });
  }

  auto await_all() noexcept -> value_type {
    std::unique_lock guard(await_mutex);
    std::unique_lock guard2(mutex);
    cv.wait(guard2, [&] { return !completed_values.empty(); });
    value_type v = std::move(completed_values.front());
    completed_values.pop();
    return v;
  }

  auto await() noexcept -> std::optional<value_type> {
    std::unique_lock guard(await_mutex);
    std::unique_lock guard2(mutex);
    while (true) {
      if (!completed_values.empty()) {
        value_type v = std::move(completed_values.front());
        completed_values.pop();
        return v;
      }

      if (pending_futures == 0) {
        return std::nullopt;
      }
      consumer_waiting = true;
      cv.wait(guard2);
    }
  }

 private:
  void on_completion(value_type&& v) noexcept {
    bool consumer_was_waiting = false;
    {
      std::unique_lock guard(mutex);
      completed_values.push(std::move(v));  // TODO this invocation is not noexcept
      std::swap(consumer_was_waiting, consumer_waiting);
      pending_futures.fetch_sub(1, std::memory_order_release);
    }

    if (consumer_was_waiting) {
      cv.notify_one();
    }
  }

  bool consumer_waiting = false;
  std::atomic<std::size_t> pending_futures = 0;
  std::queue<value_type> completed_values;
  std::mutex mutex;
  std::mutex await_mutex;
  std::condition_variable cv;
};

}  // namespace mellon

#endif  // FUTURES_COMPLETION_QUEUE_H
