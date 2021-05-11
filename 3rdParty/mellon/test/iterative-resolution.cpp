#include "test-helper.h"

struct destruction_marker {
  destruction_marker() = default;
  explicit destruction_marker(bool* ptr) : ptr(ptr) {}
  destruction_marker(destruction_marker const&) = delete;
  destruction_marker(destruction_marker&& other) noexcept : ptr(nullptr) {
    std::swap(ptr, other.ptr);
  }
  ~destruction_marker() {
    if (ptr) {
      *ptr = true;
    }
  }

 private:
  bool* ptr = nullptr;
};

TEST(iterative_resolution, simple_test) {
  bool first = false;
  bool second = false;
  bool third = false;

  auto&& [f, p] = make_promise<destruction_marker>();

  std::move(f)
      .and_then_direct([&](destruction_marker&& dm) noexcept {
        EXPECT_FALSE(first);
        return destruction_marker(&second);
      })
      .and_then_direct([&](destruction_marker&& dm) noexcept {
        EXPECT_TRUE(first);
        EXPECT_FALSE(second);
        return destruction_marker(&third);
      })
      .finally([&](destruction_marker&& dm) noexcept {
        EXPECT_TRUE(first);
        EXPECT_TRUE(second);
        EXPECT_FALSE(third);
      });

  EXPECT_FALSE(third);
  std::move(p).fulfill(&first);
  EXPECT_TRUE(third);
}

#if 0
// Destruction order of temporaries is broken
TEST(iterative_resolution, simple_test_temporaries) {
  bool first = false;
  bool second = false;
  bool third = false;

  auto&& [f, p] = make_promise<destruction_marker>();

  std::move(f)
      .and_then([&](destruction_marker&& dm) noexcept {
        EXPECT_FALSE(first);
        return destruction_marker(&second);
      })
      .and_then([&](destruction_marker&& dm) noexcept {
        EXPECT_TRUE(first);
        EXPECT_FALSE(second);
        return destruction_marker(&third);
      })
      .finally([&](destruction_marker&& dm) noexcept {
        EXPECT_TRUE(first);
        EXPECT_TRUE(second);
        EXPECT_FALSE(third);
      });

  EXPECT_FALSE(third);
  std::move(p).fulfill(&first);
  EXPECT_TRUE(third);
}
#endif
