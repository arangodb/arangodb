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
  fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([](ErrorCode, char const*){ });
  
  VPackOptions options;
  fakeit::Mock<transaction::Context> trxCtxMock;
  fakeit::When(Method(trxCtxMock, getVPackOptions)).AlwaysReturn(&options);
  fakeit::When(Method(trxCtxMock, leaseBuilder)).AlwaysDo([]() { return new arangodb::velocypack::Builder(); });
  fakeit::When(Method(trxCtxMock, returnBuilder)).AlwaysDo([](arangodb::velocypack::Builder* b) { delete b; });
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
  AqlValueGuard guard(actual, true);
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
  if constexpr (std::is_same<T, std::vector<int64_t>>::value) {
    EXPECT_TRUE(actual.isArray());
    std::vector<int64_t> out;
    for (auto it : VPackArrayIterator(actual.slice())) {
      out.push_back(it.getNumber<int64_t>());
    }
    return out;
  } 
  EXPECT_TRUE(false);
}

void expectFailed(AstNode const& node, char const* input1, char const* input2 = nullptr, char const* input3 = nullptr) {
  AqlValue actual = callFn(node, input1, input2, input3);
  EXPECT_TRUE(actual.isNull(false));
}

void expectNull(AstNode const& node, char const* input1, char const* input2 = nullptr, char const* input3 = nullptr) {
  AqlValue actual = callFn(node, input1, input2, input3);
  EXPECT_TRUE(actual.isNull(false));
}

} // namespace

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
  expectFailed(node, "0", "null");
  expectFailed(node, "0", "false");
  expectFailed(node, "0", "true");
  expectFailed(node, "0", "-1");
  expectFailed(node, "0", "\"\"");
  expectFailed(node, "0", "\"1\"");
  expectFailed(node, "0", "\"abc\"");
  expectFailed(node, "0", "[]");
  expectFailed(node, "0", "{}");
  expectFailed(node, "null", "0");
  expectFailed(node, "false", "0");
  expectFailed(node, "true", "0");
  expectFailed(node, "-1", "0");
  expectFailed(node, "\"\"", "0");
  expectFailed(node, "\"1\"", "0");
  expectFailed(node, "\"abc\"", "0");
  expectFailed(node, "[]", "0");
  expectFailed(node, "{}", "0");
  expectFailed(node, "4294967295", "4294967296");
  expectFailed(node, "4294967296", "4294967296");
  expectFailed(node, "4294967296", "4294967295");
  
  expectNull(node, "[]");
  expectNull(node, "[null]");
  expectNull(node, "[null, null, null, null]");

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
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0"));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "127", "255"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "65535", "65536"));
  ASSERT_EQ(int64_t(65536), evaluate<int64_t>(node, "65536", "65536"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "2147483650", "2"));
  ASSERT_EQ(int64_t(254), evaluate<int64_t>(node, "255", "4294967294"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "4294967294", "4294967295"));
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
  expectFailed(node, "0", "null");
  expectFailed(node, "0", "false");
  expectFailed(node, "0", "true");
  expectFailed(node, "0", "-1");
  expectFailed(node, "0", "\"\"");
  expectFailed(node, "0", "\"1\"");
  expectFailed(node, "0", "\"abc\"");
  expectFailed(node, "0", "[]");
  expectFailed(node, "0", "{}");
  expectFailed(node, "null", "0");
  expectFailed(node, "false", "0");
  expectFailed(node, "true", "0");
  expectFailed(node, "-1", "0");
  expectFailed(node, "\"\"", "0");
  expectFailed(node, "\"1\"", "0");
  expectFailed(node, "\"abc\"", "0");
  expectFailed(node, "[]", "0");
  expectFailed(node, "{}", "0");
  expectFailed(node, "4294967295", "4294967296");
  expectFailed(node, "4294967296", "4294967296");
  expectFailed(node, "4294967296", "4294967295");
  
  expectNull(node, "[]");
  expectNull(node, "[null]");
  expectNull(node, "[null, null, null, null]");

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
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "127", "255"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "254", "255"));
  ASSERT_EQ(int64_t(511), evaluate<int64_t>(node, "256", "255"));
  ASSERT_EQ(int64_t(131071), evaluate<int64_t>(node, "65535", "65536"));
  ASSERT_EQ(int64_t(65536), evaluate<int64_t>(node, "65536", "65536"));
  ASSERT_EQ(int64_t(2147483650), evaluate<int64_t>(node, "2147483650", "2"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "255", "4294967294"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "1", "4294967294"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "4294967294", "4294967295"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "4294967294", "0"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "4294967294", "1"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "4294967294", "2"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "4294967294", "3"));
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
  expectFailed(node, "0", "null");
  expectFailed(node, "0", "false");
  expectFailed(node, "0", "true");
  expectFailed(node, "0", "-1");
  expectFailed(node, "0", "\"\"");
  expectFailed(node, "0", "\"1\"");
  expectFailed(node, "0", "\"abc\"");
  expectFailed(node, "0", "[]");
  expectFailed(node, "0", "{}");
  expectFailed(node, "null", "0");
  expectFailed(node, "false", "0");
  expectFailed(node, "true", "0");
  expectFailed(node, "-1", "0");
  expectFailed(node, "\"\"", "0");
  expectFailed(node, "\"1\"", "0");
  expectFailed(node, "\"abc\"", "0");
  expectFailed(node, "[]", "0");
  expectFailed(node, "{}", "0");
  expectFailed(node, "4294967295", "4294967296");
  expectFailed(node, "4294967296", "4294967296");
  expectFailed(node, "4294967296", "4294967295");
  
  expectNull(node, "[]");
  expectNull(node, "[null]");
  expectNull(node, "[null, null, null, null]");

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
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0"));
  ASSERT_EQ(int64_t(128), evaluate<int64_t>(node, "127", "255"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "254", "255"));
  ASSERT_EQ(int64_t(511), evaluate<int64_t>(node, "256", "255"));
  ASSERT_EQ(int64_t(131071), evaluate<int64_t>(node, "65535", "65536"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "65536", "65536"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "2147483650", "2"));
  ASSERT_EQ(int64_t(4294967041), evaluate<int64_t>(node, "255", "4294967294"));
  ASSERT_EQ(int64_t(4294967041), evaluate<int64_t>(node, "4294967294", "255"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "1", "4294967294"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967294", "4294967295"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "4294967294", "0"));
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
  
  expectFailed(node, "null", "0", "0");
  expectFailed(node, "false", "0", "0");
  expectFailed(node, "true", "0", "0");
  expectFailed(node, "-1", "0", "0");
  expectFailed(node, "4294967296", "0", "0");
  expectFailed(node, "\"\"", "0", "0");
  expectFailed(node, "\"1\"", "0", "0");
  expectFailed(node, "\"abc\"", "0", "0");
  expectFailed(node, "[]", "0", "0");
  expectFailed(node, "{}", "0", "0");
  expectFailed(node, "0", "null", "0");
  expectFailed(node, "0", "false", "0");
  expectFailed(node, "0", "true", "0");
  expectFailed(node, "0", "-1", "0");
  expectFailed(node, "0", "33", "0");
  expectFailed(node, "0", "4294967296", "0");
  expectFailed(node, "0", "\"\"", "0");
  expectFailed(node, "0", "\"1\"", "0");
  expectFailed(node, "0", "\"abc\"", "0");
  expectFailed(node, "0", "[]", "0");
  expectFailed(node, "0", "{}", "0");
  expectFailed(node, "0", "0", "null");
  expectFailed(node, "0", "0", "false");
  expectFailed(node, "0", "0", "true");
  expectFailed(node, "0", "0", "-1");
  expectFailed(node, "0", "0", "33");
  expectFailed(node, "0", "0", "4294967296");
  expectFailed(node, "0", "0", "\"\"");
  expectFailed(node, "0", "0", "\"1\"");
  expectFailed(node, "0", "0", "\"abc\"");
  expectFailed(node, "0", "0", "[]");
  expectFailed(node, "0", "0", "{}");
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "1", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "1", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "1", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "30"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "30"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "30"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "31"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "31"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "31"));
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "0", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "1", "0", "1"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "1", "0", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "1", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "1", "1"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "1", "1", "2"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "1", "1", "32"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "1", "2", "32"));
  ASSERT_EQ(int64_t(8), evaluate<int64_t>(node, "1", "3", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "3"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "4"));
  ASSERT_EQ(int64_t(16), evaluate<int64_t>(node, "1", "4", "5"));
  ASSERT_EQ(int64_t(16), evaluate<int64_t>(node, "1", "4", "32"));
  ASSERT_EQ(int64_t(32), evaluate<int64_t>(node, "1", "5", "32"));
  ASSERT_EQ(int64_t(64), evaluate<int64_t>(node, "1", "6", "32"));
  ASSERT_EQ(int64_t(128), evaluate<int64_t>(node, "1", "7", "32"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "1", "8", "32"));
  ASSERT_EQ(int64_t(1024), evaluate<int64_t>(node, "1", "10", "32"));
  ASSERT_EQ(int64_t(1073741824), evaluate<int64_t>(node, "1", "30", "32"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "1", "31", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "32", "32"));
  
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "2", "0", "32"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "2", "1", "32"));
  ASSERT_EQ(int64_t(8), evaluate<int64_t>(node, "2", "2", "32"));
  ASSERT_EQ(int64_t(16), evaluate<int64_t>(node, "2", "3", "32"));
  ASSERT_EQ(int64_t(32), evaluate<int64_t>(node, "2", "4", "32"));
  ASSERT_EQ(int64_t(64), evaluate<int64_t>(node, "2", "5", "32"));
  ASSERT_EQ(int64_t(128), evaluate<int64_t>(node, "2", "6", "32"));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "2", "7", "32"));
  ASSERT_EQ(int64_t(512), evaluate<int64_t>(node, "2", "8", "32"));
  ASSERT_EQ(int64_t(2048), evaluate<int64_t>(node, "2", "10", "32"));
  ASSERT_EQ(int64_t(1073741824), evaluate<int64_t>(node, "2", "29", "32"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "2", "30", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "2", "31", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "2", "32", "32"));
  
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "3", "0", "32"));
  ASSERT_EQ(int64_t(6), evaluate<int64_t>(node, "3", "1", "32"));
  ASSERT_EQ(int64_t(12), evaluate<int64_t>(node, "3", "2", "32"));
  ASSERT_EQ(int64_t(24), evaluate<int64_t>(node, "3", "3", "32"));
  ASSERT_EQ(int64_t(48), evaluate<int64_t>(node, "3", "4", "32"));
  ASSERT_EQ(int64_t(96), evaluate<int64_t>(node, "3", "5", "32"));
  ASSERT_EQ(int64_t(192), evaluate<int64_t>(node, "3", "6", "32"));
  ASSERT_EQ(int64_t(384), evaluate<int64_t>(node, "3", "7", "32"));
  ASSERT_EQ(int64_t(768), evaluate<int64_t>(node, "3", "8", "32"));
  ASSERT_EQ(int64_t(3072), evaluate<int64_t>(node, "3", "10", "32"));
  ASSERT_EQ(int64_t(805306368), evaluate<int64_t>(node, "3", "28", "32"));
  ASSERT_EQ(int64_t(1610612736), evaluate<int64_t>(node, "3", "29", "32"));
  ASSERT_EQ(int64_t(3221225472), evaluate<int64_t>(node, "3", "30", "32"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "3", "31", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "3", "32", "32"));
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "127", "0", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "127", "0", "1"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "127", "0", "2"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "127", "0", "3"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "127", "0", "4"));
  ASSERT_EQ(int64_t(31), evaluate<int64_t>(node, "127", "0", "5"));
  ASSERT_EQ(int64_t(63), evaluate<int64_t>(node, "127", "0", "6"));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "127", "0", "7"));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "127", "0", "32"));
  ASSERT_EQ(int64_t(254), evaluate<int64_t>(node, "127", "1", "32"));
  ASSERT_EQ(int64_t(508), evaluate<int64_t>(node, "127", "2", "32"));
  ASSERT_EQ(int64_t(1016), evaluate<int64_t>(node, "127", "3", "32"));
  ASSERT_EQ(int64_t(2032), evaluate<int64_t>(node, "127", "4", "32"));
  ASSERT_EQ(int64_t(4026531840), evaluate<int64_t>(node, "127", "28", "32"));
  ASSERT_EQ(int64_t(3758096384), evaluate<int64_t>(node, "127", "29", "32"));
  ASSERT_EQ(int64_t(3221225472), evaluate<int64_t>(node, "127", "30", "32"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "127", "31", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "127", "32", "32"));
  
  ASSERT_EQ(int64_t(98782592), evaluate<int64_t>(node, "98782592", "0", "32"));
  ASSERT_EQ(int64_t(197565184), evaluate<int64_t>(node, "98782592", "1", "32"));
  ASSERT_EQ(int64_t(395130368), evaluate<int64_t>(node, "98782592", "2", "32"));
  ASSERT_EQ(int64_t(790260736), evaluate<int64_t>(node, "98782592", "3", "32"));
  ASSERT_EQ(int64_t(1580521472), evaluate<int64_t>(node, "98782592", "4", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "98782592", "20", "10"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "98782592", "20", "27"));
  ASSERT_EQ(int64_t(134217728), evaluate<int64_t>(node, "98782592", "20", "28"));
  ASSERT_EQ(int64_t(402653184), evaluate<int64_t>(node, "98782592", "20", "29"));
  ASSERT_EQ(int64_t(402653184), evaluate<int64_t>(node, "98782592", "20", "30"));
  ASSERT_EQ(int64_t(1476395008), evaluate<int64_t>(node, "98782592", "20", "31"));
  ASSERT_EQ(int64_t(3623878656), evaluate<int64_t>(node, "98782592", "20", "32"));
  ASSERT_EQ(int64_t(2952790016), evaluate<int64_t>(node, "98782592", "21", "32"));
  ASSERT_EQ(int64_t(1610612736), evaluate<int64_t>(node, "98782592", "22", "32"));
  ASSERT_EQ(int64_t(3221225472), evaluate<int64_t>(node, "98782592", "23", "32"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "98782592", "24", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "98782592", "25", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "98782592", "30", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "98782592", "31", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "98782592", "32", "32"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "4294967295", "0", "32"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "4294967295", "1", "32"));
  ASSERT_EQ(int64_t(4294967292), evaluate<int64_t>(node, "4294967295", "2", "32"));
  ASSERT_EQ(int64_t(4294967288), evaluate<int64_t>(node, "4294967295", "3", "32"));
  ASSERT_EQ(int64_t(4294966272), evaluate<int64_t>(node, "4294967295", "10", "32"));
  ASSERT_EQ(int64_t(4293918720), evaluate<int64_t>(node, "4294967295", "20", "32"));
  ASSERT_EQ(int64_t(3221225472), evaluate<int64_t>(node, "4294967295", "30", "32"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "4294967295", "31", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "32", "32"));
}

