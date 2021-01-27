#ifndef FUTURES_EXPECTED_H
#define FUTURES_EXPECTED_H
#include <functional>
#include <stdexcept>
#include <utility>
#include <optional>

namespace expect {

template <typename T>
struct expected;

template <typename T>
struct is_expected : std::false_type {};
template <typename T>
struct is_expected<expected<T>> : std::true_type {};
template <typename T>
inline constexpr auto is_expected_v = is_expected<T>::value;

/**
 * Invokes `f` with `args...` and captures the return value in an expected. If
 * and exception is thrown is it also captured.
 * @tparam F Callable
 * @tparam Args Argument types
 * @tparam R Return value type (deduced)
 * @param f Callable
 * @param args Argument values
 * @return `expected<R>` containing `f(args...)` or a exception.
 */
template <typename F, typename... Args, std::enable_if_t<std::is_invocable_v<F, Args...>, int> = 0,
          typename R = std::invoke_result_t<F, Args...>, std::enable_if_t<!is_expected_v<R>, int> = 0>
auto captured_invoke(F&& f, Args&&... args) noexcept -> expected<R>;

/**
 * Invokes `f` with `args...` and captures the returns the result. If
 * and exception is thrown is it also captured and returned instead.
 * @tparam F Callable
 * @tparam Args Argument types
 * @tparam R Return value of `f`.
 * @param f Callable
 * @param args Argument values
 * @return `expected<R>` as result of `f(args...)` or a exception.
 */
template <typename F, typename... Args, std::enable_if_t<std::is_invocable_v<F, Args...>, int> = 0,
          typename R = std::invoke_result_t<F, Args...>, std::enable_if_t<is_expected_v<R>, int> = 0>
auto captured_invoke(F&& f, Args&&... args) noexcept -> R;

namespace detail {

/**
 * Common base class for specialisations of `expected<T>`.
 * @tparam T value type
 */
template <typename T>
struct expected_base {
  /**
   * Calls `f` with `std::move(*this)` as parameter and captures its return
   * value. If `f` throws an exception it is captured as well.
   * @tparam F Callable
   * @param f Function to be invoked.
   * @return Return value of `f`
   */
  template <typename F, std::enable_if_t<std::is_invocable_v<F, expected<T>&&>, int> = 0>
  auto map(F&& f) && noexcept -> expected<std::invoke_result_t<F, expected<T>&&>> {
    return captured_invoke(std::forward<F>(f), std::move(self()));
  }

  /**
   * Tests whether this holds an exception of type `E`. This method is slow
   * because it has to rethrow the exception to inspect its type.
   * @tparam E
   * @return true if `this` holds an exception of type `E`.
   */
  template <typename E>
  [[nodiscard]] bool has_exception() const noexcept {
    try {
      rethrow_error();
    } catch (E const&) {
      return true;
    } catch (...) {
      return false;
    }
    return false;
  }

  /**
   * Rethrows an error as exception.
   */
  void rethrow_error() const {
    if (self().has_error()) {
      std::rethrow_exception(self().error());
    }
  }

  /**
   * Invokes `f` with `E const&` if `this` contains an exception of type `E`.
   * Otherwise does nothing. Returns an optional containing the result of the
   * invocation or an empty `optional` if no error was present.
   *
   * @tparam E Exception type
   * @tparam F Callable
   * @param f Function object
   * @return `std::optional` containing the optional return value of `f`.
   */
  template <typename E, typename F, std::enable_if_t<std::is_invocable_v<F, E const&>, int> = 0>
  auto catch_error(F&& f) noexcept(std::is_nothrow_invocable_v<F, E const&>)
      -> std::optional<std::invoke_result_t<F, E const&>> {
    try {
      self().rethrow_error();
    } catch (E const& e) {
      return std::invoke(std::forward<F>(f), e);
    } catch (...) {
    }
    return std::nullopt;
  }

  template <typename E, typename F, std::enable_if_t<std::is_invocable_r_v<T, F, E const&>, int> = 0>
  auto map_error(F&& f) && noexcept -> expected<T> {
    return captured_invoke([&]() -> expected<T> {
      if (self().has_error()) {
        try {
          std::rethrow_exception(self().error());
        } catch (E const& e) {
          if constexpr (std::is_void_v<T>) {
            std::invoke(std::forward<F>(f), e);
            return {};
          } else {
            return std::invoke(std::forward<F>(f), e);
          }
        }
      }
      return std::move(self());
    });
  }

