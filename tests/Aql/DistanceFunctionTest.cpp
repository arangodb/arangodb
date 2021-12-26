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
AqlValue createArray(const VPackSlice slice) {
  if (!slice.isArray()) {
    return AqlValue(slice);
  }

  VPackBuilder builder;
  {
    VPackArrayBuilder arrayBuilder(&builder);
    for (const auto arg : VPackArrayIterator(slice)) {
      builder.add(arg);
    }
  }

  return AqlValue(builder.slice());
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

AqlValue evaluateDistanceFunction(const SmallVector<AqlValue>& params,
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

  auto distanceFunction =
      static_cast<arangodb::aql::Function const*>(node.getData());
  return distanceFunction->implementation(&expressionContext, node, params);
}

void assertDistanceFunction(char const* expected, char const* x, char const* y,
                            const arangodb::aql::AstNode& node) {
  // get slice for expected value
  auto const expectedJson = VPackParser::fromJson(expected);
  auto const expectedSlice = expectedJson->slice();
  ASSERT_TRUE(expectedSlice.isArray() || expectedSlice.isNumber());

  // get slice for x value
  auto const jsonX = VPackParser::fromJson(x);
  auto const sliceX = jsonX->slice();

  // create params vector from x slice
  AqlValue arrayX = createArray(sliceX);

  // get slice for y value
  auto const jsonY = VPackParser::fromJson(y);
  auto const sliceY = jsonY->slice();

  // create params vector from y slice
  AqlValue arrayY = createArray(sliceY);

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{2, arena};
  params[0] = arrayX;
  params[1] = arrayY;

  // evaluate
  auto actual_value = evaluateDistanceFunction(params, node);

  // check equality
  expectEqSlices(actual_value.slice(), expectedSlice);

  // destroy AqlValues
  for (auto& p : params) {
    p.destroy();
  }

  actual_value.destroy();
}

void assertDistanceFunctionFail(char const* x, char const* y,
                                const arangodb::aql::AstNode& node) {
  // get slice for x value
  auto const jsonX = VPackParser::fromJson(x);
  auto const sliceX = jsonX->slice();

  // create params vector from x slice
  AqlValue arrayX = createArray(sliceX);

  // get slice for y value
  auto const jsonY = VPackParser::fromJson(y);
  auto const sliceY = jsonY->slice();

  // create params vector from y slice
  AqlValue arrayY = createArray(sliceY);

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{2, arena};
  params[0] = arrayX;
  params[1] = arrayY;

  ASSERT_TRUE(evaluateDistanceFunction(params, node).isNull(false));

  // destroy AqlValues
  for (auto& p : params) {
    p.destroy();
  }
}

TEST(DistanceFuncton, CosineSimilarityTest) {
  // preparing
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  arangodb::aql::Function f("COSINE_SIMILARITY", &Functions::CosineSimilarity);
  node.setData(static_cast<void const*>(&f));

  // correct result
  assertDistanceFunction("0", "[0,1]", "[1,0]", node);
  assertDistanceFunction("0.9769856305801876", "[0.5, -1.23, 0.33]",
                         "[1.0,-3.015,0.1231]", node);
  assertDistanceFunction("-0.00026332365622013654", "[19, 14, -8, 6317]",
                         "[0.89, 0.19, 1000, 1]", node);
  assertDistanceFunction("0.7817515661170301", "[3456, 191, -90, 500, 0.32]",
                         "[713, 201, 508, -0.5, 0.75]", node);
  assertDistanceFunction("-1", "[2]", "[-1]", node);
  assertDistanceFunction("1", "[1]", "[1]", node);
  assertDistanceFunction("-1", "[-1,0]", "[1,0]", node);
  assertDistanceFunction("0", "[0,1,0]", "[1,0,1]", node);
  assertDistanceFunction("0", "[1,1,1,1,1,0]", "[0,0,0,0,0,1]", node);
  assertDistanceFunction("0", "[1,1]", "[-1,1]", node);
  assertDistanceFunction("0", "[1,-1]", "[-1,-1]", node);

  // with matrix
  assertDistanceFunction("[1,1,1,1]",
                         "[[1,1,1,1],[1,1,1,1],[1,1,1,1],[1,1,1,1]]",
                         "[1,1,1,1]", node);
  assertDistanceFunction("[1,1,1]", "[[1,1,1,1],[1,1,1,1],[1,1,1,1]]",
                         "[1,1,1,1]", node);
  assertDistanceFunction("[1,1,1]", "[1,1,1,1]",
                         "[[1,1,1,1],[1,1,1,1],[1,1,1,1]]", node);
  assertDistanceFunction(
      "[0.7071067811865475, 0.7071067811865475, 0.8660254037844387, 0.5]",
      "[[0,1,0,1],[1,0,0,1],[1,1,1,0],[0,0,0,1]]", "[1,1,1,1]", node);
  assertDistanceFunction(
      "[0.7071067811865475, 0.7071067811865475, 0.8660254037844387, 0.5]",
      "[1,1,1,1]", "[[0,1,0,1],[1,0,0,1],[1,1,1,0],[0,0,0,1]]", node);

  // will fail
  assertDistanceFunctionFail("[0]", "[0]", node);
  assertDistanceFunctionFail("[0]", "[1]", node);
  assertDistanceFunctionFail("[1]", "[0]", node);
  assertDistanceFunctionFail("[]", "[]", node);
  assertDistanceFunctionFail("[1]", "[]", node);
  assertDistanceFunctionFail("[]", "[1]", node);
  assertDistanceFunctionFail("[\"one\"]", "[\"zero\"]", node);
  assertDistanceFunctionFail("[true]", "[false]", node);
  assertDistanceFunctionFail("[1]", "0", node);
  assertDistanceFunctionFail("1", "[0]", node);
  assertDistanceFunctionFail("true", "false", node);
  assertDistanceFunctionFail("\"one\"", "\"zero\"", node);

  // with matrix
  assertDistanceFunctionFail("[[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]",
                             "[1,1,1,1]", node);
  assertDistanceFunctionFail("[[1,1,1,1],[1,1,1,1],[1,1,1,1],[1,1,1,1,1]]",
                             "[1,1,1,1]", node);
  assertDistanceFunctionFail("[[1,1,1,1],[1,1,1,1],[1,1,1,1],[1,1,1,1]]",
                             "[1,1,1,1,1]", node);
  assertDistanceFunctionFail("[[1,1,1,1],[1,1,1,1],[1,1,1,1],[1,1,1,1]]",
                             "[1,1,1,true]", node);
  assertDistanceFunctionFail("[[1,1,1,1],[0,0,0,0],[1,1,1,1],[1,1,1,1]]",
                             "[1,1,1,1]", node);
  assertDistanceFunctionFail("[1,1,1,1]",
                             "[[1,1,1,1],[0,0,0,0],[1,1,1,1],[1,1,1,1]]", node);
  assertDistanceFunctionFail("[[1,1,1,1],1,1,1,1,1,1,1,1,1,1,1,1]", "[1,1,1,1]",
                             node);
}

TEST(DistanceFuncton, L1DistanceTest) {
  // preparing
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  arangodb::aql::Function f("L1_DISTANCE", &Functions::L1Distance);
  node.setData(static_cast<void const*>(&f));

  // correct result
  assertDistanceFunction("6", "[-1,-1]", "[2,2]", node);
  assertDistanceFunction("0", "[0,0,0]", "[0,0,0]", node);
  assertDistanceFunction("3", "[-1,0,-1]", "[0,-1,0]", node);
  assertDistanceFunction("3", "[-0.5,0.5,-0.5]", "[0.5,-0.5,0.5]", node);
  assertDistanceFunction("7", "[0,0,0,0,0,0,0]", "[1,1,1,1,1,1,1]", node);
  assertDistanceFunction("1.5", "[1.5]", "[3]", node);

  // with matrix
  assertDistanceFunction("[3,9,9,7]", "[[1,2,3],[-1,-2,-3],[3,4,5],[-5,2,1]]",
                         "[1,1,1]", node);
  assertDistanceFunction("[4,4,4,4]", "[1,1,1,1]",
                         "[[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]", node);

  // will fail with matrix
  assertDistanceFunctionFail("[[1,1,1,1]]", "[[1,1,1,1]]", node);
  assertDistanceFunctionFail("[[1,1,1,1]]", "[[1,1,1,1]]", node);
}

TEST(DistanceFuncton, L2DistanceTest) {
  // preparing
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  arangodb::aql::Function f("L2_DISTANCE", &Functions::L2Distance);
  node.setData(static_cast<void const*>(&f));

  // correct result
  assertDistanceFunction("0", "[0,0]", "[0,0]", node);
  assertDistanceFunction("4.1231056256176606", "[1,1]", "[5,2]", node);
  assertDistanceFunction("1.4142135623730951", "[0,1]", "[1,0]", node);
  assertDistanceFunction("2.449489742783178", "[0,1,0,0,1]", "[2,0,0,0,0]",
                         node);
}

}  // namespace
