#ifndef FUTURES_FUTURES_H
#define FUTURES_FUTURES_H
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

#include "detail/box.h"
#include "detail/invalid-pointer-flags.h"
#include "detail/gadgets.h"

#include "expected.h"
#include "traits.h"


#ifdef FUTURES_COUNT_ALLOC
#include <cmath>
#endif

namespace mellon {

struct promise_abandoned_error : std::exception {
  [[nodiscard]] const char* what() const noexcept override;
};

struct default_tag {};

template <>
struct tag_trait<default_tag> {
  struct allocator {
    // TODO is this how you do such things?
    template <typename T>
    static T* allocate() {
      return reinterpret_cast<T*>(::operator new(sizeof(T)));
    }
    template <typename T>
    static T* allocate(std::nothrow_t) noexcept {
      return reinterpret_cast<T*>(::operator new(sizeof(T), std::nothrow));
    }
  };

  struct assertion_handler {
    void operator()(bool test, const char*) const noexcept {
      if (!test) {
        std::abort();
      }
    }
  };

  template <typename T>
  struct abandoned_future_handler {
    void operator()(T&&) noexcept {}  // this is like ignoring a return value
  };

  template <typename T>
  struct abandoned_future_handler<expect::expected<T>> {
    void operator()(expect::expected<T>&& e) noexcept {
      if (e.has_error()) {
        e.rethrow_error();  // this is like an uncaught exception
      }
    }
  };

  template <typename T>
  struct abandoned_promise_handler {
    T operator()() noexcept { std::abort(); }  // in general we can not continue
  };

  template <typename T>
  struct abandoned_promise_handler<expect::expected<T>> {
    expect::expected<T> operator()() noexcept {
      // at least in this case we can throw a nice exception :)
      return std::make_exception_ptr(promise_abandoned_error{});
    }
  };

  static constexpr auto small_value_size = 64;
  static constexpr auto finally_prealloc_size = 32;
};

template <typename T, typename Tag>
struct future;
template <typename T, typename Tag>
struct promise;

template <typename T>
struct is_future : std::false_type {};
template <typename T, typename Tag>
struct is_future<future<T, Tag>> : std::true_type {};
template <typename T>
inline constexpr auto is_future_v = is_future<T>::value;

template <typename T>
struct is_future_temporary : std::false_type {};
template <typename T, typename F, typename R, typename Tag>
struct is_future_temporary<future_temporary<T, F, R, Tag>> : std::true_type {};
template <typename T>
inline constexpr auto is_future_temporary_v = is_future_temporary<T>::value;


template <typename T>
inline constexpr auto is_future_like_v = is_future_v<T> || is_future_temporary_v<T>;

/**
 * Create a new pair of init_future and promise with value type `T`.
 * @tparam T value type
 * @return pair of init_future and promise.
 * @throws std::bad_alloc
 */
template <typename T, typename Tag = default_tag>
auto make_promise() -> std::pair<future<T, Tag>, promise<T, Tag>>;

namespace detail {
template <typename Tag, typename T>
struct handler_helper {
  static T abandon_promise() noexcept {
    return detail::tag_trait_helper<Tag>::template abandon_promise<T>();
  }

  template <typename U>
  static void abandon_future(U&& u) noexcept {
    detail::tag_trait_helper<Tag>::template abandon_future<T>(std::forward<U>(u));
  }
};

}


namespace detail {
template <typename T>
struct continuation;
template <typename Tag, typename T, std::size_t = tag_trait_helper<Tag>::finally_prealloc_size()>
struct continuation_base;
template <typename Tag, typename T>
struct continuation_start;
template <typename Tag, typename T, typename F, typename R>
struct continuation_step;
template <typename T, typename F, typename>
struct continuation_final;

template <typename Tag, typename T, typename... Args,
          std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
void fulfill_continuation(continuation_base<Tag, T>* base,
                          Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");
  base->emplace(std::forward<Args>(args)...);  // this can throw an exception

  // the remainder should be noexcept
  static_assert(std::is_nothrow_destructible_v<T>,
                "type should be nothrow destructible.");
  std::invoke([&]() noexcept {
    continuation<T>* expected = nullptr;
    if (!base->_next.compare_exchange_strong(expected, FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T),
                                             std::memory_order_release,
                                             std::memory_order_acquire)) {
      if (expected != FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T)) {
        std::invoke(*expected, base->cast_move());
        // FIXME this is a recursive call. Build this a loop!
      } else {
        detail::tag_trait_helper<Tag>::assert_true(
            expected == FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T),
            "invalid continuation state");
        detail::handler_helper<Tag, T>::abandon_future(base->cast_move());
        static_assert(std::is_nothrow_destructible_v<T>);
      }

      base->destroy();
      delete base;
    }
  });
}

template <typename Tag, typename T>
void abandon_continuation(continuation_base<Tag, T>* base) noexcept {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");
  continuation<T>* expected = nullptr;
  if (!base->_next.compare_exchange_strong(expected, FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T),
                                           std::memory_order_release,
                                           std::memory_order_acquire)) {  // ask mpoeter
    if (expected == FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {
      detail::handler_helper<Tag, T>::abandon_future(base->cast_move());
      static_assert(std::is_nothrow_destructible_v<T>);
      base->destroy();
    } else {
      detail::tag_trait_helper<Tag>::assert_true(expected == FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T),
                                                 "invalid continuation state");
    }

    delete base;
  }
}

template <typename Tag, typename T>
void abandon_promise(continuation_start<Tag, T>* base) noexcept {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");
  continuation<T>* expected = nullptr;
  if (!base->_next.compare_exchange_strong(expected, FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T),
                                           std::memory_order_release,
                                           std::memory_order_acquire)) {  // ask mpoeter
    if (expected == FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T)) {
      delete base;  // we all agreed on not having this promise-init_future-chain
    } else {
      return fulfill_continuation<Tag>(base, detail::handler_helper<Tag, T>::abandon_promise());
    }
  }
}

template <typename Tag, typename T, typename... Args>
auto allocate_frame_noexcept(Args&&... args) noexcept -> T* {
  static_assert(std::is_nothrow_constructible_v<T, Args...>,
                "type should be nothrow constructable");
  auto frame = detail::tag_trait_helper<Tag>::template allocate<T>(std::nothrow);
  new (frame) T(std::forward<Args>(args)...);
  detail::tag_trait_helper<Tag>::assert_true(frame != nullptr, "critical allocation failed");
  return frame;
}

