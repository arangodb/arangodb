#include "test-helper.h"

enum class invocable_ref_qualification {
  NO_QUALIFIER,
  LVALUE_REF_QUALIFIER,
  RVALUE_QUALIFIER,
  NO_QUALIFIER_CONST,
  LVALUE_REF_QUALIFIER_CONST,
  RVALUE_QUALIFIER_CONST,
};

template <invocable_ref_qualification irq>
struct invocable_ref_qualification_t {
  static constexpr auto value = irq;
};

using invocable_ref_qualification_list =
    ::testing::Types<invocable_ref_qualification_t<invocable_ref_qualification::NO_QUALIFIER>,
                     invocable_ref_qualification_t<invocable_ref_qualification::LVALUE_REF_QUALIFIER>,
                     invocable_ref_qualification_t<invocable_ref_qualification::RVALUE_QUALIFIER>,
                     invocable_ref_qualification_t<invocable_ref_qualification::NO_QUALIFIER_CONST>,
                     invocable_ref_qualification_t<invocable_ref_qualification::LVALUE_REF_QUALIFIER_CONST>,
                     invocable_ref_qualification_t<invocable_ref_qualification::RVALUE_QUALIFIER_CONST>>;

enum class constructor_type { NOT_NOEXCEPT, NOEXCEPT, DELETED };

template <constructor_type ct>
struct copy_constructor_trait;
template <>
struct copy_constructor_trait<constructor_type::NOT_NOEXCEPT> {
  copy_constructor_trait() = default;
  copy_constructor_trait(copy_constructor_trait const&) noexcept(false) = default;
  copy_constructor_trait(copy_constructor_trait&&) noexcept = default;
};
template <>
struct copy_constructor_trait<constructor_type::NOEXCEPT> {
  copy_constructor_trait() = default;
  copy_constructor_trait(copy_constructor_trait const&) noexcept = default;
  copy_constructor_trait(copy_constructor_trait&&) noexcept = default;
};
template <>
struct copy_constructor_trait<constructor_type::DELETED> {
  copy_constructor_trait() = default;
  copy_constructor_trait(copy_constructor_trait const&) = delete;
  copy_constructor_trait(copy_constructor_trait&&) noexcept = default;
};

template <constructor_type ct>
struct move_constructor_trait;
template <>
struct move_constructor_trait<constructor_type::NOT_NOEXCEPT> {
  move_constructor_trait() = default;
  move_constructor_trait(move_constructor_trait&&) noexcept(false) = default;
  move_constructor_trait(move_constructor_trait const&) = default;
};
template <>
struct move_constructor_trait<constructor_type::NOEXCEPT> {
  move_constructor_trait() = default;
  move_constructor_trait(move_constructor_trait&&) noexcept = default;
  move_constructor_trait(move_constructor_trait const&) = default;
};
template <>
struct move_constructor_trait<constructor_type::DELETED> {
  move_constructor_trait() = default;
  move_constructor_trait(move_constructor_trait&&) = delete;
  move_constructor_trait(move_constructor_trait const&) = default;
};

template <typename T, invocable_ref_qualification irq, bool is_noexcept_invocable,
          constructor_type copy_constructor, constructor_type move_constructor>
struct invocable_test_object : copy_constructor_trait<copy_constructor>,
                               move_constructor_trait<move_constructor> {
  invocable_test_object() = default;

  template <auto U = irq, std::enable_if_t<U == invocable_ref_qualification::NO_QUALIFIER, int> = 0>
  void operator()(T) noexcept(is_noexcept_invocable) {}
  template <auto U = irq, std::enable_if_t<U == invocable_ref_qualification::LVALUE_REF_QUALIFIER, int> = 0>
  void operator()(T) & noexcept(is_noexcept_invocable) {}
  template <auto U = irq, std::enable_if_t<U == invocable_ref_qualification::RVALUE_QUALIFIER, int> = 0>
  void operator()(T) && noexcept(is_noexcept_invocable) {}
  template <auto U = irq, std::enable_if_t<U == invocable_ref_qualification::NO_QUALIFIER_CONST, int> = 0>
  void operator()(T) const noexcept(is_noexcept_invocable) {}
  template <auto U = irq, std::enable_if_t<U == invocable_ref_qualification::LVALUE_REF_QUALIFIER_CONST, int> = 0>
  void operator()(T) const& noexcept(is_noexcept_invocable) {}
  template <auto U = irq, std::enable_if_t<U == invocable_ref_qualification::RVALUE_QUALIFIER_CONST, int> = 0>
  void operator()(T) const&& noexcept(is_noexcept_invocable) {}
};

template <typename T>
struct identity {
  using type = T;
};

template <typename T, invocable_ref_qualification, typename = void>
struct add_ref_qualifier : identity<T> {};
template <typename T, invocable_ref_qualification irq>
struct add_ref_qualifier<T, irq, std::enable_if_t<irq == invocable_ref_qualification::LVALUE_REF_QUALIFIER>>
    : identity<T&> {};
