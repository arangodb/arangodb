#include "test-helper.h"

template<typename T>
struct InlineValueTests : testing::Test {
  signal_marker reached_last{"reached-last"};
};

using inline_value_test_tags = ::testing::Types<default_test_tag, always_inline_test_tag, no_temporaries_always_inline_test_tag, only_inline_these_types<int>>;
TYPED_TEST_SUITE(InlineValueTests, inline_value_test_tags);

TYPED_TEST(InlineValueTests, fulfilled_promise_inline_temporary) {
  auto f = mellon::future<int, TypeParam>(std::in_place, 12);
  EXPECT_TRUE(f.holds_inline_value());

  auto f2 = std::move(f).and_then([&](int x) noexcept {
    EXPECT_EQ(x, 12);
    return 5;
  });
  EXPECT_TRUE(f2.holds_inline_value());

  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 5);
    this->reached_last.signal();
  });
}

TYPED_TEST(InlineValueTests, fulfilled_promise_inline) {
  auto f = mellon::future<double, TypeParam>(std::in_place, 12);

  mellon::future<int, TypeParam> f2 = std::move(f).and_then([&](double x) noexcept {
    EXPECT_EQ(x, 12);
    return 5;
  });
  EXPECT_TRUE(f2.holds_inline_value());

  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 5);
    this->reached_last.signal();
  });
}
