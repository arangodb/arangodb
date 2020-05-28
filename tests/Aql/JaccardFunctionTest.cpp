////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/ExpressionContext.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {

AqlValue evaluate(AqlValue const& lhs, AqlValue const& rhs) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();
  fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([](int, char const*){ });

  fakeit::Mock<transaction::Context> trxCtxMock;
  fakeit::When(Method(trxCtxMock, getVPackOptions)).AlwaysDo([](){
    static VPackOptions options;
    return &options;
  });
  transaction::Context& trxCtx = trxCtxMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  fakeit::When(Method(trxMock, transactionContextPtr)).AlwaysReturn(&trxCtx);
  transaction::Methods& trx = trxMock.get();

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{arena};
  params.emplace_back(lhs);
  params.emplace_back(rhs);
  params.emplace_back(VPackSlice::nullSlice()); // redundant argument

  return Functions::Jaccard(&expressionContext, &trx, params);
}

AqlValue evaluate(char const* lhs, char const* rhs) {
  auto const lhsJson = arangodb::velocypack::Parser::fromJson(lhs);
  auto const rhsJson = arangodb::velocypack::Parser::fromJson(rhs);

  AqlValue lhsValue(lhsJson->slice());
  AqlValueGuard lhsGuard(lhsValue, true);
  
  AqlValue rhsValue(rhsJson->slice());
  AqlValueGuard rhsGuard(rhsValue, true);

  return evaluate(lhsValue, rhsValue);
}

void assertJaccardFail(char const* lhs, char const* rhs) {
  ASSERT_TRUE(evaluate(lhs, rhs).isNull(false));
  ASSERT_TRUE(evaluate(rhs, lhs).isNull(false));
}

void assertJaccardFail(char const* lhs, AqlValue const& rhs) {
  auto const lhsJson = arangodb::velocypack::Parser::fromJson(lhs);

  ASSERT_TRUE(evaluate(AqlValue(lhsJson->slice()), rhs).isNull(false));
  ASSERT_TRUE(evaluate(rhs, AqlValue(lhsJson->slice())).isNull(false));
}

void assertJaccard(double_t expectedValue, char const* lhs, char const* rhs) {
  auto assertJaccardCoef = [](
      double_t expectedValue,
      char const* lhs,
      char const* rhs) {
    auto const value = evaluate(lhs, rhs);
    ASSERT_TRUE(value.isNumber());
    bool failed = true;
    const auto actualValue = value.toDouble(failed);
    ASSERT_FALSE(failed);
    ASSERT_EQ(expectedValue, actualValue);
  };

  assertJaccardCoef(expectedValue, lhs, rhs);
  assertJaccardCoef(expectedValue, rhs, lhs);
}

}

TEST(JaccardFunctionTest, test) {
  assertJaccard(1.0, "[]", "[]");
  assertJaccard(1.0, "[null]", "[null]");
  assertJaccard(0.0, "[null]", "[]");
  assertJaccard(0.0, "[null]", "[1]");
  assertJaccard(1.0, "[\"1\", 2, true, null, false]", "[\"1\", 2, true, null, false]");
  assertJaccard(1.0, "[\"1\", 2, true, true, null, null, false, false]", "[\"1\", 2, true, null, false]");
  assertJaccard(0.5, "[\"1\", 3, null, true]", "[\"1\", 2, \"null\", true, 3]");
  assertJaccard(0.5, "[\"1\", 2, \"null\", true, false]", "[\"1\", 2, null, false]");
  assertJaccard(0.25, "[\"1\"]", "[\"1\", 3, null, 4]");
  assertJaccard(0.125, "[1, {}, 2, \"null\", [\"2\"]]", "[\"22\", {}, null, false]");
  assertJaccardFail("{}", "[]");
  assertJaccardFail("\"[]\"", "[]");
  assertJaccardFail("1", "[]");
  assertJaccardFail("null", "[]");
  assertJaccardFail("false", "[]");
  assertJaccardFail("[]", AqlValue(AqlValueHintNull{}));
  assertJaccardFail("[]", AqlValue(AqlValueHintInt{1}));
  assertJaccardFail("[]", AqlValue(AqlValueHintUInt{1}));
  assertJaccardFail("[]", AqlValue(AqlValueHintDouble{1.}));
  assertJaccardFail("[]", AqlValue(AqlValueHintBool(false)));
  assertJaccardFail("[]", AqlValue("foo"));
}