TEST(BitFunctionsTest, BitShiftRight) {
  arangodb::aql::Function f("BIT_SHIFT_RIGHT", &Functions::BitShiftRight);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null", "0", "0");
  expectFailed(node, "false", "0", "0");
  expectFailed(node, "true", "0", "0");
  expectFailed(node, "-1", "0", "0");
  expectFailed(node, "4294967296", "0", "0");
  expectFailed(node, "\"\"", "0", "0");
  expectFailed(node, "\"1\"", "0", "0");
  expectFailed(node, "\"abc\"", "0", "0");
  expectFailed(node, "[]", "0", "0");
  expectFailed(node, "{}", "0", "0");
  expectFailed(node, "0", "null", "0");
  expectFailed(node, "0", "false", "0");
  expectFailed(node, "0", "true", "0");
  expectFailed(node, "0", "-1", "0");
  expectFailed(node, "0", "33", "0");
  expectFailed(node, "0", "4294967296", "0");
  expectFailed(node, "0", "\"\"", "0");
  expectFailed(node, "0", "\"1\"", "0");
  expectFailed(node, "0", "\"abc\"", "0");
  expectFailed(node, "0", "[]", "0");
  expectFailed(node, "0", "{}", "0");
  expectFailed(node, "0", "0", "null");
  expectFailed(node, "0", "0", "false");
  expectFailed(node, "0", "0", "true");
  expectFailed(node, "0", "0", "-1");
  expectFailed(node, "0", "0", "33");
  expectFailed(node, "0", "0", "4294967296");
  expectFailed(node, "0", "0", "\"\"");
  expectFailed(node, "0", "0", "\"1\"");
  expectFailed(node, "0", "0", "\"abc\"");
  expectFailed(node, "0", "0", "[]");
  expectFailed(node, "0", "0", "{}");
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "0", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "1", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "1", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "1", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "30"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "30"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "30"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "31"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "31"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "0", "5", "31"));
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "0", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "1", "0", "1"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "1", "0", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "1", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "1", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "1", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "1", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "2", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "3", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "0"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "1"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "2"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "3"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "4"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "5"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "4", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "5", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "6", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "7", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "8", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "1", "32", "32"));
  
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "2", "0", "32"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "2", "1", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "2", "2", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "2", "3", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "2", "32", "32"));
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "3", "0", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "3", "0", "1"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "3", "0", "2"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "3", "0", "32"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "3", "1", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "3", "2", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "3", "3", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "3", "32", "32"));
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "127", "0", "0"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "127", "0", "1"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "127", "0", "2"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "127", "0", "3"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "127", "0", "4"));
  ASSERT_EQ(int64_t(31), evaluate<int64_t>(node, "127", "0", "5"));
  ASSERT_EQ(int64_t(63), evaluate<int64_t>(node, "127", "0", "6"));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "127", "0", "7"));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "127", "0", "32"));
  ASSERT_EQ(int64_t(63), evaluate<int64_t>(node, "127", "1", "32"));
  ASSERT_EQ(int64_t(31), evaluate<int64_t>(node, "127", "2", "32"));
  ASSERT_EQ(int64_t(15), evaluate<int64_t>(node, "127", "3", "32"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "127", "4", "32"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "127", "5", "32"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "127", "6", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "127", "7", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "127", "8", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "127", "32", "32"));
  
  ASSERT_EQ(int64_t(98782592), evaluate<int64_t>(node, "98782592", "0", "32"));
  ASSERT_EQ(int64_t(49391296), evaluate<int64_t>(node, "98782592", "1", "32"));
  ASSERT_EQ(int64_t(24695648), evaluate<int64_t>(node, "98782592", "2", "32"));
  ASSERT_EQ(int64_t(12347824), evaluate<int64_t>(node, "98782592", "3", "32"));
  ASSERT_EQ(int64_t(6173912), evaluate<int64_t>(node, "98782592", "4", "32"));
  ASSERT_EQ(int64_t(3086956), evaluate<int64_t>(node, "98782592", "5", "32"));
  ASSERT_EQ(int64_t(1543478), evaluate<int64_t>(node, "98782592", "6", "32"));
  ASSERT_EQ(int64_t(771739), evaluate<int64_t>(node, "98782592", "7", "32"));
  ASSERT_EQ(int64_t(385869), evaluate<int64_t>(node, "98782592", "8", "32"));
  ASSERT_EQ(int64_t(192934), evaluate<int64_t>(node, "98782592", "9", "32"));
  ASSERT_EQ(int64_t(96467), evaluate<int64_t>(node, "98782592", "10", "32"));
  ASSERT_EQ(int64_t(48233), evaluate<int64_t>(node, "98782592", "11", "32"));
  ASSERT_EQ(int64_t(24116), evaluate<int64_t>(node, "98782592", "12", "32"));
  ASSERT_EQ(int64_t(12058), evaluate<int64_t>(node, "98782592", "13", "32"));
  ASSERT_EQ(int64_t(6029), evaluate<int64_t>(node, "98782592", "14", "32"));
  ASSERT_EQ(int64_t(3014), evaluate<int64_t>(node, "98782592", "15", "32"));
  ASSERT_EQ(int64_t(1507), evaluate<int64_t>(node, "98782592", "16", "32"));
  ASSERT_EQ(int64_t(753), evaluate<int64_t>(node, "98782592", "17", "32"));
  ASSERT_EQ(int64_t(376), evaluate<int64_t>(node, "98782592", "18", "32"));
  ASSERT_EQ(int64_t(188), evaluate<int64_t>(node, "98782592", "19", "32"));
  ASSERT_EQ(int64_t(94), evaluate<int64_t>(node, "98782592", "20", "32"));
  ASSERT_EQ(int64_t(47), evaluate<int64_t>(node, "98782592", "21", "32"));
  ASSERT_EQ(int64_t(23), evaluate<int64_t>(node, "98782592", "22", "32"));
  ASSERT_EQ(int64_t(11), evaluate<int64_t>(node, "98782592", "23", "32"));
  ASSERT_EQ(int64_t(5), evaluate<int64_t>(node, "98782592", "24", "32"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "98782592", "25", "32"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "98782592", "26", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "98782592", "27", "32"));
  
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "4294967295", "0", "32"));
  ASSERT_EQ(int64_t(2147483647), evaluate<int64_t>(node, "4294967295", "1", "32"));
  ASSERT_EQ(int64_t(134217727), evaluate<int64_t>(node, "4294967295", "5", "32"));
  ASSERT_EQ(int64_t(2097151), evaluate<int64_t>(node, "4294967295", "11", "32"));
  ASSERT_EQ(int64_t(8191), evaluate<int64_t>(node, "4294967295", "19", "32"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "4294967295", "29", "32"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "4294967295", "30", "32"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "4294967295", "31", "32"));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "4294967295", "32", "32"));
}

