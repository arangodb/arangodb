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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
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

AqlValue callFn(AstNode const& node, char const* input1, char const* input2 = nullptr, char const* input3 = nullptr) {
  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{arena};
  
  auto add = [&](char const* input) {
    if (input != nullptr) {
      auto b = arangodb::velocypack::Parser::fromJson(input);
      AqlValue a(b->slice());
      params.emplace_back(a);
    }
  };
  add(input1);
  add(input2);
  add(input3);

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();
  fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([](int, char const*){ });
  
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

  auto f = static_cast<arangodb::aql::Function const*>(node.getData());
  AqlValue a = f->implementation(&expressionContext, node, params);
  
  for (auto& p : params) {
    p.destroy();
  }
  return a;
}

template <typename T>
T evaluate(AstNode const& node, char const* input1, char const* input2 = nullptr, char const* input3 = nullptr) {
  AqlValue actual = callFn(node, input1, input2, input3);
  if constexpr (std::is_same<T, int64_t>::value) {
    EXPECT_TRUE(actual.isNumber());
    return actual.toInt64();
  } 
  if constexpr (std::is_same<T, bool>::value) {
    EXPECT_TRUE(actual.isBoolean());
    return actual.toBoolean();
  } 
  if constexpr (std::is_same<T, std::string>::value) {
    EXPECT_TRUE(actual.isString());
    return actual.slice().copyString();
  } 
  EXPECT_TRUE(false);
}

void expectFailed(AstNode const& node, char const* input1, char const* input2 = nullptr, char const* input3 = nullptr) {
  AqlValue actual = callFn(node, input1, input2, input3);
  EXPECT_TRUE(actual.isNull(false));
}

}

TEST(BitFunctionsTest, BitAnd) {
  arangodb::aql::Function f("BIT_AND", &Functions::BitAnd);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));

  expectFailed(node, "null");
  expectFailed(node, "false");
  expectFailed(node, "true");
  expectFailed(node, "-1");
  expectFailed(node, "-1000");
  expectFailed(node, "\"\"");
  expectFailed(node, "\"0\"");
  expectFailed(node, "\"1\"");
  expectFailed(node, "\"-1\"");
  expectFailed(node, "\" \"");
  expectFailed(node, "\"foo\"");
  expectFailed(node, "{}");
  expectFailed(node, "[1, -1]");
  expectFailed(node, "[1, \"foo\"]");
  expectFailed(node, "[1, false]");
  expectFailed(node, "[4294967296]");
  expectFailed(node, "[4294967296, 1]");
  expectFailed(node, "[1, 4294967296]");
  expectFailed(node, "[4294967295, 4294967296]");
  expectFailed(node, "[9223372036854775808]");
  expectFailed(node, "[18446744073709551615]");

  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[null]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[null, null, null, null]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 0]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 0, 0]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, null]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, null, null]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[null, null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, 1, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 1, 0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[1, 2, 4, 8, 16, 32]"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "[255, 15, 255, 15, 255, 15]"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255, 1]"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255, 2]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255, 16]"));
  ASSERT_EQ(int64_t(65), evaluate<int64_t>(node, "[65]"));
  ASSERT_EQ(int64_t(65), evaluate<int64_t>(node, "[65, null]"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "[256]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[256, 0]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[256, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[256, 2]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[256, 3]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[256, 4]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[256, 128]"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "[256, 256]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[255, 0]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[255, 1]"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "[255, 2]"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "[255, 3]"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "[255, 4]"));
  ASSERT_EQ(int64_t(128), evaluate<int64_t>(node, "[255, 128]"));
  ASSERT_EQ(int64_t(129), evaluate<int64_t>(node, "[255, 129]"));
  ASSERT_EQ(int64_t(130), evaluate<int64_t>(node, "[255, 130]"));
  ASSERT_EQ(int64_t(131), evaluate<int64_t>(node, "[255, 131]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[255, 256]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[255, 257]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 65535]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[65535, 255]"));
  ASSERT_EQ(int64_t(65535), evaluate<int64_t>(node, "[65535, 65535]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[65535, 65536]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[65536, 65535]"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "[2147483648]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[2147483648, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[2147483648, 1, 2]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[2147483649, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[2147483649, 1, 2]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[2147483650, 1]"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "[2147483650, 2]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[2147483650, 1, 2]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[4294967295, 1]"));
  ASSERT_EQ(int64_t(254), evaluate<int64_t>(node, "[255, 4294967294, 4294967295]"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "[4294967294, 4294967295]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295, 4294967295]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295, 4294967295, null, null]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[4294967295, 1, null, null]"));
}

