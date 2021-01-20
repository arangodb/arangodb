#ifndef FUTURES_TEST_HELPER_H
#define FUTURES_TEST_HELPER_H
#include <gtest/gtest.h>
#include <mellon/futures.h>

struct default_test_tag {};
struct no_inline_test_tag {};
struct always_inline_test_tag {};
struct no_temporaries_test_tag {};
struct no_temporaries_always_inline_test_tag {};
template<typename... Ts>
struct only_inline_these_types {};

using my_test_tags =
    ::testing::Types<default_test_tag, no_inline_test_tag, always_inline_test_tag, no_temporaries_test_tag, no_temporaries_always_inline_test_tag>;

template <>
struct mellon::tag_trait<default_test_tag> {
  struct assertion_handler {
    void operator()(bool test) const noexcept {
      if (!test) {
        std::abort();
      }
      ASSERT_TRUE(test);
    }  // TRI_ASSERT(test);
  };

  template <typename T>
  using abandoned_future_handler = mellon::use_default_handler<T>;
  template <typename T>
  using abandoned_promise_handler = mellon::use_default_handler<T>;
};

template <>
struct mellon::tag_trait<no_inline_test_tag> : mellon::tag_trait<default_test_tag> {
  static constexpr auto small_value_size = 0;  // turn off
};
template <>
struct mellon::tag_trait<always_inline_test_tag> : mellon::tag_trait<default_test_tag> {
  static constexpr auto small_value_size = std::numeric_limits<std::size_t>::max();  // turn off
};
template <>
struct mellon::tag_trait<no_temporaries_test_tag> : mellon::tag_trait<default_test_tag> {
  static constexpr bool disable_temporaries = true;
};
template <>
struct mellon::tag_trait<no_temporaries_always_inline_test_tag>
    : mellon::tag_trait<default_test_tag> {
  static constexpr bool disable_temporaries = true;
  static constexpr auto small_value_size = std::numeric_limits<std::size_t>::max();  // turn off
};
template <typename... Ts>
struct mellon::tag_trait<only_inline_these_types<Ts...>>
    : mellon::tag_trait<default_test_tag> {
  static constexpr bool disable_temporaries = true;
  template<typename T>
  static constexpr bool is_type_inlined = (std::is_same_v<T, Ts> || ...);
};

template <typename T>
using future = mellon::future<T, default_test_tag>;
template <typename T>
using promise = mellon::promise<T, default_test_tag>;

template <typename T>
auto make_promise() {
  return mellon::make_promise<T, default_test_tag>();
}

struct constructor_counter_base {
  constructor_counter_base(constructor_counter_base const&) = delete;
  constructor_counter_base(constructor_counter_base&&) noexcept
      : _memory(new int(4)) {}
  constructor_counter_base() noexcept : _memory(new int(4)) {}
  ~constructor_counter_base() {
    auto x = _counter.fetch_sub(1);
    if (x == 0) {
      std::abort();
    }
    delete _memory;
  }

  int* _memory;
  static std::atomic<std::size_t> _counter;
};

template <typename T>
struct constructor_counter : constructor_counter_base {
  static std::atomic<std::size_t> _counter;

  ~constructor_counter() = default;
  template <typename... Args>
  explicit constructor_counter(std::in_place_t, Args&&... args)
      : _value(std::forward<Args>(args)...) {}
  constructor_counter(T v) : _value(std::move(v)) {}
  constructor_counter(constructor_counter const& v) = default;
  constructor_counter(constructor_counter&& v) noexcept = default;
  constructor_counter& operator=(constructor_counter const& v) = default;
  constructor_counter& operator=(constructor_counter&& v) noexcept = default;

  T& operator*() { return _value; }
  T const& operator*() const { return _value; }
  T* operator->() { return &_value; }
  T const* operator->() const { return &_value; }

 private:
  T _value;
};

struct signal_marker {
  explicit signal_marker(const char* name) : name(name) {}

  ~signal_marker() {
    EXPECT_TRUE(signal_marker_was_reached) << "Signal marker was not reached: " << name;
  }

  void signal() noexcept { signal_marker_was_reached = true; }

 private:
  const char* name = nullptr;
  bool signal_marker_was_reached = false;
};

#endif  // FUTURES_TEST_HELPER_H
