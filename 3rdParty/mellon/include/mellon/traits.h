#ifndef FUTURES_TRAITS_H
#define FUTURES_TRAITS_H

//#define FUTURES_COUNT_ALLOC 0

namespace mellon {

template <typename, typename>
struct future;
template <typename, typename, typename, typename>
struct future_temporary;
struct default_tag;

template <typename F>
struct future_trait;

template <typename T>
struct tag_trait;

template <typename T, typename Tag>
struct future_trait<future<T, Tag>> {
  static constexpr auto is_value_inlined = future<T, Tag>::is_value_inlined;

  template <typename U>
  using future_for_type = future<U, Tag>;
  using value_type = T;
  using tag_type = Tag;
};

template <typename T, typename F, typename R, typename Tag>
struct future_trait<future_temporary<T, F, R, Tag>> : future_trait<future<R, Tag>> {};

template <typename F>
using future_value_type_t = typename future_trait<F>::value_type;
template <typename F>
using future_tag_type_t = typename future_trait<F>::tag_type;
template <typename F>
inline constexpr auto is_future_value_inlined = future_trait<F>::is_value_inlined;

template <typename T = void>
struct use_default_handler {};
template <typename T>
struct is_use_default_handler : std::false_type {};
template <typename T>
struct is_use_default_handler<use_default_handler<T>> : std::true_type {};
template <typename T>
inline constexpr auto is_use_default_handler_v = is_use_default_handler<T>::value;

template<typename Tag, typename T, template<typename> typename Fut, typename = void>
struct user_defined_additions {
  struct type {};
};

template<typename Tag, typename T, template<typename> typename Fut>
struct user_defined_additions<Tag, T, Fut, std::void_t<decltype(typename tag_trait<Tag>::template user_defined_additions<T, Fut>{})>> {
  using type = typename tag_trait<Tag>::template user_defined_additions<T, Fut>;
};

template<typename Tag, typename T, template<typename> typename Fut>
using user_defined_additions_t = typename user_defined_additions<Tag, T, Fut>::type;

template<typename Tag, typename T, typename = void>
struct user_defined_promise_additions {
  struct type {};
};

template<typename Tag, typename T>
struct user_defined_promise_additions<Tag, T, std::void_t<decltype(typename tag_trait<Tag>::template user_defined_promise_additions<T>{})>> {
  using type = typename tag_trait<Tag>::template user_defined_promise_additions<T>;
};

template<typename Tag, typename T>
using user_defined_promise_additions_t = typename user_defined_promise_additions<Tag, T>::type;

namespace detail {
#ifdef FUTURES_COUNT_ALLOC
extern std::atomic<std::size_t> number_of_allocations;
extern std::atomic<std::size_t> number_of_bytes_allocated;
extern std::atomic<std::size_t> number_of_inline_value_placements;
extern std::atomic<std::size_t> number_of_temporary_objects;
extern std::atomic<std::size_t> number_of_prealloc_usage;


extern std::atomic<std::size_t> number_of_inline_value_allocs;
extern std::atomic<std::size_t> number_of_final_usage;
extern std::atomic<std::size_t> number_of_step_usage;
extern std::atomic<std::size_t> number_of_promises_created;


extern std::atomic<std::size_t> number_of_and_then_on_inline_future;


extern std::array<std::atomic<std::size_t>, 10> histogram_value_sizes;
extern std::array<std::atomic<std::size_t>, 10> histogram_final_lambda_sizes;

extern std::string message_prefix;
#endif


template <typename Tag>
struct tag_trait_helper {
  template <typename tag, typename = void>
  struct has_assertion_handler : std::false_type {};
  template <typename tag>
  struct has_assertion_handler<tag, std::void_t<typename tag_trait<tag>::assertion_handler>>
      : std::true_type {};
  template <typename tag, typename = void>
  struct has_debug_assertion_handler : std::false_type {};
  template <typename tag>
  struct has_debug_assertion_handler<tag, std::void_t<typename tag_trait<tag>::debug_assertion_handler>>
      : std::true_type {};

  template <typename tag, typename T, typename = void>
  struct has_promise_aborted_handler : std::false_type {};
  template <typename tag, typename T>
  struct has_promise_aborted_handler<tag, T, std::void_t<typename tag_trait<tag>::template abandoned_promise_handler<T>>>
      : std::true_type {};

  template <typename tag, typename T, typename = void>
  struct has_future_aborted_handler : std::false_type {};
  template <typename tag, typename T>
  struct has_future_aborted_handler<tag, T, std::void_t<typename tag_trait<tag>::template abandoned_future_handler<T>>>
      : std::true_type {};

  template <typename tag, typename = void>
  struct has_small_value_size : std::false_type {};
  template <typename tag>
  struct has_small_value_size<tag, std::void_t<decltype(tag_trait<tag>::small_value_size)>>
      : std::true_type {};
  template <typename tag, typename = void>
  struct has_finally_prealloc_size : std::false_type {};
  template <typename tag>
  struct has_finally_prealloc_size<tag, std::void_t<decltype(tag_trait<tag>::finally_prealloc_size)>>
      : std::true_type {};

