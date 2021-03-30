#ifndef FUTURES_IMPL
#error "this is an impl file which must not be included directly!"
#endif

namespace mellon {

template <typename T, typename Tag>
future<T, Tag>::future(future&& o) noexcept(!is_value_inlined ||
                                            std::is_nothrow_move_constructible_v<T>)
    : _base(nullptr) {
  if constexpr (is_value_inlined) {
    if (o.holds_inline_value()) {
      detail::internal_store<Tag, T>::emplace(o.cast_move());
      o.destroy();
    }
  }
  std::swap(_base, o._base);
}

template <typename T, typename Tag>
future<T, Tag>& future<T, Tag>::operator=(future&& o) noexcept(
    !is_value_inlined || std::is_nothrow_move_constructible_v<T>) {
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

template <typename T, typename Tag>
template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int>>
future<T, Tag>::future(std::in_place_t, Args&&... args) noexcept(
    std::conjunction_v<std::is_nothrow_constructible<T, Args...>, std::bool_constant<is_value_inlined>>) {
#ifdef FUTURES_COUNT_ALLOC
  constexpr std::size_t lambda_size = sizeof(T);
  constexpr double lambda_log2 = std::max(0.0, std::log2(lambda_size) - 2.0);
  std::size_t bucket =
      static_cast<std::size_t>(std::max(0., std::ceil(lambda_log2) - 1));
  FUTURES_INC_ALLOC_COUNTER(histogram_value_sizes[bucket]);
#endif
  if constexpr (is_value_inlined) {
    FUTURES_INC_ALLOC_COUNTER(number_of_inline_value_placements);
    detail::internal_store<Tag, T>::emplace(std::forward<Args>(args)...);
    _base.reset(FUTURES_INVALID_POINTER_INLINE_VALUE(Tag, T));
  } else {
    FUTURES_INC_ALLOC_COUNTER(number_of_inline_value_allocs);
    // TODO no preallocated memory needed
    _base.reset(detail::tag_trait_helper<Tag>::template allocate_construct<detail::continuation_base<Tag, T>>(
        std::in_place, std::forward<Args>(args)...));
  }
}

template <typename T, typename Tag>
future<T, Tag>::~future() {
  if (_base) {
    std::move(*this).abandon();
  }
}

template <typename T, typename Tag>
template <typename F, std::enable_if_t<std::is_nothrow_invocable_v<F, T&&>, int>, typename R>
auto future<T, Tag>::and_then(F&& f) && noexcept {
  static_assert(!std::is_void_v<R>, "the lambda object must return a value");
  detail::tag_trait_helper<Tag>::debug_assert_true(
      !empty(), "and_then called on empty future");
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

template <typename T, typename Tag>
template <typename F, std::enable_if_t<std::is_nothrow_invocable_v<F, T&&>, int>, typename R>
auto future<T, Tag>::and_then_direct(F&& f) && noexcept -> future<R, Tag> {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      !empty(), "and_then called on empty future");
  if constexpr (is_value_inlined) {
    if (holds_inline_value()) {
      FUTURES_INC_ALLOC_COUNTER(number_of_and_then_on_inline_future);
      auto fut = future<R, Tag>{std::in_place,
                                std::invoke(std::forward<F>(f), this->cast_move())};
      cleanup_local_state();
      return std::move(fut);
    }
  }
  return detail::insert_continuation_step<Tag, T, F, R>(_base.release(),
                                                        std::forward<F>(f));
}

template <typename T, typename Tag>
template <typename F, std::enable_if_t<std::is_nothrow_invocable_r_v<void, F, T&&>, int>>
void future<T, Tag>::finally(F&& f) && noexcept {
  static_assert(std::is_nothrow_constructible_v<F, F>,
                "the lambda object must be nothrow constructible from "
                "itself. You should pass it as rvalue reference and it "
                "should be nothrow move constructible. If it's passed as "
                "lvalue reference it has to be nothrow copyable.");

  detail::tag_trait_helper<Tag>::debug_assert_true(
      !empty(), "finally called on empty future");
  if constexpr (is_value_inlined) {
    if (holds_inline_value()) {
      FUTURES_INC_ALLOC_COUNTER(number_of_and_then_on_inline_future);
      std::invoke(std::forward<F>(f), detail::internal_store<Tag, T>::cast_move());
      cleanup_local_state();
      return;
    }
  }

  return detail::insert_continuation_final<Tag, T, F>(_base.release(),
                                                      std::forward<F>(f));
}

template <typename T, typename Tag>
void future<T, Tag>::abandon() && noexcept {
  if constexpr (is_value_inlined) {
    if (holds_inline_value()) {
      cleanup_local_state();
      return;
    }
  }

  detail::abandon_continuation<Tag>(_base.release());
}

template <typename T, typename Tag>
void future<T, Tag>::swap(future<T, Tag>& o) noexcept(
    !is_value_inlined ||
    (std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<T>)) {
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

template <typename T, typename Tag>
void future<T, Tag>::cleanup_local_state() noexcept {
  detail::internal_store<Tag, T>::destroy();
  _base.reset();
}

}  // namespace mellon