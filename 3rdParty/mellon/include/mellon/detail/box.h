#ifndef FUTURES_BOX_H
#define FUTURES_BOX_H
#include <cstddef>

#ifndef FUTURES_EMPTY_BASE
#ifdef _MSC_VER
#define FUTURES_EMPTY_BASE __declspec(empty_bases)
#else
#define FUTURES_EMPTY_BASE
#endif
#endif

namespace mellon::detail {

template <typename T, unsigned = 0>
struct box {
  static_assert(!std::is_reference_v<T>);
  box() noexcept = default;

  template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  explicit box(std::in_place_t,
      Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    emplace(std::forward<Args>(args)...);
  }

  template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  void emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    new (&value) T(std::forward<Args>(args)...);
  }

  void destroy() noexcept(std::is_nothrow_destructible_v<T>) {
    std::destroy_at(ptr());
  }

  T* ptr() { return reinterpret_cast<T*>(&value); }
  T const* ptr() const { return reinterpret_cast<T const*>(&value); }

  T& ref() & { return *ptr(); }
  T const& ref() const& { return *ptr(); }
  T&& ref() && { return std::move(*ptr()); }

  T& operator*() & { return ref(); }
  T const& operator*() const& { return ref(); }
  T&& operator*() && { return std::move(ref()); }

  T* operator->() { return ptr(); }
  T const* operator->() const { return ptr(); }

  template <typename U = T, std::enable_if_t<std::is_move_constructible_v<U>, int> = 0>
  T&& cast_move() {
    return std::move(ref());
  }
  template <typename U = T, std::enable_if_t<!std::is_move_constructible_v<U>, int> = 0>
  T& cast_move() {
    return ref();
  }

 private:
   std::aligned_storage_t<sizeof(T), alignof(T)> value;
};

template <>
struct FUTURES_EMPTY_BASE box<void> {
  box() noexcept = default;
  explicit box(std::in_place_t) noexcept {}
  void emplace() noexcept {}
};

template <typename T, bool inline_value>
struct FUTURES_EMPTY_BASE optional_box : box<void> {
  static constexpr bool stores_value = false;
};
template <typename T>
struct FUTURES_EMPTY_BASE optional_box<T, true> : box<T> {
  using box<T>::box;
  static constexpr bool stores_value = true;
};
}

#endif  // FUTURES_BOX_H