TEST(BitFunctionsTest, BitConstruct) {
  arangodb::aql::Function f("BIT_CONSTRUCT", &Functions::BitConstruct);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null");
  expectFailed(node, "false");
  expectFailed(node, "true");
  expectFailed(node, "-1");
  expectFailed(node, "-1000");
  expectFailed(node, "0");
  expectFailed(node, "1");
  expectFailed(node, "\"\"");
  expectFailed(node, "\"0\"");
  expectFailed(node, "\"1\"");
  expectFailed(node, "\"-1\"");
  expectFailed(node, "\" \"");
  expectFailed(node, "\"foo\"");
  expectFailed(node, "{}");
  expectFailed(node, "[null]");
  expectFailed(node, "[false]");
  expectFailed(node, "[true]");
  expectFailed(node, "[-1]");
  expectFailed(node, "[\"\"]");
  expectFailed(node, "[\"1\"]");
  expectFailed(node, "[[]]");
  expectFailed(node, "[[1]]");
  expectFailed(node, "[{}]");
  expectFailed(node, "[1, -1]");
  expectFailed(node, "[1, -1]");
  expectFailed(node, "[1, \"foo\"]");
  expectFailed(node, "[1, null]");
  expectFailed(node, "[1, false]");
  expectFailed(node, "[1, true]");
  expectFailed(node, "[1, 1, 32]");
  expectFailed(node, "[32]");
  expectFailed(node, "[4294967296]");
  expectFailed(node, "[4294967296, 1]");
  expectFailed(node, "[1, 4294967296]");
  expectFailed(node, "[4294967295, 4294967296]");
  expectFailed(node, "[9223372036854775808]");
  expectFailed(node, "[18446744073709551615]");

  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "[]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[0]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[0, 0]"));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "[0, 0, 0]"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "[1]"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "[1, 1]"));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "[1, 1, 1]"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "[1, 0]"));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "[0, 1]"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "[2]"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "[2, 2]"));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "[2, 2, 2, 2]"));
  ASSERT_EQ(int64_t(5), evaluate<int64_t>(node, "[0, 2]"));
  ASSERT_EQ(int64_t(5), evaluate<int64_t>(node, "[2, 0]"));
  ASSERT_EQ(int64_t(6), evaluate<int64_t>(node, "[1, 2]"));
  ASSERT_EQ(int64_t(6), evaluate<int64_t>(node, "[2, 1]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2, 0, 1, 2]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2, 2, 1, 0]"));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "[0, 1, 2, 2, 1]"));
  ASSERT_EQ(int64_t(191), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 7]"));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(511), evaluate<int64_t>(node, "[0, 1, 2, 3, 4, 5, 6, 7, 8]"));
  ASSERT_EQ(int64_t(1024), evaluate<int64_t>(node, "[10]"));
  ASSERT_EQ(int64_t(65536), evaluate<int64_t>(node, "[16]"));
  ASSERT_EQ(int64_t(65812), evaluate<int64_t>(node, "[16, 8, 4, 2]"));
  ASSERT_EQ(int64_t(1048576), evaluate<int64_t>(node, "[20]"));
  ASSERT_EQ(int64_t(3145728), evaluate<int64_t>(node, "[20, 21]"));
  ASSERT_EQ(int64_t(16777216), evaluate<int64_t>(node, "[24]"));
  ASSERT_EQ(int64_t(33554432), evaluate<int64_t>(node, "[25]"));
  ASSERT_EQ(int64_t(1073741824), evaluate<int64_t>(node, "[30]"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "[31]"));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "[31]"));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "[31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]"));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "[31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1]"));
  ASSERT_EQ(int64_t(4294967292), evaluate<int64_t>(node, "[31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2]"));
  ASSERT_EQ(int64_t(4294967288), evaluate<int64_t>(node, "[31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3]"));
}

