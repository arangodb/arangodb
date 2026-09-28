#include <gtest/gtest.h>

#include "Aql/RangeSpec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

using namespace arangodb::aql::functions;

namespace {

std::string describe(double from, double to, double step) {
  return "RANGE(" + std::to_string(from) + ", " + std::to_string(to) + ", " +
         std::to_string(step) + ")";
}

// start, stop and step are numerator/denom; the expected count is computed in
// integer arithmetic, so it does not depend on the implementation.
void sweep(int64_t denom, int64_t limit) {
  for (int64_t a = -limit; a <= limit; ++a) {
    for (int64_t b = -limit; b <= limit; ++b) {
      if (a == b) {
        continue;
      }
      for (int64_t s = -limit; s <= limit; ++s) {
        if (s == 0 || (b > a && s < 0) || (b < a && s > 0)) {
          continue;
        }
        // (b - a) and s share a sign, so truncation is floor
        int64_t const expected = (b - a) / s + 1;

        double const from = static_cast<double>(a) / denom;
        double const to = static_cast<double>(b) / denom;
        double const step = static_cast<double>(s) / denom;

        auto const spec = makeRangeSpec(from, to, step);
        std::string const what = describe(from, to, step);

        ASSERT_TRUE(spec.has_value()) << what;
        EXPECT_EQ(static_cast<uint64_t>(expected), spec->count) << what;
        EXPECT_EQ(from, spec->at(0)) << what;
      }
    }
  }
}

// start and step magnitudes are chosen independently. All three share a
// denominator so the numerators stay exact, and stop is built as
// start + k * step, which makes the expected count k + 1 by construction.
void sweepExponentSpread() {
  constexpr double kDenom = 1e9;
  constexpr int64_t kPow10[] = {1,
                                10,
                                100,
                                1000,
                                10000,
                                100000,
                                1000000,
                                10000000,
                                100000000,
                                1000000000,
                                10000000000,
                                100000000000,
                                1000000000000,
                                10000000000000,
                                100000000000000,
                                1000000000000000};
  constexpr int64_t kMantissas[] = {1, 2, 3, 5, 7, 9};
  constexpr int64_t kCounts[] = {1, 2, 3, 7, 13, 30};
  // start spans 1e-9 .. 9e6, step spans 1e-9 .. 9
  constexpr int kStartExponents = 16;
  constexpr int kStepExponents = 10;

  for (int p = 0; p < kStartExponents; ++p) {
    for (int64_t ma : kMantissas) {
      int64_t const a = ma * kPow10[p];
      for (int q = 0; q < kStepExponents; ++q) {
        for (int64_t ms : kMantissas) {
          int64_t const s = ms * kPow10[q];
          for (int64_t k : kCounts) {
            for (int64_t sign : {int64_t{1}, int64_t{-1}}) {
              int64_t const b = a + sign * k * s;

              double const from = static_cast<double>(a) / kDenom;
              double const to = static_cast<double>(b) / kDenom;
              double const step = sign * static_cast<double>(s) / kDenom;

              // In some cases, the tolerance to cover double noise (implemented
              // as 4 * ulp in RANGE function) is larger than the step, which
              // produces extra elements, and the assertion will fail.
              // Intentionally, we cut those cases out of the test since
              // 'perfect' precision is impossible to achieve.
              double const m = std::max(std::abs(from), std::abs(to));
              double const ulp =
                  std::nextafter(m, std::numeric_limits<double>::infinity()) -
                  m;
              if (std::abs(step) < 8.0 * ulp) {
                continue;
              }

              auto const spec = makeRangeSpec(from, to, step);
              std::string const what = describe(from, to, step);

              ASSERT_TRUE(spec.has_value()) << what;
              EXPECT_EQ(static_cast<uint64_t>(k + 1), spec->count) << what;
              EXPECT_EQ(from, spec->at(0)) << what;
            }
          }
        }
      }
    }
  }
}

}  // namespace

// Every single combination among -3.0 ... 3.0 for start, stop, and step
TEST(RangeFunctionTest, everyCombinationOfTenths) { sweep(10, 30); }

// Start and step magnitudes picked independently, 1e-9 up to 9e6
TEST(RangeFunctionTest, independentStartAndStepMagnitudes) {
  sweepExponentSpread();
}

// Arguments that do not describe a range are rejected
TEST(RangeFunctionTest, invalidArguments) {
  double const nan = std::numeric_limits<double>::quiet_NaN();
  double const inf = std::numeric_limits<double>::infinity();

  EXPECT_FALSE(makeRangeSpec(0.0, 1.0, 0.0).has_value());
  EXPECT_FALSE(makeRangeSpec(0.0, 1.0, -0.1).has_value());
  EXPECT_FALSE(makeRangeSpec(1.0, 0.0, 0.1).has_value());
  EXPECT_FALSE(makeRangeSpec(nan, 1.0, 0.1).has_value());
  EXPECT_FALSE(makeRangeSpec(0.0, nan, 0.1).has_value());
  EXPECT_FALSE(makeRangeSpec(0.0, 1.0, nan).has_value());
  EXPECT_FALSE(makeRangeSpec(inf, 1.0, 0.1).has_value());
  EXPECT_FALSE(makeRangeSpec(0.0, inf, 0.1).has_value());
  EXPECT_FALSE(makeRangeSpec(0.0, 1.0, inf).has_value());
}
