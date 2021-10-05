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
/// @author Andrei Lobov
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

class NgramSimilarityFunctionTest : public ::testing::Test {
 public:
   NgramSimilarityFunctionTest()  {
    arangodb::tests::init();

  }

 protected:
  AqlValue evaluate(AqlValue const* attribute,
    AqlValue const* target,
    AqlValue const* ngram_size,
    std::set<int>* warnings = nullptr) {
    fakeit::Mock<ExpressionContext> expressionContextMock;
    ExpressionContext& expressionContext = expressionContextMock.get();
    fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([warnings](ErrorCode c, char const*) {
      if (warnings) {
        warnings->insert(static_cast<int>(c));
      }});
    TRI_vocbase_t mockVocbase(TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto trx = server.createFakeTransaction();
    fakeit::When(Method(expressionContextMock, trx)).AlwaysDo([&]() -> transaction::Methods& {
      return *trx;
    });
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{ arena };
    if (attribute) {
      params.emplace_back(*attribute);
    }
    if (target) {
      params.emplace_back(*target);
    }
    if (ngram_size) {
      params.emplace_back(*ngram_size);
    }
    
    arangodb::aql::Function f("NGRAM_SIMILARITY", &Functions::NgramSimilarity);
    arangodb::aql::AstNode node(NODE_TYPE_FCALL);
    node.setData(static_cast<void const*>(&f));

    return Functions::NgramSimilarity(&expressionContext, node, params);
  }

  void assertNgramSimilarityFail(size_t line,
    std::set<int> const& expected_warnings,
    AqlValue const* attribute,
    AqlValue const* target,
    AqlValue const* ngram_size) {
    SCOPED_TRACE(testing::Message("assertNgramSimilarityFail failed on line:") << line);
    std::set<int> warnings;
    ASSERT_TRUE(evaluate(attribute, target, ngram_size, &warnings).isNull(false));
    ASSERT_EQ(expected_warnings, warnings);
  }

  void assertNgramSimilarity(size_t line,
    double expectedValue,
    AqlValue const* attribute,
    AqlValue const* target,
    AqlValue const* ngram_size) {
    SCOPED_TRACE(testing::Message("assertNgramSimilarity failed on line:") << line);
    std::set<int> warnings;
    auto value = evaluate(attribute, target, ngram_size, &warnings);
    ASSERT_TRUE(warnings.empty());
    ASSERT_TRUE(value.isNumber());
    ASSERT_DOUBLE_EQ(expectedValue, value.toDouble());
  }

 private:
  arangodb::tests::mocks::MockAqlServer server;
};

TEST_F(NgramSimilarityFunctionTest, test) {
  { // invalid cases
    AqlValue const InvalidBool{ AqlValueHintBool{true} };
    AqlValue const InvalidNull{ AqlValueHintNull{} };
    AqlValue const InvalidInt{ AqlValueHintInt{0} };
    AqlValue const InvalidArray{ AqlValueHintEmptyArray{} };
    AqlValue const InvalidObject{ AqlValueHintEmptyObject{} };
    AqlValue const ValidString{ "ValidString" };
    AqlValue const ValidInt{ AqlValueHintInt{5} };

    const std::set<int> badParamWarning{ static_cast<int>(TRI_ERROR_BAD_PARAMETER) };
    const std::set<int> typeMismatchWarning{ static_cast<int>(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) };
    const std::set<int> invalidArgsCount{ static_cast<int>(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH) };

    //invalid args count
    assertNgramSimilarityFail(__LINE__, invalidArgsCount, &ValidString, &ValidString, nullptr);
    assertNgramSimilarityFail(__LINE__, invalidArgsCount, &ValidString, nullptr, nullptr);
    assertNgramSimilarityFail(__LINE__, invalidArgsCount, nullptr, nullptr, nullptr);

    // invalid attribute
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &InvalidBool, &ValidString, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &InvalidNull, &ValidString, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &InvalidInt, &ValidString, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &InvalidArray, &ValidString, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &InvalidObject, &ValidString, &ValidInt);

    //invalid target
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidBool, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidNull, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidInt, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidArray, &ValidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidObject, &ValidInt);

