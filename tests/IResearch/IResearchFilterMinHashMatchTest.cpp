////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// The Programs (which include both the software and documentation) contain
/// proprietary information of ArangoDB GmbH; they are provided under a license
/// agreement containing restrictions on use and disclosure and are also
/// protected by copyright, patent and other intellectual and industrial
/// property laws. Reverse engineering, disassembly or decompilation of the
/// Programs, except to the extent required to obtain interoperability with
/// other independently created software or as specified by law, is prohibited.
///
/// It shall be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of
/// applications if the Programs are used for purposes such as nuclear,
/// aviation, mass transit, medical, or other inherently dangerous applications,
/// and ArangoDB GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of ArangoDB
/// GmbH. You shall not disclose such confidential and proprietary information
/// and shall use it only in accordance with the terms of the license agreement
/// you entered into with ArangoDB GmbH.
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
#include "IResearch/ExpressionContextMock.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/Methods/Collections.h"

using namespace std::string_view_literals;

auto makeByTerms(std::string_view name,
                 std::span<const std::string_view> values, size_t match_count,
                 irs::score_t boost,
                 irs::sort::MergeType type = irs::sort::MergeType::kSum) {
  irs::by_terms filter;
  *filter.mutable_field() = name;
  filter.boost(boost);
  auto& [terms, min_match, merge_type] = *filter.mutable_options();
  min_match = match_count;
  merge_type = type;
  for (irs::string_ref value : values) {
    terms.emplace(irs::ref_cast<irs::byte_type>(value), irs::kNoBoost);
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
    auto res = analyzers.emplace(result, "testVocbase::test_analyzer",
                                 "minhash", props->slice());
#ifdef USE_ENTERPRISE
    EXPECT_TRUE(res.ok());
#else
    EXPECT_FALSE(res.ok());
#endif
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};

// TODO Add community only tests (byExpression)

#if USE_ENTERPRISE
#include "tests/IResearch/IResearchFilterMinHashMatchTestEE.hpp"
#endif

TEST_F(IResearchFilterMinHashMatchTest, MinMatch3Hashes) {
  irs::Or expected;
  expected.add<irs::by_terms>() = makeByTerms(
      "foo", std::array{"44OTL2BvXFU"sv, "F3tEoNARof4"sv, "ZZHTGoxTKjQ"sv}, 3,
      irs::kNoBoost);

  ExpressionContextMock ctx;
  arangodb::aql::Variable varAnalyzer("analyzer", 0, false);
  arangodb::aql::AqlValue valueAnalyzer("testVocbase::test_analyzer");
  arangodb::aql::AqlValueGuard guardAnalyzer(valueAnalyzer, true);
  arangodb::aql::Variable varField("field", 1, false);
  arangodb::aql::AqlValue valueField("foo");
  arangodb::aql::AqlValueGuard guardField(valueField, true);
  arangodb::aql::Variable varCount("count", 2, false);
  arangodb::aql::AqlValue valueCount(arangodb::aql::AqlValueHintUInt{1});
  arangodb::aql::AqlValueGuard guardCount(valueCount, true);
  arangodb::aql::Variable varInput("input", 3, false);
  arangodb::aql::AqlValue valueInput("foo bar baz");
  arangodb::aql::AqlValueGuard guardInput(valueInput, true);
  ctx.vars.emplace(varAnalyzer.name, valueAnalyzer);
  ctx.vars.emplace(varField.name, valueField);
  ctx.vars.emplace(varCount.name, valueCount);
  ctx.vars.emplace(varInput.name, valueInput);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER MINHASH_MATCH(d.foo, "foo bar baz", 1, "testVocbase::test_analyzer") RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(MINHASH_MATCH(d.foo, "foo bar baz", 1, "testVocbase::test_analyzer"), 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(ANALYZER(MINHASH_MATCH(d.foo, "foo bar baz", 1), "testVocbase::test_analyzer"), 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(Let count = 1 LET field = "foo" LET analyzer = "testVocbase::test_analyzer" let input = "foo bar baz"
         FOR d IN myView FILTER BOOST(ANALYZER(MINHASH_MATCH(d[field], input, count), analyzer), 1) RETURN d)",
      expected, &ctx);

  // Not a MinHash analyzer
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER MINHASH_MATCH(d.foo, "foo bar baz", 1, "text_en") RETURN d)");
  // Invalid threshold
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER MINHASH_MATCH(d.foo, "foo bar baz", 1.1, "testVocbase::test_analyzer") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER MINHASH_MATCH(d.foo, "foo bar baz", 0, "testVocbase::test_analyzer") RETURN d)");
}
