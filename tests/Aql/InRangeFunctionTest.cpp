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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/ExpressionContext.h"
#include "Aql/Functions.h"
#include "Containers/SmallVector.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "IResearch/common.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <set>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

class InRangeFunctionTest : public ::testing::Test {
 public:
  InRangeFunctionTest() {
    arangodb::tests::init();
  }

 protected:
  AqlValue evaluate(AqlValue const* attribute,
    AqlValue const* lower,
    AqlValue const* upper,
    AqlValue const* includeLower,
    AqlValue const* includeUpper,
    std::set<int>* warnings = nullptr) {
    fakeit::Mock<ExpressionContext> expressionContextMock;
    ExpressionContext& expressionContext = expressionContextMock.get();
    fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([warnings](int c, char const*) {
      if (warnings) {
        warnings->insert(c);
      }});
    fakeit::Mock<transaction::Context> trxCtxMock;
    fakeit::When(Method(trxCtxMock, getVPackOptions)).AlwaysDo([]() {
      static VPackOptions options;
      return &options;
      });
    TRI_vocbase_t mockVocbase(TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto trx = server.createFakeTransaction();
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{ arena };
    if (attribute) {
      params.emplace_back(*attribute);
    }
    if (lower) {
      params.emplace_back(*lower);
    }
    if (upper) {
      params.emplace_back(*upper);
    }
    if (includeLower) {
      params.emplace_back(*includeLower);
    }
    if (includeUpper) {
      params.emplace_back(*includeUpper);
    }
    return Functions::InRange(&expressionContext, trx.get(), params);
  }

  void assertInRangeFail(size_t line,
    std::set<int> const& expected_warnings,
    AqlValue const* attribute,
    AqlValue const* lower,
    AqlValue const* upper,
    AqlValue const* includeLower,
    AqlValue const* includeUpper) {
    SCOPED_TRACE(testing::Message("assertInRangeFail failed on line:") << line);
    std::set<int> warnings;
    ASSERT_TRUE(evaluate(attribute, lower, upper, includeLower, includeUpper, &warnings).isNull(false));
    ASSERT_EQ(expected_warnings, warnings);
  }

  void assertInRange(size_t line,
    bool expectedValue,
    AqlValue const* attribute,
    AqlValue const* lower,
    AqlValue const* upper,
    bool includeLower,
    bool includeUpper) {
    SCOPED_TRACE(testing::Message("assertInRange failed on line:") << line);
    std::set<int> warnings;
    AqlValue includeLowerAql{ AqlValueHintBool(includeLower) };
    AqlValue includeUpperAql{ AqlValueHintBool(includeUpper) };
    auto value = evaluate(attribute, lower, upper,
      &includeLowerAql,
      &includeUpperAql, &warnings);
    ASSERT_TRUE(warnings.empty());
    ASSERT_TRUE(value.isBoolean());
    ASSERT_EQ(expectedValue, value.toBoolean());
  }

 private:
  arangodb::tests::mocks::MockAqlServer server;
};

TEST_F(InRangeFunctionTest, testValidArgs) {
  // strings
  {
    AqlValue foo("foo");
    AqlValue boo("boo");
    AqlValue poo("poo");
    assertInRange(__LINE__, true, &foo, &boo, &poo, true, true);
    assertInRange(__LINE__, false, &foo, &poo, &boo, true, true);
    assertInRange(__LINE__, true, &foo, &foo, &poo, true, true);
    assertInRange(__LINE__, true, &foo, &foo, &poo, true, false);
    assertInRange(__LINE__, false, &foo, &foo, &poo, false, true);
    assertInRange(__LINE__, true, &foo, &boo, &foo, true, true);
    assertInRange(__LINE__, true, &foo, &boo, &foo, false, true);
    assertInRange(__LINE__, false, &foo, &boo, &foo, true, false);
  }
  // non ASCII
  {
    AqlValue foo("ПУИ");
    AqlValue boo("ПУЗ");
    AqlValue poo("ПУЙ");
    assertInRange(__LINE__, true, &foo, &boo, &poo, true, true);
    assertInRange(__LINE__, false, &foo, &poo, &boo, true, true);
    assertInRange(__LINE__, true, &foo, &foo, &poo, true, true);
    assertInRange(__LINE__, true, &foo, &foo, &poo, true, false);
    assertInRange(__LINE__, false, &foo, &foo, &poo, false, true);
    assertInRange(__LINE__, true, &foo, &boo, &foo, true, true);
    assertInRange(__LINE__, true, &foo, &boo, &foo, false, true);
    assertInRange(__LINE__, false, &foo, &boo, &foo, true, false);
  }
  // numbers
  {
    AqlValue foo{ AqlValueHintInt(5) };
    AqlValue boo{ AqlValueHintDouble(4.9999) };
    AqlValue poo{ AqlValueHintDouble(5.0001) };
    assertInRange(__LINE__, true, &foo, &boo, &poo, true, true);
    assertInRange(__LINE__, false, &foo, &poo, &boo, true, true);
    assertInRange(__LINE__, true, &foo, &foo, &poo, true, true);
    assertInRange(__LINE__, true, &foo, &foo, &poo, true, false);
    assertInRange(__LINE__, false, &foo, &foo, &poo, false, true);
    assertInRange(__LINE__, true, &foo, &boo, &foo, true, true);
    assertInRange(__LINE__, true, &foo, &boo, &foo, false, true);
    assertInRange(__LINE__, false, &foo, &boo, &foo, true, false);
  }
  // type mix
  {
    AqlValue const Int5{ AqlValueHintInt(5) };
    AqlValue const NullVal{ AqlValueHintNull{} };
    AqlValue const ArrayVal{ AqlValueHintEmptyArray{} };
    AqlValue const ObjectVal{ AqlValueHintEmptyObject{} };
    AqlValue const StringVal("foo");
    assertInRange(__LINE__, true, &StringVal, &NullVal, &ObjectVal, true, true);
    assertInRange(__LINE__, true, &StringVal, &NullVal, &ArrayVal, true, true);
    assertInRange(__LINE__, false, &StringVal, &ObjectVal, &NullVal, true, true);
    assertInRange(__LINE__, false, &StringVal, &ArrayVal, &NullVal, true, true);
    assertInRange(__LINE__, false, &StringVal, &ObjectVal, &ArrayVal, true, true);
    assertInRange(__LINE__, false, &StringVal, &ArrayVal, &ObjectVal, true, true);
    assertInRange(__LINE__, false, &StringVal, &NullVal, &Int5, true, true);
    assertInRange(__LINE__, true, &StringVal, &NullVal, &StringVal, true, true);
    assertInRange(__LINE__, false, &StringVal, &StringVal, &NullVal, true, true);
    assertInRange(__LINE__, false, &StringVal, &StringVal, &Int5, true, true);
    assertInRange(__LINE__, true, &Int5, &NullVal, &StringVal, true, true);
    assertInRange(__LINE__, false, &Int5, &ArrayVal, &StringVal, true, true);
    assertInRange(__LINE__, true, &Int5, &NullVal, &ArrayVal, true, true);
    assertInRange(__LINE__, true, &Int5, &NullVal, &ObjectVal, true, true);
    assertInRange(__LINE__, false, &ArrayVal, &NullVal, &StringVal, true, true);
    assertInRange(__LINE__, true, &ArrayVal, &NullVal, &ObjectVal, true, true);
    assertInRange(__LINE__, true, &ArrayVal, &StringVal, &ObjectVal, true, true);
    assertInRange(__LINE__, true, &ArrayVal, &Int5, &ObjectVal, true, true);
    assertInRange(__LINE__, true, &ObjectVal, &Int5, &ObjectVal, true, true);
    assertInRange(__LINE__, false, &ObjectVal, &Int5, &ObjectVal, true, false);
  }
}

TEST_F(InRangeFunctionTest, testInvalidArgs) {
  const std::set<int> typeMismatchWarning{ TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH };
  const std::set<int> invalidArgsCount{ TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH };
  AqlValue const ValidString{ "ValidString" };
  AqlValue const ValidBool{ AqlValueHintBool{true} };
  assertInRangeFail(__LINE__, invalidArgsCount, &ValidString, &ValidString, &ValidString, &ValidBool, nullptr);
  assertInRangeFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &ValidString, &ValidBool, &ValidString);
  assertInRangeFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &ValidString, &ValidString, &ValidBool);
}