TEST(BitFunctionsTest, BitDeconstruct) {
  arangodb::aql::Function f("BIT_DECONSTRUCT", &Functions::BitDeconstruct);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null");
  expectFailed(node, "false");
  expectFailed(node, "true");
  expectFailed(node, "-1");
  expectFailed(node, "-1000");
  expectFailed(node, "4294967296");
  expectFailed(node, "\"\"");
  expectFailed(node, "\"0\"");
  expectFailed(node, "\"1\"");
  expectFailed(node, "\"-1\"");
  expectFailed(node, "\" \"");
  expectFailed(node, "\"foo\"");
  expectFailed(node, "{}");
  
  ASSERT_EQ(std::vector<int64_t>({}), evaluate<std::vector<int64_t>>(node, "0"));
  ASSERT_EQ(std::vector<int64_t>({0}), evaluate<std::vector<int64_t>>(node, "1"));
  ASSERT_EQ(std::vector<int64_t>({1}), evaluate<std::vector<int64_t>>(node, "2"));
  ASSERT_EQ(std::vector<int64_t>({0, 1}), evaluate<std::vector<int64_t>>(node, "3"));
  ASSERT_EQ(std::vector<int64_t>({2}), evaluate<std::vector<int64_t>>(node, "4"));
  ASSERT_EQ(std::vector<int64_t>({0, 1, 2}), evaluate<std::vector<int64_t>>(node, "7"));
  ASSERT_EQ(std::vector<int64_t>({3}), evaluate<std::vector<int64_t>>(node, "8"));
  ASSERT_EQ(std::vector<int64_t>({1, 3}), evaluate<std::vector<int64_t>>(node, "10"));
  ASSERT_EQ(std::vector<int64_t>({0, 1, 2, 3, 4, 5, 7}), evaluate<std::vector<int64_t>>(node, "191"));
  ASSERT_EQ(std::vector<int64_t>({0, 1, 2, 3, 4, 5, 6, 7}), evaluate<std::vector<int64_t>>(node, "255"));
  ASSERT_EQ(std::vector<int64_t>({0, 1, 2, 3, 4, 5, 6, 7, 8}), evaluate<std::vector<int64_t>>(node, "511"));
  ASSERT_EQ(std::vector<int64_t>({10}), evaluate<std::vector<int64_t>>(node, "1024"));
  ASSERT_EQ(std::vector<int64_t>({30}), evaluate<std::vector<int64_t>>(node, "1073741824"));
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
  ASSERT_EQ("", evaluate<std::string>(node, "1", "0"));
  ASSERT_EQ("1", evaluate<std::string>(node, "1", "1"));
  ASSERT_EQ("01", evaluate<std::string>(node, "1", "2"));
  ASSERT_EQ("0000000001", evaluate<std::string>(node, "1", "10"));
  ASSERT_EQ("00000000000000000000000000000001", evaluate<std::string>(node, "1", "32"));
  ASSERT_EQ("", evaluate<std::string>(node, "15", "0"));
  ASSERT_EQ("1", evaluate<std::string>(node, "15", "1"));
  ASSERT_EQ("11", evaluate<std::string>(node, "15", "2"));
  ASSERT_EQ("111", evaluate<std::string>(node, "15", "3"));
  ASSERT_EQ("1111", evaluate<std::string>(node, "15", "4"));
  ASSERT_EQ("01111", evaluate<std::string>(node, "15", "5"));
  ASSERT_EQ("00000000000000000000000000001111", evaluate<std::string>(node, "15", "32"));
  ASSERT_EQ("", evaluate<std::string>(node, "16", "0"));
  ASSERT_EQ("0", evaluate<std::string>(node, "16", "1"));
  ASSERT_EQ("00", evaluate<std::string>(node, "16", "2"));
  ASSERT_EQ("000", evaluate<std::string>(node, "16", "3"));
  ASSERT_EQ("0000", evaluate<std::string>(node, "16", "4"));
  ASSERT_EQ("10000", evaluate<std::string>(node, "16", "5"));
  ASSERT_EQ("00000000000000000000000000010000", evaluate<std::string>(node, "16", "32"));
  ASSERT_EQ("", evaluate<std::string>(node, "1365", "0"));
  ASSERT_EQ("1", evaluate<std::string>(node, "1365", "1"));
  ASSERT_EQ("01", evaluate<std::string>(node, "1365", "2"));
  ASSERT_EQ("101", evaluate<std::string>(node, "1365", "3"));
  ASSERT_EQ("0101", evaluate<std::string>(node, "1365", "4"));
  ASSERT_EQ("10101", evaluate<std::string>(node, "1365", "5"));
  ASSERT_EQ("010101", evaluate<std::string>(node, "1365", "6"));
  ASSERT_EQ("1010101", evaluate<std::string>(node, "1365", "7"));
  ASSERT_EQ("01010101", evaluate<std::string>(node, "1365", "8"));
  ASSERT_EQ("101010101", evaluate<std::string>(node, "1365", "9"));
  ASSERT_EQ("0101010101", evaluate<std::string>(node, "1365", "10"));
  ASSERT_EQ("10101010101", evaluate<std::string>(node, "1365", "11"));
  ASSERT_EQ("010101010101", evaluate<std::string>(node, "1365", "12"));
  ASSERT_EQ("0010101010101", evaluate<std::string>(node, "1365", "13"));
  ASSERT_EQ("00000000000000000000010101010101", evaluate<std::string>(node, "1365", "32"));
  ASSERT_EQ("", evaluate<std::string>(node, "4294967295", "0"));
  ASSERT_EQ("1", evaluate<std::string>(node, "4294967295", "1"));
  ASSERT_EQ("1111111111", evaluate<std::string>(node, "4294967295", "10"));
  ASSERT_EQ("1111111111111111111111111111111", evaluate<std::string>(node, "4294967295", "31"));
  ASSERT_EQ("11111111111111111111111111111111", evaluate<std::string>(node, "4294967295", "32"));
  ASSERT_EQ("0000000", evaluate<std::string>(node, "4294967040", "7"));
  ASSERT_EQ("00000000", evaluate<std::string>(node, "4294967040", "8"));
  ASSERT_EQ("100000000", evaluate<std::string>(node, "4294967040", "9"));
  ASSERT_EQ("1100000000", evaluate<std::string>(node, "4294967040", "10"));
  ASSERT_EQ("1111111111111111111111100000000", evaluate<std::string>(node, "4294967040", "31"));
  ASSERT_EQ("11111111111111111111111100000000", evaluate<std::string>(node, "4294967040", "32"));
}

