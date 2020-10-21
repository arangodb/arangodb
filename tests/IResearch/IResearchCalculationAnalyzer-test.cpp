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

  // calculation
  {
    auto ptr = irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN TO_STRING(TO_NUMBER(@field)+1)\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "2", {{"3", 0}});
  }
  // object
  {
    auto ptr =
        irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(
                                        VPackParser::fromJson("{\"queryString\": \"LET a = [{f:@field, c:NOOPT('test')}] FOR d IN a RETURN CONCAT(d.f, d.c)\"}")
                                          ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "2", {{"2test", 0}});
    assert_analyzer(ptr.get(), "3", {{"3test", 0}});
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
    assert_analyzer(ptr.get(), "b",
                    {{"B1", 0}, {"B2", 1}, {"B3", 2}, {"B4", 3}, {"B5", 4}});
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
    assert_analyzer(ptr.get(), "TeST",
                    {{"TEST", 0}, {"TeST", 1}, {"test", 2}});
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
    assert_analyzer(ptr.get(), "5", {{"53", 0}});
  }

  // filter nulls
  {
    auto ptr =
        irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(
                                        VPackParser::fromJson("{\"keepNull\":false, \"queryString\": \"FOR d IN 1..5 LET t = d%2==0?  CONCAT(UPPER(@field), d) : NULL RETURN t \"}")
                                                                ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a", {{"A2", 0}, {"A4", 1}});
  }

  // keep nulls
  {
    auto ptr =
        irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(
                                        VPackParser::fromJson("{\"keepNull\":true, \"queryString\": \"FOR d IN 1..5 LET t = d%2==0?  CONCAT(UPPER(@field), d) : NULL RETURN t \"}")
                                                               ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a", {{"", 0}, {"A2", 1}, {"", 2}, {"A4", 3}, {"", 4}});
  }

  // non string
  {
    auto ptr = irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN 1\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    ASSERT_TRUE(ptr->reset("2"));
    ASSERT_FALSE(ptr->next());
  }

  // type mix
  {
    auto ptr = irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"FOR d IN ['e', 1, ['v', 'w'], null, true, @field, 'b'] RETURN d\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a", {{"e", 0}, {"", 1}, {"a", 2}, {"b", 3}});
  }
}

TEST_F(IResearchCalculationAnalyzerTest, test_create_invalid) {
  // Forbidden function TOKENS
  ASSERT_FALSE(
      irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN TOKENS(@field, 'identity')\"}")
                                                                       ->slice()),
                                    false));
  // Forbidden function NGRAM_MATCH
  ASSERT_FALSE(
      irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN NGRAM_MATCH(@field, 'test', 0.5, 'identity')\"}")
                                                                       ->slice()),
                                    false));
  // Forbidden function PHRASE
  ASSERT_FALSE(
      irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN PHRASE(@field, 'test', 'text_en')\"}")
                                                                       ->slice()),
                                    false));
  // Forbidden function ANALYZER
  ASSERT_FALSE(
      irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN ANALYZER(@field, 'text_en')\"}")
                                                                       ->slice()),
                                    false));
  // UDF function 
  ASSERT_FALSE(
      irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN MY::SOME_UDF_FUNCTION(@field, 'text_en')\"}")
                                                                       ->slice()),
                                    false));
  // V8 function
  ASSERT_FALSE(
      irs::analysis::analyzers::get(CALC_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN V8(@field)\"}")
                                                                       ->slice()),
                                    false));

  // TRAVERSAL
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..3 ANY '1' GRAPH my_graph RETURN v\"}")->slice()),
        false));
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..3 ANY '1' GRAPH my_graph RETURN v\"}")->slice()),
        false));
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..3 ANY SHORTEST_PATH '1'  TO '2' GRAPH my_graph RETURN v\"}")->slice()),
        false));
  // COLLECT WITH COUNT
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..@field  COLLECT WITH COUNT INTO c RETURN c\"}")->slice()),
        false));
  // COLLECT
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..@field  COLLECT c = v * 10 RETURN c\"}")->slice()),
        false));
  // Wrong AQL syntax
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"RETAURN 1\"}")->slice()),
        false));
  // Collection access
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR d IN some RETURN d\"}")->slice()),
        false));
  // unknown parameter
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"RETURN CONCAT(@field, @field2)\"}")->slice()),
        false));

  // parameter data source
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR d IN @@field RETURN d\"}")->slice()),
        false));
  // INSERT
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR d IN 1..@field INSERT {f:d} INTO some_collection\"}")->slice()),
        false));
  // UPDATE
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"FOR d IN some UPDATE d._key WITH {f:@field} IN some\"}")->slice()),
        false));
  // REMOVE
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        CALC_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"FOR d IN 1..@field REMOVE {_key:d} IN some\"}")->slice()),
        false));
}

