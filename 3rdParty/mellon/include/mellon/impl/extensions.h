#ifndef FUTURES_IMPL
#error "this is an impl file which must not be included directly!"
#endif

namespace mellon {

template <typename T, template <typename> typename F, typename Tag>
struct FUTURES_EMPTY_BASE future_type_based_extensions
    : detail::future_prototype<T, F, Tag> {};

template <typename T, template <typename> typename Fut, typename Tag>
struct FUTURES_EMPTY_BASE future_type_based_extensions<expect::expected<T>, Fut, Tag>
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
    // mpoeter - I think it would make sense!
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

  /**
   * @brief void-variant of @c{then}
   */
  template <typename F, typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0,
            std::enable_if_t<std::is_invocable_v<F>, int> = 0, typename R = std::invoke_result_t<F>>
  auto then(F&& f) && noexcept {
    static_assert(!is_future_like_v<R>);
    return std::move(self()).and_then(
        [f = std::forward<F>(f)](expect::expected<T>&& e) mutable noexcept
        -> expect::expected<R> { return std::move(e).map_value(f); });
  }

  // mpoeter - documentation missing
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

  // mpoeter - documentation missing
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
        return std::move(p).fulfill(t.error());
      } else {
        expect::expected<ReturnType> result =
            expect::captured_invoke(g, std::move(t).unwrap());
        if (result.has_value()) {
          std::move(result).unwrap().finally([p = std::move(p)](ValueType&& v) mutable noexcept {
            std::move(p).fulfill(std::move(v));
          });
        } else {
          std::move(p).fulfill(result.error());
        }
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
   * @return A new future containing a value if the exception was caught.
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
    return std::move(self()).await().unwrap();
  }

  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  void await_unwrap() && {
    std::move(self()).await().rethrow_error();
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
   * `E` is only constructed if an exception was found. The arguments are
   * forwarded into the chain.
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
   * arguments are forwarded into the chain.
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

template <typename T, typename Tag>
struct FUTURES_EMPTY_BASE promise_type_based_extension<expect::expected<T>, Tag> {
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
  void throw_exception(Args&&... args) && noexcept(std::is_nothrow_constructible_v<E, Args...>) {
    std::move(self()).throw_into(E(std::forward<Args>(args)...));
  }

  template <typename E, typename... Args, std::enable_if_t<std::is_constructible_v<E, Args...>, int> = 0>
  void throw_with_nested(Args&&... args) && noexcept(std::is_nothrow_constructible_v<E, Args...>) {
    if (std::uncaught_exceptions() != 0) {
      std::move(self()).capture([&]() -> T {
        std::throw_with_nested(E(std::forward<Args>(args)...));
      });
    } else {
      std::move(self()).template throw_exception<E>(std::forward<Args>(args)...);
    }
  }

  void capture_current_exception() && noexcept {
    if (std::uncaught_exceptions() != 0) {
      std::move(self()).fulfill(std::current_exception());
    } else {
      std::move(self()).template throw_exception<std::logic_error>(
          "no uncaught exception");
    }
  }

 private:
  using promise_type = promise<expect::expected<T>, Tag>;
  promise_type& self() noexcept { return static_cast<promise_type&>(*this); }
  promise_type const& self() const noexcept {
    return static_cast<promise_type const&>(*this);
  }
};

template <typename... Ts, template <typename> typename Fut, typename Tag>
struct FUTURES_EMPTY_BASE future_type_based_extensions<std::tuple<Ts...>, Fut, Tag>
    : detail::future_prototype<std::tuple<Ts...>, Fut, Tag> {
  using tuple_type = std::tuple<Ts...>;

  /**
   * Applies `std::get<Idx> to the result. All other values are discarded.
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
   * Transposes a future and a tuple. Returns a tuple of futures awaiting
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
