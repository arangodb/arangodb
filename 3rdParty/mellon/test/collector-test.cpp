#include "test-helper.h"

namespace mellon {
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

template <typename T, typename Tag>
struct collector {
  ~collector() {
    detail::tag_trait_helper<Tag>::assert_true(
        futures.empty(), "a collector was not finalized properly");
  }

  using result_type = std::vector<T>;

  void push_back(future<T, Tag>&& f) { futures.push_back(std::move(f)); }

  auto into_future() && -> future<result_type, Tag> {
    auto&& [f, p] = make_promise<result_type, Tag>();
    auto ctx = std::make_shared<context>(std::move(p), futures.size());

    std::move(*this).for_all([ctx](std::size_t idx, T&& r) noexcept {
      ctx->fulfill(idx, std::move(r));
    });

    return std::move(f);
  }

  template <typename F, typename Func = std::decay_t<F>,
            std::enable_if_t<std::is_nothrow_invocable_r_v<void, Func, std::size_t, T>, int> = 0>
  void for_all(F f) && {
    static_assert(std::is_copy_constructible_v<Func>);
    static_assert(std::is_nothrow_move_constructible_v<Func>);
    for (size_t i = 0; i < futures.size(); i++) {
      std::move(futures[i]).finally([f = f, i](T&& v) noexcept {
        std::invoke(f, i, std::move(v));
      });
    }
    futures.clear();
  }

  [[nodiscard]] auto empty() const noexcept -> bool { return futures.empty(); }

 private:
  struct context : std::enable_shared_from_this<context> {
    explicit context(promise<result_type, Tag>&& p, std::size_t size)
        : out_promise(std::move(p)), results(size, detail::box<T>{}) {}
    promise<result_type, Tag> out_promise;
    std::vector<detail::box<T>> results;

    ~context() {
      result_type v;
      v.reserve(results.size());

      for (auto&& r : results) {
        static_assert(std::is_nothrow_move_constructible_v<T>);
        v.emplace_back(r.cast_move());
        r.destroy();
      }
      std::move(out_promise).fulfill(std::move(v));
    }

    void fulfill(std::size_t idx, T&& v) noexcept {
      results[idx].emplace(std::move(v));
    }
  };

  std::vector<future<T, Tag>> futures;
};

}  // namespace mellon

template <typename T>
using collector = mellon::collector<T, default_test_tag>;

template <typename C, typename F>
struct collect_guard : F {
  collect_guard(C& c, F f) : F(f), c(c) {}
  ~collect_guard() {
    if (!c.empty()) {
      std::move(c).for_all(function_self());
    }
  }

 private:
  F& function_self() { return *this; }

  C& c;
};

struct CollectorTest : ::testing::Test {};

TEST_F(CollectorTest, simple_usage) {
  collector<int> c;
  auto [f1, p1] = make_promise<int>();
  try {
    c.push_back(std::move(f1));

    future<int> f2{std::in_place, 2};
    c.push_back(std::move(f2));

    auto fut = std::move(c).into_future();
    std::move(p1).fulfill(1);

    std::vector<int> r = std::move(fut).await(mellon::yes_i_know_that_this_call_will_block);

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
  collect_guard cg(c, [](auto idx, auto&&) noexcept {
    /* handle errors */
    ADD_FAILURE() << "Should not be executed";
  });

  auto [f1, p1] = make_promise<int>();
  c.push_back(std::move(f1));

  future<int> f2{std::in_place, 2};
  c.push_back(std::move(f2));

  auto fut = std::move(c).into_future();
  std::move(p1).fulfill(1);

  std::vector<int> r = std::move(fut).await(mellon::yes_i_know_that_this_call_will_block);

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
    collect_guard cg(c, [&](auto idx, auto&& v) noexcept {
      /* handle errors */
      EXPECT_TRUE(idx == 0);
      EXPECT_EQ(v, 2);
      was_handled = true;
    });

    auto [f1, p1] = make_promise<int>();
    c.push_back(std::move(f1));
    std::move(p1).fulfill(2);

    throw std::runtime_error("whoops");
  } catch (...) {
    was_caught = true;
  }

  ASSERT_TRUE(was_handled);
  ASSERT_TRUE(was_caught);
}
