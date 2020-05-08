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
#include "IResearch/IResearchAnalyzerFeature.h"
#include "analysis/token_attributes.hpp"
#include "Mocks/Servers.h"
#include "VocBase/Methods/Collections.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <set>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

class NgramMatchFunctionTest : public ::testing::Test {
 public:
  NgramMatchFunctionTest()  {
    arangodb::tests::init();

    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::methods::Collections::createSystem(server.getSystemDatabase(), arangodb::tests::AnalyzerCollectionName,
      false, unused);

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto res =
      analyzers.emplace(result, TwoGramAnalyzer(), "ngram",
        VPackParser::fromJson("{\"min\":2, \"max\":2, \"streamType\":\"utf8\", \"preserveOriginal\":false}")->slice(),
        irs::flags{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get() });
    EXPECT_TRUE(res.ok());
  }

 protected:
  constexpr char const* TwoGramAnalyzer() { return "_system::myngram"; }

  AqlValue evaluate(AqlValue const* &Attribute,
    AqlValue const* Target,
    AqlValue const* analyzer,
    AqlValue const* threshold = nullptr,
    std::set<int>* warnings = nullptr) {
    fakeit::Mock<ExpressionContext> expressionContextMock;
    ExpressionContext& expressionContext = expressionContextMock.get();
    fakeit::When(Method(expressionContextMock, registerWarning)).AlwaysDo([warnings](int c, char const*) {
      if (warnings) {
        warnings->insert(c);
      }});
    auto trx = server.createFakeTransaction();
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{ arena };
    if (Attribute) {
      params.emplace_back(*Attribute);
    }
    if (Target) {
      params.emplace_back(*Target);
    }
    if (threshold) {
      params.emplace_back(*threshold);
    }
    if (analyzer) {
      params.emplace_back(*analyzer);
    }
    return Functions::NgramMatch(&expressionContext, trx.get(), params);
  }

  void assertNgramMatchFail(size_t line,
    std::set<int> const& expected_warnings,
    AqlValue const* Attribute,
    AqlValue const* Target,
    AqlValue const* analyzer,
    AqlValue const* threshold = nullptr) {
    SCOPED_TRACE(testing::Message("assertNgramMatchFail failed on line:") << line);
    std::set<int> warnings;
    ASSERT_TRUE(evaluate(Attribute, Target, analyzer, threshold, &warnings).isNull(false));
    ASSERT_EQ(expected_warnings, warnings);
  }

  void assertNgramMatch(size_t line,
    bool expectedValue,
    AqlValue const* Attribute,
    AqlValue const* Target,
    AqlValue const* analyzer,
    AqlValue const* threshold = nullptr) {
    SCOPED_TRACE(testing::Message("assertNgramMatch failed on line:") << line);
    std::set<int> warnings;
    auto value = evaluate(Attribute, Target, analyzer, threshold, &warnings);
    ASSERT_TRUE(warnings.empty());
    ASSERT_TRUE(value.isBoolean());
    ASSERT_EQ(expectedValue, value.toBoolean());
  }

 private:
  arangodb::tests::mocks::MockAqlServer server;
};

