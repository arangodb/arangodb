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
#include "Aql/AqlValue.h"
#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include "IResearch/IResearchAqlAnalyzer.h"
#include <analysis/token_streams.hpp>


class IResearchAqlAnalyzerTest : public IResearchQueryTest {};

namespace {
constexpr irs::string_ref AQL_ANALYZER_NAME{"aql"};

struct analyzer_token {
  std::string value;
  uint32_t pos;
};

using analyzer_tokens = std::vector<analyzer_token>;

void assert_analyzer(irs::analysis::analyzer* analyzer, const std::string& data,
                     const analyzer_tokens& expected_tokens) {
  SCOPED_TRACE(data);
  auto* term = irs::get<irs::term_attribute>(*analyzer);
  auto* vpack_term = irs::get<arangodb::iresearch::VPackTermAttribute>(*analyzer);
  auto* value_type = irs::get<arangodb::iresearch::AnalyzerValueTypeAttribute>(*analyzer);
  ASSERT_TRUE(term);
  ASSERT_TRUE(vpack_term);
  ASSERT_TRUE(value_type);
  auto* inc = irs::get<irs::increment>(*analyzer);
  ASSERT_TRUE(inc);
  ASSERT_TRUE(analyzer->reset(data));
  uint32_t pos{std::numeric_limits<uint32_t>::max()};
  auto expected_token = expected_tokens.begin();
  while (analyzer->next()) {
    ASSERT_NE(expected_token, expected_tokens.end());
    SCOPED_TRACE(testing::Message("Expected Term:") << expected_token->value);
    if (value_type->value == arangodb::iresearch::AnalyzerValueType::String) {
      auto term_value =
          std::string(irs::ref_cast<char>(term->value).c_str(), term->value.size());
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(expected_token->value), term->value);
    } else {
      ASSERT_EQ(0, arangodb::basics::VelocyPackHelper::compare(
        vpack_term->value, VPackSlice(reinterpret_cast<uint8_t const*>(expected_token->value.c_str())),
                                      false));
    }
    pos += inc->value;
    ASSERT_EQ(expected_token->pos, pos);
    ++expected_token;
  }
  ASSERT_EQ(expected_token, expected_tokens.end());
}

} // namespace

