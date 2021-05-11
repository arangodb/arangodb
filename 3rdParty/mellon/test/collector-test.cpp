#include <mellon/collector.h>
#include "test-helper.h"

/*
template <typename T>
struct collect_result {

  using container = std::vector<detail::box<T>>;

  explicit collect_result(container v)
      : values(std::move(v)) {}

  using value_type = T;
  using base_iterator = typename container::iterator ;

  struct iterator {
    explicit iterator(base_iterator iter) : iter(std::move(iter)) {}

    T& operator*() {
      return iter->ref();
    }
    T const& operator*() const {
      return iter->ref();
    }

    T* operator->() {
      return iter->ptr();
    }
    T const* operator->() const {
      return iter->ptr();
    }

    auto& operator++() {
      ++iter;
      return *this;
    }

    friend void swap(iterator a, iterator b) {
      using std::swap;
      swap(a.iter, b.iter);
    }

    auto operator==(iterator const& o) const {
      return iter == o.iter;
    }
    auto operator!=(iterator const& o) const {
      return iter != o.iter;
    }

    using value_type = T;
   private:
    base_iterator iter;
  };

  auto begin() { return iterator{values.begin()}; }
  auto end() { return iterator{values.end()}; }
 private:
  std::vector<detail::box<T>> values;
};*/



template <typename T>
using collector = mellon::collector<T, default_test_tag>;



struct CollectorTest : ::testing::Test {};

TEST_F(CollectorTest, simple_usage) {
  collector<int> c;
  auto [f1, p1] = make_promise<int>();
  try {
    c.push_result([&, f1 = std::ref(f1)] { return std::move(f1.get()); });
    c.push_result([&, f1 = std::ref(f1)] {
      return future<int>{std::in_place, 2};
    });

    auto fut = std::move(c).into_future();
    std::move(p1).fulfill(1);

    std::vector<int> r = std::move(fut).await();

    {
      auto it = r.begin();
      EXPECT_EQ(*it, 1);
      ++it;
      EXPECT_NE(it, r.end());
      EXPECT_EQ(*it, 2);
      ++it;
      EXPECT_EQ(it, r.end());
    }

  } catch (...) {
    std::move(c).for_all([](auto, auto&&) noexcept {
      /* handle errors */
      ADD_FAILURE() << "Should not be executed";
    });
  }
}

TEST_F(CollectorTest, guard_usage) {
  collector<int> c;
  mellon::collect_guard cg(c, [](auto idx, auto&&) noexcept {
    /* handle errors */
    ADD_FAILURE() << "Should not be executed";
  });

  auto [f1, p1] = make_promise<int>();

  c.push_result([&, f1 = std::ref(f1)] { return std::move(f1.get()); });
  c.push_result([&] {
    return future<int>{std::in_place, 2};
  });


  auto fut = std::move(c).into_future();
  std::move(p1).fulfill(1);

  std::vector<int> r = std::move(fut).await();

  {
    auto it = r.begin();
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_NE(it, r.end());
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(it, r.end());
  }
}

TEST_F(CollectorTest, guard_usage_exception) {
  bool was_handled = false;
  bool was_caught = false;
  try {
    collector<int> c;
    mellon::collect_guard cg(c, [&](auto idx, auto&& v) noexcept {
      /* handle errors */
      EXPECT_TRUE(idx == 0);
      EXPECT_EQ(v, 2);
      was_handled = true;
    });

    auto [f1, p1] = make_promise<int>();
    c.push_result([&, f1 = std::ref(f1)] { return std::move(f1.get()); });
    std::move(p1).fulfill(2);

    throw std::runtime_error("whoops");
  } catch (...) {
    was_caught = true;
  }

  ASSERT_TRUE(was_handled);
  ASSERT_TRUE(was_caught);
}
