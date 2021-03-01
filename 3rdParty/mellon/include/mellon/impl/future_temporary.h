#ifndef FUTURES_IMPL
#error "this is an impl file which must not be included directly!"
#endif

namespace mellon {

template <typename T, typename F, typename R, typename Tag>
future_temporary<T, F, R, Tag>::future_temporary(future_temporary&& o) noexcept
    : detail::function_store<F>(std::move(o)), _base(nullptr) {
  if constexpr (is_value_inlined) {
    if (o.holds_inline_value()) {
      detail::internal_store<Tag, T>::emplace(o.cast_move());
      o.destroy();
    }
  }
  std::swap(_base, o._base);
}

template <typename T, typename F, typename R, typename Tag>
future_temporary<T, F, R, Tag>& future_temporary<T, F, R, Tag>::operator=(future_temporary&& o) noexcept {
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

template <typename T, typename F, typename R, typename Tag>
future_temporary<T, F, R, Tag>::~future_temporary() {
  if (_base) {
    std::move(*this).abandon();
  }
}

template <typename T, typename F, typename R, typename Tag>
template <typename G, std::enable_if_t<std::is_nothrow_invocable_v<G, R&&>, int>, typename S>
auto future_temporary<T, F, R, Tag>::and_then(G&& f) && noexcept {
  static_assert(!std::is_same_v<S, void>,
                "the lambda object must return a value");
  FUTURES_INC_ALLOC_COUNTER(number_of_temporary_objects);
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

template <typename T, typename F, typename R, typename Tag>
template <typename G, std::enable_if_t<std::is_nothrow_invocable_r_v<void, G, R&&>, int>>
void future_temporary<T, F, R, Tag>::finally(G&& f) && noexcept {
  static_assert(
      std::is_nothrow_constructible_v<G, G>,
      "the lambda object must be nothrow constructible from itself. If it's "
      "passed as lvalue reference it has to be nothrow copyable.");
  FUTURES_INC_ALLOC_COUNTER(number_of_temporary_objects);
  auto&& composition =
      detail::compose(std::forward<G>(f),
                      std::move(detail::function_store<F>::function_self()));

  if constexpr (is_value_inlined) {
    if (holds_inline_value()) {
      FUTURES_INC_ALLOC_COUNTER(number_of_and_then_on_inline_future);
      std::invoke(composition, detail::internal_store<Tag, T>::cast_move());
      cleanup_local_state();
      return;
    }
  }

  return detail::insert_continuation_final<Tag, T, decltype(composition)>(
      _base.release(), std::move(composition));
}

template <typename T, typename F, typename R, typename Tag>
auto future_temporary<T, F, R, Tag>::finalize() && noexcept -> future<R, Tag> {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      !empty(), "finalize called on empty temporary");
  if constexpr (is_value_inlined) {
    if (holds_inline_value()) {
      FUTURES_INC_ALLOC_COUNTER(number_of_and_then_on_inline_future);
      static_assert(std::is_nothrow_move_constructible_v<R>);
      static_assert(std::is_nothrow_destructible_v<T>);
      auto ff =
          future<R, Tag>(std::in_place,
                         std::invoke(detail::function_store<F>::function_self(),
                                     detail::internal_store<Tag, T>::cast_move()));
      cleanup_local_state();
      return ff;
    }
  }

  return detail::insert_continuation_step<Tag, T, F, R>(
      _base.release(), std::move(detail::function_store<F>::function_self()));
}

template <typename T, typename F, typename R, typename Tag>
void future_temporary<T, F, R, Tag>::cleanup_local_state() {
  detail::internal_store<Tag, T>::destroy();
  _base.reset();
}

}  // namespace mellon