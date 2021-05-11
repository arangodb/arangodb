#ifndef FUTURES_PROTOTYPE_H
#define FUTURES_PROTOTYPE_H

#include <condition_variable>
#include <mutex>

#include "../commons.h"
#include "../expected.h"

namespace mellon::detail {

template <typename T, typename F, typename Tag>
struct future_temporary_proxy {
  template <typename R>
  using instance = future_temporary<T, F, R, Tag>;
};
template <typename Tag>
struct future_proxy {
  template <typename R>
  using instance = future<R, Tag>;
};

/**
 * Base class unifies all common operations on future and future_temporaries.
 * Futures and Temporaries are derived from this class and only specialise
 * the functions `and_then` and `finally`.
 * @tparam T Value type
 * @tparam Fut Parent class template expecting one parameter.
 */
template <typename T, template <typename> typename Fut, typename Tag>
struct FUTURES_EMPTY_BASE future_prototype {
  static_assert(!std::is_void_v<T>);

  using value_type = T;

  /**
   * _Blocks_ the current thread until the init_future is fulfilled. This is
   * **not** something you should do unless you have a very good reason to do
   * so. The whole point of futures is to make code non-blocking.
   *
   * @return Returns the result value of type T.
   */
  T await() && {
    struct data {
      detail::box<T> box;
      bool is_waiting = false, has_value = false;
      std::mutex mutex;
      std::condition_variable cv;
    } data;
    // put everthing in that struct and only store a reference
    // to that => small lambda optimization, no allocation for finally <3
    move_self().finally([&data](T&& v) noexcept {
      bool was_waiting;
      {
        std::unique_lock guard(data.mutex);
        data.box.emplace(std::move(v));
        data.has_value = true;
        was_waiting = data.is_waiting;
      }
      if (was_waiting) {
        data.cv.notify_one();
      }
    });
    std::unique_lock guard(data.mutex);
    data.is_waiting = true;
    data.cv.wait(guard, [&] { return data.has_value; });
    T value(std::move(data.box).ref());
    data.box.destroy();
    return value;
  }

  /**
   * DO NOT USE THIS FUNCTION! Awaits the future for the given amount of time.
   * If the wait timed out a `std::nullopt` is returned, otherwise the value
   * is returned. After the call the future is empty, regardless whether the
   * value was there or not. If the promise is later fulfilled the
   * `abandoned_future` handler will be called.
   * @tparam Rep
   * @tparam Period
   * @param duration
   * @return
   */
  template <typename Rep, typename Period>
  std::optional<T> await_with_timeout(std::chrono::duration<Rep, Period> const& duration) && {
    struct await_context {
      detail::box<T> box;
      bool is_waiting = false, has_value = false, abandoned = false;
      std::mutex mutex;
      std::condition_variable cv;
    };

    auto ctx = std::make_shared<await_context>();
    move_self().finally([ctx](T&& v) noexcept {
      bool was_waiting;
      {
        std::unique_lock guard(ctx->mutex);
        ctx->box.emplace(std::move(v));
        ctx->has_value = true;
        was_waiting = ctx->is_waiting;
      }
      if (ctx->abandoned) {
        return detail::handler_helper<Tag, T>::abandon_future(std::move(v));
      }
      if (was_waiting) {
        ctx->cv.notify_one();
      }
    });
    std::unique_lock guard(ctx->mutex);
    ctx->is_waiting = true;
    ctx->cv.wait_for(guard, duration, [&] { return ctx->has_value; });
    if (!ctx->has_value) {
      ctx->abandoned = true;
      return std::nullopt;
    }
    T value(std::move(ctx->box).ref());
    ctx->box.destroy();
    return value;
  }

  /**
   * Calls `f` and captures its return value in an `expected<R>`.
   * @tparam F
   * @param f
   * @return
   */
  template <typename F, std::enable_if_t<std::is_invocable_v<F, T&&>, int> = 0,
            typename R = std::invoke_result_t<F, T&&>>
  auto and_capture(F&& f) && noexcept {
    return move_self().and_then([f = std::forward<F>(f)](T&& v) noexcept {
      return expect::captured_invoke(f, std::move(v));
    });
  }