TEST_F(IResearchAqlAnalyzerTest, test_create_valid) {
  // const value
  {
    auto ptr = irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
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
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN @param\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "2", {{"2", 0}});
  }

  // calculation
  {
    auto ptr = irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN TO_STRING(TO_NUMBER(@param)+1)\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "2", {{"3", 0}});
  }
  // object
  {
    auto ptr =
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(
                                        VPackParser::fromJson("{\"queryString\": \"LET a = [{f:@param, c:NOOPT('test')}] FOR d IN a RETURN CONCAT(d.f, d.c)\"}")
                                          ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "2", {{"2test", 0}});
    assert_analyzer(ptr.get(), "3", {{"3test", 0}});
  }
  // cycle
  {
    auto ptr = irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"FOR d IN 1..5 RETURN CONCAT(UPPER(@param), d)\"}")->slice()),
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
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"collapsePositions\": true, \"batchSize\":3,"
                                                                                           "\"queryString\": \"FOR d IN 1..5 RETURN CONCAT(UPPER(@param), d)\"}")
                                                                         ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a",
                    {{"A1", 0}, {"A2", 0}, {"A3", 0}, {"A4", 0}, {"A5", 0}});
  }
  // cycle with array
  {
    auto ptr =
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"collapsePositions\": false,"
                                                                                           "\"queryString\": \"FOR d IN [UPPER(@param), @param, LOWER(@param)] RETURN d\"}")
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
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("\
                                        {\"collapsePositions\": false,\
                                         \"queryString\": \"FOR d IN 1..TO_NUMBER(@param)\
                                                             FILTER d%2 != 0\
                                                               FOR c IN 1..TO_NUMBER(@param)\
                                                                 FILTER c%2 == 0\
                                                                   RETURN CONCAT(d,c)\"}")->slice()), false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "4", {{"12", 0}, {"14", 1}, {"32", 2}, {"34", 3}});
  }
  // subquery
  {
    auto ptr =
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("\
                                        {\"collapsePositions\": false,\
                                         \"queryString\": \"FOR d IN [@param]\
                                                               LET Avg = (FOR c IN 1..TO_NUMBER(@param) FILTER c%2==0 RETURN c )\
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
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(
                                        VPackParser::fromJson("{\"keepNull\":false, \"queryString\": \"FOR d IN 1..5 LET t = d%2==0?  CONCAT(UPPER(@param), d) : NULL RETURN t \"}")
                                                                ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a", {{"A2", 0}, {"A4", 1}});
  }

  // keep nulls
  {
    auto ptr =
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(
                                        VPackParser::fromJson("{\"keepNull\":true, \"queryString\": \"FOR d IN 1..5 LET t = d%2==0?  CONCAT(UPPER(@param), d) : NULL RETURN t \"}")
                                                               ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a", {{"", 0}, {"A2", 1}, {"", 2}, {"A4", 3}, {"", 4}});
  }

  // only null
  {
    auto ptr = irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN null\", \"keepNull\":false}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    ASSERT_TRUE(ptr->reset("2"));
    ASSERT_FALSE(ptr->next());
  }

  // type mix
  {
    auto ptr = irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"FOR d IN ['e', 1, ['v', 'w'], null, true, @param, 'b'] RETURN d\"}")->slice()),
        false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a", {{"e", 0}, {"1", 1}, {"[\"v\",\"w\"]", 2}, {"", 3}, {"true", 4}, {"a", 5}, {"b", 6}});
  }

  // nulls with collapsed positions
  {
    auto ptr =
        irs::analysis::analyzers::get(
          AQL_ANALYZER_NAME,
          irs::type<irs::text_format::vpack>::get(),
          arangodb::iresearch::ref<char>(VPackParser::fromJson(
            "{\"collapsePositions\": true, \"keepNull\":true,"
            "\"queryString\": \"FOR d IN [null, null, @param, 'b'] RETURN d\"}")
            ->slice()),
          false);
    ASSERT_NE(nullptr, ptr);
    assert_analyzer(ptr.get(), "a", {{"", 0}, {"", 0}, {"a", 0}, {"b", 0}});
  }

  // check memoryLimit does not kill query
  {
    auto ptr =
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN CONCAT(FOR i IN 1..100 RETURN @param)\", \"memoryLimit\":1048576}")
                                                                         ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    ASSERT_TRUE(ptr->reset("AAAAAAAAA"));
    ASSERT_TRUE(ptr->next());
  }

  // check memoryLimit kills query
  {
    // note: setting a memoryLimit value of 1 is effectively a memoryLimit of 64kb,
    // because the memory usage tracking granularity is 64kb
    auto ptr =
        irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                      irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN CONCAT(FOR i IN 1..10000 RETURN NOOPT(@param))\", \"memoryLimit\":1}")
                                                                         ->slice()),
                                      false);
    ASSERT_NE(nullptr, ptr);
    ASSERT_TRUE(ptr->reset("AAAAAAAAA"));
    ASSERT_FALSE(ptr->next());
  }
}

