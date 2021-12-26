////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "IResearch/IResearchQueryCommon.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {

// helper functions
SmallVector<AqlValue> createArgVec(const VPackSlice slice) {
  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{arena};

  for (const auto arg : VPackArrayIterator(slice)) {
    if (arg.isObject()) {
      // range
      int64_t low = arg.get("low").getNumber<decltype(low)>();
      int64_t high = arg.get("high").getNumber<decltype(high)>();
      params.emplace_back(AqlValue(low, high));
    } else {
      params.emplace_back(AqlValue(arg));
    }
  }

  return params;
}

void expectEqSlices(const VPackSlice actualSlice,
                    const VPackSlice expectedSlice) {
  ASSERT_TRUE((actualSlice.isNumber() && expectedSlice.isNumber()) ||
              (actualSlice.isArray() && expectedSlice.isArray()));

  if (actualSlice.isArray() && expectedSlice.isArray()) {
    VPackValueLength actualSize = actualSlice.length();
    VPackValueLength expectedSize = expectedSlice.length();
    ASSERT_EQ(actualSize, expectedSize);

    double lhs, rhs;
    for (VPackValueLength i = 0; i < actualSize; ++i) {
      lhs = actualSlice.at(i).getNumber<decltype(lhs)>();
      rhs = expectedSlice.at(i).getNumber<decltype(rhs)>();
      ASSERT_DOUBLE_EQ(lhs, rhs);
    }
  } else {
    double lhs = actualSlice.getNumber<decltype(lhs)>();
    double rhs = expectedSlice.getNumber<decltype(rhs)>();
    ASSERT_DOUBLE_EQ(lhs, rhs);
  }

  return;
}

AqlValue evaluateDecayFunction(const SmallVector<AqlValue>& params,
                               const arangodb::aql::AstNode& node) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();
  fakeit::When(Method(expressionContextMock, registerWarning))
      .AlwaysDo([](ErrorCode, char const*) {});

  VPackOptions options;
  fakeit::Mock<transaction::Context> trxCtxMock;
  fakeit::When(Method(trxCtxMock, getVPackOptions)).AlwaysReturn(&options);
  transaction::Context& trxCtx = trxCtxMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  fakeit::When(Method(trxMock, transactionContextPtr)).AlwaysReturn(&trxCtx);
  fakeit::When(Method(trxMock, vpackOptions)).AlwaysReturn(options);
  transaction::Methods& trx = trxMock.get();

  fakeit::When(Method(expressionContextMock, trx))
      .AlwaysDo([&trx]() -> transaction::Methods& { return trx; });

  auto decayFunction =
      static_cast<arangodb::aql::Function const*>(node.getData());
  return decayFunction->implementation(&expressionContext, node, params);
}

void assertDecayFunction(char const* expected, char const* args,
                         const arangodb::aql::AstNode& node) {
  // get slice for expected value
  auto const expectedJson = VPackParser::fromJson(expected);
  auto const expectedSlice = expectedJson->slice();
  ASSERT_TRUE(expectedSlice.isArray() || expectedSlice.isNumber());

  // get slice for args value
  auto const argsJson = VPackParser::fromJson(args);
  auto const argsSlice = argsJson->slice();
  ASSERT_TRUE(argsSlice.isArray());

  // create params vector from args slice
  SmallVector<AqlValue> params = createArgVec(argsSlice);

  // evaluate
  auto actual_value = evaluateDecayFunction(params, node);

  // check equality
  expectEqSlices(actual_value.slice(), expectedSlice);

  // destroy AqlValues
  for (auto& p : params) {
    p.destroy();
  }

  actual_value.destroy();
}

void assertDecayFunctionFail(char const* args,
                             const arangodb::aql::AstNode& node) {
  // get slice for args value
  auto const argsJson = VPackParser::fromJson(args);
  auto const argsSlice = argsJson->slice();
  ASSERT_TRUE(argsSlice.isArray());

  // create params vector from args slice
  SmallVector<AqlValue> params = createArgVec(argsSlice);

  ASSERT_TRUE(evaluateDecayFunction(params, node).isNull(false));

  // destroy AqlValues
  for (auto& p : params) {
    p.destroy();
  }
}

TEST(GaussDecayFunctionTest, test) {
  // preparing
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  arangodb::aql::Function f("DECAY_GAUSS", &Functions::DecayGauss);
  node.setData(static_cast<void const*>(&f));

  // expecting 1
  assertDecayFunction("1", "[41, 40, 5, 5, 0.7]", node);
  assertDecayFunction("1.0", "[40, 40, 5, 5, 0.5]", node);
  assertDecayFunction("1.0", "[49.987, 49.987, 0.001, 0.001, 0.2]", node);

  // test range input
  assertDecayFunction(
      "[0.5, 0.6417129487814521, 0.7791645796604999, 0.8950250709279725, "
      "0.9726549474122855, 1.0, "
      "0.9726549474122855, 0.8950250709279725, 0.7791645796604999, "
      "0.6417129487814521, 0.5, 0.36856730432277535, 0.2570284566640167]",
      "[{\"low\":-5, \"high\":7}, 0, 5, 0, 0.5]", node);

  assertDecayFunction(
      "1.0", "[49.987, 49.987, 0.000000000000000001, 0.001, 0.2]", node);

  // with offset=0
  assertDecayFunction("0.9840344433634576", "[1, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.9376509540020155", "[2, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.668740304976422", "[5, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.21316171604122283", "[9.8, 0, 10, 0, 0.2]", node);

  // with scale=0.001 (almost zero)
  // also test array input and array output
  assertDecayFunction("[1.0, 1.0, 1e0, 1, 0.0]",
                      "[[0,1,9.8,10,11], 0, 0.001, 10, 0.2]", node);

  // test array input and array output
  assertDecayFunction("[0.0019531250000000017, 1.0]",
                      "[[20.0, 41], 40, 5, 5, 0.5]", node);

  assertDecayFunction("0.0019531250000000017", "[20, 40, 5, 5, 0.5]", node);
  assertDecayFunction("0.2715403018822964",
                      "[49.9889, 49.987, 0.001, 0.001, 0.2]", node);
  assertDecayFunction("1.0000000000000458e-100", "[-10, 40, 5, 0, 0.1]", node);

  // incorrect input
  assertDecayFunctionFail("[10, 10, 0.0, 2, 0.2]", node);
  assertDecayFunctionFail("[30, 40, 5]", node);
  assertDecayFunctionFail("[30, 40, 5, 100]", node);
  assertDecayFunctionFail("[30, 40, 5, 100, -100]", node);
  assertDecayFunctionFail("[\"a\", 40, 5, 5, 0.5]", node);
}

