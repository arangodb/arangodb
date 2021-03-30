#ifndef FUTURES_GADGETS_H
#define FUTURES_GADGETS_H

#include "../commons.h"

namespace mellon::detail {
template <typename T>
struct no_deleter {
  void operator()(T*) const noexcept {}
};

template <typename T>
using unique_but_not_deleting_pointer = std::unique_ptr<T, no_deleter<T>>;  // TODO find a better name

template <typename F, typename Func = std::decay_t<F>, typename = std::enable_if_t<std::is_class_v<Func>>>
struct FUTURES_EMPTY_BASE function_store : Func {
  template <typename G = F>
  explicit function_store(std::in_place_t,
      G&& f) noexcept(std::is_nothrow_constructible_v<Func, G>)
      : Func(std::forward<G>(f)) {}

  [[nodiscard]] F function_self() { return std::forward<F>(*this); }
};


template <typename Tag>
struct deleter_dealloc {
  template <typename T>
  void operator()(T* ptr) {
    detail::tag_trait_helper<Tag>::release(ptr);
  }
};

struct deleter_destroy {
  template <typename T>
  void operator()(T* ptr) {
    ptr->~T();
  }
};

template <std::size_t size, std::size_t max, std::size_t align>
struct size_tester {
  //  static_assert(size <= max);
  static_assert(align == 8);
};

template <std::size_t Size>
struct memory_buffer {
  template <typename T>
  void* try_allocate() noexcept {
    size_tester<sizeof(T), Size, alignof(T)> test;
    (void)test;
    void* data = store;
    std::size_t size = Size;
    void* base = std::align(alignof(T), sizeof(T), data, size);
    return base;
  }

  std::byte store[Size] = {};
};

template <>
struct memory_buffer<0> {
  template <typename T>
  void* try_allocate() noexcept {
    return nullptr;
  }
};


template <typename F, unsigned>
struct FUTURES_EMPTY_BASE composer_tag : function_store<F> {
  using function_store<F>::function_store;
  using function_store<F>::function_self;
};

template <typename F, typename G>
struct FUTURES_EMPTY_BASE composer : composer_tag<F, 0>, composer_tag<G, 1> {
  template <typename S, typename T>
  explicit composer(S&& s, T&& t)
      : composer_tag<F, 0>(std::in_place, std::forward<S>(s)),
        composer_tag<G, 1>(std::in_place, std::forward<T>(t)) {}

  composer(composer&&) noexcept = default;

  template <typename... Args>
  auto operator()(Args&&... args) noexcept {
    static_assert(std::is_nothrow_invocable_v<G, Args...>);
    using return_value = std::invoke_result_t<G, Args...>;
    static_assert(std::is_nothrow_invocable_v<F, return_value>);

    return std::invoke(composer_tag<F, 0>::function_self(),
        std::invoke(composer_tag<G, 1>::function_self(),
            std::forward<Args>(args)...));
  }
};

template <typename F, typename G>
auto compose(F&& f, G&& g) {
  return composer<F, G>(std::forward<F>(f), std::forward<G>(g));
}

template <template <typename...> typename T, typename...>
struct unpack_tuple_into;

template <template <typename...> typename T, typename... Vs, typename... Us>
struct unpack_tuple_into<T, std::tuple<Us...>, Vs...> : T<Vs..., Us...> {};

template <template <typename...> typename T, typename... Vs>
inline constexpr auto unpack_tuple_into_v = unpack_tuple_into<T, Vs...>::value;

template <typename>
struct is_tuple : std::false_type {};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};
template <typename T>
inline constexpr auto is_tuple_v = is_tuple<T>::value;

template <typename F, typename T>
struct apply_result;
template <typename F, typename... Ts>
struct apply_result<F, std::tuple<Ts...>> : std::invoke_result<F, Ts...> {};
template <typename F, typename T>
using apply_result_t = typename apply_result<F, T>::type;

template <template <typename...> typename T, typename, typename>
struct is_applicable;
template <template <typename...> typename T, typename F, typename... Ts>
struct is_applicable<T, F, std::tuple<Ts...>> : T<F, Ts...> {};
template <template <typename...> typename T, typename F, typename Tup>
inline constexpr auto is_applicable_v = is_applicable<T, F, Tup>::value;

}

#endif  // FUTURES_GADGETS_H