  /**
   * Rethrows any exception with `E` constructed using `args...`. `E` is
   * constructed only if an exception is present.
   * @tparam E
   * @tparam Args
   * @param args
   * @return
   */
  template <typename E, typename... Args>
  auto rethrow_nested(Args&&... args) && noexcept -> expected<T> {
    try {
      try {
        self().rethrow_error();
      } catch (...) {
        std::throw_with_nested(E(std::forward<Args>(args)...));
      }
    } catch (...) {
      return std::current_exception();
    }

    return std::move(self());
  }

  /**
   * Rethrows an exception of type `W` with `E` constructed using `args...`.
   * `E` is constructed only if an exception of type `W` was present.
   * @tparam W
   * @tparam E
   * @tparam Args
   * @param args
   * @return
   */
  template <typename W, typename E, typename... Args>
  auto rethrow_nested(Args&&... args) && noexcept -> expected<T> {
    try {
      try {
        self().rethrow_error();
      } catch (W const&) {
        std::throw_with_nested(E(std::forward<Args>(args)...));
      }
    } catch (...) {
      return std::current_exception();
    }

    return std::move(self());
  }

  /**
   * Converts to true, if it contains an value. False otherwise.
   * @return `has_value()`
   */
  explicit operator bool() const noexcept { return self().has_value(); }

  using value_type = T;

 private:
  [[nodiscard]] expected<T>& self() { return static_cast<expected<T>&>(*this); }
  [[nodiscard]] expected<T> const& self() const {
    return static_cast<expected<T> const&>(*this);
  }
};

}  // namespace detail

/**
 * Either contains a value of type T or an exception.
 * @tparam T
 */
template <typename T>
struct expected : detail::expected_base<T> {
  static_assert(!std::is_void_v<T> && !std::is_reference_v<T>);

  /**
   * Default constructs a value into the result. Only available if `T` is
   * default constructible.
   */
  template <typename U = T, std::enable_if_t<std::is_default_constructible_v<U>, int> = 0>
  expected() noexcept(std::is_nothrow_default_constructible_v<T>)
      : expected(std::in_place) {}

  /**
   * Stores the exception pointer.
   * @param p exception pointer
   */
  /* implicit */ expected(std::exception_ptr p) noexcept
      : _exception(std::move(p)), _has_value(false) {}
  /**
   * Stores the given value.
   * @param t value
   */
  /* implicit */ expected(T t) noexcept(std::is_nothrow_move_constructible_v<T>)
      : _value(std::move(t)), _has_value(true) {}

  /**
   * In place constructs a `T` using the given parameters. If the constructor
   * of `T` throws an exception it is *not* captured in the expected but bubbles
   * up.
   * @tparam Args argument types
   * @param ts argument values
   */
  template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  explicit expected(std::in_place_t,
                    Args&&... ts) noexcept(std::is_nothrow_constructible_v<T, Args...>)
      : _value(std::forward<Args>(ts)...), _has_value(true) {}

  /**
   * Move constructor. Only available if `T` is move constructible.
   * @tparam U
   * @param o
   */
  template <typename U = T, std::enable_if_t<std::is_move_constructible_v<U>, int> = 0>
  /* implicit */ expected(expected&& o) noexcept(std::is_nothrow_move_constructible_v<T>)
      : _has_value(o._has_value) {
    if (o._has_value) {
      new (&this->_value) T(std::move(o._value));
    } else {
      new (&this->_exception) std::exception_ptr(std::move(o._exception));
    }
  }

  /**
   * Conversion constructor. Only present if `U` is convertible to `T`.
   * Converts a value from `U` to `T` and keeps all exceptions. Exceptions during
   * conversion are not captured.
   * @tparam U
   * @param u
   */
  template <typename U, std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
  expected(expected<U>&& u) noexcept(false /* std::is_nothrow_convertible_v<U, T> C++20 */)
      : expected(std::move(u).template as<T>()) {}

  ~expected() {
    if (_has_value) {
      _value.~T();
    } else {
      _exception.~exception_ptr();
    }
  }

  expected(expected const&) = delete;
  expected& operator=(expected const&) = delete;
  expected& operator=(expected&&) noexcept = delete;  // TODO implement this

  /**
   * Returns the value or throws the containing exception.
   * @return Underlying value.
   */
  // TODO do we want to keep the & and const& variants?
  T& unwrap() & {
    if (_has_value) {
      return _value;
    }
    std::rethrow_exception(_exception);
  }
  T const& unwrap() const& {
    if (_has_value) {
      return _value;
    }
    std::rethrow_exception(_exception);
  }
  T&& unwrap() && {
    if (_has_value) {
      return std::move(_value);
    }
    std::rethrow_exception(_exception);
  }

