////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include "search/terms_filter.hpp"

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"
#include "IResearch/common.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/ExpressionContextMock.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/Methods/Collections.h"

using namespace std::string_view_literals;

auto makeByTerms(std::string_view name,
                 std::span<const std::string_view> values, size_t match_count,
                 irs::score_t boost,
                 irs::ScoreMergeType type = irs::ScoreMergeType::kSum) {
  irs::by_terms filter;
  *filter.mutable_field() = name;
  filter.boost(boost);
  auto& [terms, min_match, merge_type] = *filter.mutable_options();
  min_match = match_count;
  merge_type = type;
  for (std::string_view value : values) {
    terms.emplace(irs::ViewCast<irs::byte_type>(value), irs::kNoBoost);
  }
  return filter;
}

class IResearchFilterMinHashMatchTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterMinHashMatchTest() {
    arangodb::tests::init();

    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();

    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::VPackFunctionParametersView params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic,
            arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::VPackFunctionParametersView params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(
        testDBInfo(server.server()),
        _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *_vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);

    auto props = arangodb::velocypack::Parser::fromJson(R"({
          "analyzer" : { "type": "delimiter", "properties": { "delimiter": " " } },
          "numHashes": 10
        })");
    auto res = analyzers.emplace(
        result, "testVocbase::test_analyzer", "minhash", props->slice(),
        arangodb::transaction::Hints::TrxType::INTERNAL);
#ifdef USE_ENTERPRISE
    EXPECT_TRUE(res.ok());
#else
    EXPECT_FALSE(res.ok());
#endif
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};

#if USE_ENTERPRISE
#include "tests/IResearch/IResearchFilterMinHashMatchTestEE.hpp"
#else
TEST_F(IResearchFilterMinHashMatchTest, MinHashMatchCE) {
  assertFilterFail(vocbase(),
                   R"(FOR d IN myView
                         FILTER MINHASH_MATCH(d.foo, "foo bar baz quick brown fox jumps over the lazy dog",
                                              1, "testVocbase::test_analyzer")
                         RETURN d)");
}
#endif
