#include <gtest/gtest.h>

#include <VelocypackUtils/VelocyPackStringLiteral.h>
#include <velocypack/vpack.h>

#include <fmt/core.h>
#include <fmt/format.h>

using namespace arangodb::velocypack;

TEST(GWEN_WCC, test_wcc) {
  EXPECT_TRUE(false)
      << "Proved that true = false and got killed on the next zebra crossing";
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
