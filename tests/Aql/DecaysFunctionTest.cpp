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

#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "IResearch/IResearchQueryCommon.h"
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {

// helper functions
SmallVector<AqlValue> create_arg_vec(const VPackSlice slice) {

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{arena};

  for (const auto arg : VPackArrayIterator(slice)) {
    params.emplace_back(AqlValue(arg));
  }

  return params;
}

void expect_eq_slices(const VPackSlice actual_slice, const VPackSlice expected_slice) {
  ASSERT_TRUE((actual_slice.isNumber() && expected_slice.isNumber()) ||
              (actual_slice.isArray() && expected_slice.isArray()));

  if (actual_slice.isArray() && expected_slice.isArray()) {
    VPackValueLength actual_size = actual_slice.length();
    VPackValueLength expected_size = actual_slice.length();
    ASSERT_EQ(actual_size, expected_size);

    double lhs, rhs;
    for(VPackValueLength i = 0; i < actual_size; ++i) {
      lhs = actual_slice.at(i).getNumber<decltype (lhs)>();
      rhs = actual_slice.at(i).getNumber<decltype (rhs)>();
      ASSERT_DOUBLE_EQ(lhs, rhs);
    }
  } else {
    double lhs = actual_slice.getNumber<decltype (lhs)>();
    double rhs = expected_slice.getNumber<decltype (rhs)>();
    ASSERT_DOUBLE_EQ(lhs, rhs);
  }

  return;
}

enum class FunctionType {
  Gauss = 0,
  Exp,
  Linear
};

AqlValue evaluateDecayFunction(const SmallVector<AqlValue>& params, const FunctionType& type ) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();
  fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([](ErrorCode, char const*){ });
  
  VPackOptions options;
  fakeit::Mock<transaction::Context> trxCtxMock;
  fakeit::When(Method(trxCtxMock, getVPackOptions)).AlwaysReturn(&options);
  transaction::Context& trxCtx = trxCtxMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  fakeit::When(Method(trxMock, transactionContextPtr)).AlwaysReturn(&trxCtx);
  fakeit::When(Method(trxMock, vpackOptions)).AlwaysReturn(options);
  transaction::Methods& trx = trxMock.get();
  
  fakeit::When(Method(expressionContextMock, trx)).AlwaysDo([&trx]() -> transaction::Methods& {
    return trx;
  });

  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  switch(type) {
    case FunctionType::Gauss: {
        arangodb::aql::Function f("GAUSS_DECAY", &Functions::GaussDecay);
        node.setData(static_cast<void const*>(&f));
        return Functions::GaussDecay(&expressionContext, node, params);
      }
    case FunctionType::Exp: {
        arangodb::aql::Function f("EXP_DECAY", &Functions::ExpDecay);
        node.setData(static_cast<void const*>(&f));
        return Functions::ExpDecay(&expressionContext, node, params);
      }
    case FunctionType::Linear: {
        arangodb::aql::Function f("LINEAR_DECAY", &Functions::LinearDecay);
        node.setData(static_cast<void const*>(&f));
        return Functions::LinearDecay(&expressionContext, node, params);
      }
  }
}

void assertDecayFunction(char const* expected, char const* args, const FunctionType& type) {

  // get slice for expected value
  auto const expected_json = VPackParser::fromJson(expected);
  auto const expected_slice = expected_json->slice();
  ASSERT_TRUE(expected_slice.isArray() || expected_slice.isNumber());

  // get slice for args value
  auto const args_json = VPackParser::fromJson(args);
  auto const args_slice = args_json->slice();
  ASSERT_TRUE(args_slice.isArray());

  // create params vector from args slice
  SmallVector<AqlValue> params = create_arg_vec(args_slice);

  // evaluate
  auto const actual_value = evaluateDecayFunction(params, type);

  // check equality
  expect_eq_slices(actual_value.slice(), expected_slice);

  return;
}

void assertDecayFunctionFail(char const* args, const FunctionType& type) {
  // get slice for args value
  auto const args_json = VPackParser::fromJson(args);
  auto const args_slice = args_json->slice();
  ASSERT_TRUE(args_slice.isArray());

  // create params vector from args slice
  SmallVector<AqlValue> params = create_arg_vec(args_slice);

  ASSERT_TRUE(evaluateDecayFunction(params, type).isNull(false));
}

TEST(GaussDecayFunctionTest, test) {
  // expecting 1
  assertDecayFunction("1", "[41, 40, 5, 5, 0.5]", FunctionType::Gauss);
  assertDecayFunction("1.0", "[40, 40, 5, 5, 0.5]", FunctionType::Gauss);
  assertDecayFunction("1.0", "[49.987, 49.987, 0.001, 0.001, 0.2]", FunctionType::Gauss);

  assertDecayFunction("1.0", "[49.987, 49.987, 0.000000000000000001, 0.001, 0.2]", FunctionType::Gauss);

  // with offset=0
  assertDecayFunction("0.9840344433634576", "[1, 0, 10, 0, 0.2]", FunctionType::Gauss);
  assertDecayFunction("0.9376509540020155", "[2, 0, 10, 0, 0.2]", FunctionType::Gauss);
  assertDecayFunction("0.668740304976422", "[5, 0, 10, 0, 0.2]", FunctionType::Gauss);
  assertDecayFunction("0.21316171604122283", "[9.8, 0, 10, 0, 0.2]", FunctionType::Gauss);

  // with scale=0.001 (almost zero)
  // also test array input and array output
  assertDecayFunction("[1.0, 1.0, 1e0, 1, 2e-1]", "[[0,1,9.8,10,11], 0, 0.001, 10, 0.2]", FunctionType::Gauss);

  // test array input and array output
  assertDecayFunction("[0.5, 1.0]", "[[20.0, 41], 40, 5, 5, 0.5]", FunctionType::Gauss);

  // expecting decay value
  assertDecayFunction("0.5", "[20, 40, 5, 5, 0.5]", FunctionType::Gauss);
  assertDecayFunction("0.2715403018822964", "[49.9889, 49.987, 0.001, 0.001, 0.2]", FunctionType::Gauss);
  assertDecayFunction("0.1", "[-10, 40, 5, 0, 0.1]", FunctionType::Gauss);

  // incorrect input
  assertDecayFunctionFail("[10, 10, 0.0, 2, 0.2]", FunctionType::Gauss);
  assertDecayFunctionFail("[30, 40, 5]", FunctionType::Gauss);
  assertDecayFunctionFail("[30, 40, 5, 100]", FunctionType::Gauss);
  assertDecayFunctionFail("[30, 40, 5, 100, -100]", FunctionType::Gauss);
  assertDecayFunctionFail("[\"a\", 40, 5, 5, 0.5]", FunctionType::Gauss);
}