TEST(BitFunctionsTest, BitFromString) {
  arangodb::aql::Function f("BIT_FROM_STRING", &Functions::BitFromString);
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
  expectFailed(node, "[]");
  expectFailed(node, "{}");
  expectFailed(node, "\" \"");
  expectFailed(node, "\"2\"");
  expectFailed(node, "\"02\"");
  expectFailed(node, "\"9\"");
  expectFailed(node, "\"12\"");
  expectFailed(node, "\"2102\"");
  expectFailed(node, "\"010101a\"");
  expectFailed(node, "\"010101b\"");
  expectFailed(node, "\"0b1\"");
  expectFailed(node, "\"0b10\"");
  expectFailed(node, "\"111111110000000011111111000000001\"");
  expectFailed(node, "\" 0\"");
  expectFailed(node, "\"0 \"");
  expectFailed(node, "\" 0 \"");
  expectFailed(node, "\"10 \"");
  expectFailed(node, "\"01 \"");
  expectFailed(node, "\" 01 \"");
  expectFailed(node, "\"111120 114\"");
  expectFailed(node, "\"0000000000000000000000000000000000000000000000000000000000000000000000000000000000\"");
  expectFailed(node, "\"00000000000000000000000000000000000000000000000000000000000001\"");
  expectFailed(node, "\"0000000010000000000000000000000000000000\"");
  expectFailed(node, "\"0000000000000000000000000000000100000000\"");
  
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "\"\""));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "\"0\""));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "\"00\""));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "\"000\""));
  ASSERT_EQ(int64_t(0), evaluate<int64_t>(node, "\"00000000000000000000000000000000\""));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "\"1\""));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "\"01\""));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "\"000001\""));
  ASSERT_EQ(int64_t(1), evaluate<int64_t>(node, "\"00000000000000000000000000000001\""));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "\"10\""));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "\"010\""));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "\"11\""));
  ASSERT_EQ(int64_t(3), evaluate<int64_t>(node, "\"0011\""));
  ASSERT_EQ(int64_t(2), evaluate<int64_t>(node, "\"00010\""));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "\"100\""));
  ASSERT_EQ(int64_t(4), evaluate<int64_t>(node, "\"0100\""));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "\"111\""));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "\"000000000111\""));
  ASSERT_EQ(int64_t(7), evaluate<int64_t>(node, "\"00000000000000000000000000000111\""));
  ASSERT_EQ(int64_t(21), evaluate<int64_t>(node, "\"10101\""));
  ASSERT_EQ(int64_t(16), evaluate<int64_t>(node, "\"10000\""));
  ASSERT_EQ(int64_t(32), evaluate<int64_t>(node, "\"100000\""));
  ASSERT_EQ(int64_t(64), evaluate<int64_t>(node, "\"1000000\""));
  ASSERT_EQ(int64_t(127), evaluate<int64_t>(node, "\"1111111\""));
  ASSERT_EQ(int64_t(128), evaluate<int64_t>(node, "\"10000000\""));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "\"11111111\""));
  ASSERT_EQ(int64_t(255), evaluate<int64_t>(node, "\"0000000011111111\""));
  ASSERT_EQ(int64_t(256), evaluate<int64_t>(node, "\"100000000\""));
  ASSERT_EQ(int64_t(65791), evaluate<int64_t>(node, "\"10000000011111111\""));
  ASSERT_EQ(int64_t(196863), evaluate<int64_t>(node, "\"110000000011111111\""));
  ASSERT_EQ(int64_t(1245439), evaluate<int64_t>(node, "\"100110000000011111111\""));
  ASSERT_EQ(int64_t(2147483648), evaluate<int64_t>(node, "\"10000000000000000000000000000000\""));
  ASSERT_EQ(int64_t(3221225472), evaluate<int64_t>(node, "\"11000000000000000000000000000000\""));
  ASSERT_EQ(int64_t(3221225472), evaluate<int64_t>(node, "\"11000000000000000000000000000000\""));
  ASSERT_EQ(int64_t(4294967294), evaluate<int64_t>(node, "\"11111111111111111111111111111110\""));
  ASSERT_EQ(int64_t(4294967295), evaluate<int64_t>(node, "\"11111111111111111111111111111111\""));
}
