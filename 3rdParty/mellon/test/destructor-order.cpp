#include "test-helper.h"

struct destruction_order : ::testing::Test {
  destruction_order() {
    destructor_recorder.clear();
  }
  static std::string destructor_recorder;
};

std::string destruction_order::destructor_recorder;


struct destruction_probe {
  explicit destruction_probe(std::string_view c) : label(c) {}
  destruction_probe(destruction_probe const& other) : label(other.label + "c") {};
  destruction_probe(destruction_probe && other) noexcept : label(other.label + "m") {};
  ~destruction_probe() {
    if (!label.empty()) {
      destruction_order::destructor_recorder += label;
    }
  }

  std::string label;
};

TEST_F(destruction_order, simple_test) {
  auto [f, p] = make_promise<destruction_probe>();

  std::move(f)
      .and_then_direct([](destruction_probe&& probe) noexcept {
        EXPECT_EQ(probe.label, "Am");
        return destruction_probe{"B"};
      })
      .and_then_direct([](destruction_probe&& probe) noexcept {
        EXPECT_EQ(probe.label, "Bm");
        return destruction_probe{"C"};
      })
      .finally([](destruction_probe&& probe) noexcept {
        EXPECT_EQ(probe.label, "Cm");
      });

  std::move(p).fulfill(destruction_probe{"A"});

  ASSERT_EQ(destructor_recorder, "BAmCBmCmA");
}

// Destruction order of futures is broken
#if 0
TEST_F(destruction_order, simple_test_temporaries) {
  auto [f, p] = make_promise<destruction_probe>();

  std::move(f)
      .and_then([](destruction_probe&& probe) noexcept {
        EXPECT_EQ(probe.label, "Am");
        destruction_order::destructor_recorder += "1";
        return destruction_probe{"B"};
      })
      .and_then([](destruction_probe&& probe) noexcept {
        EXPECT_EQ(probe.label, "B");
        destruction_order::destructor_recorder += "2";
        return destruction_probe{"C"};
      })
      .finally([](destruction_probe&& probe) noexcept {
        EXPECT_EQ(probe.label, "C");
        destruction_order::destructor_recorder += "3";
      });

  std::move(p).fulfill(destruction_probe{"A"});

  ASSERT_EQ(destructor_recorder, "BAmCBmCmA");
}
#endif