TEST(ExpDecayFunctionTest, test) {
  // expecting 1
  assertDecayFunction("1", "[41, 40, 5, 5, 0.5]", FunctionType::Exp);
  assertDecayFunction("1.0", "[40, 40, 5, 5, 0.5]", FunctionType::Exp);
  assertDecayFunction("1.0", "[49.987, 49.987, 0.001, 0.001, 0.2]", FunctionType::Exp);

  // with offset=0
  assertDecayFunction("0.8513399225207846", "[1, 0, 10, 0, 0.2]", FunctionType::Exp);
  assertDecayFunction("0.7247796636776955", "[2, 0, 10, 0, 0.2]", FunctionType::Exp);
  assertDecayFunction("0.447213595499958", "[5, 0, 10, 0, 0.2]", FunctionType::Exp);
  assertDecayFunction("0.20654248397928862", "[9.8, 0, 10, 0, 0.2]", FunctionType::Exp);

  // with scale=0.001 (almost zero)
  assertDecayFunction("1", "[0, 0, 0.001, 10, 0.2]", FunctionType::Exp);
  assertDecayFunction("1", "[1, 0, 0.001, 10, 0.2]", FunctionType::Exp);
  assertDecayFunction("1", "[9.8, 0, 0.001, 10, 0.2]", FunctionType::Exp);
  assertDecayFunction("1", "[10, 0, 0.001, 10, 0.2]", FunctionType::Exp);
  assertDecayFunction("0.2", "[11, 0, 0.001, 10, 0.2]", FunctionType::Exp);

  // expecting decay value
  assertDecayFunction("[0.5, 1.0]", "[[20.0, 41], 40, 5, 5, 0.5]", FunctionType::Exp);
  assertDecayFunction("0.2", "[49.9889, 50, 0.001, 0.001, 0.2]", FunctionType::Exp);
  assertDecayFunction("0.1", "[-10, 40, 5, 0, 0.1]", FunctionType::Exp);

  // incorrect input
  assertDecayFunctionFail("[10, 10, 3, 2, 1]", FunctionType::Exp);
  assertDecayFunctionFail("[30, 40, 5]", FunctionType::Exp);
  assertDecayFunctionFail("[30, 40, 5, 100]", FunctionType::Exp);
  assertDecayFunctionFail("[30, 40, 5, 100, -100]", FunctionType::Exp);
  assertDecayFunctionFail("[\"a\", 40, 5, 5, 0.5]", FunctionType::Exp);
}

TEST(LinDecayFunctionTest, test) {
  // expecting 1
  assertDecayFunction("1", "[41, 40, 5, 5, 0.5]", FunctionType::Exp);
  assertDecayFunction("1.0", "[40, 40, 5, 5, 0.5]", FunctionType::Exp);
  assertDecayFunction("1.0", "[49.987, 49.987, 0.001, 0.001, 0.2]", FunctionType::Exp);

  // with offset=0
  assertDecayFunction("0.92", "[1, 0, 10, 0, 0.2]", FunctionType::Linear);
  assertDecayFunction("0.84", "[2, 0, 10, 0, 0.2]", FunctionType::Linear);
  assertDecayFunction("0.6", "[5, 0, 10, 0, 0.2]", FunctionType::Linear);
  assertDecayFunction("0.21599999999999994", "[9.8, 0, 10, 0, 0.2]", FunctionType::Linear);

  // with scale=0.001 (almost zero)
  assertDecayFunction("[1,1,1,1,0.2]", "[[0,1,5,9.8,10,11], 0, 10, 0, 0.2]", FunctionType::Linear);

  // expecting decay value
  assertDecayFunction("[0.5, 1.0]", "[[20.0, 41], 40, 5, 5, 0.5]", FunctionType::Exp);
  assertDecayFunction("0.2", "[49.9889, 50, 0.001, 0.001, 0.2]", FunctionType::Exp);
  assertDecayFunction("0.1", "[-10, 40, 5, 0, 0.1]", FunctionType::Exp);

  // incorrect input
  assertDecayFunctionFail("[30, 40, 5]", FunctionType::Exp);
  assertDecayFunctionFail("[30, 40, 5, 100]", FunctionType::Exp);
  assertDecayFunctionFail("[30, 40, 5, 100, -100]", FunctionType::Exp);
  assertDecayFunctionFail("[\"a\", 40, 5, 5, 0.5]", FunctionType::Exp);
}

} // namespase