TEST(BitFunctionsTest, BitOr) {
  arangodb::aql::Function f("BIT_OR", &Functions::BitOr);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null");
  expectFailed(node, "false");
  expectFailed(node, "true");
  expectFailed(node, "-1");
  expectFailed(node, "-1000");
  expectFailed(node, "\"\"");
  expectFailed(node, "\"0\"");
  expectFailed(node, "\"1\"");
  expectFailed(node, "\"-1\"");
  expectFailed(node, "\" \"");
  expectFailed(node, "\"foo\"");
  expectFailed(node, "{}");
  expectFailed(node, "[1, -1]");
  expectFailed(node, "[1, \"foo\"]");
  expectFailed(node, "[1, false]");
  expectFailed(node, "[4294967296]");
  expectFailed(node, "[4294967296, 1]");
  expectFailed(node, "[9223372036854775808]");
  expectFailed(node, "[18446744073709551615]");

  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[null]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[null, null, null, null]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 0]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 0, 0]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, null]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, null, null]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[null, null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, 1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[0, 1, 0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(63), evaluate<int64_t>(node, "[1, 2, 4, 8, 16, 32]"));
  ASSERT_EQ(int64_t(65535), evaluate<int64_t>(node, "[1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 15, 255, 15, 255, 15]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255, 1]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255, 2]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[15, 255, 15, 255, 15, 255, 16]"));
  ASSERT_EQ(int64_t(65), evaluate<int64_t>(node, "[65]"));
  ASSERT_EQ(int64_t(65), evaluate<int64_t>(node, "[65, null]"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "[256]"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "[256, 0]"));
  ASSERT_EQ(int64_t(257), evaluate<int64_t>(node, "[256, 1]"));
  ASSERT_EQ(int64_t(258), evaluate<int64_t>(node, "[256, 2]"));
  ASSERT_EQ(int64_t(259), evaluate<int64_t>(node, "[256, 3]"));
  ASSERT_EQ(int64_t(260), evaluate<int64_t>(node, "[256, 4]"));
  ASSERT_EQ(int64_t(384), evaluate<int64_t>(node, "[256, 128]"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "[256, 256]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 0]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 1]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 2]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 3]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 4]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 128]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 129]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 130]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 131]"));
  ASSERT_EQ(int64_t(511), evaluate<int64_t>(node, "[255, 256]"));
  ASSERT_EQ(int64_t(511), evaluate<int64_t>(node, "[255, 257]"));
  ASSERT_EQ(int64_t(65535), evaluate<int64_t>(node, "[255, 65535]"));
  ASSERT_EQ(int64_t(65535), evaluate<int64_t>(node, "[65535, 255]"));
  ASSERT_EQ(int64_t(65535), evaluate<int64_t>(node, "[65535, 65535]"));
  ASSERT_EQ(int64_t(131071), evaluate<int64_t>(node, "[65535, 65536]"));
  ASSERT_EQ(int64_t(131071), evaluate<int64_t>(node, "[65536, 65535]"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "[2147483648]"));
  ASSERT_EQ(int64_t(2147483651), evaluate<int64_t>(node, "[2147483648, 1, 2]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295, 1]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[255, 4294967294, 4294967295]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967294, 4294967295]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295, 4294967295]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295, 4294967295, null, null]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295, 1, null, null]"));
}