    //invalid ngram_size
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidBool);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidNull);
    assertNgramSimilarityFail(__LINE__, badParamWarning, &ValidString, &ValidString, &InvalidInt);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidArray);
    assertNgramSimilarityFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidObject);
  }

  {
    AqlValue const ValidString{ "ValidString" };
    // no match
    {
      AqlValue const Attribute{ "abcd" };
      AqlValue const Target{ "efgh" };
      for (int i = 2; i <= 5; ++i) {
        SCOPED_TRACE(testing::Message("Ngram size is ") << i);
        AqlValue const ValidInt{ AqlValueHintInt{i} };
        assertNgramSimilarity(__LINE__, 0, &Attribute, &Target, &ValidInt);
        assertNgramSimilarity(__LINE__, 0, &Target, &Attribute, &ValidInt);
      }
    }
    //different length
    {
      AqlValue const Target{ "aplejuice" };
      AqlValue const Attribute{ "applejuice" };
      std::vector<double> expected{ 1, 1, 6.f / 7.f,  5.f / 6.f, 4.f / 5.f };
      std::vector<double> expected_rev{ 0.9f, 8.f / 9.f, 0.75f, 5.f / 7.f, 4.f / 6.f };
      for (int i = 1; i <= 5; ++i) {
        SCOPED_TRACE(testing::Message("Ngram size is ") << i);
        AqlValue const ValidInt{ AqlValueHintInt{i} };
        assertNgramSimilarity(__LINE__, expected[i-1], &Attribute, &Target, &ValidInt);
        assertNgramSimilarity(__LINE__, expected_rev[i-1], &Target, &Attribute, &ValidInt);
      }
    }
    // with gaps
    {
      AqlValue const Attribute{ "apple1234juice" };
      AqlValue const Target{ "aple567juice" };
      std::vector<double> expected{ 9.f / 12.f, 7.f / 11.f, 4.f / 10.f, 2.f / 9.f, 1.f / 8.f };
      std::vector<double> expected_rev{ 9.f / 14.f, 7.f / 13.f, 4.f / 12.f, 2.f / 11.f, 1.f / 10.f };
      for (int i = 1; i <= 5; ++i) {
        SCOPED_TRACE(testing::Message("Ngram size is ") << i);
        AqlValue const ValidInt{ AqlValueHintInt{i} };
        assertNgramSimilarity(__LINE__, expected[i - 1], &Attribute, &Target, &ValidInt);
        assertNgramSimilarity(__LINE__, expected_rev[i - 1], &Target, &Attribute, &ValidInt);
      }
    }
    // empty strings
    {
      AqlValue const Attribute{ "" };
      AqlValue const Target{ "" };
      for (int i = 1; i <= 5; ++i) {
        SCOPED_TRACE(testing::Message("Ngram size is ") << i);
        AqlValue const ValidInt{ AqlValueHintInt{i} };
        assertNgramSimilarity(__LINE__, 1, &Attribute, &Target, &ValidInt);
        assertNgramSimilarity(__LINE__, 1, &Target, &Attribute, &ValidInt);
        assertNgramSimilarity(__LINE__, 0, &ValidString, &Target, &ValidInt);
        assertNgramSimilarity(__LINE__, 0, &Target, &ValidString, &ValidInt);
      }
    }
    // less than ngram size
    {
      AqlValue const Attribute{ "a" };
      AqlValue const Target{ "b" };
      AqlValue const Target2{ "a" };
      for (int i = 1; i <= 5; ++i) {
        SCOPED_TRACE(testing::Message("Ngram size is ") << i);
        AqlValue const ValidInt{ AqlValueHintInt{i} };
        assertNgramSimilarity(__LINE__, 0, &Attribute, &Target, &ValidInt);
        assertNgramSimilarity(__LINE__, 0, &Target, &Attribute, &ValidInt);
        assertNgramSimilarity(__LINE__, 1, &Attribute, &Target2, &ValidInt);
        assertNgramSimilarity(__LINE__, 1, &Target2, &Attribute, &ValidInt);
      }
    }
  }
}