TEST_F(IResearchAqlAnalyzerTest, test_create_invalid) {
  // Forbidden function TOKENS
  ASSERT_FALSE(
      irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN TOKENS(@param, 'identity')\"}")
                                                                       ->slice()),
                                    false));
  // Forbidden function NGRAM_MATCH
  ASSERT_FALSE(
      irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN NGRAM_MATCH(@param, 'test', 0.5, 'identity')\"}")
                                                                       ->slice()),
                                    false));
  // Forbidden function PHRASE
  ASSERT_FALSE(
      irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN PHRASE(@param, 'test', 'text_en')\"}")
                                                                       ->slice()),
                                    false));
  // Forbidden function ANALYZER
  ASSERT_FALSE(
      irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN ANALYZER(@param, 'text_en')\"}")
                                                                       ->slice()),
                                    false));
  // UDF function
  ASSERT_FALSE(
      irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN MY::SOME_UDF_FUNCTION(@param, 'text_en')\"}")
                                                                       ->slice()),
                                    false));
  // V8 function
  ASSERT_FALSE(
      irs::analysis::analyzers::get(AQL_ANALYZER_NAME,
                                    irs::type<irs::text_format::vpack>::get(),
                                    arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"RETURN V8(@param)\"}")
                                                                       ->slice()),
                                    false));

  // TRAVERSAL
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..3 ANY '1' GRAPH my_graph RETURN v\"}")->slice()),
        false));
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..3 ANY '1' GRAPH my_graph RETURN v\"}")->slice()),
        false));
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..3 ANY SHORTEST_PATH '1'  TO '2' GRAPH my_graph RETURN v\"}")->slice()),
        false));
  // COLLECT WITH COUNT
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..@param  COLLECT WITH COUNT INTO c RETURN c\"}")->slice()),
        false));
  // COLLECT
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR v IN 2..@param  COLLECT c = v * 10 RETURN c\"}")->slice()),
        false));
  // Wrong AQL syntax
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"RETAURN 1\"}")->slice()),
        false));
  // Collection access
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR d IN some RETURN d\"}")->slice()),
        false));
  // unknown parameter
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"RETURN CONCAT(@param, @param2)\"}")->slice()),
        false));

  // parameter data source
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR d IN @@param RETURN d\"}")->slice()),
        false));
  // INSERT
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
        VPackParser::fromJson("{\"queryString\": \"FOR d IN 1..@param INSERT {f:d} INTO some_collection\"}")->slice()),
        false));
  // UPDATE
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"FOR d IN some UPDATE d._key WITH {f:@param} IN some\"}")->slice()),
        false));
  // REMOVE
  ASSERT_FALSE(
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackParser::fromJson("{\"queryString\": \"FOR d IN 1..@param REMOVE {_key:d} IN some\"}")->slice()),
        false));
}

TEST_F(IResearchAqlAnalyzerTest, test_create_json) {
  auto ptr =
      irs::analysis::analyzers::get(
        AQL_ANALYZER_NAME,
        irs::type<irs::text_format::json>::get(),
        "{\"collapsePositions\": true, \"keepNull\":true,"
        "\"queryString\": \"FOR d IN [null, null, @param, 'b'] RETURN d\"}",
        false);
  ASSERT_NE(nullptr, ptr);
  assert_analyzer(ptr.get(), "a", {{"", 0}, {"", 0}, {"a", 0}, {"b", 0}});
}

TEST_F(IResearchAqlAnalyzerTest, test_normalize_json) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
      actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::json>::get(),
      "{\"queryString\": \"RETURN '1'\"}", false));
  auto actualVPack = VPackParser::fromJson(actual);
  auto actualSlice = actualVPack->slice();
  ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
  ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
  ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
  ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
  ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
}