template <typename Tag, typename T, typename F, typename R, typename G>
auto insert_continuation_step(continuation_base<Tag, T>* base, G&& f) noexcept
    -> future<R, Tag> {
  static_assert(std::is_nothrow_invocable_r_v<R, F, T&&>);
  static_assert(std::is_nothrow_destructible_v<T>);

  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");

  if (base->_next.load(std::memory_order_acquire) ==
      FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {
    // short path
    static_assert(std::is_nothrow_move_constructible_v<R>);
    auto fut = future<R, Tag>{std::in_place,
                              std::invoke(std::forward<G>(f), base->cast_move())};
    base->destroy();
    delete base;
    return fut;
  }

#ifdef FUTURES_COUNT_ALLOC
  ::mellon::detail::number_of_step_usage.fetch_add(1, std::memory_order_relaxed);
#endif

  auto step = detail::allocate_frame_noexcept<Tag, continuation_step<Tag, T, F, R>>(
      std::in_place, std::forward<G>(f));
  continuation<T>* expected = nullptr;
  if (!base->_next.compare_exchange_strong(expected, step, std::memory_order_release,
                                           std::memory_order_acquire)) {  // ask mpoeter
    detail::tag_trait_helper<Tag>::assert_true(expected == FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T),
                                               "invalid continuation state");
    if constexpr (future<R, Tag>::is_value_inlined) {
      auto fut = future<R, Tag>{std::in_place, std::invoke(step->function_self(),
                                                           base->cast_move())};
      base->destroy();
      delete base;
      delete step;
      return fut;
    } else {
      step->emplace(std::invoke(step->function_self(), base->cast_move()));
      base->destroy();
      delete base;
    }
  }

  return future<R, Tag>{step};
}

template <typename T>
struct continuation {
  virtual ~continuation() = default;
  virtual void operator()(T&&) noexcept = 0;
};

template <typename T, typename F, typename Deleter>
struct continuation_final final : continuation<T>, function_store<F> {
  static_assert(std::is_nothrow_invocable_r_v<void, F, T>);
  template <typename G = F>
  explicit continuation_final(std::in_place_t, G&& f) noexcept(
      std::is_nothrow_constructible_v<function_store<F>, std::in_place_t, G>)
      : function_store<F>(std::in_place, std::forward<G>(f)) {}
  void operator()(T&& t) noexcept override {
    std::invoke(function_store<F>::function_self(), std::move(t));
    Deleter{}(this);
  }
};


template <typename Tag, typename T, std::size_t prealloc_size>
struct continuation_base : memory_buffer<prealloc_size>, box<T> {
  template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  explicit continuation_base(std::in_place_t, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<box<T>, std::in_place_t, Args...>)
      : box<T>(std::in_place, std::forward<Args>(args)...),
        _next(FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {}

  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  explicit continuation_base(std::in_place_t) noexcept
      : box<T>(std::in_place), _next(FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {}

  continuation_base() noexcept : box<T>() {}
  virtual ~continuation_base() = default;

  std::atomic<continuation<T>*> _next = nullptr;
};

template <typename Tag, typename T>
struct continuation_start final : continuation_base<Tag, T> {};

template <typename Tag, typename T, typename F, typename R>
struct continuation_step final : continuation_base<Tag, R>,
                                 function_store<F>,
                                 continuation<T> {
  static_assert(std::is_nothrow_invocable_r_v<R, F, T&&>);

  template <typename G = F>
  explicit continuation_step(std::in_place_t, G&& f) noexcept(
      std::is_nothrow_constructible_v<function_store<F>, std::in_place_t, G>)
      : function_store<F>(std::in_place, std::forward<G>(f)) {}

  void operator()(T&& t) noexcept override {
    detail::fulfill_continuation<Tag>(this, std::invoke(function_store<F>::function_self(),
                                                        std::move(t)));
  }
};

template <typename Tag, typename T, typename F>
void insert_continuation_final(continuation_base<Tag, T>* base, F&& f) noexcept {
  static_assert(std::is_nothrow_invocable_r_v<void, F, T&&>);
  static_assert(std::is_nothrow_constructible_v<F, F>);
  static_assert(std::is_nothrow_destructible_v<T>);
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");

  if (base->_next.load(std::memory_order_acquire) ==
      FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {
    // short path
    std::invoke(std::forward<F>(f), base->cast_move());
    base->destroy();
    delete base;
    return;
  }

#ifdef FUTURES_COUNT_ALLOC
  std::size_t lambda_size = sizeof(continuation_final<T, F, deleter_destroy>);
  double lambda_log2 = std::max(0.0, std::log2(lambda_size) - 2.0);
  std::size_t bucket =
      static_cast<std::size_t>(std::max(0., std::ceil(lambda_log2) - 1));

  ::mellon::detail::number_of_final_usage.fetch_add(1, std::memory_order_relaxed);
  ::mellon::detail::histogram_final_lambda_sizes[bucket].fetch_add(1, std::memory_order_relaxed);
#endif

  // try to emplace the final into the steps local memory
  continuation<T>* cont;
  auto* mem = base->template try_allocate<continuation_final<T, F, deleter_destroy>>();
  if (mem != nullptr) {
    new (mem)
        continuation_final<T, F, deleter_destroy>(std::in_place, std::forward<F>(f));
    cont = mem;
#ifdef FUTURES_COUNT_ALLOC
    ::mellon::detail::number_of_prealloc_usage.fetch_add(1, std::memory_order_relaxed);
#endif
  } else {
    cont = detail::allocate_frame_noexcept<Tag, continuation_final<T, F, deleter_dealloc>>(
        std::in_place, std::forward<F>(f));
  }

  detail::tag_trait_helper<Tag>::assert_true(
      cont != nullptr, "finally allocation failed unexpectedly");
  continuation<T>* expected = nullptr;
  if (!base->_next.compare_exchange_strong(expected, cont, std::memory_order_release,
                                           std::memory_order_acquire)) {  // ask mpoeter
    if (expected == FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T)) {
      cont->operator()(detail::handler_helper<Tag, T>::abandon_promise());
    } else {
      detail::tag_trait_helper<Tag>::assert_true(expected == FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T),
          "invalid continuation state");
      cont->operator()(base->cast_move());  // TODO we can get rid of this virtual call
      base->destroy();
    }

    delete base;
  }
}
}  // namespace detail

template <typename T, typename Tag>
struct promise_type_based_extension {};

/**
 * Producing end of init_future-chain. When the promise is `fulfilled` the chain
 * of mellon is evaluated.
 * @tparam T
 */