  template <typename tag, typename = void>
  struct has_is_disable_temporaries : std::false_type {};
  template <typename tag>
  struct has_is_disable_temporaries<tag, std::void_t<decltype(tag_trait<tag>::disable_temporaries)>>
      : std::true_type {};

  template <typename tag, typename = void>
  struct has_allocator : std::false_type {};
  template <typename tag>
  struct has_allocator<tag, std::void_t<typename tag_trait<tag>::allocator>>
      : std::true_type {};

  template <typename tag, typename T, typename = void>
  struct has_is_type_inlined : std::false_type {};
  template <typename tag, typename T>
  struct has_is_type_inlined<tag, T, std::void_t<decltype(tag_trait<tag>::template is_type_inlined<T>)>>
      : std::true_type {};

  static void assert_true(bool condition) noexcept {
    if constexpr (has_assertion_handler<Tag>::value) {
      using assertion_handler = typename tag_trait<Tag>::assertion_handler;
      static_assert(std::is_nothrow_invocable_r_v<void, assertion_handler, bool>);
      assertion_handler{}(condition);
    } else {
      static_assert(!std::is_same_v<default_tag, Tag>);
      tag_trait_helper<default_tag>::assert_true(condition);
    }
  }

  static void debug_assert_true(bool condition) noexcept {
    if constexpr (has_debug_assertion_handler<Tag>::value) {
      using assertion_handler = typename tag_trait<Tag>::debug_assertion_handler;
      static_assert(std::is_nothrow_invocable_r_v<void, assertion_handler, bool>);
      assertion_handler{}(condition);
    }
  }

  template <typename T>
  static T abandon_promise() noexcept {
    static_assert(has_promise_aborted_handler<Tag, T>::value,
                  "please provide a abandoned_promise_handler in the "
                  "tag_traits, or set it to use_default_handler<T>");
    using handler = typename tag_trait<Tag>::template abandoned_promise_handler<T>;
    if constexpr (is_use_default_handler_v<handler>) {
      return tag_trait_helper<default_tag>::abandon_promise<T>();
    } else {
      return handler{}();
    }
  }

  template <typename T, typename U>
  static void abandon_future(U&& u) noexcept {
    static_assert(has_future_aborted_handler<Tag, T>::value,
                  "please provide a abandoned_future_handler in the "
                  "tag_traits, or set it to use_default_handler<T>");
    using handler = typename tag_trait<Tag>::template abandoned_future_handler<T>;
    if constexpr (is_use_default_handler_v<handler>) {
      tag_trait_helper<default_tag>::abandon_future<T>(std::forward<U>(u));
    } else {
      handler{}(std::forward<U>(u));
    }
  }

  static constexpr std::size_t small_value_size() {
    if constexpr (has_small_value_size<Tag>::value) {
      return tag_trait<Tag>::small_value_size;
    } else {
      static_assert(!std::is_same_v<default_tag, Tag>);
      return tag_trait_helper<default_tag>::small_value_size();
    }
  }

  static constexpr std::size_t finally_prealloc_size() {
    if constexpr (has_finally_prealloc_size<Tag>::value) {
      return tag_trait<Tag>::finally_prealloc_size;
    } else {
      static_assert(!std::is_same_v<default_tag, Tag>);
      return tag_trait_helper<default_tag>::finally_prealloc_size();
    }
  }

  template<typename T>
  static constexpr bool is_type_inlined() {
    if constexpr (has_is_type_inlined<Tag, T>::value) {
      return tag_trait<Tag>::template is_type_inlined<T>;
    } else {
      return sizeof(T) < small_value_size();
    }
  }

  static constexpr bool is_disable_temporaries() {
    if constexpr (has_is_disable_temporaries<Tag>::value) {
      return tag_trait<Tag>::disable_temporaries;
    }
    return false;
  }

  template<typename T>
  static T* allocate() {
    if constexpr (has_allocator<Tag>::value) {
#if FUTURES_COUNT_ALLOC
      number_of_allocations.fetch_add(1, std::memory_order_relaxed);
      number_of_bytes_allocated.fetch_add(sizeof(T), std::memory_order_relaxed);
#endif
      using allocator = typename tag_trait<Tag>::allocator;
      return allocator::template allocate<T>();
    } else {
      static_assert(has_allocator<default_tag>::value);
      return tag_trait_helper<default_tag>::allocate<T>();
    }
  }
  template<typename T>
  static T* allocate(std::nothrow_t) {
    if constexpr (has_allocator<Tag>::value) {
#if FUTURES_COUNT_ALLOC
      number_of_allocations.fetch_add(1, std::memory_order_relaxed);
      number_of_bytes_allocated.fetch_add(sizeof(T), std::memory_order_relaxed);
#endif
      using allocator = typename tag_trait<Tag>::allocator;
      return allocator::template allocate<T>(std::nothrow);
    } else {
      return tag_trait_helper<default_tag>::allocate<T>(std::nothrow);
    }
  }

  template<typename T, typename... Args>
  static T* allocate_construct(Args&&... args) {
    return new (allocate<T>()) T(std::forward<Args>(args)...);
  }
};
}  // namespace detail
}  // namespace mellon

#endif  // FUTURES_TRAITS_H