TEST_F(IResearchAqlAnalyzerTest, test_normalize) {
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\"}")->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
  }
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"keepNull\":false}")->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), false);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
  }
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"collapsePositions\":true}")->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), true);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
  }
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"batchSize\":1000}")->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 1000);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
  }
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, AQL_ANALYZER_NAME,
                                                    irs::type<irs::text_format::vpack>::get(),
                                                    arangodb::iresearch::ref<char>(
                                                        VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                                                              "\"batchSize\":10, \"keepNull\":false,"
                                                                              "\"collapsePositions\":true}")
                                                            ->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), false);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), true);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
  }
  //memory limit
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, AQL_ANALYZER_NAME,
                                                    irs::type<irs::text_format::vpack>::get(),
                                                    arangodb::iresearch::ref<char>(
                                                        VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"batchSize\":1000, \"memoryLimit\":1}")
                                                            ->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 1000);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1);
  }
  // memory limit max
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, AQL_ANALYZER_NAME,
                                                    irs::type<irs::text_format::vpack>::get(),
                                                    arangodb::iresearch::ref<char>(
                                                        VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"batchSize\":1000, \"memoryLimit\":33554432}")
                                                            ->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 1000);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 33554432U);
  }
  // string return type
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, AQL_ANALYZER_NAME,
                                                    irs::type<irs::text_format::vpack>::get(),
                                                    arangodb::iresearch::ref<char>(
                                                        VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"returnType\":\"string\"}")
                                                            ->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
    ASSERT_EQ(actualSlice.get("returnType").stringView(), "string");
  }

  // bool return type
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, AQL_ANALYZER_NAME,
                                                    irs::type<irs::text_format::vpack>::get(),
                                                    arangodb::iresearch::ref<char>(
                                                        VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"returnType\":\"bool\"}")
                                                            ->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
    ASSERT_EQ(actualSlice.get("returnType").stringView(), "bool");
  }

  // number return type
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, AQL_ANALYZER_NAME,
                                                    irs::type<irs::text_format::vpack>::get(),
                                                    arangodb::iresearch::ref<char>(
                                                        VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"returnType\":\"number\"}")
                                                            ->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), true);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), false);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
    ASSERT_EQ(actualSlice.get("returnType").stringView(), "number");
  }

  // empty query
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"\","
                                  "\"batchSize\":10, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // missing query
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{"
                                  "\"batchSize\":10, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // invalid batch size
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"batchSize\":0, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // invalid batch size
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"batchSize\":1001, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // invalid batch size
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"batchSize\":false, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // invalid keepNull
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"batchSize\":1, \"keepNull\":10,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // invalid collapsePositions
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"batchSize\":11, \"keepNull\":false,"
                                  "\"collapsePositions\":2}")
                ->slice())));
  }
  // invalid memoryLimit
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"memoryLimit\":0, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // invalid memoryLimit
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"memoryLimit\":33554433, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
  }
  // Unknown parameter
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\", \"unknown_argument\":1,"
                                  "\"batchSize\":10, \"keepNull\":false,"
                                  "\"collapsePositions\":true}")
                ->slice())));
    VPackSlice actualSlice(reinterpret_cast<uint8_t const*>(actual.c_str()));
    ASSERT_EQ(actualSlice.get("queryString").stringView(), "RETURN '1'");
    ASSERT_EQ(actualSlice.get("keepNull").getBool(), false);
    ASSERT_EQ(actualSlice.get("collapsePositions").getBool(), true);
    ASSERT_EQ(actualSlice.get("batchSize").getInt(), 10);
    ASSERT_EQ(actualSlice.get("memoryLimit").getInt(), 1048576U);
  }

  // invalid returnType
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"returnType\":1001 }")
                ->slice())));
  }

  // invalid returnType
  {
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
        actual, AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(
            VPackParser::fromJson("{\"queryString\": \"RETURN '1'\","
                                  "\"returnType\":\"array\" }")
                ->slice())));
  }
}

TEST_F(IResearchAqlAnalyzerTest, test_numeric_return) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"RETURN @param\", \"returnType\":\"number\"}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  arangodb::aql::AqlValue val{arangodb::aql::AqlValueHintDouble(2)};
  analyzer_tokens expected_tokens;
  analyzer_token token;
  token.pos = 0;
  token.value.assign(val.slice().startAs<char>(), val.slice().byteSize());
  expected_tokens.push_back(std::move(token));
  assert_analyzer(ptr.get(), "2", expected_tokens);
}

TEST_F(IResearchAqlAnalyzerTest, test_numeric_return_array) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"FOR a IN 1..@param RETURN a\", \"returnType\":\"number\"}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;

  for (uint32_t i = 1; i <= 3; ++i) {
    arangodb::aql::AqlValue val{arangodb::aql::AqlValueHintDouble(i)};
    analyzer_token token;
    token.pos = i - 1;
    token.value.assign(val.slice().startAs<char>(), val.slice().byteSize());
    expected_tokens.push_back(std::move(token));
  }
  assert_analyzer(ptr.get(), "3", expected_tokens);
}

