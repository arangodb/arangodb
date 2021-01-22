#include "test-helper.h"

struct SwapTests : testing::Test {
};


TEST_F(SwapTests, swap_promise) {

  signal_marker reached_last_1{"reached-last-1"};
  signal_marker reached_last_2{"reached-last-2"};

  auto&& [f1, p1] = make_promise<int>();
  auto&& [f2, p2] = make_promise<int>();

  using std::swap;
  swap(p1, p2);

  std::move(f1).finally([&](int x) noexcept {
    EXPECT_EQ(x, 2);
    reached_last_1.signal();
  });
  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 1);
    reached_last_2.signal();
  });

  std::move(p1).fulfill(1);
  std::move(p2).fulfill(2);
}

TEST_F(SwapTests, swap_futures) {

  signal_marker reached_last_1{"reached-last-1"};
  signal_marker reached_last_2{"reached-last-2"};

  auto&& [f1, p1] = make_promise<int>();
  auto&& [f2, p2] = make_promise<int>();

  using std::swap;
  swap(f1, f2);

  std::move(f1).finally([&](int x) noexcept {
    EXPECT_EQ(x, 2);
    reached_last_1.signal();
  });
  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 1);
    reached_last_2.signal();
  });

  std::move(p1).fulfill(1);
  std::move(p2).fulfill(2);
}


TEST_F(SwapTests, swap_futures_with_inline_value) {

  signal_marker reached_last_1{"reached-last-1"};
  signal_marker reached_last_2{"reached-last-2"};

  auto f1 = future<int>{std::in_place, 1};
  auto f2 = future<int>{std::in_place, 2};

  static_assert(std::is_nothrow_swappable_v<future<int>>);

  using std::swap;
  swap(f1, f2);

  std::move(f1).finally([&](int x) noexcept {
    EXPECT_EQ(x, 2);
    reached_last_1.signal();
  });
  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 1);
    reached_last_2.signal();
  });
}

struct move_constructor_tester {
  ~move_constructor_tester() { constructor_counter -= 1; }
  explicit move_constructor_tester(int x) : x(x) { constructor_counter += 1; }
  move_constructor_tester(move_constructor_tester&& o) noexcept : x(o.x), was_move_constructed(true) { constructor_counter += 1; }
  move_constructor_tester& operator=(move_constructor_tester&& o) noexcept {
    x = o.x;
    return *this;
  }
  int x;
  const bool was_move_constructed = false;
  static std::size_t constructor_counter;
};

std::size_t move_constructor_tester::constructor_counter = 0;

TEST_F(SwapTests, swap_futures_with_one_inline_value) {

  signal_marker reached_last_1{"reached-last-1"};
  signal_marker reached_last_2{"reached-last-2"};

  auto&& [f1, p1] = make_promise<int>();
  auto f2 = future<int>{std::in_place, 2};

  static_assert(std::is_nothrow_swappable_v<future<int>>);

  using std::swap;
  swap(f1, f2);

  std::move(f1).finally([&](int x) noexcept {
    EXPECT_EQ(x, 2);
    reached_last_1.signal();
  });
  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 1);
    reached_last_2.signal();
  });

  std::move(p1).fulfill(1);
  ASSERT_EQ(move_constructor_tester::constructor_counter, 0);
}

TEST_F(SwapTests, swap_futures_with_one_inline_value_move_test) {

  signal_marker reached_last_1{"reached-last-1"};
  signal_marker reached_last_2{"reached-last-2"};

  auto&& [f1, p1] = make_promise<move_constructor_tester>();
  auto f2 = future<move_constructor_tester>{std::in_place, 2};

  using std::swap;
  swap(f1, f2);

  std::move(f1).finally([&](move_constructor_tester x) noexcept {
    EXPECT_EQ(x.x, 2);
    EXPECT_TRUE(x.was_move_constructed);
    reached_last_1.signal();
  });
  std::move(f2).finally([&](move_constructor_tester x) noexcept {
    EXPECT_EQ(x.x, 1);
    reached_last_2.signal();
  });

  std::move(p1).fulfill(1);
  ASSERT_EQ(move_constructor_tester::constructor_counter, 0);
}

TEST_F(SwapTests, swap_futures_with_inline_value_move_test) {

  signal_marker reached_last_1{"reached-last-1"};
  signal_marker reached_last_2{"reached-last-2"};

  auto f1 = future<move_constructor_tester>{std::in_place, 1};
  auto f2 = future<move_constructor_tester>{std::in_place, 2};

  using std::swap;
  swap(f1, f2);

  std::move(f1).finally([&](move_constructor_tester x) noexcept {
    EXPECT_EQ(x.x, 2);
    EXPECT_TRUE(x.was_move_constructed);
    reached_last_1.signal();
  });
  std::move(f2).finally([&](move_constructor_tester x) noexcept {
    EXPECT_EQ(x.x, 1);
    EXPECT_TRUE(x.was_move_constructed);
    reached_last_2.signal();
  });
  ASSERT_EQ(move_constructor_tester::constructor_counter, 0);
}


struct swap_tester {
  ~swap_tester() { constructor_counter -= 1; }
  explicit swap_tester(int x) : x(x) { constructor_counter += 1; }
  swap_tester(swap_tester&& o) noexcept : x(o.x), was_swapped(o.was_swapped) { constructor_counter += 1; }
  // This call has intentionally no move operator= to ensure that it does not use
  // the default swap algorithm.
  int x;
  bool was_swapped = false;
  static std::size_t constructor_counter;
  friend void swap(swap_tester& a, swap_tester& b) noexcept {
    std::swap(a.x, b.x);
    a.was_swapped = true;
    b.was_swapped = true;
  }
};

std::size_t swap_tester::constructor_counter = 0;


TEST_F(SwapTests, swap_futures_with_inline_value_swap_test) {

  signal_marker reached_last_1{"reached-last-1"};
  signal_marker reached_last_2{"reached-last-2"};

  auto f1 = future<swap_tester>{std::in_place, 1};
  auto f2 = future<swap_tester>{std::in_place, 2};

  using std::swap;
  swap(f1, f2);

  std::move(f1).finally([&](swap_tester x) noexcept {
    EXPECT_EQ(x.x, 2);
    EXPECT_TRUE(x.was_swapped);
    reached_last_1.signal();
  });
  std::move(f2).finally([&](swap_tester x) noexcept {
    EXPECT_EQ(x.x, 1);
    EXPECT_TRUE(x.was_swapped);
    reached_last_2.signal();
  });
  ASSERT_EQ(move_constructor_tester::constructor_counter, 0);
}
