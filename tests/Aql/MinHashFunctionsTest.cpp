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

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "IResearch/common.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace {

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

#ifndef USE_ENTERPRISE

void assertFuncThrow(std::span<const AqlValue> args,
                     arangodb::aql::Function const& f) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();
  fakeit::When(Method(expressionContextMock, registerWarning))
      .AlwaysDo([](ErrorCode, std::string_view) {});

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

void assertFuncThrow(char const* args, arangodb::aql::Function const& f) {
  ASSERT_NE(nullptr, args);

  auto params = buildArgs(args);

  auto cleanup = arangodb::scopeGuard([&params]() noexcept {
    for (auto& p : params) {
      p.destroy();
    }
  });

  assertFuncThrow(params, f);
}

void assertMinHashErrorThrow(char const* args) {
  arangodb::aql::Function f("MINHASH_ERROR", &functions::MinHashError);
  assertFuncThrow(args, f);
}

void assertMinHashCountThrow(char const* args) {
  arangodb::aql::Function f("MINHASH_COUNT", &functions::MinHashCount);
  assertFuncThrow(args, f);
}

void assertMinHashThrow(char const* args) {
  arangodb::aql::Function f("MINHASH", &functions::MinHash);
  assertFuncThrow(args, f);
}

void assertMinHashMatchThrow(char const* args) {
  arangodb::aql::Function f("MINHASH_MATCH", &functions::MinHash);
  assertFuncThrow(args, f);
}

#endif

}  // namespace

#if USE_ENTERPRISE
#include "tests/Aql/MinHashFunctionsTestEE.hpp"
#else
TEST(MinHashErrorFunctionTest, test) { assertMinHashErrorThrow("[ 400 ]"); }
TEST(MinHashCountFunctionTest, test) { assertMinHashCountThrow("[ 0.5 ]"); }
TEST(MinHashFunctionTest, test) {
  assertMinHashThrow(R"([ ["foo", "bar", "baz" ], 5 ])");
}
TEST(MinMatchHashFunctionTest, test) {
  assertMinHashMatchThrow(
      R"([ ["foo", "bar", "baz" ], ["foo", "bar", "baz" ], 0.75, "analyzer" ])");
}
#endif