template <typename T, typename Tag>
struct promise : promise_type_based_extension<T, Tag>,
                 user_defined_promise_additions_t<Tag, T> {
  /**
   * Destroys the promise. If the promise has not been fulfilled or moved away
   * it will be abandoned.
   */
  ~promise() {
    if (_base) {
      std::move(*this).abandon();
    }
  }

  promise(promise const&) = delete;
  promise& operator=(promise const&) = delete;
  promise(promise&& o) noexcept = default;
  promise& operator=(promise&& o) noexcept {
    if (_base) {
      std::move(*this).abandon();
    }
    std::swap(_base, o._base);
    return *this;
  }

  /**
   * In place constructs the result using the given parameters.
   * @tparam Args
   * @param args
   */
  template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  void fulfill(Args&&... args) && noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    detail::fulfill_continuation<Tag>(_base.get(), std::forward<Args>(args)...);
    _base.reset();  // we can not use _base.release() because the constructor of T could
    // throw an exception. In that case the promise has to stay active.
  }

  /**
   * Abandons the promise. The `abandoned_promise_handler` will be called.
   * This either generates a default value for `T` or calls `std::abort()`.
   */
  void abandon() && { detail::abandon_promise<Tag>(_base.release()); }

  template <typename Tuple, typename tuple_type = std::remove_reference_t<Tuple>>
  void fulfill_from_tuple(Tuple&& t) && noexcept(
      detail::unpack_tuple_into_v<std::is_nothrow_constructible, tuple_type, T>) {
    return std::move(*this).fulfill_from_tuple_impl(
        std::forward<Tuple>(t), std::make_index_sequence<std::tuple_size_v<tuple_type>>{});
  }

  [[nodiscard]] bool empty() const noexcept { return _base == nullptr; }

  void swap(promise<T, Tag>& o) { std::swap(_base, o._base); }

 private:
  template <typename Tuple, std::size_t... Is, typename tuple_type = std::remove_reference_t<Tuple>>
  void fulfill_from_tuple_impl(Tuple&& t, std::index_sequence<Is...>) && noexcept(
      std::is_nothrow_constructible_v<T, std::tuple_element_t<Is, tuple_type>...>) {
    static_assert(std::is_constructible_v<T, std::tuple_element_t<Is, tuple_type>...>);
    std::move(*this).fulfill(std::get<Is>(std::forward<Tuple>(t))...);
  }

  template <typename S, typename STag>
  friend auto make_promise() -> std::pair<future<S, STag>, promise<S, STag>>;
  explicit promise(detail::continuation_start<Tag, T>* base) : _base(base) {}
  detail::unique_but_not_deleting_pointer<detail::continuation_start<Tag, T>> _base = nullptr;
};

template <typename T, typename Tag>
void swap(promise<T, Tag>& a, promise<T, Tag>& b) {
  a.swap(b);
}

template <typename T, typename F, typename R, typename Tag>
struct future_temporary;

namespace detail {
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
}  // namespace detail

struct yes_i_know_that_this_call_will_block_t {};
inline constexpr yes_i_know_that_this_call_will_block_t yes_i_know_that_this_call_will_block;

template <typename T, template <typename> typename F, typename Tag>
struct future_type_based_extensions;
namespace detail {

template <typename Tag, typename T, template <typename> typename F, typename Ts, typename = void>
struct has_constructor : std::false_type {};
template <typename Tag, typename T, template <typename> typename F, typename... Ts>
struct has_constructor<Tag, T, F, std::tuple<Ts...>,
                       std::void_t<decltype(future_type_based_extensions<T, F, Tag>::construct(std::declval<Ts>()...))>>
    : std::true_type {};
template <typename Tag, typename T, template <typename> typename F, typename... Ts>
inline constexpr auto has_constructor_v =
    has_constructor<Tag, T, F, std::tuple<Ts...>>::value;

/**
 * Base class unifies all common operations on future and future_temporaries.
 * Futures and Temporaries are derived from this class and only specialise
 * the functions `and_then` and `finally`.
 * @tparam T Value type
 * @tparam Fut Parent class template expecting one parameter.
 */
template <typename T, template <typename> typename Fut, typename Tag>
struct future_prototype {
  static_assert(!std::is_void_v<T>);

  using value_type = T;

