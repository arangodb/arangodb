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

#include "IResearchQueryCommon.h"


#include "Aql/AqlFunctionFeature.h"
#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include "IResearch/IResearchCalculationAnalyzer.h"


class IResearchCalculationAnalyzerTest : public IResearchQueryTest {
 public:
  IResearchCalculationAnalyzerTest() { 
    // we need  calculation vocbase
    arangodb::DatabaseFeature::initCalculationVocbase(server.server());
  }
};

namespace {
constexpr irs::string_ref CALC_ANALYZER_NAME{"calculation"};

struct analyzer_token {
  irs::string_ref value;
  uint32_t pos;
};

using analyzer_tokens = std::vector<analyzer_token>;

void assert_analyzer(irs::analysis::analyzer* analyzer, const std::string& data,
                     const analyzer_tokens& expected_tokens) {
  SCOPED_TRACE(data);
  auto* term = irs::get<irs::term_attribute>(*analyzer);
  ASSERT_TRUE(term);
  auto* inc = irs::get<irs::increment>(*analyzer);
  ASSERT_TRUE(inc);
  ASSERT_TRUE(analyzer->reset(data));
  uint32_t pos{irs::integer_traits<uint32_t>::const_max};
  auto expected_token = expected_tokens.begin();
  while (analyzer->next()) {
    auto term_value =
        std::string(irs::ref_cast<char>(term->value).c_str(), term->value.size());
    SCOPED_TRACE(testing::Message("Term:") << term_value);
    auto old_pos = pos;
    pos += inc->value;
    ASSERT_NE(expected_token, expected_tokens.end());
    ASSERT_EQ(irs::ref_cast<irs::byte_type>(expected_token->value), term->value);
    ASSERT_EQ(expected_token->pos, pos);
    ++expected_token;
  }
  ASSERT_EQ(expected_token, expected_tokens.end());
}
} // namespace

TEST_F(IResearchCalculationAnalyzerTest, test_create_valid) {
  // const value
  {
    auto ptr = irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                             irs::type<irs::text_format::vpack>::get(),
                                             arangodb::iresearch::ref<char>(
                                               VPackParser::fromJson("{\"queryString\": \"RETURN '1'\"}")->slice()),
                                             false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "2", {{"1", 0}});
  }
  // just parameter
  {
    auto ptr = irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN @field\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "2", {{"2", 0}});
  }

  // cycle 
  {
    auto ptr = irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"FOR d IN 1..5 RETURN CONCAT(UPPER(@field), d)\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a",
                    {{"A1", 0}, {"A2", 1}, {"A3", 2}, {"A4", 3}, {"A5", 4}});
  }
  // cycle with collapse
  {
    auto ptr =
        irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"collapseArrayPos\": true,"
                                                                                           "\"queryString\": \"FOR d IN 1..5 RETURN CONCAT(UPPER(@field), d)\"}")
                                                                         ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a",
                    {{"A1", 0}, {"A2", 0}, {"A3", 0}, {"A4", 0}, {"A5", 0}});
  }
  // cycle with array
  {
    auto ptr =
        irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"collapseArrayPos\": false,"
                                                                                           "\"queryString\": \"FOR d IN [UPPER(@field), @field, LOWER(@field)] RETURN d\"}")
                                                                         ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "ArangoDB",
                    {{"ARANGODB", 0}, {"ArangoDB", 1}, {"arangodb", 2}});
  }
  // nested cycles
  {
    auto ptr =
        irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("\
                                        {\"collapseArrayPos\": false,\
                                         \"queryString\": \"FOR d IN 1..TO_NUMBER(@field)\
                                                             FILTER d%2 != 0\
                                                               FOR c IN 1..TO_NUMBER(@field)\
                                                                 FILTER c%2 == 0\
                                                                   RETURN CONCAT(d,c)\"}")->slice()), false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "4", {{"12", 0}, {"14", 1}, {"32", 2}, {"34", 3}});
  }
  // subquery
  {
    auto ptr =
        irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("\
                                        {\"collapseArrayPos\": false,\
                                         \"queryString\": \"FOR d IN [@field]\
                                                               LET Avg = (FOR c IN 1..TO_NUMBER(@field) FILTER c%2==0 RETURN c )\
                                                                   RETURN CONCAT(d,AVERAGE(Avg))\"}")
                                                                         ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "4", {{"43", 0}});
  }
}
