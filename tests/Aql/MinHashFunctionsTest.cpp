////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "IResearch/common.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {

AqlValue evaluateFunc(std::span<const AqlValue> args,
                      arangodb::aql::Function const& f) {
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

  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));

  return f.implementation(&expressionContext, node, args);
}

std::vector<AqlValue> buildArgs(char const* args) {
  EXPECT_NE(nullptr, args);
  auto const argsJson = arangodb::velocypack::Parser::fromJson(args);

  auto const argsSlice = argsJson->slice();
  std::vector<AqlValue> params;
  params.reserve(argsSlice.length());
  for (auto arg : velocypack::ArrayIterator{argsSlice}) {
    params.emplace_back(arg);
  }

  return params;
}

void assertMinHashError(double_t expected, char const* args) {
  arangodb::aql::Function f("MINHASH_ERROR", &functions::MinHashError);
  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  auto const res = evaluateFunc(params, f);

  ASSERT_TRUE(res.isNumber());
  ASSERT_DOUBLE_EQ(expected, res.toDouble());
}

void assertMinHashErrorFail(char const* args) {
  arangodb::aql::Function f("MINHASH_ERROR", &functions::MinHashError);

  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  auto const res = evaluateFunc(params, f);

  ASSERT_TRUE(res.isNull(false));
}

void assertMinHashCount(size_t expected, char const* args) {
  arangodb::aql::Function f("MINHASH_COUNT", &functions::MinHashCount);

  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  auto const res = evaluateFunc(params, f);

  ASSERT_TRUE(res.isNumber());
  ASSERT_EQ(expected, res.toInt64());
}

void assertMinHashCountFail(char const* arg) {
  ASSERT_NE(nullptr, arg);
  auto const argJson = arangodb::velocypack::Parser::fromJson(arg);
  arangodb::aql::AqlValue argValue{argJson->slice()};
  arangodb::aql::AqlValueGuard argGuard{argValue, true};

  arangodb::aql::Function f("MINHASH_COUNT", &functions::MinHashCount);

  containers::SmallVector<AqlValue, 2> params;
  params.emplace_back(arg);
  params.emplace_back(VPackSlice::nullSlice());  // redundant argument

  auto const res = evaluateFunc(params, f);

  ASSERT_TRUE(res.isNull(false));
}

void assertMinHashFail(char const* args) {
  arangodb::aql::Function f("MINHASH", &functions::MinHash);

  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  auto const res = evaluateFunc(params, f);

  ASSERT_TRUE(res.isNull(false));
}

void assertMinHash(char const* expected, char const* args) {
  ASSERT_NE(nullptr, expected);
  auto const expectedJson = arangodb::velocypack::Parser::fromJson(expected);
  ASSERT_TRUE(expectedJson->slice().isArray());

  arangodb::aql::Function f("MINHASH", &functions::MinHash);

  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  auto const res = evaluateFunc(params, f);

  ASSERT_TRUE(res.isArray());
  EXPECT_EQUAL_SLICES(expectedJson->slice(), res.slice());
}

#ifndef USE_ENTERPRISE

void assertFuncThrow(std::span<const AqlValue> args,
                     arangodb::aql::Function const& f) {
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

  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));

  ASSERT_THROW(f.implementation(&expressionContext, node, args),
               arangodb::basics::Exception);
}

void assertMinHashErrorThrow(char const* args) {
  arangodb::aql::Function f("MINHASH_ERROR", &functions::MinHashError);
  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  assertFuncThrow(params, f);
}

void assertMinHashCountThrow(char const* args) {
  arangodb::aql::Function f("MINHASH_COUNT", &functions::MinHashCount);

  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  assertFuncThrow(params, f);
}

void assertMinHashThrow(char const* args) {
  arangodb::aql::Function f("MINHASH", &functions::MinHash);

  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  assertFuncThrow(params, f);
}
#endif

/*
AqlValue evaluateMinHash(AqlValue const& lhs, AqlValue const& rhs, AqlValue
const& threshold, AqlValue const& analyzer) { fakeit::Mock<ExpressionContext>
expressionContextMock; ExpressionContext& expressionContext =
expressionContextMock.get(); fakeit::When(Method(expressionContextMock,
registerWarning)) .AlwaysDo([](ErrorCode, char const*) {});

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

  containers::SmallVector<AqlValue, 2> params;
  params.emplace_back(values);
  params.emplace_back(count);
  params.emplace_back(VPackSlice::nullSlice());  // redundant argument

  arangodb::aql::Function f("MINHASH", &functions::MinHashError);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));

  return f.implementation(&expressionContext, node, params);
}

AqlValue evaluateMinHashMatch(char const* lhs, char const* rhs) {
  auto const lhsJson = arangodb::velocypack::Parser::fromJson(lhs);
  auto const rhsJson = arangodb::velocypack::Parser::fromJson(rhs);

  AqlValue lhsValue(lhsJson->slice());
  AqlValueGuard lhsGuard(lhsValue, true);

  AqlValue rhsValue(rhsJson->slice());
  AqlValueGuard rhsGuard(rhsValue, true);

  return evaluate(lhsValue, rhsValue);
}
*/

}  // namespace

#if USE_ENTERPRISE
TEST(MinHashErrorFunctionTest, test) {
  assertMinHashError(0.05, "[ 400 ]");
  assertMinHashError(std::numeric_limits<double_t>::infinity(), "[ 0 ]");

  assertMinHashErrorFail("[ [] ]");
  assertMinHashErrorFail("[ {} ]");
  assertMinHashErrorFail("[ true ]");
  assertMinHashErrorFail("[ null ]");
  assertMinHashErrorFail(R"([ "400" ])");
  assertMinHashCountFail(R"([ 400, "redundant" ])");
}

TEST(MinHashCountFunctionTest, test) {
  assertMinHashCount(400, "[ 0.05 ]");

  assertMinHashCountFail("[ 0 ]");
  assertMinHashCountFail("[ [] ]");
  assertMinHashCountFail("[ {} ]");
  assertMinHashCountFail("[ true ]");
  assertMinHashCountFail("[ null ]");
  assertMinHashCountFail(R"([ "0.05" ])");
  assertMinHashCountFail(R"([ 0.05, "redundant" ])");
}

TEST(MinHashFunctionTest, test) {
  assertMinHash(R"([])", R"([ ["foo", "bar", "baz" ], 5 ] ])");
  assertMinHash(R"([])", R"([ [], 5 ])");

  assertMinHashFail(R"([ {}, 5 ])");
  assertMinHashFail(R"([ null, 5 ])");
  assertMinHashFail(R"([ true, 5 ])");
  assertMinHashFail(R"([ 42, 5 ])");
  assertMinHashFail(R"([ "42", 5 ])");

  assertMinHashFail(R"([ ["foo" ], "5" ])");
  assertMinHashFail(R"([ ["foo" ], null ])");
  assertMinHashFail(R"([ ["foo" ], true ])");
  assertMinHashFail(R"([ ["foo" ], {} ])");
  assertMinHashFail(R"([ ["foo" ], [] ])");

  assertMinHashFail(R"([ ["foo" ], 5, "redundant" ])");
}

#else
TEST(MinHashErrorFunctionTest, test) { assertMinHashErrorThrow("[ 400 ]"); }
TEST(MinHashCountFunctionTest, test) { assertMinHashCountThrow("[ 0.5 ]"); }
TEST(MinHashFunctionTest, test) {
  assertMinHashThrow(R"([ ["foo", "bar", "baz" ], 5 ] ])");
}
TEST(MinMatchHashFunctionTest, test) {
// FIXME(gnusi): implement
#endif