TEST_F(IResearchAqlAnalyzerTest, test_bool_return) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"RETURN @param\", \"returnType\":\"bool\"}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;
  arangodb::aql::AqlValue val{arangodb::aql::AqlValueHintBool(true)};
  analyzer_token token;
  token.pos = 0;
  token.value.assign(val.slice().startAs<char>(), val.slice().byteSize());
  expected_tokens.push_back(std::move(token));
  assert_analyzer(ptr.get(), "2", expected_tokens);
}

TEST_F(IResearchAqlAnalyzerTest, test_bool_return_array) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"FOR a IN 1..@param RETURN a == 2\", \"returnType\":\"bool\"}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;

  for (uint32_t i = 1; i <= 3; ++i) {
    arangodb::aql::AqlValue val{arangodb::aql::AqlValueHintBool(i == 2)};
    analyzer_token token;
    token.pos = i - 1;
    token.value.assign(val.slice().startAs<char>(), val.slice().byteSize());
    expected_tokens.push_back(std::move(token));
  }
  assert_analyzer(ptr.get(), "3", expected_tokens);
}

TEST_F(IResearchAqlAnalyzerTest, test_string_return) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"RETURN TO_NUMBER(@param) + 10\", \"returnType\":\"string\"}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;
  analyzer_token token;
  token.pos = 0;
  token.value = "12";
  expected_tokens.push_back(std::move(token));
  assert_analyzer(ptr.get(), "2", expected_tokens);
}

TEST_F(IResearchAqlAnalyzerTest, test_string_return_array) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"FOR a IN 1..@param RETURN a\", \"returnType\":\"string\"}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;

  for (uint32_t i = 1; i <= 3; ++i) {
    analyzer_token token;
    token.pos = i - 1;
    token.value = std::to_string(i);
    expected_tokens.push_back(std::move(token));
  }
  assert_analyzer(ptr.get(), "3", expected_tokens);
}

TEST_F(IResearchAqlAnalyzerTest, test_string_return_array_keep_null) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"FOR a IN 1..(@param * 10) "
                                " RETURN a > 5 ? null : a \", "
                                "\"returnType\":\"string\", \"keepNull\":true}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;

  for (uint32_t i = 1; i <= 30; ++i) {
    analyzer_token token;
    token.pos = i - 1;
    token.value = i <= 5  ? std::to_string(i) : "";
    expected_tokens.push_back(std::move(token));
  }
  assert_analyzer(ptr.get(), "3", expected_tokens);
}

TEST_F(IResearchAqlAnalyzerTest, test_number_return_array_keep_null) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"FOR a IN 1..(@param * 10) "
                                " RETURN a > 5 ? null : a \", "
                                "\"returnType\":\"number\", \"keepNull\":true}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;

  for (uint32_t i = 1; i <= 30; ++i) {
    arangodb::aql::AqlValue val{arangodb::aql::AqlValueHintDouble(i <= 5  ? i : 0)};
    analyzer_token token;
    token.pos = i - 1;
    token.value.assign(val.slice().startAs<char>(), val.slice().byteSize());
    expected_tokens.push_back(std::move(token));
  }
  assert_analyzer(ptr.get(), "3", expected_tokens);
}


TEST_F(IResearchAqlAnalyzerTest, test_bool_return_array_keep_null) {
  auto ptr = irs::analysis::analyzers::get(
      AQL_ANALYZER_NAME, irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(
          VPackParser::fromJson("{\"queryString\": \"FOR a IN 1..(@param * 10) "
                                " RETURN a > 5 ? null : true \", "
                                "\"returnType\":\"bool\", \"keepNull\":true}")->slice()),
      false);
  ASSERT_NE(nullptr, ptr);

  analyzer_tokens expected_tokens;

  for (uint32_t i = 1; i <= 30; ++i) {
    arangodb::aql::AqlValue val{arangodb::aql::AqlValueHintBool(i <= 5)};
    analyzer_token token;
    token.pos = i - 1;
    token.value.assign(val.slice().startAs<char>(), val.slice().byteSize());
    expected_tokens.push_back(std::move(token));
  }
  assert_analyzer(ptr.get(), "3", expected_tokens);
}
