#ifndef FUTURES_FUTURES_H
#define FUTURES_FUTURES_H
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>

#include "detail/box.h"
#include "detail/continuation.h"
#include "detail/gadgets.h"
#include "detail/has_constructor.h"
#include "detail/invalid-pointer-flags.h"
#include "detail/prototype.h"

#include "commons.h"
#include "expected.h"
#include "traits.h"

namespace mellon {

struct promise_abandoned_error : std::exception {
  [[nodiscard]] const char* what() const noexcept override;
};

template <>
struct tag_trait<default_tag> {
  struct allocator {
    // TODO is this how you do such things?
    template <typename T>
    static void* allocate() {
      return ::operator new(sizeof(T));
    }
    template <typename T>
    static void* allocate(std::nothrow_t) noexcept {
      return ::operator new(sizeof(T), std::nothrow);
    }
    template <typename T>
    static void release(T* p) noexcept {
      delete p;
      //p->~T();
      //::operator delete(p);
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

/**
 * Producing end of init_future-chain. When the promise is `fulfilled` the chain
 * of futures is evaluated.
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
    detail::tag_trait_helper<Tag>::debug_assert_true(
        !empty(), "fulfill called on empty promise");
    detail::fulfill_continuation<Tag>(_base.get(), std::forward<Args>(args)...);
    _base.reset();  // we can not use _base.release() in the call to fulfill_continuation because
    // the constructor of T could throw an exception. In that case the promise has to stay active.
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
  explicit promise(detail::base_ptr_t, detail::continuation_start<Tag, T>* base)
      : _base(base) {}
  detail::unique_but_not_deleting_pointer<detail::continuation_start<Tag, T>> _base = nullptr;
};

template <typename T, typename Tag>
void swap(promise<T, Tag>& a, promise<T, Tag>& b) {
  a.swap(b);
}

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
  static_assert(!is_future_v<T>,
                "future<future<T>> is a bad idea and thus not supported");
  static_assert(std::is_nothrow_move_constructible_v<T>);
  static_assert(std::is_nothrow_destructible_v<T>);

  future(future const&) = delete;
  future& operator=(future const&) = delete;
  future(future&& o) noexcept(!is_value_inlined || std::is_nothrow_move_constructible_v<T>);

  future& operator=(future&& o) noexcept(!is_value_inlined ||
                                         std::is_nothrow_move_constructible_v<T>);

  template <typename U, std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
  future(future<U, Tag>&& o) noexcept : future(std::move(o).template as<T>()) {}

  template <typename U, typename F, typename S, std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
  explicit future(future_temporary<S, F, U, Tag>&& o) noexcept
      : future(std::move(o).template as<T>().finalize()) {}

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
      std::conjunction_v<std::is_nothrow_constructible<T, Args...>, std::bool_constant<is_value_inlined>>);

  explicit future(T t) : future(std::in_place, std::move(t)) {}

  template <typename... Ts, typename U = T, std::enable_if_t<std::is_constructible_v<U, Ts...>, int> = 0,
            std::enable_if_t<detail::has_constructor_v<Tag, T, detail::future_proxy<Tag>::template instance, Ts...>, int> = 0>
  explicit future(Ts&&... ts)
      : future(future_type_based_extensions<T, detail::future_proxy<Tag>::template instance, Tag>::construct(
            std::forward<Ts>(ts)...)) {}

  explicit future(detail::base_ptr_t, detail::continuation_base<Tag, T>* ptr) noexcept
      : _base(ptr) {}

  /**
   * If the init_future was not used or moved away, the init_future is
   * abandoned. For more, see `abandon`.
   */
  ~future();

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
  auto and_then(F&& f) && noexcept;

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
  auto and_then_direct(F&& f) && noexcept -> future<R, Tag>;

  /**
   * Enqueues a final callback and ends the future chain.
   *
   * @tparam F callback type
   * @param f callback
   */
  template <typename F, std::enable_if_t<std::is_nothrow_invocable_r_v<void, F, T&&>, int> = 0>
  void finally(F&& f) && noexcept;

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
  void abandon() && noexcept;

  void swap(future<T, Tag>& o) noexcept(!is_value_inlined ||
                                        (std::is_nothrow_swappable_v<T> &&
                                         std::is_nothrow_move_constructible_v<T>));

  friend void swap(future<T, Tag>& a, future<T, Tag>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
  }

 private:
  template <typename, typename>
  friend struct future;

  void cleanup_local_state() noexcept;

  // TODO move _base pointer to future_base.
  detail::unique_but_not_deleting_pointer<detail::continuation_base<Tag, T>> _base;
};

/**
 * A temporary object that is used to chain together multiple `and_then` calls
 * into a single step to reduce memory and allocation overhead. Is derived from
 * future_base and thus provides all functions that mellon do.
 *
 * The temporary is implicitly convertible to `future<T>`.
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
  future_temporary(future_temporary&& o) noexcept;

  future_temporary& operator=(future_temporary&& o) noexcept;

  ~future_temporary();

  template <typename G, std::enable_if_t<std::is_nothrow_invocable_v<G, R&&>, int> = 0,
            typename S = std::invoke_result_t<G, R&&>>
  auto and_then(G&& f) && noexcept;

  template <typename G, std::enable_if_t<std::is_nothrow_invocable_r_v<void, G, R&&>, int> = 0>
  void finally(G&& f) && noexcept;

  void abandon() && noexcept { std::move(*this).finalize().abandon(); }

  /* implicit */ operator future<R, Tag>() && noexcept {
    return std::move(*this).finalize();
  }

  auto finalize() && noexcept -> future<R, Tag>;

  [[nodiscard]] bool holds_inline_value() const noexcept {
    return is_value_inlined &&
           _base.get() == FUTURES_INVALID_POINTER_INLINE_VALUE(Tag, T);
  }
  [[nodiscard]] bool empty() const noexcept { return _base == nullptr; }

 private:
  void cleanup_local_state();

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
 * Create a new pair of init_future and promise with value type `T`.
 * @tparam T value type
 * @return pair of init_future and promise.
 * @throws std::bad_alloc
 */
template <typename T, typename Tag>
auto make_promise() -> std::pair<future<T, Tag>, promise<T, Tag>> {
  FUTURES_INC_ALLOC_COUNTER(number_of_promises_created);
  auto start =
      detail::tag_trait_helper<Tag>::template allocate_construct<detail::continuation_start<Tag, T>>();
  return std::make_pair(future<T, Tag>{detail::base_ptr, start},
                        promise<T, Tag>{detail::base_ptr, start});
}
}  // namespace mellon

#define FUTURES_IMPL
#include "impl/extensions.h"
#include "impl/future.h"
#include "impl/future_temporary.h"
#undef FUTURES_IMPL

static_assert(mellon::detail::has_constructor_v<mellon::default_tag, expect::expected<int>,
                                                mellon::detail::future_proxy<mellon::default_tag>::template instance,
                                                std::in_place_t, int>);

#endif  // FUTURES_FUTURES_H
