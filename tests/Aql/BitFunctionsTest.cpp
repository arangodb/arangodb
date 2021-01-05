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

AqlValue evaluate(AstNode const& node, AqlValue const& input) {
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

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params{arena};
  params.emplace_back(input);

  auto f = static_cast<arangodb::aql::Function const*>(node.getData());
  return f->implementation(&expressionContext, node, params);
}

int64_t evaluate(AstNode const& node, char const* input) {
  auto b = arangodb::velocypack::Parser::fromJson(input);

  AqlValue a(b->slice());
  AqlValueGuard guard(a, true);

  AqlValue actual = evaluate(node, a);
  EXPECT_TRUE(actual.isNumber());
  return actual.toInt64();
}

void expectFailed(AstNode const& node, char const* input) {
  auto b = arangodb::velocypack::Parser::fromJson(input);

  AqlValue a(b->slice());
  AqlValueGuard guard(a, true);

  AqlValue actual = evaluate(node, a);
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
  expectFailed(node, "[9223372036854775808]");
  expectFailed(node, "[18446744073709551615]");

  ASSERT_EQ(int64_t(0), evaluate(node, "[]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[null]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[null, null, null, null]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 0]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 0, 0]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, null]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, null, null]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[null, null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, 1, 1]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 1, 0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[1, 2, 4, 8, 16, 32]"));
  ASSERT_EQ(int64_t(15), evaluate(node, "[255, 15, 255, 15, 255, 15]"));
  ASSERT_EQ(int64_t(15), evaluate(node, "[15, 255, 15, 255, 15, 255]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[15, 255, 15, 255, 15, 255, 1]"));
  ASSERT_EQ(int64_t(2), evaluate(node, "[15, 255, 15, 255, 15, 255, 2]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[15, 255, 15, 255, 15, 255, 16]"));
  ASSERT_EQ(int64_t(65), evaluate(node, "[65]"));
  ASSERT_EQ(int64_t(65), evaluate(node, "[65, null]"));
  ASSERT_EQ(int64_t(256), evaluate(node, "[256]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[256, 0]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[256, 1]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[256, 2]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[256, 3]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[256, 4]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[256, 128]"));
  ASSERT_EQ(int64_t(256), evaluate(node, "[256, 256]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[255, 0]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[255, 1]"));
  ASSERT_EQ(int64_t(2), evaluate(node, "[255, 2]"));
  ASSERT_EQ(int64_t(3), evaluate(node, "[255, 3]"));
  ASSERT_EQ(int64_t(4), evaluate(node, "[255, 4]"));
  ASSERT_EQ(int64_t(128), evaluate(node, "[255, 128]"));
  ASSERT_EQ(int64_t(129), evaluate(node, "[255, 129]"));
  ASSERT_EQ(int64_t(130), evaluate(node, "[255, 130]"));
  ASSERT_EQ(int64_t(131), evaluate(node, "[255, 131]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[255, 256]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[255, 257]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 65535]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[65535, 255]"));
  ASSERT_EQ(int64_t(65535), evaluate(node, "[65535, 65535]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[65535, 65536]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[65536, 65535]"));
  ASSERT_EQ(int64_t(8589934591), evaluate(node, "[8589934591]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[8589934591, 1]"));
  ASSERT_EQ(int64_t(8589934590), evaluate(node, "[8589934591, 8589934590]"));
  ASSERT_EQ(int64_t(8589934590), evaluate(node, "[8589934591, 8589934590, null, null]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807, 9223372036854775807]"));
  ASSERT_EQ(int64_t(9223372036854775806), evaluate(node, "[9223372036854775807, 9223372036854775806]"));
  ASSERT_EQ(int64_t(65534), evaluate(node, "[9223372036854775807, 9223372036854775806, 65535]"));
  ASSERT_EQ(int64_t(512), evaluate(node, "[84790232, 88623582851, 79884822, 27781056, 9960235642]"));
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
  expectFailed(node, "[9223372036854775808]");
  expectFailed(node, "[18446744073709551615]");

  ASSERT_EQ(int64_t(0), evaluate(node, "[]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[null]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[null, null, null, null]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 0]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 0, 0]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, null]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, null, null]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[null, null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, 1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[0, 1, 0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(7), evaluate(node, "[0, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(7), evaluate(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(63), evaluate(node, "[1, 2, 4, 8, 16, 32]"));
  ASSERT_EQ(int64_t(65535), evaluate(node, "[1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 15, 255, 15, 255, 15]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[15, 255, 15, 255, 15, 255]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[15, 255, 15, 255, 15, 255, 1]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[15, 255, 15, 255, 15, 255, 2]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[15, 255, 15, 255, 15, 255, 16]"));
  ASSERT_EQ(int64_t(65), evaluate(node, "[65]"));
  ASSERT_EQ(int64_t(65), evaluate(node, "[65, null]"));
  ASSERT_EQ(int64_t(256), evaluate(node, "[256]"));
  ASSERT_EQ(int64_t(256), evaluate(node, "[256, 0]"));
  ASSERT_EQ(int64_t(257), evaluate(node, "[256, 1]"));
  ASSERT_EQ(int64_t(258), evaluate(node, "[256, 2]"));
  ASSERT_EQ(int64_t(259), evaluate(node, "[256, 3]"));
  ASSERT_EQ(int64_t(260), evaluate(node, "[256, 4]"));
  ASSERT_EQ(int64_t(384), evaluate(node, "[256, 128]"));
  ASSERT_EQ(int64_t(256), evaluate(node, "[256, 256]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 0]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 1]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 2]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 3]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 4]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 128]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 129]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 130]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 131]"));
  ASSERT_EQ(int64_t(511), evaluate(node, "[255, 256]"));
  ASSERT_EQ(int64_t(511), evaluate(node, "[255, 257]"));
  ASSERT_EQ(int64_t(65535), evaluate(node, "[255, 65535]"));
  ASSERT_EQ(int64_t(65535), evaluate(node, "[65535, 255]"));
  ASSERT_EQ(int64_t(65535), evaluate(node, "[65535, 65535]"));
  ASSERT_EQ(int64_t(131071), evaluate(node, "[65535, 65536]"));
  ASSERT_EQ(int64_t(131071), evaluate(node, "[65536, 65535]"));
  ASSERT_EQ(int64_t(8589934591), evaluate(node, "[8589934591]"));
  ASSERT_EQ(int64_t(8589934591), evaluate(node, "[8589934591, 1]"));
  ASSERT_EQ(int64_t(8589934591), evaluate(node, "[8589934591, 8589934590]"));
  ASSERT_EQ(int64_t(8589934591), evaluate(node, "[8589934591, 8589934590, null]"));
  ASSERT_EQ(int64_t(8589934591), evaluate(node, "[null, 8589934591, 8589934590, null]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807, 9223372036854775807]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807, 9223372036854775806]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807, 9223372036854775806, 65535]"));
  ASSERT_EQ(int64_t(98648981503), evaluate(node, "[84790232, 88623582851, 79884822, 27781056, 9960235642]"));
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
  expectFailed(node, "[9223372036854775808]");
  expectFailed(node, "[18446744073709551615]");

  ASSERT_EQ(int64_t(0), evaluate(node, "[]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[null]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[null, null, null, null]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 0]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 0, 0]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, null]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[null, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, null, null]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[null, null, 1]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, 1, 1]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[1, 1, 1, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[1, 1, 1, 1, 1]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[0, 1, 0, 1, 0, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[0, 1]"));
  ASSERT_EQ(int64_t(3), evaluate(node, "[0, 1, 2]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 1, 2, 3]"));
  ASSERT_EQ(int64_t(4), evaluate(node, "[0, 1, 2, 3, 4]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[0, 1, 2, 3, 4, 5]"));
  ASSERT_EQ(int64_t(7), evaluate(node, "[0, 1, 2, 3, 4, 5, 6]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3]"));
  ASSERT_EQ(int64_t(7), evaluate(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7]"));
  ASSERT_EQ(int64_t(63), evaluate(node, "[1, 2, 4, 8, 16, 32]"));
  ASSERT_EQ(int64_t(65535), evaluate(node, "[1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]"));
  ASSERT_EQ(int64_t(240), evaluate(node, "[255, 15]"));
  ASSERT_EQ(int64_t(15), evaluate(node, "[255, 15, 255]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[255, 15, 255, 15]"));
  ASSERT_EQ(int64_t(114), evaluate(node, "[255, 12, 129]"));
  ASSERT_EQ(int64_t(66), evaluate(node, "[255, 12, 129, 48]"));
  ASSERT_EQ(int64_t(65), evaluate(node, "[65]"));
  ASSERT_EQ(int64_t(65), evaluate(node, "[65, null]"));
  ASSERT_EQ(int64_t(256), evaluate(node, "[256]"));
  ASSERT_EQ(int64_t(256), evaluate(node, "[256, 0]"));
  ASSERT_EQ(int64_t(257), evaluate(node, "[256, 1]"));
  ASSERT_EQ(int64_t(258), evaluate(node, "[256, 2]"));
  ASSERT_EQ(int64_t(259), evaluate(node, "[256, 3]"));
  ASSERT_EQ(int64_t(260), evaluate(node, "[256, 4]"));
  ASSERT_EQ(int64_t(384), evaluate(node, "[256, 128]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[256, 256]"));
  ASSERT_EQ(int64_t(255), evaluate(node, "[255, 0]"));
  ASSERT_EQ(int64_t(254), evaluate(node, "[255, 1]"));
  ASSERT_EQ(int64_t(253), evaluate(node, "[255, 2]"));
  ASSERT_EQ(int64_t(252), evaluate(node, "[255, 3]"));
  ASSERT_EQ(int64_t(251), evaluate(node, "[255, 4]"));
  ASSERT_EQ(int64_t(127), evaluate(node, "[255, 128]"));
  ASSERT_EQ(int64_t(126), evaluate(node, "[255, 129]"));
  ASSERT_EQ(int64_t(125), evaluate(node, "[255, 130]"));
  ASSERT_EQ(int64_t(124), evaluate(node, "[255, 131]"));
  ASSERT_EQ(int64_t(511), evaluate(node, "[255, 256]"));
  ASSERT_EQ(int64_t(510), evaluate(node, "[255, 257]"));
  ASSERT_EQ(int64_t(65280), evaluate(node, "[255, 65535]"));
  ASSERT_EQ(int64_t(65280), evaluate(node, "[65535, 255]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[65535, 65535]"));
  ASSERT_EQ(int64_t(131071), evaluate(node, "[65535, 65536]"));
  ASSERT_EQ(int64_t(131071), evaluate(node, "[65536, 65535]"));
  ASSERT_EQ(int64_t(8589934591), evaluate(node, "[8589934591]"));
  ASSERT_EQ(int64_t(8589934590), evaluate(node, "[8589934591, 1]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[8589934591, 8589934590]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[null, 9223372036854775807]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[9223372036854775807, null]"));
  ASSERT_EQ(int64_t(9223372036854775807), evaluate(node, "[null, 9223372036854775807, null]"));
  ASSERT_EQ(int64_t(0), evaluate(node, "[9223372036854775807, 9223372036854775807]"));
  ASSERT_EQ(int64_t(1), evaluate(node, "[9223372036854775807, 9223372036854775806]"));
  ASSERT_EQ(int64_t(2), evaluate(node, "[9223372036854775807, 9223372036854775805]"));
  ASSERT_EQ(int64_t(2), evaluate(node, "[9223372036854775807, 9223372036854775805, null]"));
  ASSERT_EQ(int64_t(65534), evaluate(node, "[9223372036854775807, 9223372036854775806, 65535]"));
  ASSERT_EQ(int64_t(98576986871), evaluate(node, "[84790232, 88623582851, 79884822, 27781056, 9960235642]"));
  ASSERT_EQ(int64_t(98576986871), evaluate(node, "[null, 84790232, null, 88623582851, null, 79884822, null, 27781056, 9960235642]"));
}

TEST(BitFunctionsTest, BitCount) {
  arangodb::aql::Function f("BIT_COUNT", &Functions::BitCount);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));
  
  expectFailed(node, "null");
  expectFailed(node, "false");
  expectFailed(node, "true");
  expectFailed(node, "-1");
  expectFailed(node, "-1000");
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

  ASSERT_EQ(int64_t(0), evaluate(node, "0"));
  ASSERT_EQ(int64_t(1), evaluate(node, "1"));
  ASSERT_EQ(int64_t(1), evaluate(node, "2"));
  ASSERT_EQ(int64_t(2), evaluate(node, "3"));
  ASSERT_EQ(int64_t(1), evaluate(node, "4"));
  ASSERT_EQ(int64_t(2), evaluate(node, "5"));
  ASSERT_EQ(int64_t(2), evaluate(node, "6"));
  ASSERT_EQ(int64_t(3), evaluate(node, "7"));
  ASSERT_EQ(int64_t(1), evaluate(node, "8"));
  ASSERT_EQ(int64_t(4), evaluate(node, "15"));
  ASSERT_EQ(int64_t(1), evaluate(node, "16"));
  ASSERT_EQ(int64_t(8), evaluate(node, "255"));
  ASSERT_EQ(int64_t(1), evaluate(node, "256"));
  ASSERT_EQ(int64_t(2), evaluate(node, "257"));
  ASSERT_EQ(int64_t(2), evaluate(node, "384"));
  ASSERT_EQ(int64_t(9), evaluate(node, "511"));
  ASSERT_EQ(int64_t(15), evaluate(node, "65534"));
  ASSERT_EQ(int64_t(16), evaluate(node, "65535"));
  ASSERT_EQ(int64_t(1), evaluate(node, "65536"));
  ASSERT_EQ(int64_t(12), evaluate(node, "1234567890"));
  ASSERT_EQ(int64_t(20), evaluate(node, "12345678901"));
  ASSERT_EQ(int64_t(63), evaluate(node, "9223372036854775807"));
  ASSERT_EQ(int64_t(62), evaluate(node, "9223372036854775806"));
  ASSERT_EQ(int64_t(62), evaluate(node, "9223372036854775805"));
  ASSERT_EQ(int64_t(61), evaluate(node, "9223372036854775804"));
  ASSERT_EQ(int64_t(62), evaluate(node, "9223372036854775803"));
  ASSERT_EQ(int64_t(61), evaluate(node, "9223372036854775802"));
  ASSERT_EQ(int64_t(61), evaluate(node, "9223372036854775801"));
  ASSERT_EQ(int64_t(60), evaluate(node, "9223372036854775800"));
}
