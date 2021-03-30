#include "test-helper.h"

using namespace expect;

struct FutureTests : testing::Test {
  struct Value {
    static inline unsigned created = 0;
    static inline unsigned move_constructed = 0;
    static inline unsigned destroyed = 0;
    
    Value() noexcept { ++created; }
    Value(Value&&) noexcept { ++move_constructed; }
    Value(Value&) = delete;
    
    ~Value() { ++destroyed; }
    
    friend inline void swap(Value&, Value&) {}
  };
  
  void SetUp() override {
    Value::created = 0;
    Value::move_constructed = 0;
    Value::destroyed = 0;
  }
};


TEST_F(FutureTests, simple_abandon_before) {
  auto&& [future, promise] = mellon::make_promise<int, mellon::default_tag>();
  std::move(promise).abandon();
  EXPECT_DEATH(std::move(future).finally(
                   [&](int x) noexcept { EXPECT_EQ(x, 12); }),
               "");
  std::move(future).abandon();
}

TEST_F(FutureTests, simple_abandon_after) {
  auto&& [future, promise] = mellon::make_promise<int, mellon::default_tag>();
  std::move(future).finally([&](int x) noexcept { EXPECT_EQ(x, 12); });
  EXPECT_DEATH(std::move(promise).abandon(), "");
  std::move(promise).fulfill(12);
}

TEST_F(FutureTests, expected_throw_test) {
  {
    auto&& [future, promise] = make_promise<expected<int>>();

    std::move(promise).throw_exception<std::runtime_error>("test");
    std::move(future)
        .then([&](int x) {
          ADD_FAILURE() << "This should never be executed";
          return 2 * x;
        })
        .catch_error<std::runtime_error>([&](auto&& e) { return 5; })
        .finally([&](expected<int>&& e) noexcept {
          EXPECT_TRUE(e.has_value());
          EXPECT_EQ(e.unwrap(), 5);
        });
  }

  {
    auto&& [future, promise] = make_promise<expected<int>>();

    std::move(future)
        .then([&](int x) {
          ADD_FAILURE() << "This should never be executed";
          return 2 * x;
        })
        .catch_error<std::runtime_error>([&](auto&& e) { return 5; })
        .finally([&](expected<int>&& e) noexcept {
          EXPECT_TRUE(e.has_value());
          EXPECT_EQ(e.unwrap(), 5);
        });

    std::move(promise).throw_exception<std::runtime_error>("test");
  }
}

TEST_F(FutureTests, expected_no_throw_test) {
  auto&& [future, promise] = make_promise<expected<int>>();

  std::move(promise).fulfill(3);
  std::move(future)
      .then([&](int x) { return 2 * x; })
      .catch_error<std::runtime_error>([&](auto&& e) {
        ADD_FAILURE() << "This should never be executed";
        return 5;
      })
      .finally([&](expected<int>&& e) noexcept {
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.unwrap(), 6);
      });
}

TEST_F(FutureTests, fulfill_midway) {
  auto&& [future, promise] = make_promise<int>();

  bool executed_last = false;

  auto f2 = std::move(future).and_then_direct([](int x) noexcept {
    EXPECT_EQ(x, 3);
    return x * 2;
  });

  std::move(promise).fulfill(3);

  std::move(f2).finally([&](int x) noexcept {
    EXPECT_EQ(x, 6);
    executed_last = true;
  });

  EXPECT_TRUE(executed_last);
}

TEST_F(FutureTests, fulfill_in_vector) {
  auto&& [f1, p] = make_promise<int>();

  bool executed_last = false;

  std::vector<future<int>> v;

  v.emplace_back(std::move(f1).and_then([](int x) noexcept {
     return 2 * x;
   }));

  std::move(p).fulfill(3);

  std::move(v.back()).finally([&](int x) noexcept {
    EXPECT_EQ(x, 6);
    executed_last = true;
  });

  EXPECT_TRUE(executed_last);
}

TEST_F(FutureTests, expected_rethrow_nested) {
  auto&& [future, promise] = make_promise<expected<int>>();

  std::move(promise).throw_exception<std::runtime_error>("fail");
  std::move(future)
      .rethrow_nested_if<std::runtime_error, std::logic_error>(
          "exceptions are not allowed")
      .catch_error<std::runtime_error>([](auto&& e) {
        ADD_FAILURE() << "This should never be executed";
        return 1;
      })
      .finally([](expected<int>&& e) noexcept {
        EXPECT_TRUE(e.has_exception<std::logic_error>());
      });
}