TEST(BitFunctionsTest, BitXOr) {
  arangodb::aql::Function f("BIT_XOR", &Functions::BitXOr);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null");
  expectFailed(node, "false");
  expectFailed(node, "true");
  expectFailed(node, "-1");
  expectFailed(node, "-1000");
  expectFailed(node, "\"\"");
  expectFailed(node, "\"0\"");
  expectFailed(node, "\"1\"");
  expectFailed(node, "\"-1\"");
  expectFailed(node, "\" \"");
  expectFailed(node, "\"foo\"");
  expectFailed(node, "{}");
  expectFailed(node, "[1, -1]");
  expectFailed(node, "[1, \"foo\"]");
  expectFailed(node, "[1, false]");
  expectFailed(node, "[4294967296]");
  expectFailed(node, "[4294967296, 1]");
  expectFailed(node, "[1, 4294967296]");
  expectFailed(node, "[4294967295, 4294967296]");
  expectFailed(node, "[9223372036854775808]");
  expectFailed(node, "[18446744073709551615]");

  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[null]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[null, null, null, null]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 0]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 0, 0]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, null]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, null, null]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[null, null, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, 1, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[1, 1, 1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[1, 1, 1, 1, 1]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[0, 1, 0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[0, 1]"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "[0, 1, 2]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 1, 2, 3]"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "[0, 1, 2, 3, 4]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(63), evaluate<int64_t>(node, "[1, 2, 4, 8, 16, 32]"));
  ASSERT_EQ(int64_t(65535), evaluate<int64_t>(node, "[1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]"));
  ASSERT_EQ(int64_t(240), evaluate<int64_t>(node, "[255, 15]"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "[255, 15, 255]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[255, 15, 255, 15]"));
  ASSERT_EQ(int64_t(114), evaluate<int64_t>(node, "[255, 12, 129]"));
  ASSERT_EQ(int64_t(66), evaluate<int64_t>(node, "[255, 12, 129, 48]"));
  ASSERT_EQ(int64_t(65), evaluate<int64_t>(node, "[65]"));
  ASSERT_EQ(int64_t(65), evaluate<int64_t>(node, "[65, null]"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "[256]"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "[256, 0]"));
  ASSERT_EQ(int64_t(257), evaluate<int64_t>(node, "[256, 1]"));
  ASSERT_EQ(int64_t(258), evaluate<int64_t>(node, "[256, 2]"));
  ASSERT_EQ(int64_t(259), evaluate<int64_t>(node, "[256, 3]"));
  ASSERT_EQ(int64_t(260), evaluate<int64_t>(node, "[256, 4]"));
  ASSERT_EQ(int64_t(384), evaluate<int64_t>(node, "[256, 128]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[256, 256]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[255, 0]"));
  ASSERT_EQ(int64_t(254), evaluate<int64_t>(node, "[255, 1]"));
  ASSERT_EQ(int64_t(253), evaluate<int64_t>(node, "[255, 2]"));
  ASSERT_EQ(int64_t(252), evaluate<int64_t>(node, "[255, 3]"));
  ASSERT_EQ(int64_t(251), evaluate<int64_t>(node, "[255, 4]"));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "[255, 128]"));
  ASSERT_EQ(int64_t(126), evaluate<int64_t>(node, "[255, 129]"));
  ASSERT_EQ(int64_t(125), evaluate<int64_t>(node, "[255, 130]"));
  ASSERT_EQ(int64_t(124), evaluate<int64_t>(node, "[255, 131]"));
  ASSERT_EQ(int64_t(511), evaluate<int64_t>(node, "[255, 256]"));
  ASSERT_EQ(int64_t(510), evaluate<int64_t>(node, "[255, 257]"));
  ASSERT_EQ(int64_t(65280), evaluate<int64_t>(node, "[255, 65535]"));
  ASSERT_EQ(int64_t(65280), evaluate<int64_t>(node, "[65535, 255]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[65535, 65535]"));
  ASSERT_EQ(int64_t(131071), evaluate<int64_t>(node, "[65535, 65536]"));
  ASSERT_EQ(int64_t(131071), evaluate<int64_t>(node, "[65536, 65535]"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "[2147483648]"));
  ASSERT_EQ(int64_t(2147483651), evaluate<int64_t>(node, "[2147483648, 1, 2]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967295]"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "[4294967295, 1]"));
  ASSERT_EQ(int64_t(4294967040), evaluate<int64_t>(node, "[255, 4294967295]"));
  ASSERT_EQ(int64_t(254), evaluate<int64_t>(node, "[255, 4294967294, 4294967295]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[4294967294, 4294967295]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[4294967294, 4294967295, 4294967294]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[4294967295, 4294967295]"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[4294967295, 4294967295, null, null]"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "[4294967295, 1, null, null]"));
}

TEST(BitFunctionsTest, BitPopcount) {
  arangodb::aql::Function f("BIT_POPCOUNT", &Functions::BitPopcount);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null");
  expectFailed(node, "false");
  expectFailed(node, "true");
  expectFailed(node, "-1");
  expectFailed(node, "-1000");
  expectFailed(node, "4294967296");
  expectFailed(node, "9223372036854775808");
  expectFailed(node, "18446744073709551615");
  expectFailed(node, "\"\"");
  expectFailed(node, "\"0\"");
  expectFailed(node, "\"1\"");
  expectFailed(node, "\"-1\"");
  expectFailed(node, "\" \"");
  expectFailed(node, "\"foo\"");
  expectFailed(node, "[]");
  expectFailed(node, "{}");

  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "1"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "2"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "3"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "5"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "6"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "7"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "8"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "15"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "16"));
  ASSERT_EQ(int64_t(8), evaluate<int64_t>(node, "255"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "256"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "257"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "384"));
  ASSERT_EQ(int64_t(9), evaluate<int64_t>(node, "511"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "65534"));
  ASSERT_EQ(int64_t(16), evaluate<int64_t>(node, "65535"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "65536"));
  ASSERT_EQ(int64_t(12), evaluate<int64_t>(node, "1234567890"));
  ASSERT_EQ(int64_t(30), evaluate<int64_t>(node, "2147483646"));
  ASSERT_EQ(int64_t(31), evaluate<int64_t>(node, "2147483647"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "2147483648"));
  ASSERT_EQ(int64_t(31), evaluate<int64_t>(node, "4294967294"));
  ASSERT_EQ(int64_t(32), evaluate<int64_t>(node, "4294967295"));
}