TEST(ExpDecayFunctionTest, test) {
  // preparing
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  arangodb::aql::Function f("DECAY_EXP", &Functions::DecayExp);
  node.setData(static_cast<void const*>(&f));

  // expecting 1
  assertDecayFunction("1", "[41, 40, 5, 5, 0.7]", node);
  assertDecayFunction("1.0", "[40, 40, 5, 5, 0.5]", node);
  assertDecayFunction("1.0", "[49.987, 49.987, 0.001, 0.001, 0.2]", node);

  // with offset=0
  assertDecayFunction("0.8513399225207846", "[1, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.7247796636776955", "[2, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.447213595499958", "[5, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.20654248397928862", "[9.8, 0, 10, 0, 0.2]", node);

  // with scale=0.001 (almost zero)
  assertDecayFunction("1", "[0, 0, 0.001, 10, 0.2]", node);
  assertDecayFunction("1", "[1, 0, 0.001, 10, 0.2]", node);
  assertDecayFunction("1", "[9.8, 0, 0.001, 10, 0.2]", node);
  assertDecayFunction("1", "[10, 0, 0.001, 10, 0.2]", node);
  assertDecayFunction("0.0", "[11, 0, 0.001, 10, 0.2]", node);

  // test range input
  assertDecayFunction(
      "[0.5, 0.5743491774985175, 0.6597539553864472, 0.7578582832551991, "
      "0.8705505632961241, 1.0, 0.8705505632961241, "
      "0.7578582832551991, 0.6597539553864472, 0.5743491774985175, 0.5, "
      "0.4352752816480621, 0.37892914162759955]",
      "[{\"low\":-5, \"high\":7}, 0, 5, 0, 0.5]", node);

  assertDecayFunction("[0.12500000000000003, 1.0]",
                      "[[20.0, 41], 40, 5, 5, 0.5]", node);
  assertDecayFunction("8.717720806626885e-08",
                      "[49.9889, 50, 0.001, 0.001, 0.2]", node);
  assertDecayFunction("9.999999999999996e-11", "[-10, 40, 5, 0, 0.1]", node);

  // incorrect input
  assertDecayFunctionFail("[10, 10, 3, 2, 1]", node);
  assertDecayFunctionFail("[30, 40, 5]", node);
  assertDecayFunctionFail("[30, 40, 5, 100]", node);
  assertDecayFunctionFail("[30, 40, 5, 100, -100]", node);
  assertDecayFunctionFail("[\"a\", 40, 5, 5, 0.5]", node);
}

TEST(LinDecayFunctionTest, test) {
  // preparing
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  arangodb::aql::Function f("DECAY_LINEAR", &Functions::DecayLinear);
  node.setData(static_cast<void const*>(&f));

  // expecting 1
  assertDecayFunction("1", "[41, 40, 5, 5, 0.5]", node);
  assertDecayFunction("1.0", "[40, 40, 5, 5, 0.5]", node);
  assertDecayFunction("1.0", "[49.987, 49.987, 0.001, 0.001, 0.2]", node);

  // with offset=0
  assertDecayFunction("0.92", "[1, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.84", "[2, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.6", "[5, 0, 10, 0, 0.2]", node);
  assertDecayFunction("0.21599999999999994", "[9.8, 0, 10, 0, 0.2]", node);

  // with scale=0.001 (almost zero)
  // also test array input and array output
  assertDecayFunction("[1,1,1,1,0]", "[[0,1,9.8,10,11], 0, 0.001, 10, 0.2]",
                      node);

  // test range input
  assertDecayFunction(
      "[0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3]",
      "[{\"low\":-5, \"high\":7}, 0, 5, 0, 0.5]", node);

  assertDecayFunction("[0, 1.0]", "[[20.0, 41], 40, 5, 5, 0.5]", node);
  assertDecayFunction("0", "[49.9889, 50, 0.001, 0.001, 0.2]", node);
  assertDecayFunction("0", "[-10, 40, 5, 0, 0.1]", node);

  // incorrect input
  assertDecayFunctionFail("[30, 40, 5]", node);
  assertDecayFunctionFail("[30, 40, 5, 100]", node);
  assertDecayFunctionFail("[30, 40, 5, 100, -100]", node);
  assertDecayFunctionFail("[\"a\", 40, 5, 5, 0.5]", node);
}

}  // namespace