TEST_F(NgramMatchFunctionTest, test) {
  { // invalid cases
    AqlValue const InvalidBool{ AqlValueHintBool{true} };
    AqlValue const InvalidNull{ AqlValueHintNull{} };
    AqlValue const InvalidInt{ AqlValueHintInt{5} };
    AqlValue const InvalidArray{ AqlValueHintEmptyArray{} };
    AqlValue const InvalidObject{ AqlValueHintEmptyObject{} };
    AqlValue const LowThreshold{ AqlValueHintInt{0} };
    AqlValue const HighThreshold{ AqlValueHintDouble{1.1} };
    AqlValue const ValidThreshold{ AqlValueHintDouble{0.5} };
    AqlValue const ValidString{ "ValidString" };
    AqlValue const ValidAnalyzerName{ "TestAnalyzer" };
    const std::set<int> badParamWarning{ TRI_ERROR_BAD_PARAMETER };
    const std::set<int> typeMismatchWarning{ TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH };
    const std::set<int> invalidArgsCount{ TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH };

    //invalid args count
    assertNgramMatchFail(__LINE__, invalidArgsCount, &ValidString, &ValidString, nullptr, nullptr);

    // invalid Attribute
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &InvalidBool, &ValidString, &ValidString, nullptr);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &InvalidBool, &ValidString, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &InvalidNull, &ValidString, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &InvalidInt, &ValidString, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &InvalidArray, &ValidString, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &InvalidObject, &ValidString, &ValidString, &ValidThreshold);

    //invalid Target
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidBool, &ValidString, nullptr);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidBool, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidNull, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidInt, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidArray, &ValidString, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &InvalidObject, &ValidString, &ValidThreshold);

    //invalid analyzer
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidBool, nullptr);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidBool, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidNull, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidInt, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidArray, &ValidThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &InvalidObject, &ValidThreshold);
    // Valid string is invalid analyzer name
    assertNgramMatchFail(__LINE__, badParamWarning, &ValidString, &ValidString, &ValidString, &ValidThreshold);

    // invalid threshold
    assertNgramMatchFail(__LINE__, badParamWarning, &ValidString, &ValidString, &ValidString, &LowThreshold);
    assertNgramMatchFail(__LINE__, badParamWarning, &ValidString, &ValidString, &ValidString, &HighThreshold);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &ValidString, &ValidString);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &ValidString, &InvalidBool);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &ValidString, &InvalidNull);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &ValidString, &InvalidArray);
    assertNgramMatchFail(__LINE__, typeMismatchWarning, &ValidString, &ValidString, &ValidString, &InvalidObject);
  }


  {
    AqlValue ValidAnalyzerName{ TwoGramAnalyzer() };
    AqlValueGuard guard(ValidAnalyzerName, true);

    AqlValue const Threshold09{ AqlValueHintDouble{0.9} };
    AqlValue const Threshold05{ AqlValueHintDouble{0.5} };
    AqlValue const Threshold02{ AqlValueHintDouble{0.2} };
    AqlValue const ValidString{ "&ValidString" };
    // simple
    {
      AqlValue const Attribute{ "aecd" };
      AqlValue const Target{ "abcd" };
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName);
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName, &Threshold05);
      assertNgramMatch(__LINE__, true, &Attribute, &Target, &ValidAnalyzerName, &Threshold02);
    }
    // no match
    {
      AqlValue const Attribute{ "abcd" };
      AqlValue const Target{ "efgh" };
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName);
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName, &Threshold05);
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName, &Threshold02);
    }
    //different length
    {
      AqlValue const Attribute{ "aplejuice" };
      AqlValue const Target{ "applejuice" };
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName, &Threshold09);
      assertNgramMatch(__LINE__, true, &Attribute, &Target, &ValidAnalyzerName);
      assertNgramMatch(__LINE__, true, &Target, &Attribute, &ValidAnalyzerName, &Threshold05);

      assertNgramMatch(__LINE__, true, &Target, &Attribute, &ValidAnalyzerName, &Threshold09);
      assertNgramMatch(__LINE__, true, &Target, &Attribute, &ValidAnalyzerName);
      assertNgramMatch(__LINE__, true, &Target, &Attribute, &ValidAnalyzerName, &Threshold05);
    }
    // with gaps
    {
      AqlValue const Attribute{ "apple1234juice" };
      AqlValue const Target{ "aple567juice" };
      AqlValue const Threshold064{ AqlValueHintDouble{0.64} };
      AqlValue const Threshold062{ AqlValueHintDouble{0.62} };
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName, &Threshold09);
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName);
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName, &Threshold064);
      assertNgramMatch(__LINE__, true, &Attribute, &Target, &ValidAnalyzerName, &Threshold062);
    }
    // empty strings
    {
      AqlValue const Attribute{ "" };
      AqlValue const Target{ "" };
      // two empty strings never matches to mimic search results
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName);
      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName, &Threshold02);

      // even with low threshold empty vs non-empty don`t match
      assertNgramMatch(__LINE__, false, &ValidString, &Target, &ValidAnalyzerName, &Threshold02);
      assertNgramMatch(__LINE__, false, &Attribute, &ValidString, &ValidAnalyzerName, &Threshold02);
    }
    // less than ngram size (no ngrams emitted as PreserveOriginal is false for test analyzer)
    {
      AqlValue const Attribute{ "a" };
      AqlValue const Target{ "b" };
      AqlValue const Target2{ "a" };

      assertNgramMatch(__LINE__, false, &Attribute, &Target, &ValidAnalyzerName);
      // this are full binary match actually, but analyzer will emit no ngrams, so no match
      // will be found in index, so we also report no match
      assertNgramMatch(__LINE__, false, &Attribute, &Target2, &ValidAnalyzerName);
    }
  }
}