TEST(BitFunctionsTest, BitNegate) {
  arangodb::aql::Function f("BIT_NEGATE", &Functions::BitNegate);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null", "32");
  expectFailed(node, "false", "32");
  expectFailed(node, "true", "32");
  expectFailed(node, "-1", "32");
  expectFailed(node, "-1000", "32");
  expectFailed(node, "4294967296", "32");
  expectFailed(node, "9223372036854775808", "32");
  expectFailed(node, "18446744073709551615", "32");
  expectFailed(node, "\"\"", "32");
  expectFailed(node, "\"0\"", "32");
  expectFailed(node, "\"1\"", "32");
  expectFailed(node, "\"-1\"", "32");
  expectFailed(node, "\" \"", "32");
  expectFailed(node, "\"foo\"", "32");
  expectFailed(node, "[]", "32");
  expectFailed(node, "{}", "32");
  
  expectFailed(node, "0", "33");
  expectFailed(node, "0", "64");
  expectFailed(node, "0", "null");
  expectFailed(node, "0", "false");
  expectFailed(node, "0", "true");
  expectFailed(node, "0", "-1");
  expectFailed(node, "0", "\"\"");
  expectFailed(node, "0", "\"abc\"");
  expectFailed(node, "0", "[]");
  expectFailed(node, "0", "{}");

  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "0", "1"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "0", "2"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "0", "4"));
  ASSERT_EQ(int64_t(1023), evaluate<int64_t>(node, "0", "10"));
  ASSERT_EQ(int64_t(65535), evaluate<int64_t>(node, "0", "16"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "0", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "1"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "1", "2"));
  ASSERT_EQ(int64_t(6), evaluate<int64_t>(node, "1", "3"));
  ASSERT_EQ(int64_t(14), evaluate<int64_t>(node, "1", "4"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "12", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "12", "1"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "12", "2"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "12", "3"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "12", "4"));
  ASSERT_EQ(int64_t(19), evaluate<int64_t>(node, "12", "5"));
  ASSERT_EQ(int64_t(51), evaluate<int64_t>(node, "12", "6"));
  ASSERT_EQ(int64_t(115), evaluate<int64_t>(node, "12", "7"));
  ASSERT_EQ(int64_t(243), evaluate<int64_t>(node, "12", "8"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "15", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "15", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "15", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "15", "3"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "15", "4"));
  ASSERT_EQ(int64_t(16), evaluate<int64_t>(node, "15", "5"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "3"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "4"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "5"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "6"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "7"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "255", "8"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "255", "9"));
  ASSERT_EQ(int64_t(768), evaluate<int64_t>(node, "255", "10"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "256", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "256", "1"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "256", "2"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "256", "3"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "256", "4"));
  ASSERT_EQ(int64_t(31), evaluate<int64_t>(node, "256", "5"));
  ASSERT_EQ(int64_t(63), evaluate<int64_t>(node, "256", "6"));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "256", "7"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "256", "8"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "256", "9"));
  ASSERT_EQ(int64_t(767), evaluate<int64_t>(node, "256", "10"));
  ASSERT_EQ(int64_t(357913941), evaluate<int64_t>(node, "2863311530", "30"));
  ASSERT_EQ(int64_t(1431655765), evaluate<int64_t>(node, "2863311530", "31"));
  ASSERT_EQ(int64_t(1431655765), evaluate<int64_t>(node, "2863311530", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967246", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967246", "1"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967246", "2"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967246", "3"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967246", "4"));
  ASSERT_EQ(int64_t(17), evaluate<int64_t>(node, "4294967246", "5"));
  ASSERT_EQ(int64_t(49), evaluate<int64_t>(node, "4294967246", "10"));
  ASSERT_EQ(int64_t(49), evaluate<int64_t>(node, "4294967246", "31"));
  ASSERT_EQ(int64_t(49), evaluate<int64_t>(node, "4294967246", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967294", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967294", "1"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967294", "2"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967294", "3"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967294", "10"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967294", "31"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967294", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "10"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "31"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "32"));
}

TEST(BitFunctionsTest, BitTest) {
  arangodb::aql::Function f("BIT_TEST", &Functions::BitTest);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null", "0");
  expectFailed(node, "false", "0");
  expectFailed(node, "true", "0");
  expectFailed(node, "-1", "0");
  expectFailed(node, "-1000", "0");
  expectFailed(node, "4294967296", "0");
  expectFailed(node, "9223372036854775808", "0");
  expectFailed(node, "18446744073709551615", "0");
  expectFailed(node, "\"\"", "0");
  expectFailed(node, "\"0\"", "0");
  expectFailed(node, "\"1\"", "0");
  expectFailed(node, "\"-1\"", "0");
  expectFailed(node, "\" \"", "0");
  expectFailed(node, "\"foo\"", "0");
  expectFailed(node, "[]", "0");
  expectFailed(node, "{}", "0");
  
  expectFailed(node, "0", "32");
  expectFailed(node, "0", "64");
  expectFailed(node, "0", "null");
  expectFailed(node, "0", "false");
  expectFailed(node, "0", "true");
  expectFailed(node, "0", "-1");
  expectFailed(node, "0", "\"\"");
  expectFailed(node, "0", "\"abc\"");
  expectFailed(node, "0", "[]");
  expectFailed(node, "0", "{}");
  
  ASSERT_FALSE(evaluate<bool>(node, "0", "0"));
  ASSERT_FALSE(evaluate<bool>(node, "0", "1"));
  ASSERT_FALSE(evaluate<bool>(node, "0", "2"));
  ASSERT_FALSE(evaluate<bool>(node, "0", "4"));
  ASSERT_FALSE(evaluate<bool>(node, "0", "10"));
  ASSERT_FALSE(evaluate<bool>(node, "0", "16"));
  ASSERT_FALSE(evaluate<bool>(node, "0", "31"));
  ASSERT_TRUE(evaluate<bool>(node, "1", "0"));
  ASSERT_FALSE(evaluate<bool>(node, "1", "1"));
  ASSERT_FALSE(evaluate<bool>(node, "1", "2"));
  ASSERT_FALSE(evaluate<bool>(node, "1", "3"));
  ASSERT_FALSE(evaluate<bool>(node, "1", "4"));
  ASSERT_FALSE(evaluate<bool>(node, "1", "31"));
  ASSERT_FALSE(evaluate<bool>(node, "12", "0"));
  ASSERT_FALSE(evaluate<bool>(node, "12", "1"));
  ASSERT_TRUE(evaluate<bool>(node, "12", "2"));
  ASSERT_TRUE(evaluate<bool>(node, "12", "3"));
  ASSERT_FALSE(evaluate<bool>(node, "12", "4"));
  ASSERT_FALSE(evaluate<bool>(node, "12", "5"));
  ASSERT_TRUE(evaluate<bool>(node, "15", "0"));
  ASSERT_TRUE(evaluate<bool>(node, "15", "1"));
  ASSERT_TRUE(evaluate<bool>(node, "15", "2"));
  ASSERT_TRUE(evaluate<bool>(node, "15", "3"));
  ASSERT_FALSE(evaluate<bool>(node, "15", "4"));
  ASSERT_FALSE(evaluate<bool>(node, "15", "5"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "0"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "1"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "2"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "3"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "4"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "5"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "6"));
  ASSERT_TRUE(evaluate<bool>(node, "255", "7"));
  ASSERT_FALSE(evaluate<bool>(node, "255", "8"));
  ASSERT_FALSE(evaluate<bool>(node, "255", "9"));
  ASSERT_FALSE(evaluate<bool>(node, "255", "10"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "0"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "1"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "2"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "3"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "4"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "5"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "6"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "7"));
  ASSERT_TRUE(evaluate<bool>(node, "256", "8"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "9"));
  ASSERT_FALSE(evaluate<bool>(node, "256", "10"));
  ASSERT_FALSE(evaluate<bool>(node, "2863311530", "30"));
  ASSERT_TRUE(evaluate<bool>(node, "2863311530", "31"));
  ASSERT_FALSE(evaluate<bool>(node, "4294967246", "0"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967246", "1"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967246", "2"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967246", "3"));
  ASSERT_FALSE(evaluate<bool>(node, "4294967246", "4"));
  ASSERT_FALSE(evaluate<bool>(node, "4294967246", "5"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967246", "10"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967246", "30"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967246", "31"));
  ASSERT_FALSE(evaluate<bool>(node, "4294967294", "0"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967294", "1"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967294", "2"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967294", "3"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967294", "10"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967294", "30"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967294", "31"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967295", "0"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967295", "1"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967295", "2"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967295", "10"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967295", "30"));
  ASSERT_TRUE(evaluate<bool>(node, "4294967295", "31"));
}

TEST(BitFunctionsTest, BitShiftLeft) {
  arangodb::aql::Function f("BIT_SHIFT_LEFT", &Functions::BitShiftLeft);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
}

TEST(BitFunctionsTest, BitShiftRight) {
  arangodb::aql::Function f("BIT_SHIFT_RIGHT", &Functions::BitShiftRight);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
}

TEST(BitFunctionsTest, BitConstruct) {
  arangodb::aql::Function f("BIT_CONSTRUCT", &Functions::BitConstruct);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
}

TEST(BitFunctionsTest, BitDeonstruct) {
  arangodb::aql::Function f("BIT_DECONSTRUCT", &Functions::BitDeconstruct);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
}

TEST(BitFunctionsTest, BitToString) {
  arangodb::aql::Function f("BIT_TO_STRING", &Functions::BitToString);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null", "0");
  expectFailed(node, "false", "0");
  expectFailed(node, "true", "0");
  expectFailed(node, "-1", "0");
  expectFailed(node, "-1000", "0");
  expectFailed(node, "4294967296", "0");
  expectFailed(node, "9223372036854775808", "0");
  expectFailed(node, "18446744073709551615", "0");
  expectFailed(node, "\"\"", "0");
  expectFailed(node, "\"0\"", "0");
  expectFailed(node, "\"1\"", "0");
  expectFailed(node, "\"-1\"", "0");
  expectFailed(node, "\" \"", "0");
  expectFailed(node, "\"foo\"", "0");
  expectFailed(node, "[]", "0");
  expectFailed(node, "{}", "0");
  expectFailed(node, "0", "null");
  expectFailed(node, "0", "false");
  expectFailed(node, "0", "true");
  expectFailed(node, "0", "-1");
  expectFailed(node, "0", "33");
  expectFailed(node, "0", "\"\"");
  expectFailed(node, "0", "\"abc\"");
  expectFailed(node, "0", "[]");
  expectFailed(node, "0", "{}");
  
  ASSERT_EQ("", evaluate<std::string>(node, "0", "0"));
  ASSERT_EQ("0", evaluate<std::string>(node, "0", "1"));
  ASSERT_EQ("00", evaluate<std::string>(node, "0", "2"));
  ASSERT_EQ("000", evaluate<std::string>(node, "0", "3"));
  ASSERT_EQ("0000", evaluate<std::string>(node, "0", "4"));
  ASSERT_EQ("00000", evaluate<std::string>(node, "0", "5"));
  ASSERT_EQ("000000", evaluate<std::string>(node, "0", "6"));
  ASSERT_EQ("0000000", evaluate<std::string>(node, "0", "7"));
  ASSERT_EQ("00000000", evaluate<std::string>(node, "0", "8"));
  ASSERT_EQ("000000000", evaluate<std::string>(node, "0", "9"));
  ASSERT_EQ("0000000000", evaluate<std::string>(node, "0", "10"));
  ASSERT_EQ("00000000000", evaluate<std::string>(node, "0", "11"));
  ASSERT_EQ("000000000000", evaluate<std::string>(node, "0", "12"));
  ASSERT_EQ("0000000000000", evaluate<std::string>(node, "0", "13"));
  ASSERT_EQ("00000000000000", evaluate<std::string>(node, "0", "14"));
  ASSERT_EQ("000000000000000", evaluate<std::string>(node, "0", "15"));
  ASSERT_EQ("0000000000000000", evaluate<std::string>(node, "0", "16"));
  ASSERT_EQ("00000000000000000", evaluate<std::string>(node, "0", "17"));
  ASSERT_EQ("000000000000000000", evaluate<std::string>(node, "0", "18"));
  ASSERT_EQ("0000000000000000000", evaluate<std::string>(node, "0", "19"));
  ASSERT_EQ("00000000000000000000", evaluate<std::string>(node, "0", "20"));
  ASSERT_EQ("000000000000000000000", evaluate<std::string>(node, "0", "21"));
  ASSERT_EQ("0000000000000000000000", evaluate<std::string>(node, "0", "22"));
  ASSERT_EQ("00000000000000000000000", evaluate<std::string>(node, "0", "23"));
  ASSERT_EQ("000000000000000000000000", evaluate<std::string>(node, "0", "24"));
  ASSERT_EQ("0000000000000000000000000", evaluate<std::string>(node, "0", "25"));
  ASSERT_EQ("00000000000000000000000000", evaluate<std::string>(node, "0", "26"));
  ASSERT_EQ("000000000000000000000000000", evaluate<std::string>(node, "0", "27"));
  ASSERT_EQ("0000000000000000000000000000", evaluate<std::string>(node, "0", "28"));
  ASSERT_EQ("00000000000000000000000000000", evaluate<std::string>(node, "0", "29"));
  ASSERT_EQ("000000000000000000000000000000", evaluate<std::string>(node, "0", "30"));
  ASSERT_EQ("0000000000000000000000000000000", evaluate<std::string>(node, "0", "31"));
  ASSERT_EQ("00000000000000000000000000000000", evaluate<std::string>(node, "0", "32"));
}

TEST(BitFunctionsTest, BitFromString) {
  arangodb::aql::Function f("BIT_FROM_STRING", &Functions::BitFromString);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
}