  /**
   * _Blocks_ the current thread until the init_future is fulfilled. This
   * **not** something you should do unless you have a very good reason to do
   * so. The whole point of futures is to make code non-blocking.
   *
   * @return Returns the result value of type T.
   */
  T await(yes_i_know_that_this_call_will_block_t) && {
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
        data.box.template emplace(std::move(v));
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
      bool was_waiting, was_abandoned;
      {
        std::unique_lock guard(ctx->mutex);
        ctx->box.template emplace(std::move(v));
        ctx->has_value = true;
        was_waiting = ctx->is_waiting;
        was_abandoned = ctx->abandoned;
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

  template <typename G, std::enable_if_t<std::is_nothrow_invocable_v<G, T&&>, int> = 0,
            typename ReturnValue = std::invoke_result_t<G, T&&>,
            std::enable_if_t<is_future_like_v<ReturnValue>, int> = 0>
  auto bind(G&& g) {
    using future_tag = typename future_trait<ReturnValue>::tag_type;
    using value_type = typename future_trait<ReturnValue>::value_type;
    auto&& [f, p] = make_promise<value_type, future_tag>();

    move_self().finally([p = std::move(p), g = std::forward<G>(g)](T&& result) mutable noexcept {
      std::invoke(g, std::move(result)).fulfill(std::move(p));
    });

    return std::move(f);
  }

  template <typename G, std::enable_if_t<std::is_invocable_v<G, T&&>, int> = 0,
            typename ReturnValue = std::invoke_result_t<G, T&&>,
            std::enable_if_t<is_future_like_v<ReturnValue>, int> = 0>
  auto bind_capture(G&& g) {
    using future_tag = typename future_trait<ReturnValue>::tag_type;
    using value_type = typename future_trait<ReturnValue>::value_type;
    auto&& [f, p] = make_promise<expect::expected<value_type>, future_tag>();
    move_self().finally([p = std::move(p), g = std::forward<G>(g)](T&& result) mutable noexcept {
      expect::expected<ReturnValue> expected_future =
          expect::captured_invoke(g, std::move(result));
      if (expected_future.has_error()) {
        std::move(p).fulfill(expected_future.error());
      } else {
        std::move(expected_future).unwrap().finally([p = std::move(p)](value_type&& v) mutable noexcept {
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
struct future_base : user_defined_additions_t<Tag, T, Fut>,
                     future_type_based_extensions<T, Fut, Tag> {
  static_assert(std::is_base_of_v<future_prototype<T, Fut, Tag>, future_type_based_extensions<T, Fut, Tag>>);
};

template <typename Tag, typename T>
using internal_store =
    optional_box<T, tag_trait_helper<Tag>::template is_type_inlined<T>()>;
}  // namespace detail

template <typename T, template <typename> typename F, typename Tag>
struct future_type_based_extensions : detail::future_prototype<T, F, Tag> {};

/**
 * A temporary object that is used to chain together multiple `and_then` calls
 * into a single step to reduce memory and allocation overhead. Is derived from
 * future_base and thus provides all functions that mellon do.
 *
 * The temporary is implicitly convertible to `init_future<T>`.
 * @tparam T Initial value type.
 * @tparam F Temporary function type. Chain of functions that have been applied.
 * @tparam R Result type of the chain.
 */
template <typename T, typename F, typename R, typename Tag>
struct future_temporary
    : detail::future_base<R, detail::future_temporary_proxy<T, F, Tag>::template instance, Tag>,
      private detail::function_store<F>,
      private detail::internal_store<Tag, T> {
  static constexpr auto is_value_inlined = detail::internal_store<Tag, T>::stores_value;

  future_temporary(future_temporary const&) = delete;
  future_temporary& operator=(future_temporary const&) = delete;
  future_temporary(future_temporary&& o) noexcept
      : detail::function_store<F>(std::move(o)), _base(nullptr) {
    if constexpr (is_value_inlined) {
      if (o.holds_inline_value()) {
        detail::internal_store<Tag, T>::emplace(o.cast_move());
        o.destroy();
      }
    }
    std::swap(_base, o._base);
  }
  future_temporary& operator=(future_temporary&& o) noexcept {
    if (_base) {
      std::move(*this).abandon();
    }
    detail::tag_trait_helper<Tag>::assert_true(_base == nullptr,
                                               "invalid future state");
    detail::function_store<F>::operator=(std::move(o));
    if constexpr (is_value_inlined) {
      if (o.holds_inline_value()) {
        detail::internal_store<Tag, T>::emplace(o.cast_move());
        o.destroy();
      }
    }
    std::swap(_base, o._base);
  }

  ~future_temporary() {
    if (_base) {
      std::move(*this).abandon();
    }
  }

  template <typename G, std::enable_if_t<std::is_nothrow_invocable_v<G, R&&>, int> = 0,
            typename S = std::invoke_result_t<G, R&&>>
  auto and_then(G&& f) && noexcept {
#ifdef FUTURES_COUNT_ALLOC
    ::mellon::detail::number_of_temporary_objects.fetch_add(1, std::memory_order_relaxed);
#endif
    auto&& composition =
        detail::compose(std::forward<G>(f),
                        std::move(detail::function_store<F>::function_self()));

    if constexpr (is_value_inlined) {
      if (holds_inline_value()) {
        auto fut = future_temporary<T, decltype(composition), S, Tag>(
            std::in_place, std::move(composition),
            detail::internal_store<Tag, T>::cast_move());
        cleanup_local_state();
        return fut;
      }
    }

    return future_temporary<T, decltype(composition), S, Tag>(std::move(composition),
                                                              std::move(_base));
  }

  template <typename G, std::enable_if_t<std::is_nothrow_invocable_r_v<void, G, R&&>, int> = 0>
  void finally(G&& f) && noexcept {
    static_assert(
        std::is_nothrow_constructible_v<G, G>,
        "the lambda object must be nothrow constructible from itself. If it's "
        "passed as lvalue reference it has to be nothrow copyable.");
#ifdef FUTURES_COUNT_ALLOC
    ::mellon::detail::number_of_temporary_objects.fetch_add(1, std::memory_order_relaxed);
#endif
    auto&& composition =
        detail::compose(std::forward<G>(f),
                        std::move(detail::function_store<F>::function_self()));

    if constexpr (is_value_inlined) {
      if (holds_inline_value()) {
#ifdef FUTURES_COUNT_ALLOC
        ::mellon::detail::number_of_and_then_on_inline_future.fetch_add(1, std::memory_order_relaxed);
#endif
        std::invoke(composition, detail::internal_store<Tag, T>::cast_move());
        cleanup_local_state();
        return;
      }
    }

    return detail::insert_continuation_final<Tag, T, decltype(composition)>(
        _base.release(), std::move(composition));
  }

  void abandon() && noexcept { std::move(*this).finalize().abandon(); }

  /* implicit */ operator future<R, Tag>() && noexcept {
    return std::move(*this).finalize();
  }

  auto finalize() && noexcept -> future<R, Tag> {
    if constexpr (is_value_inlined) {
      if (holds_inline_value()) {
#ifdef FUTURES_COUNT_ALLOC
        ::mellon::detail::number_of_and_then_on_inline_future.fetch_add(1, std::memory_order_relaxed);
#endif
        static_assert(std::is_nothrow_move_constructible_v<R>);
        auto f =
            future<R, Tag>(std::in_place,
                           std::invoke(detail::function_store<F>::function_self(),
                                       detail::internal_store<Tag, T>::cast_move()));
        static_assert(std::is_nothrow_destructible_v<T>);
        cleanup_local_state();
        return f;
      }
    }

    return detail::insert_continuation_step<Tag, T, F, R>(
        _base.release(), std::move(detail::function_store<F>::function_self()));
  }

  [[nodiscard]] bool holds_inline_value() const noexcept {
    return is_value_inlined &&
           _base.get() == FUTURES_INVALID_POINTER_INLINE_VALUE(Tag, T);
  }
  [[nodiscard]] bool empty() const noexcept { return _base == nullptr; }

 private:
  void cleanup_local_state() {
    detail::internal_store<Tag, T>::destroy();
    _base.reset();
  }
  template <typename G = F>
  future_temporary(G&& f,
                   detail::unique_but_not_deleting_pointer<detail::continuation_base<Tag, T>> base)
      : detail::function_store<F>(std::in_place, std::forward<G>(f)),
        _base(std::move(base)) {}
  template <typename G = F, typename S = T>
  future_temporary(std::in_place_t, G&& f, S&& s)
      : detail::function_store<F>(std::in_place, std::forward<G>(f)),
        detail::internal_store<Tag, T>(std::in_place, std::forward<S>(s)),
        _base(FUTURES_INVALID_POINTER_INLINE_VALUE(Tag, T)) {}

  template <typename, typename>
  friend struct future;
  template <typename, typename, typename, typename>
  friend struct future_temporary;
  detail::unique_but_not_deleting_pointer<detail::continuation_base<Tag, T>> _base;
};

/**
 * Consuming end of a future chain. You can add more elements to the chain
 * using this interface.
 * @tparam T value type
 */
template <typename T, typename Tag>
struct future
    : detail::future_base<T, detail::future_proxy<Tag>::template instance, Tag>,
      private detail::internal_store<Tag, T> {
  /**
   * Is true if the init_future can store an inline value.
   */
  static constexpr auto is_value_inlined = detail::internal_store<Tag, T>::stores_value;

  static_assert(!std::is_void_v<T>,
                "void is not supported, use std::monostate instead");
  static_assert(
      !is_future_v<T>,
      "init_future<init_future<T>> is a bad idea and thus not supported");
  static_assert(std::is_nothrow_move_constructible_v<T>);
  static_assert(std::is_nothrow_destructible_v<T>);

  future(future const&) = delete;
  future& operator=(future const&) = delete;
  future(future&& o) noexcept(!is_value_inlined || std::is_nothrow_move_constructible_v<T>)
      : _base(nullptr) {
    if constexpr (is_value_inlined) {
      if (o.holds_inline_value()) {
        detail::internal_store<Tag, T>::emplace(o.cast_move());
        o.destroy();
      }
    }
    std::swap(_base, o._base);
  }
  future& operator=(future&& o) noexcept(!is_value_inlined ||
                                         std::is_nothrow_move_constructible_v<T>) {
    if (_base) {
      std::move(*this).abandon();
    }
    detail::tag_trait_helper<Tag>::assert_true(_base == nullptr,
                                               "invalid future state");
    if constexpr (is_value_inlined) {
      if (o.holds_inline_value()) {
        detail::internal_store<Tag, T>::emplace(o.cast_move());
        o.destroy();  // o will have _base == nullptr
      }
    }
    std::swap(_base, o._base);
    return *this;
  }

  template <typename U, std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
  future(future<U, Tag>&& o) noexcept : future(std::move(o).template as<T>()) {}
  template <typename U, typename F, typename S, std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
  future(future_temporary<S, F, U, Tag>&& o) noexcept
      : future(std::move(o).template as<U>().finalize()) {}

  /**
   * If the init_future was not used or moved away, the init_future is
   * abandoned. For more, see `abandon`.
   */
  ~future() {
    if (_base) {
      std::move(*this).abandon();
    }
  }

  /**
   * Constructs a fulfilled init_future in place. The arguments are passed to
   * the constructor of `T`. If the value can be inlined it is constructed
   * inline, otherwise memory is allocated.
   *
   * This constructor is noexcept if the used constructor of `T`. If the value
   * is not small, `std::bad_alloc` can occur.
   * @tparam Args
   * @param args
   */
  template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  explicit future(std::in_place_t, Args&&... args) noexcept(
      std::conjunction_v<std::is_nothrow_constructible<T, Args...>, std::bool_constant<is_value_inlined>>) {
#ifdef FUTURES_COUNT_ALLOC
    std::size_t lambda_size = sizeof(T);
    double lambda_log2 = std::max(0.0, std::log2(lambda_size) - 2.0);
    std::size_t bucket =
        static_cast<std::size_t>(std::max(0., std::ceil(lambda_log2) - 1));
    ::mellon::detail::histogram_value_sizes[bucket].fetch_add(1, std::memory_order_relaxed);
#endif
    if constexpr (is_value_inlined) {
#ifdef FUTURES_COUNT_ALLOC
      ::mellon::detail::number_of_inline_value_placements.fetch_add(1, std::memory_order_relaxed);
#endif
      detail::internal_store<Tag, T>::emplace(std::forward<Args>(args)...);
      _base.reset(FUTURES_INVALID_POINTER_INLINE_VALUE(Tag, T));
    } else {
#ifdef FUTURES_COUNT_ALLOC
      ::mellon::detail::number_of_inline_value_allocs.fetch_add(1, std::memory_order_relaxed);
#endif
      // TODO no preallocated memory needed
      _base.reset(detail::tag_trait_helper<Tag>::template allocate_construct<detail::continuation_base<Tag, T>>(
          std::in_place, std::forward<Args>(args)...));
    }
  }

  explicit future(T t) : future(std::in_place, std::move(t)) {}

  template <typename... Ts, typename U = T, std::enable_if_t<std::is_constructible_v<U, Ts...>, int> = 0,
            std::enable_if_t<detail::has_constructor_v<Tag, T, detail::future_proxy<Tag>::template instance, Ts...>, int> = 0>
  explicit future(Ts&&... ts)
      : future(future_type_based_extensions<T, detail::future_proxy<Tag>::template instance, Tag>::construct(
            std::forward<Ts>(ts)...)) {}

  /**
   * (fmap) Enqueues a callback to the future chain and returns a new future that awaits
   * the return value of the provided callback. It is undefined in which thread
   * the callback is executed. `F` must be nothrow invocable with `T&&` as parameter.
   *
   * This function returns a temporary object which is implicitly convertible to future<R>.
   *
   * @tparam F
   * @param f
   * @return A new init_future with `value_type` equal to the result type of `F`.
   */
  template <typename F, std::enable_if_t<std::is_nothrow_invocable_v<F, T&&>, int> = 0,
            typename R = std::invoke_result_t<F, T&&>>
  auto and_then(F&& f) && noexcept {
    if constexpr (detail::tag_trait_helper<Tag>::is_disable_temporaries()) {
      return std::move(*this).and_then_direct(std::forward<F>(f));
    } else {
      if constexpr (is_value_inlined) {
        if (holds_inline_value()) {
          auto fut =
              future_temporary<T, F, R, Tag>(std::in_place, std::forward<F>(f),
                                             detail::internal_store<Tag, T>::cast_move());
          cleanup_local_state();
          return fut;
        }
      }
      return future_temporary<T, F, R, Tag>(std::forward<F>(f), std::move(_base));
    }
  }

  /**
   * (fmap) Enqueues a callback to the init_future chain and returns a new init_future that awaits
   * the return value of the provided callback. It is undefined in which thread
   * the callback is executed. `F` must be nothrow invocable with `T&&` as parameter.
   *
   * Unlike `and_then` this function does not return a temporary.
   *
   * @tparam F callback type
   * @param f callback
   * @return A new init_future with `value_type` equal to the result type of `F`.
   */
  template <typename F, std::enable_if_t<std::is_nothrow_invocable_v<F, T&&>, int> = 0,
            typename R = std::invoke_result_t<F, T&&>>
  auto and_then_direct(F&& f) && noexcept -> future<R, Tag> {
    if constexpr (is_value_inlined) {
      if (holds_inline_value()) {
#ifdef FUTURES_COUNT_ALLOC
        ::mellon::detail::number_of_and_then_on_inline_future.fetch_add(1, std::memory_order_relaxed);
#endif
        auto fut = future<R, Tag>{std::in_place,
                                  std::invoke(std::forward<F>(f), this->cast_move())};
        cleanup_local_state();
        return std::move(fut);
      }
    }
    return detail::insert_continuation_step<Tag, T, F, R>(_base.release(),
                                                          std::forward<F>(f));
  }

  /**
   * Enqueues a final callback and ends the future chain.
   *
   * @tparam F callback type
   * @param f callback
   */
  template <typename F, std::enable_if_t<std::is_nothrow_invocable_r_v<void, F, T&&>, int> = 0>
  void finally(F&& f) && noexcept {
    static_assert(std::is_nothrow_constructible_v<F, F>,
                  "the lambda object must be nothrow constructible from "
                  "itself. You should pass it as rvalue reference and it "
                  "should be nothrow move constructible. If it's passed as "
                  "lvalue reference it has to be nothrow copyable.");

    if constexpr (is_value_inlined) {
      if (holds_inline_value()) {
#ifdef FUTURES_COUNT_ALLOC
        ::mellon::detail::number_of_and_then_on_inline_future.fetch_add(1, std::memory_order_relaxed);
#endif
        std::invoke(std::forward<F>(f), detail::internal_store<Tag, T>::cast_move());
        cleanup_local_state();
        return;
      }
    }

    return detail::insert_continuation_final<Tag, T, F>(_base.release(),
                                                        std::forward<F>(f));
  }

  auto&& finalize() { return std::move(*this); }

  /**
   * Returns true if the init_future holds a value locally.
   * @return true if a local value is present.
   */
  [[nodiscard]] bool holds_inline_value() const noexcept {
    return is_value_inlined &&
           _base.get() == FUTURES_INVALID_POINTER_INLINE_VALUE(Tag, T);
  }
  // TODO move _base pointer to future_prototype.
  [[nodiscard]] bool empty() const noexcept { return _base == nullptr; }

  /**
   * Abandons a future chain. If the promise is abandoned as well, nothing happens.
   * If, however, the promise is fulfilled it depends on the `abandoned_future_handler`
   * for that type what will happen next. In most cases this is just cleanup.
   *
   * Some types, e.g. `expected<S>` will call terminated if it contains an
   * unhandled exception.
   */
  void abandon() && noexcept {
    if constexpr (is_value_inlined) {
      if (holds_inline_value()) {
        cleanup_local_state();
        return;
      }
    }

    detail::abandon_continuation<Tag>(_base.release());
  }

  void swap(future<T, Tag>& o) noexcept(!is_value_inlined ||
                                        (std::is_nothrow_swappable_v<T> &&
                                         std::is_nothrow_move_constructible_v<T>)) {
    if constexpr (is_value_inlined) {
      if (holds_inline_value() && o.holds_inline_value()) {
        using std::swap;
        swap(this->ref(), o.ref());
      } else if (holds_inline_value()) {
        o.emplace(this->cast_move());
        this->destroy();
      } else if (o.holds_inline_value()) {
        this->emplace(o.cast_move());
        o.destroy();
      }
    }
    std::swap(_base, o._base);
  }

  friend void swap(future<T, Tag>& a, future<T, Tag>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
  }

  explicit future(detail::continuation_base<Tag, T>* ptr) noexcept
      : _base(ptr) {}

 private:
  template <typename, typename>
  friend struct future;

  void cleanup_local_state() noexcept {
    detail::internal_store<Tag, T>::destroy();
    _base.reset();
  }
  // TODO move _base pointer to future_base.
  detail::unique_but_not_deleting_pointer<detail::continuation_base<Tag, T>> _base;
};

template <typename T, template <typename> typename Fut, typename Tag>
struct future_type_based_extensions<expect::expected<T>, Fut, Tag>
    : detail::future_prototype<expect::expected<T>, Fut, Tag> {
  /**
   * If the `expected<T>` contains a value, the callback is called with the value.
   * Otherwise it is not executed. Any thrown exception is captured by the `expected<T>`.
   * @tparam F
   * @tparam R
   * @param f
   * @return
   */
  template <typename F, typename U = T, std::enable_if_t<std::is_invocable_v<F, U&&>, int> = 0,
            typename R = std::invoke_result_t<F, U&&>>
  auto then(F&& f) && noexcept {
    // TODO what if `F` returns an `expected<U>`. Do we want to flatten automagically?
    static_assert(std::is_nothrow_constructible_v<F, F>,
                  "the lambda object must be nothrow constructible from "
                  "itself. You should pass it as rvalue reference and it "
                  "should be nothrow move constructible. If it's passed as "
                  "lvalue reference it has to be nothrow copyable.");
    static_assert(!is_future_like_v<R>);
    return std::move(self()).and_then(
        [f = std::forward<F>(f)](expect::expected<T>&& e) mutable noexcept -> expect::expected<R> {
          return std::move(e).map_value(std::forward<F>(f));
        });
  }

  template <typename F, typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0,
            std::enable_if_t<std::is_invocable_v<F>, int> = 0, typename R = std::invoke_result_t<F>>
  auto then(F&& f) && noexcept {
    static_assert(!is_future_like_v<R>);
    return std::move(self()).and_then(
        [f = std::forward<F>(f)](expect::expected<T>&& e) mutable noexcept
        -> expect::expected<R> { return std::move(e).map_value(f); });
  }

  /**
   * If the `expected<T>` contains a value, the callback is called with the value.
   * Otherwise it is not executed. Any thrown exception is captured by the `expected<T>`.
   * @tparam F
   * @tparam R
   * @param f
   * @return
   */
  /*template <typename F, std::enable_if_t<std::is_invocable_v<F, expect::expected<T>&&>, int> = 0,
            typename R = std::invoke_result_t<F, expect::expected<T>&&>,
            typename U = T, std::enable_if_t<!expect::is_expected_v<U>, int> = 0>
  auto then(F&& f) && noexcept {
    // TODO what if `F` returns an `expected<U>`. Do we want to flatten automagically?
    return std::move(self()).and_capture(std::forward<F>(f));
  }*/

  template <typename G, typename U = T, std::enable_if_t<std::is_invocable_v<G, U&&>, int> = 0,
            typename ReturnType = std::invoke_result_t<G, U&&>,
            std::enable_if_t<is_future_like_v<ReturnType>, int> = 0,
            typename ValueType = typename future_trait<ReturnType>::value_type,
            std::enable_if_t<!expect::is_expected_v<ValueType>, int> = 0>
  auto then_bind(G&& g) && noexcept -> future<expect::expected<ValueType>, Tag> {
    auto&& [f, p] = make_promise<expect::expected<ValueType>, Tag>();
    move_self().finally([g = std::forward<G>(g),
                         p = std::move(p)](expect::expected<T>&& t) mutable noexcept {
      if (t.has_error()) {
        return std::move(p).fulfill(t.error());
      }
      expect::expected<ReturnType> result =
          expect::captured_invoke(g, std::move(t).unwrap());
      if (result.has_value()) {
        std::move(result).unwrap().finally([p = std::move(p)](ValueType&& v) mutable noexcept {
          std::move(p).fulfill(std::move(v));
        });
      } else {
        std::move(p).fulfill(result.error());
      }
    });
    return std::move(f);
  }

  template <typename G, typename U = T, std::enable_if_t<std::is_invocable_v<G, U&&>, int> = 0,
            typename ReturnType = std::invoke_result_t<G, U&&>,
            std::enable_if_t<is_future_like_v<ReturnType>, int> = 0,
            typename ValueType = typename future_trait<ReturnType>::value_type,
            std::enable_if_t<expect::is_expected_v<ValueType>, int> = 0>
  auto then_bind(G&& g) && noexcept -> future<ValueType, Tag> {
    auto&& [f, p] = make_promise<ValueType, Tag>();
    move_self().finally([g = std::forward<G>(g),
                         p = std::move(p)](expect::expected<T>&& t) mutable noexcept {
      if (t.has_error()) {
        std::move(p).fulfill(t.error());
      }
      expect::expected<ReturnType> result =
          expect::captured_invoke(g, std::move(t).unwrap());
      if (result.has_value()) {
        std::move(result).unwrap().finally([p = std::move(p)](ValueType&& v) mutable noexcept {
          std::move(p).fulfill(std::move(v));
        });
      } else {
        std::move(p).fulfill(result.error());
      }
    });
    return std::move(f);
  }

  /**
   * Catches an exception of type `E` and calls `f` with `E const&`. If the
   * values does not contain an exception `f` will _never_ be called. The return
   * value of `f` is replaced with the exception. If `f` itself throws an
   * exception, the old exception is replaced with the new one.
   * @tparam E
   * @tparam F
   * @param f
   * @return A new init_future containing a value if the exception was caught.
   */
  template <typename E, typename F, typename U = T,
            std::enable_if_t<std::is_invocable_r_v<U, F, E const&>, int> = 0>
  auto catch_error(F&& f) && noexcept {
    return std::move(self()).and_then(
        [f = std::forward<F>(f)](expect::expected<T>&& e) noexcept -> expect::expected<T> {
          return std::move(e).template map_error<E>(f);
        });
  }

  /**
   * Same as await but unwraps the `expected<T>`.
   * @return Returns the value contained in expected, or throws the
   * contained exception.
   */
  template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
  T await_unwrap() && {
    return std::move(self()).await(yes_i_know_that_this_call_will_block).unwrap();
  }

  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  void await_unwrap() && {
    std::move(self()).await(yes_i_know_that_this_call_will_block).rethrow_error();
  }

  /**
   * Extracts the value from the `expected<T>` or replaces any exception with
   * a `T` constructed using the given arguments.
   *
   * Note that those arguments are copied into the chain and only used,
   * when required. If they are not needed, they are discarded.
   * @tparam Args
   * @param args
   * @return
   */
  template <typename... Args>
  auto unwrap_or(Args&&... args) {
    return std::move(self()).and_then(
        [args_tuple = std::make_tuple(std::forward<Args>(args)...)](
            expect::expected<T>&& e) noexcept {
          if (e.has_value()) {
            return std::move(e).unwrap();
          } else {
            return std::make_from_tuple<T>(std::move(args_tuple));
          }
        });
  }

  /**
   * (join) Flattens the underlying `expected<T>`, i.e. converts
   * `expected<expected<T>>` to `expected<T>`.
   * @return Future containing just a `expected<T>`.
   */
  template <typename U = T, std::enable_if_t<expect::is_expected_v<U>, int> = 0>
  auto flatten() {
    return std::move(self()).and_then(
        [](expect::expected<T>&& e) noexcept { return std::move(e).flatten(); });
  }

  /**
   * Rethrows any exception as a nested exception combined with `E(Args...)`.
   * `E` is only constructed if an exception was found. The arguments are copied
   * into the chain.
   * @tparam E
   * @tparam Args
   * @param args
   * @return
   */
  template <typename E, typename... Args>
  auto rethrow_nested(Args&&... args) noexcept {
    return std::move(self()).and_then(
        [args_tuple = std::make_tuple(std::forward<Args>(args)...)](
            expect::expected<T>&& e) mutable noexcept -> expect::expected<T> {
          // TODO can we instead forward to expected<T>::rethrow_nested
          try {
            try {
              e.rethrow_error();
            } catch (...) {
              std::throw_with_nested(std::make_from_tuple<E>(std::move(args_tuple)));
            }
          } catch (...) {
            return std::current_exception();
          }

          return std::move(e);
        });
  }

  /**
   * Rethrows an exception of type `W` as a nested exception combined with
   * `E(Args...)`. `E` is only constructed if an exception was found. The
   * arguments are copied into the chain.
   * @tparam E
   * @tparam Args
   * @param args
   * @return
   */
  template <typename W, typename E, typename... Args>
  auto rethrow_nested_if(Args&&... args) noexcept {
    return std::move(self()).and_then(
        [args_tuple = std::make_tuple(std::forward<Args>(args)...)](
            expect::expected<T>&& e) mutable noexcept -> expect::expected<T> {
          // TODO can we instead forward to expected<T>::rethrow_nested
          try {
            try {
              e.rethrow_error();
            } catch (W const&) {
              std::throw_with_nested(std::make_from_tuple<E>(std::move(args_tuple)));
            }
          } catch (...) {
            return std::current_exception();
          }
          return std::move(e);
        });
  }

  /**
   * Converts an `expected<T>` to `expected<U>`.
   * @tparam U
   * @return
   */
  template <typename U, std::enable_if_t<expect::is_expected_v<U>, int> = 0, typename R = typename U::value_type>
  auto as() {
    return std::move(self()).and_then(
        [](expect::expected<T>&& e) noexcept -> expect::expected<R> {
          return std::move(e).template as<R>();
        });
  }

 private:
  using future_type = Fut<expect::expected<T>>;

  future_type& self() noexcept { return *static_cast<future_type*>(this); }
  future_type const& self() const noexcept {
    return *static_cast<future_type const*>(this);
  }

  future_type&& move_self() noexcept { return std::move(self()); }

 public:
  template <typename... Ts, std::enable_if_t<std::is_constructible_v<T, Ts...>, int> = 0>
  static auto construct(std::in_place_t, Ts&&... ts) -> future_type {
    return future_type{std::in_place, std::in_place, std::forward<Ts>(ts)...};
  }
};

static_assert(detail::has_constructor_v<default_tag, expect::expected<int>, detail::future_proxy<default_tag>::template instance,
                                        std::in_place_t, int>);

template <typename T, typename Tag>
struct promise_type_based_extension<expect::expected<T>, Tag> {
  /**
   * A special form of `fulfill`. Uses `capture_invoke` to call a function and
   * capture the return value of any exceptions in the promise.
   * @tparam F
   * @tparam Args
   * @param f
   * @param args
   */
  template <typename F, typename... Args, std::enable_if_t<std::is_invocable_r_v<T, F, Args...>, int> = 0>
  void capture(F&& f, Args&&... args) && noexcept {
    std::move(self()).fulfill(
        expect::captured_invoke(std::forward<F>(f), std::forward<Args>(args)...));
  }

  /**
   * Throws `e` into the promise.
   * @tparam E
   * @param e
   */
  template <typename E>
  void throw_into(E&& e) noexcept {
    std::move(self()).fulfill(std::make_exception_ptr(std::forward<E>(e)));
  }

  /**
   * Constructs an exception of type `E` with the given arguments
   * and passes it into the promise.
   * @tparam E
   * @param e
   */
  template <typename E, typename... Args, std::enable_if_t<std::is_constructible_v<E, Args...>, int> = 0>
  void throw_exception(Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>) {
    std::move(self()).throw_into(E(std::forward<Args>(args)...));
  }

 private:
  using promise_type = promise<expect::expected<T>, Tag>;
  promise_type& self() noexcept { return static_cast<promise_type&>(*this); }
  promise_type const& self() const noexcept {
    return static_cast<promise_type const&>(*this);
  }
};

/**
 * Create a new pair of init_future and promise with value type `T`.
 * @tparam T value type
 * @return pair of init_future and promise.
 * @throws std::bad_alloc
 */
template <typename T, typename Tag>
auto make_promise() -> std::pair<future<T, Tag>, promise<T, Tag>> {
#ifdef FUTURES_COUNT_ALLOC
  ::mellon::detail::number_of_promises_created.fetch_add(1, std::memory_order_relaxed);
#endif
  auto start =
      detail::tag_trait_helper<Tag>::template allocate_construct<detail::continuation_start<Tag, T>>();
  return std::make_pair(future<T, Tag>{start}, promise<T, Tag>{start});
}



template <typename... Ts, template <typename> typename Fut, typename Tag>
struct future_type_based_extensions<std::tuple<Ts...>, Fut, Tag>
    : detail::future_prototype<std::tuple<Ts...>, Fut, Tag> {
  using tuple_type = std::tuple<Ts...>;

  /**
   * Applies `std::get<Idx> to the result. All other values
   * are discarded.
   * @tparam Idx
   * @return
   */
  template <std::size_t Idx>
  auto get() {
    return std::move(self()).and_then(
        [](tuple_type&& t) noexcept -> std::tuple_element_t<Idx, tuple_type> {
          return std::move(std::get<Idx>(t));
        });
  }

  /**
   * Transposes a init_future and a tuple. Returns a tuple of mellon awaiting
   * the individual members.
   * @return tuple of mellon
   */
  auto transpose() -> std::tuple<Fut<Ts>...> {
    return transpose(std::index_sequence_for<Ts...>{});
  }

  template <typename F, std::enable_if_t<std::is_nothrow_invocable_v<F, Ts&&...>, int> = 0,
            typename R = std::invoke_result_t<F, Ts&&...>>
  auto and_then_apply(F&& f) -> Fut<R> {
    return std::move(self()).and_then(
        [f = std::forward<F>(f)](std::tuple<Ts...>&& tup) mutable noexcept {
          return std::apply(f, std::move(tup));
        });
  }

  template <typename F, std::enable_if_t<std::is_nothrow_invocable_r_v<void, F, Ts&&...>, int> = 0,
            typename R = std::invoke_result_t<F, Ts&&...>>
  void finally_apply(F&& f) {
    std::move(self()).finally([f = std::forward<F>(f)](std::tuple<Ts...>&& tup) mutable noexcept {
      std::apply(f, std::move(tup));
    });
  }

  /**
   * Like `transpose` but restricts the output to the given indices. Other
   * elements are discarded.
   * @tparam Is indexes to select
   * @return tuple of mellon
   */
  template <std::size_t... Is>
  auto transpose_some() -> std::tuple<Fut<std::tuple_element<Is, tuple_type>>...> {
    return transpose(std::index_sequence<Is...>{});
  }

 private:
  template <std::size_t... Is>
  auto transpose(std::index_sequence<Is...>) {
    std::tuple<std::pair<future<Ts, Tag>, promise<Ts, Tag>>...> pairs(
        std::invoke([] { return make_promise<Ts>(); })...);

    std::move(self()).finally(
        [ps = std::make_tuple(std::move(std::get<Is>(pairs).second)...)](auto&& t) mutable noexcept {
          (std::invoke([&] {
             std::move(std::get<Is>(ps)).fulfill(std::move(std::get<Is>(t)));
           }),
           ...);
        });

    return std::make_tuple(std::move(std::get<Is>(pairs).first)...);
  }

  using future_type = Fut<std::tuple<Ts...>>;
  future_type& self() noexcept { return static_cast<future_type&>(*this); }
};

}  // namespace mellon

#endif  // FUTURES_FUTURES_H