  /**
   * Returns the value present or, if there is an exception, constructs a `T`
   * using the provided parameters. This does not throw an exception, unless
   * the selected constructor of `T` does.
   * @tparam Args
   * @param args
   * @return a valid value
   */
  template <typename... Args>
  T unwrap_or(Args&&... args) && noexcept(
      std::is_nothrow_constructible_v<T, Args...>&& std::is_nothrow_move_constructible_v<T>) {
    static_assert(std::is_constructible_v<T, Args...>);
    if (has_value()) {
      return std::move(_value);
    } else {
      return T(std::forward<Args>(args)...);
    }
  }

  /**
   * Returns the exception pointer.
   * @return returns the exception or null if no exception is present.
   */
  [[nodiscard]] std::exception_ptr error() const noexcept {
    if (has_error()) {
      return _exception;
    }
    return nullptr;
  }

  /**
   * Accesses the value.
   */
  T* operator->() { return &unwrap(); }
  T const* operator->() const { return &unwrap(); }

  [[nodiscard]] bool has_value() const noexcept { return _has_value; }
  [[nodiscard]] bool has_error() const noexcept { return !has_value(); }

  /**
   * If a value is present, `f` is called, otherwise the exception is passed forward.
   * @tparam F
   * @param f
   * @return
   */
  template <typename F, std::enable_if_t<std::is_invocable_v<F, T&&>, int> = 0>
  auto map_value(F&& f) && noexcept -> expected<std::invoke_result_t<F, T&&>> {
    if (has_error()) {
      return std::move(_exception);
    }

    return captured_invoke(std::forward<F>(f), std::move(_value));
  }

  /**
   * Monadic join. Reduces expected<expected<T>> to expected<T>.
   * @return
   */
  template <typename U = T, std::enable_if_t<is_expected_v<U>, int> = 0, typename R = typename U::value_type>
  auto flatten() && noexcept(std::is_nothrow_move_constructible_v<R>) -> expected<R> {
    static_assert(std::is_move_constructible_v<U>);
    if (has_error()) {
      return std::move(_exception);
    }

    return std::move(_value);
  }

  /**
   * Converts `T` to `U`. If an exception is thrown, it is stored in the return
   * value.
   * @tparam U target type
   * @return
   */
  template <typename U>
  auto as() -> expected<U> {
    static_assert(std::is_convertible_v<T, U>,
                  "can not convert from `T` to `U`");
    return std::move(*this).map_value([](T&& t) -> U { return U(std::move(t)); });
  }

 private:
  union {
    T _value;
    std::exception_ptr _exception;
  };
  const bool _has_value;
};

/**
 * Void specialised version of expected. Stores a single exception pointer.
 */
template <>
struct expected<void> : detail::expected_base<void> {
  expected() = default;
  /* implicit */ expected(std::exception_ptr p) : _exception(std::move(p)) {}
  /* implicit */ expected(std::in_place_t) : _exception(nullptr) {}
  ~expected() = default;

  expected(expected const&) = delete;
  expected(expected&&) noexcept = default;

  expected& operator=(expected const&) = delete;
  expected& operator=(expected&&) noexcept = default;

  [[nodiscard]] bool has_error() const noexcept {
    return _exception != nullptr;
  }
  [[nodiscard]] auto error() const noexcept -> std::exception_ptr {
    static_assert(std::is_nothrow_copy_constructible_v<std::exception_ptr>);
    if (has_error()) {
      return _exception;
    }
    return nullptr;
  }

  /**
   * Invokes `f` if no exception is present. Otherwise does nothing.
   * @tparam F
   * @param f
   * @return captured return value of `f`.
   */
  template <typename F, std::enable_if_t<std::is_invocable_v<F>, int> = 0>
  auto map_value(F&& f) && noexcept -> expected<std::invoke_result_t<F>> {
    if (has_error()) {
      return std::move(_exception);
    }

    return captured_invoke(std::forward<F>(f));
  }

 private:
  std::exception_ptr _exception = nullptr;
};

template <typename F, typename... Args, std::enable_if_t<std::is_invocable_v<F, Args...>, int>,
          typename R, std::enable_if_t<!is_expected_v<R>, int>>
auto captured_invoke(F&& f, Args&&... args) noexcept -> expected<R> {
  try {
    if constexpr (std::is_void_v<R>) {
      std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
      return {};
    } else {
      return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    }
  } catch (...) {
    return std::current_exception();
  }
}

template <typename F, typename... Args, std::enable_if_t<std::is_invocable_v<F, Args...>, int>,
          typename R, std::enable_if_t<is_expected_v<R>, int>>
auto captured_invoke(F&& f, Args&&... args) noexcept -> R {
  try {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
  } catch (...) {
    return std::current_exception();
  }
}

}  // namespace expect

#endif  // FUTURES_EXPECTED_H