template <typename T, invocable_ref_qualification irq>
struct add_ref_qualifier<T, irq, std::enable_if_t<irq == invocable_ref_qualification::RVALUE_QUALIFIER>>
    : identity<T&&> {};
template <typename T, invocable_ref_qualification irq>
struct add_ref_qualifier<T, irq, std::enable_if_t<irq == invocable_ref_qualification::NO_QUALIFIER_CONST>>
    : identity<T const> {};
template <typename T, invocable_ref_qualification irq>
struct add_ref_qualifier<T, irq, std::enable_if_t<irq == invocable_ref_qualification::LVALUE_REF_QUALIFIER_CONST>>
    : identity<T const&> {};
template <typename T, invocable_ref_qualification irq>
struct add_ref_qualifier<T, irq, std::enable_if_t<irq == invocable_ref_qualification::RVALUE_QUALIFIER_CONST>>
    : identity<T const&&> {};

template <typename T, invocable_ref_qualification irq>
using add_ref_qualifier_t = typename add_ref_qualifier<T, irq>::type;

template <typename irq_t>
struct CallGenericFinallyTests : testing::Test {};

TYPED_TEST_SUITE(CallGenericFinallyTests, invocable_ref_qualification_list);

TYPED_TEST(CallGenericFinallyTests, invoke_finally) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::NOEXCEPT, constructor_type::NOEXCEPT>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;

  static_assert(std::is_nothrow_invocable_v<as_type, int>);
  auto f = future<int>{std::in_place, 1};
  std::move(f).finally(std::forward<as_type>(invocable));
}

TYPED_TEST(CallGenericFinallyTests, invoke_then) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::NOEXCEPT, constructor_type::NOEXCEPT>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;

  static_assert(std::is_nothrow_invocable_v<as_type, int>);
  auto f = future<expect::expected<int>>{std::in_place, 1};
  std::move(f).then(std::forward<as_type>(invocable)).finally([](auto) noexcept {});
}

TYPED_TEST(CallGenericFinallyTests, invoke_then_bind) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::NOEXCEPT, constructor_type::NOEXCEPT>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;

  static_assert(std::is_nothrow_invocable_v<as_type, int>);
  auto f = future<expect::expected<int>>{std::in_place, 1};
  std::move(f).then(std::forward<as_type>(invocable)).finally([](auto) noexcept {});
}

TYPED_TEST(CallGenericFinallyTests, invoke_finally_no_move) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::NOEXCEPT, constructor_type::DELETED>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;

  static_assert(std::is_nothrow_invocable_v<as_type, int>);
  auto f = future<int>{std::in_place, 1};
  std::move(f).finally(std::forward<as_type>(invocable));
}

TYPED_TEST(CallGenericFinallyTests, invoke_then_no_move) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::NOEXCEPT, constructor_type::DELETED>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;

  static_assert(std::is_nothrow_invocable_v<as_type, int>);
  auto f = future<expect::expected<int>>{std::in_place, 1};
  std::move(f).then(std::forward<as_type>(invocable)).finally([](auto) noexcept {});
}

TYPED_TEST(CallGenericFinallyTests, invoke_then_bind_no_move) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::NOEXCEPT, constructor_type::DELETED>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;

  static_assert(std::is_nothrow_invocable_v<as_type, int>);
  auto f = future<expect::expected<int>>{std::in_place, 1};
  std::move(f).then(std::forward<as_type>(invocable)).finally([](auto) noexcept {});
}

TYPED_TEST(CallGenericFinallyTests, invoke_finally_no_copy) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::DELETED, constructor_type::NOEXCEPT>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;
  // exclude tests that can not work
  if constexpr (std::is_constructible_v<invocable_t, as_type>) {
    static_assert(std::is_nothrow_invocable_v<as_type, int>);
    auto f = future<int>{std::in_place, 1};
    std::move(f).finally(std::forward<as_type>(invocable));
  }
}

TYPED_TEST(CallGenericFinallyTests, invoke_then_no_copy) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::DELETED, constructor_type::NOEXCEPT>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;
  // exclude tests that can not work
  if constexpr (std::is_constructible_v<invocable_t, as_type>) {
    static_assert(std::is_nothrow_invocable_v<as_type, int>);
    auto f = future<expect::expected<int>>{std::in_place, 1};
    std::move(f).then(std::forward<as_type>(invocable)).finally([](auto) noexcept {});
  }
}

TYPED_TEST(CallGenericFinallyTests, invoke_then_bind_no_copy) {
  using invocable_t =
  invocable_test_object<int, TypeParam::value, true, constructor_type::DELETED, constructor_type::NOEXCEPT>;
  invocable_t invocable;
  using as_type = add_ref_qualifier_t<invocable_t, TypeParam::value>;
  // exclude tests that can not work
  if constexpr (std::is_constructible_v<invocable_t, as_type>) {
    static_assert(std::is_nothrow_invocable_v<as_type, int>);
    auto f = future<expect::expected<int>>{std::in_place, 1};
    std::move(f).then(std::forward<as_type>(invocable)).finally([](auto) noexcept {});
  }
}