template <typename T>
struct convertible_from_T {
  convertible_from_T(T t) : t(std::move(t)) {}
  T& value() { return t; }

 private:
  T t;
};

template <typename T>
struct convertible_into_T {
  virtual operator T() { return t; };
  explicit convertible_into_T(T t) : t(std::move(t)) {}
  T& value() { return t; }

 private:
  T t;
};

TEST_F(FutureTests, as_something) {
  auto&& [future, promise] = make_promise<int>();

  std::move(future).as<convertible_from_T<int>>().finally(
      [](convertible_from_T<int>&& e) noexcept { EXPECT_EQ(e.value(), 10); });

  std::move(promise).fulfill(10);
}

TEST_F(FutureTests, as_something_from) {
  auto&& [future, promise] = make_promise<convertible_into_T<int>>();

  std::move(future).as<int>().finally([](int e) noexcept { EXPECT_EQ(e, 10); });

  std::move(promise).fulfill(10);
}

TEST_F(FutureTests, future_created_with_in_place_value_should_be_non_empty) {
  auto f = mellon::future<int, mellon::default_tag>(std::in_place, 42);
  EXPECT_TRUE(f.holds_inline_value());
  EXPECT_FALSE(f.empty());
  
  auto f2 = mellon::future<int, no_inline_test_tag>(std::in_place, 42);
  EXPECT_FALSE(f2.holds_inline_value());
  EXPECT_FALSE(f2.empty());
}

TEST_F(FutureTests, move_constructor_should_destroy_old_value) {
  using future = mellon::future<Value, mellon::default_tag>;
  {
    future f(std::in_place);
    
    future f2(std::move(f));
    EXPECT_TRUE(f.empty());
    EXPECT_FALSE(f2.empty());
    
    EXPECT_FALSE(f.holds_inline_value());
    EXPECT_TRUE(f2.holds_inline_value());
  }
  
  EXPECT_EQ(1, Value::created);
  EXPECT_EQ(1, Value::move_constructed);
  EXPECT_EQ(2, Value::destroyed);
}

TEST_F(FutureTests, move_assignment_should_destroy_old_value) {
  using future = mellon::future<Value, mellon::default_tag>;
  {
    future f(std::in_place);
    EXPECT_EQ(future::is_value_inlined, f.holds_inline_value());
    EXPECT_FALSE(f.empty());

    auto [f2, p] = mellon::make_promise<Value, mellon::default_tag>();
    EXPECT_FALSE(f2.empty());
    
    f2 = std::move(f);
    EXPECT_TRUE(f.empty());
    EXPECT_FALSE(f2.empty());
    
    EXPECT_FALSE(f.holds_inline_value());
    EXPECT_TRUE(f2.holds_inline_value());
  }
  
  EXPECT_EQ(1, Value::created);
  EXPECT_EQ(1, Value::move_constructed);
  EXPECT_EQ(2, Value::destroyed);
}

TEST_F(FutureTests, swap_with_empty_target_should_call_move_constructor_and_destroy_the_old_value) {
  using future = mellon::future<Value, mellon::default_tag>;
  {
    future f(std::in_place);
    auto [f2, p] = mellon::make_promise<Value, mellon::default_tag>();
    
    EXPECT_TRUE(f.holds_inline_value());
    EXPECT_FALSE(f2.holds_inline_value());
    
    f.swap(f2);
    
    EXPECT_FALSE(f.holds_inline_value());
    EXPECT_TRUE(f2.holds_inline_value());
  }
  
  EXPECT_EQ(1, Value::created);
  EXPECT_EQ(1, Value::move_constructed);
  EXPECT_EQ(2, Value::destroyed);
}

TEST_F(FutureTests, swap_with_empty_source_should_call_move_constructor_and_destroy_the_old_value) {
  using future = mellon::future<Value, mellon::default_tag>;
  {
    future f(std::in_place);
    auto [f2, p] = mellon::make_promise<Value, mellon::default_tag>();
    
    EXPECT_TRUE(f.holds_inline_value());
    EXPECT_FALSE(f2.holds_inline_value());
    
    f2.swap(f);
    
    EXPECT_FALSE(f.holds_inline_value());
    EXPECT_TRUE(f2.holds_inline_value());
  }
  
  EXPECT_EQ(1, Value::created);
  EXPECT_EQ(1, Value::move_constructed);
  EXPECT_EQ(2, Value::destroyed);
}
