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

#include "search/boolean_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/nested_filter.hpp"
#include "search/term_filter.hpp"

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"
#include "IResearch/common.h"
#include "IResearch/IResearchCommon.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/Methods/Collections.h"

// exists(name)
auto makeByColumnExistence(std::string_view name) {
  auto filter = std::make_unique<irs::by_column_existence>();
  *filter->mutable_field() = name;
  return filter;
}

// name == value
auto MakeByTerm(std::string_view name, std::string_view value,
                irs::score_t boost) {
  auto filter = std::make_unique<irs::by_term>();
  *filter->mutable_field() = name;
  filter->mutable_options()->term = irs::ref_cast<irs::byte_type>(value);
  filter->boost(boost);
  return filter;
}

void makeAnd(
    irs::Or& root,
    std::vector<std::tuple<std::string_view, std::string_view, irs::score_t>>
        parts) {
  auto& filter = root.add<irs::And>();
  for (const auto& [name, value, boost] : parts) {
    auto& sub = filter.add<irs::by_term>();
    sub =
        std::move(static_cast<irs::by_term&>(*MakeByTerm(name, value, boost)));
  }
}

class IResearchFilterNestedTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterNestedTest() {
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
    analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\"}")
            ->slice());  // cache analyzer
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};

// ANY
//  ? FILTER
//  ? ANY FILTER
//  ? 1 FILTER
//  ? 1..4294967295 FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAny) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAny;

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ANY FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 1 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 1..4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);

  // Same queries, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoOST(d.array[? FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER bOOST(d.array[? ANY FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOoST(d.array[? 1 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOsT(d.array[? 1..4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAnyBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAny;
  filter.boost(1.555f);

  // boost value is checked
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER bOOST(d.array[? FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.555) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(d.array[? ANY FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.555) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOoST(d.array[? 1 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.555) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER bOOSt(d.array[? 1..4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.555) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAnyChildBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, 1.45f},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAny;

  // check boost value for child field
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? FILTER BOOST(CURRENT.foo == 'bar', 1.45) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ANY FILTER BOOST(CURRENT.foo == 'bar', 1.45) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 1 FILTER BOOST(CURRENT.foo == 'bar', 1.45) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 1..4294967295 FILTER bOosT(CURRENT.foo == 'bar', 1.45) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
}

// ALL
//  ? ALL FILTER
//  ? 4294967295 FILTER
//  ? 4294967295..4294967295 FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAll) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAll;

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ALL FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967295..4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);

  // Same queries, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoosT(d.array[? ALL FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoosT(d.array[? 4294967295..4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.000) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoOsT(d.array[? 4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1e0) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAllBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAll;
  filter.boost(1.98f);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoOsT(d.array[? ALL FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.98) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoOsT(d.array[? 4294967295..4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.98) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoOsT(d.array[? 4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.98) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAllChildBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAll;

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ALL FILTER CURRENT.foo == 'bar' AND BOOST(CURRENT.bar == 'baz', 1.3)] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967295..4294967295 FILTER CURRENT.foo == 'bar' AND BOOST(CURRENT.bar == 'baz', 1.3)] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967295 FILTER CURRENT.foo == 'bar' AND BOOST(CURRENT.bar == 'baz', 1.3)] RETURN d)",
      expected);
}

// NONE
//  ? NONE FILTER
//  ? 0..0 FILTER

// FIX ME: ? 0 FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchNone) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchNone;

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? NONE FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 0..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);

  // Same queries, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? NONE FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? 0..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchNoneBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchNone;
  filter.boost(1.67f);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? NONE FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.6700) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? 0..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.67) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchNoneChildBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, 1.54},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchNone;

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? NONE FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 0..0 FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
}

// MIN
//  ? x FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchMin) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2};

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);

  // Same query, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER boosT(d.array[? 2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchMinBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2};
  filter.boost(1.65f);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER boosT(d.array[? 2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.65) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchMinChildBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(static_cast<irs::Or&>(*child),
          {{std::string_view{fooField}, std::string_view{"bar"}, 1.4f},
           {std::string_view{barField}, std::string_view{"baz"}, 1.2f}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2};

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2 FILTER Boost(CURRENT.foo == 'bar', 1.4) AND Boost(CURRENT.bar == 'baz', 1.2)] RETURN d)",
      expected);
}

// RANGE
//  ? x..y FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchRange) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 5};

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);

  // Same query, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchRangeBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or&>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 5};
  filter.boost(2.001f);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 2.001) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchRangeChildBoost) {
  irs::Or expected;
  auto& filter = expected.add<irs::ByNestedFilter>();
  auto& [parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(static_cast<irs::Or&>(*child),
          {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
           {std::string_view{barField}, std::string_view{"baz"}, 2.001f}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{1, 5};

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 1..5 FILTER CURRENT.foo == 'bar' AND BooSt(CURRENT.bar == 'baz', 2.001)] RETURN d)",
      expected);
}

// FAILING TESTS

TEST_F(IResearchFilterNestedTest, testParseFailingCases) {

  // wrong ranges
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2..1 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)");

  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 'range' FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)");

  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 'range' FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)");
}

#if USE_ENTERPRISE
#include "tests/IResearch/IResearchFilterNestedTestEE.h"
#endif
