#include <mellon/sequencer.h>
#include "test-helper.h"

struct SequencerTests : ::testing::Test {
  signal_marker reached_last{"reached-last"};
};


TEST_F(SequencerTests, simple_test) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();

  bool p1_fulfilled = false;
  bool p2_fulfilled = false;
  bool then2_executed = false;

  auto f =
      mellon::sequence(std::move(f1))
          .append([&, f2 = std::move(f2)](int x) mutable noexcept {
            EXPECT_TRUE(p1_fulfilled);
            EXPECT_FALSE(p2_fulfilled);
            if (x != 12) {
              return std::move(f2);
            }
            return future<int>{std::in_place, 12};
          })
          .append([&](int x) noexcept {
            EXPECT_TRUE(p1_fulfilled);
            EXPECT_TRUE(p2_fulfilled);
            then2_executed = true;
            return future<int>{std::in_place, 78};
          })
          .compose();

  std::move(f).finally([this](int&& x) noexcept {
    EXPECT_EQ(x, 78);
    reached_last.signal();
  });

  p1_fulfilled = true;
  std::move(p1).fulfill(13);
  p2_fulfilled = true;
  EXPECT_FALSE(then2_executed);
  std::move(p2).fulfill(37);
  EXPECT_TRUE(then2_executed);
}

TEST_F(SequencerTests, capture_except_test) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();
  auto f =
      mellon::sequence(std::move(f1))
          .append_capture([&, f2 = std::move(f2)](int x) mutable {
            if (x == 12) {
              return std::move(f2);
            }
            throw std::runtime_error("some test error");
          })
          .compose();

  std::move(f).finally([this](expect::expected<int>&& x) noexcept {
    EXPECT_TRUE(x.has_exception<std::runtime_error>());
    reached_last.signal();
  });

  std::move(p1).fulfill(13);
  std::move(p2).fulfill(37);
}

TEST_F(SequencerTests, capture_test) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();
  auto f =
      mellon::sequence(std::move(f1))
          .append_capture([&, f2 = std::move(f2)](int x) mutable {
            if (x == 12) {
              return std::move(f2);
            }
            throw std::runtime_error("some test error");
          })
          .compose();

  std::move(f).finally([this](expect::expected<int>&& x) noexcept {
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x.unwrap(), 37);
    reached_last.signal();
  });

  std::move(p1).fulfill(12);
  std::move(p2).fulfill(37);
}

TEST_F(SequencerTests, capture_then_except_test) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();
  auto f =
      mellon::sequence(std::move(f1))
          .append_capture([&, f2 = std::move(f2)](int x) mutable {
            if (x == 12) {
              return std::move(f2);
            }
            throw std::runtime_error("some test error");
          })
          .then_do([&](int x) {
            ADD_FAILURE() << "This should never be executed";
            return future<int>{std::in_place, 78};
          })
          .compose();

  std::move(f).finally([this](expect::expected<int>&& x) noexcept {
    EXPECT_TRUE(x.has_exception<std::runtime_error>());
    reached_last.signal();
  });

  std::move(p1).fulfill(13);
  std::move(p2).fulfill(37);
}

TEST_F(SequencerTests, capture_then_test) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();
  auto f =
      mellon::sequence(std::move(f1))
          .append_capture([&, f2 = std::move(f2)](int x) mutable {
            if (x == 12) {
              return std::move(f2);
            }
            throw std::runtime_error("some test error");
          })
          .then_do([&](int x) {
            return future<int>{std::in_place, 78};
          })
          .compose();

  std::move(f).finally([this](expect::expected<int>&& x) noexcept {
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x.unwrap(), 78);
    reached_last.signal();
  });

  std::move(p1).fulfill(12);
  std::move(p2).fulfill(37);
}

TEST_F(SequencerTests, capture_then_tuple_except_test) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();
  auto [f3, p3] = make_promise<int>();

  auto f =
      mellon::sequence(std::move(f1))
          .append_capture([&, f2 = std::move(f2), f3 = std::move(f3)](int x) mutable {
            if (x == 12) {
              return mellon::collect(std::move(f2), std::move(f3)).as<expect::expected<std::tuple<int, int>>>();
            }
            throw std::runtime_error("some test error");
          })
          .then_do([&](int x, int y) {
            ADD_FAILURE() << "This should never be executed";
            return future<int>{std::in_place, 78};
          })
          .compose();

  std::move(f).finally([this](expect::expected<int>&& x) noexcept {
    EXPECT_TRUE(x.has_exception<std::runtime_error>());
    reached_last.signal();
  });

  std::move(p1).fulfill(13);
  std::move(p2).fulfill(37);
}

TEST_F(SequencerTests, capture_then_tuple_test) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();
  auto [f3, p3] = make_promise<int>();

  auto f =
      mellon::sequence(std::move(f1))
          .append_capture([&, f2 = std::move(f2), f3 = std::move(f3)](int x) mutable {
            if (x == 12) {
              return mellon::collect(std::move(f2), std::move(f3)).as<expect::expected<std::tuple<int, int>>>();
            }
            throw std::runtime_error("some test error");
          })
          .then_do([&](int x, int y) {
            EXPECT_EQ(x, 37);
            EXPECT_EQ(y, 24);
            return future<int>{std::in_place, 78};
          })
          .compose();

  std::move(f).finally([this](expect::expected<int>&& x) noexcept {
    EXPECT_TRUE(x.has_exception<std::runtime_error>());
    reached_last.signal();
  });

  std::move(p3).fulfill(24);
  std::move(p1).fulfill(13);
  std::move(p2).fulfill(37);
}

TEST_F(SequencerTests, capture_then_multi_res) {
  auto [f1, p1] = make_promise<int>();
  auto [f2, p2] = make_promise<int>();
  auto [f3, p3] = make_promise<int>();

  auto f =
      mellon::sequence(std::move(f1))
          .append_capture([&, f2 = std::move(f2), f3 = std::move(f3)](int x) mutable {
            if (x == 12) {
              return mellon::mr(std::move(f2), std::move(f3), 56);
            }
            throw std::runtime_error("some test error");
          })
          .then_do([&](int x, int y, int z) {
            EXPECT_EQ(x, 37);
            EXPECT_EQ(y, 24);
            EXPECT_EQ(z, 56);
            return future<int>{std::in_place, 78};
          })
          .compose();

  std::move(f).finally([this](expect::expected<int>&& x) noexcept {
    EXPECT_TRUE(x.has_exception<std::runtime_error>());
    reached_last.signal();
  });

  std::move(p3).fulfill(24);
  std::move(p1).fulfill(13);
  std::move(p2).fulfill(37);
}