  /**
   * Converts the content to `U`.
   * @tparam U
   * @return
   */
  template <typename U, std::enable_if_t<std::is_convertible_v<T, U>, int> = 0>
  auto as() && {
    static_assert(!expect::is_expected_v<T>);
    if constexpr (std::is_same_v<T, U>) {
      return move_self();
    } else {
      return move_self().and_then(
          [](T&& v) noexcept -> U { return std::move(v); });
    }
  }

  /**
   * Invokes @c{g} with the result value of this future (A). Then uses the value
   * of the returned future (B) to resolve a promise (C). Returns a future for (C).
   * @tparam G nothrow invocable with T, returns a future-like object.
   */
  template <typename G, std::enable_if_t<std::is_nothrow_invocable_v<G, T&&>, int> = 0,
            typename ReturnValue = std::invoke_result_t<G, T&&>,
            std::enable_if_t<is_future_like_v<ReturnValue>, int> = 0>
  auto bind(G&& g) {
    using future_tag = typename future_trait<ReturnValue>::tag_type;
    using future_value_type = typename future_trait<ReturnValue>::value_type;
    auto&& [f, p] = make_promise<future_value_type, future_tag>();

    move_self().finally([p = std::move(p), g = std::forward<G>(g)](T&& result) mutable noexcept {
      std::invoke(g, std::move(result)).fulfill(std::move(p));
    });

    return std::move(f);
  }

  /**
 * Invokes @c{g} with the result value of this future (A). Then uses the value
 * of the returned future (B) to resolve a promise (C). Returns a future for (C).
 * Instead of simply calling @c{g} this function captures its output in a expected.
 * @tparam G nothrow invocable with T, returns a future-like object.
 */
  template <typename G, std::enable_if_t<std::is_invocable_v<G, T&&>, int> = 0,
            typename ReturnValue = std::invoke_result_t<G, T&&>,
            std::enable_if_t<is_future_like_v<ReturnValue>, int> = 0>
  auto bind_capture(G&& g) {
    using future_tag = typename future_trait<ReturnValue>::tag_type;
    using future_value_type = typename future_trait<ReturnValue>::value_type;
    auto&& [f, p] = make_promise<expect::expected<future_value_type>, future_tag>();
    move_self().finally([p = std::move(p), g = std::forward<G>(g)](T&& result) mutable noexcept {
      expect::expected<ReturnValue> expected_future =
          expect::captured_invoke(g, std::move(result));
      if (expected_future.has_error()) {
        std::move(p).fulfill(expected_future.error());
      } else {
        std::move(expected_future).unwrap().finally([p = std::move(p)](future_value_type&& v) mutable noexcept {
          std::move(p).fulfill(std::in_place, std::move(v));
        });
      }
    });

    return std::move(f);
  }

  /**
   * Fulfills the promise using the value produced by the future.
   * @tparam Tag (deduced)
   * @param p promise
   */
  template <typename promise_tag>
  void fulfill(promise<T, promise_tag>&& p) && noexcept {
    move_self().finally([p = std::move(p)](T&& t) mutable noexcept {
      std::move(p).fulfill(std::move(t));
    });
  }

 private:
  auto move_self() noexcept -> Fut<T>&& { return static_cast<Fut<T>&&>(*this); }
};

template <typename T, template <typename> typename Fut, typename Tag>
struct FUTURES_EMPTY_BASE future_base : user_defined_additions_t<Tag, T, Fut>,
                                        future_type_based_extensions<T, Fut, Tag> {
  static_assert(std::is_base_of_v<future_prototype<T, Fut, Tag>, future_type_based_extensions<T, Fut, Tag>>);
};

template <typename Tag, typename T>
using internal_store =
    optional_box<T, tag_trait_helper<Tag>::template is_type_inlined<T>()>;
}  // namespace mellon::detail

#endif  // FUTURES_PROTOTYPE_H
