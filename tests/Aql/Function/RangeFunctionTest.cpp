#include <gtest/gtest.h>
#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Slice.h>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

struct MockedContext {
  MockedContext() {
    fakeit::When(Method(expressionContextMock, registerWarning))
        .AlwaysDo([](ErrorCode, std::string_view) {});
    fakeit::When(Method(trxCtxMock, getVPackOptions)).AlwaysReturn(&options);
    fakeit::When(Method(trxMock, transactionContextPtr))
        .AlwaysReturn(&trxCtxMock.get());
    fakeit::When(Method(trxMock, vpackOptions)).AlwaysReturn(options);
    fakeit::When(Method(expressionContextMock, trx))
        .AlwaysDo([this]() -> transaction::Methods& { return trxMock.get(); });
  }

  AqlValue evaluate(std::span<AqlValue const> params, AstNode const& node) {
    auto function = static_cast<Function const*>(node.getData());
    return function->implementation(&expressionContextMock.get(), node, params);
  }

  VPackOptions options;
  fakeit::Mock<ExpressionContext> expressionContextMock;
  fakeit::Mock<transaction::Context> trxCtxMock;
  fakeit::Mock<transaction::Methods> trxMock;
};

// start, stop and step are numerator/denom; the expected count is computed in
// integer arithmetic, so it does not depend on the implementation.
void sweep(AstNode const& node, int64_t denom, int64_t limit) {
  MockedContext context;
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

        containers::SmallVector<AqlValue, 4> params;
        params.emplace_back(AqlValue(AqlValueHintDouble(from)));
        params.emplace_back(AqlValue(AqlValueHintDouble(to)));
        params.emplace_back(AqlValue(AqlValueHintDouble(step)));

        AqlValue result = context.evaluate(params, node);
        VPackSlice slice = result.slice();

        std::string const what = "RANGE(" + std::to_string(from) + ", " +
                                 std::to_string(to) + ", " +
                                 std::to_string(step) + ")";
        ASSERT_TRUE(slice.isArray()) << what;
        EXPECT_EQ(static_cast<uint64_t>(expected), slice.length()) << what;
        EXPECT_EQ(from, slice.at(0).getNumber<double>()) << what;

        result.destroy();
        for (auto& p : params) {
          p.destroy();
        }
      }
    }
  }
}

// start and step magnitudes are chosen independently. All three share a
// denominator so the numerators stay exact, and stop is built as
// start + k * step, which makes the expected count k + 1 by construction.
void sweepExponentSpread(AstNode const& node) {
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

  MockedContext context;
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
              // as 4 * ulp in RANGE function) is larger than 'step', which
              // produces extra elements, and the assertion will fail.
              // Intentionally, we cut those cases out of the test since
              // that 'perfect' precision would not be expected by users.
              double const m = std::max(std::abs(from), std::abs(to));
              double const ulp =
                  std::nextafter(m, std::numeric_limits<double>::infinity()) -
                  m;
              if (std::abs(step) < 6.0 * ulp) {
                continue;
              }

              containers::SmallVector<AqlValue, 4> params;
              params.emplace_back(AqlValue(AqlValueHintDouble(from)));
              params.emplace_back(AqlValue(AqlValueHintDouble(to)));
              params.emplace_back(AqlValue(AqlValueHintDouble(step)));

              AqlValue result = context.evaluate(params, node);
              VPackSlice slice = result.slice();

              std::string const what = "RANGE(" + std::to_string(from) + ", " +
                                       std::to_string(to) + ", " +
                                       std::to_string(step) + ")";
              ASSERT_TRUE(slice.isArray()) << what;
              EXPECT_EQ(static_cast<uint64_t>(k + 1), slice.length()) << what;
              EXPECT_EQ(from, slice.at(0).getNumber<double>()) << what;

              result.destroy();
              for (auto& p : params) {
                p.destroy();
              }
            }
          }
        }
      }
    }
  }
}

}  // namespace

// Every single combination among -3.0 ... 3.0 for start, stop, and step
TEST(RangeFunctionTest, everyCombinationOfTenths) {
  AstNode node(NODE_TYPE_FCALL);
  Function f("RANGE", &functions::Range);
  node.setData(static_cast<void const*>(&f));

  sweep(node, 10, 30);
}

// Start and step magnitudes picked independently, 1e-9 up to 9e6
TEST(RangeFunctionTest, independentStartAndStepMagnitudes) {
  AstNode node(NODE_TYPE_FCALL);
  Function f("RANGE", &functions::Range);
  node.setData(static_cast<void const*>(&f));

  sweepExponentSpread(node);
}
