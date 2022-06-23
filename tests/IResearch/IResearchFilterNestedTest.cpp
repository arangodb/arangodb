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

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"
#include "IResearch/ExpressionContextMock.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/Methods/Collections.h"

namespace {

// exists(name)
auto makeByColumnExistence(std::string_view name) {
  return [name](irs::sub_reader const &segment) {
    auto *col = segment.column(name);

    return col ? col->iterator(irs::ColumnHint::kMask |
                               irs::ColumnHint::kPrevDoc)
               : nullptr;
  };
}

// name == value
auto makeByTerm(std::string_view name, std::string_view value,
                irs::score_t boost) {
  auto filter = std::make_unique<irs::by_term>();
  *filter->mutable_field() = name;
  filter->mutable_options()->term = irs::ref_cast<irs::byte_type>(value);
  filter->boost(boost);
  return filter;
}

void makeAnd(
    irs::Or &root,
    std::vector<std::tuple<std::string_view, std::string_view, irs::score_t>>
        parts) {
  auto &filter = root.add<irs::And>();
  for (const auto &[name, value, boost] : parts) {
    auto &sub = filter.add<irs::by_term>();
    sub =
        std::move(static_cast<irs::by_term &>(*makeByTerm(name, value, boost)));
  }
}

class IResearchFilterNestedTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
protected:
  arangodb::tests::mocks::MockAqlServer server;

private:
  TRI_vocbase_t *_vocbase;

protected:
  IResearchFilterNestedTest() {
    arangodb::tests::init();

    auto &functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();

    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext *, arangodb::aql::AstNode const &,
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
        [](arangodb::aql::ExpressionContext *, arangodb::aql::AstNode const &,
           arangodb::aql::VPackFunctionParametersView params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    auto &analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto &dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(
        testDBInfo(server.server()),
        _vocbase); // required for IResearchAnalyzerFeature::emplace(...)
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *_vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);
    analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\"}")
            ->slice()); // cache analyzer
  }

  TRI_vocbase_t &vocbase() { return *_vocbase; }
};

// ANY
//  ? FILTER
//  ? ANY FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAny) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAny;

  ExpressionContextMock ctx;

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ANY FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
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
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAnyBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAny;
  filter.boost(1.555f);

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX(arangodb::aql::AqlValueHintInt{1});
  ctx.vars.emplace("x", valueX);
  arangodb::aql::AqlValue valueY(arangodb::aql::AqlValueHintInt{4294967295});
  ctx.vars.emplace("y", valueY);

  // boost value is checked
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER bOOST(d.array[? FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.555) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(d.array[? ANY FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.555) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAnyChildBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, 1.45f},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchAny;

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX(arangodb::aql::AqlValueHintInt{1});
  ctx.vars.emplace("x", valueX);
  arangodb::aql::AqlValue valueY(arangodb::aql::AqlValueHintInt{4294967295});
  ctx.vars.emplace("y", valueY);

  // check boost value for child field
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? FILTER BOOST(CURRENT.foo == 'bar', 1.45) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ANY FILTER BOOST(CURRENT.foo == 'bar', 1.45) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
}

// ALL
//  ? ALL FILTER
//  ? 4294967295 FILTER
//  ? 4294967295..4294967295 FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAll) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;

  // Actual implementation doesn't matter
  match = [](irs::sub_reader const &) { return nullptr; };

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{4294967294});
  ctx.vars.emplace("x", value);
  ctx.vars.emplace("y", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ALL FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);

  // Same queries, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoosT(d.array[? ALL FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAllBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  // Actual implementation doesn't matter
  match = [](irs::sub_reader const &) { return nullptr; };
  filter.boost(1.98f);

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX(arangodb::aql::AqlValueHintInt{4294967295});
  ctx.vars.emplace("x", valueX);
  arangodb::aql::AqlValue valueY(arangodb::aql::AqlValueHintInt{4294967295});
  ctx.vars.emplace("y", valueY);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BoOsT(d.array[? ALL FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.98) RETURN d)",
      expected);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchAllChildBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;

  // Actual implementation doesn't matter
  match = [](irs::sub_reader const &) { return nullptr; };

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX(arangodb::aql::AqlValueHintInt{4294967295});
  ctx.vars.emplace("x", valueX);
  arangodb::aql::AqlValue valueY(arangodb::aql::AqlValueHintInt{4294967295});
  ctx.vars.emplace("y", valueY);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? ALL FILTER CURRENT.foo == 'bar' AND BOOST(CURRENT.bar == 'baz', 1.3)] RETURN d)",
      expected);
}

// NONE
//  ? NONE FILTER
//  ? 0..0 FILTER

// FIX ME: ? 0 FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchNone) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchNone;

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{0});
  ctx.vars.emplace("x", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? NONE FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 0..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 'foo'..'bar' FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? -0..-0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x..x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);

  // Same queries, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? NONE FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? 0..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? 'foo'..'bar' FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? -0..-0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER Boost(d.array[? x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);

  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x..x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchNoneBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchNone;
  filter.boost(1.67f);

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{0});
  ctx.vars.emplace("x", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? NONE FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.6700) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? 0..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.67) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? 'foo'.."bar" FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.67) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER Boost(d.array[? -0..-0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.67) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER Boost(d.array[? x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.67) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER Boost(d.array[? x..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.67) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER Boost(d.array[? x..x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.67) RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchNoneChildBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, 1.54f},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::kMatchNone;

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{0});
  ctx.vars.emplace("x", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? NONE FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 0..0 FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 'foo'..'bar' FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 0..-0 FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x..0 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 0 FOR d IN myView FILTER d.array[? x..x FILTER BooST(CURRENT.foo == 'bar', 1.54) AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
}

// MIN
//  ? x FILTER
//  ? x..x FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchMin) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 2};

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
  ctx.vars.emplace("x", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? --2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 2 FOR d IN myView FILTER d.array[? x..x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 2 FOR d IN myView FILTER d.array[? 2..x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 2 FOR d IN myView FILTER d.array[? x..2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);

  // Same query, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER boosT(d.array[? 2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 2 FOR d IN myView FILTER boosT(d.array[? x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 2 FOR d IN myView FILTER boosT(d.array[? x..2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 2 FOR d IN myView FILTER boosT(d.array[? 2..x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchMinBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 2};
  filter.boost(1.65f);

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
  ctx.vars.emplace("x", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER boosT(d.array[? 2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.65) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER boosT(d.array[? --2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.65) RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER boosT(d.array[? x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.65) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER boosT(d.array[? x..2 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.65) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER boosT(d.array[? 2..x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1.65) RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchMinChildBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(static_cast<irs::Or &>(*child),
          {{std::string_view{fooField}, std::string_view{"bar"}, 1.4f},
           {std::string_view{barField}, std::string_view{"baz"}, 1.2f}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 2};

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
  ctx.vars.emplace("x", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2 FILTER Boost(CURRENT.foo == 'bar', 1.4) AND Boost(CURRENT.bar == 'baz', 1.2)] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? --2 FILTER Boost(CURRENT.foo == 'bar', 1.4) AND Boost(CURRENT.bar == 'baz', 1.2)] RETURN d)",
      expected);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER d.array[? x FILTER Boost(CURRENT.foo == 'bar', 1.4) AND Boost(CURRENT.bar == 'baz', 1.2)] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER d.array[? x..2 FILTER Boost(CURRENT.foo == 'bar', 1.4) AND Boost(CURRENT.bar == 'baz', 1.2)] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER d.array[? 2..x FILTER Boost(CURRENT.foo == 'bar', 1.4) AND Boost(CURRENT.bar == 'baz', 1.2)] RETURN d)",
      expected, &ctx);
}

// RANGE
//  ? x..y FILTER

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchRange) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 5};

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX(arangodb::aql::AqlValueHintInt{2});
  arangodb::aql::AqlValue valueY(arangodb::aql::AqlValueHintInt{5});
  ctx.vars.emplace("x", valueX);
  ctx.vars.emplace("y", valueY);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER d.array[? x..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let y = 5 FOR d IN myView FILTER d.array[? 2..y FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2, y = 5 FOR d IN myView FILTER d.array[? x..y FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);

  // Same query, but with BOOST function. Boost value is 1 (default)

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER BOOST(d.array[? x..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let y = 5 FOR d IN myView FILTER BOOST(d.array[? 2..y FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2, y = 5 FOR d IN myView FILTER BOOST(d.array[? x..y FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 1) RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchRangeBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 5};
  filter.boost(2.001f);

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX(arangodb::aql::AqlValueHintInt{2});
  arangodb::aql::AqlValue valueY(arangodb::aql::AqlValueHintInt{5});
  ctx.vars.emplace("x", valueX);
  ctx.vars.emplace("y", valueY);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 2.001) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER BOOST(d.array[? x..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 2.001) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let y = 5 FOR d IN myView FILTER BOOST(d.array[? 2..y FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 2.001) RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2, y = 5 FOR d IN myView FILTER BOOST(d.array[? x..y FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'], 2.001) RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, testNestedFilterMatchRangeChildBoost) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(static_cast<irs::Or &>(*child),
          {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
           {std::string_view{barField}, std::string_view{"baz"}, 2.001f}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 5};

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX(arangodb::aql::AqlValueHintInt{2});
  arangodb::aql::AqlValue valueY(arangodb::aql::AqlValueHintInt{5});
  ctx.vars.emplace("x", valueX);
  ctx.vars.emplace("y", valueY);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND BooSt(CURRENT.bar == 'baz', 2.001)] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2 FOR d IN myView FILTER d.array[? x..5 FILTER CURRENT.foo == 'bar' AND BooSt(CURRENT.bar == 'baz', 2.001)] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2, y = 5 FOR d IN myView FILTER d.array[? 2..y FILTER CURRENT.foo == 'bar' AND BooSt(CURRENT.bar == 'baz', 2.001)] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(let x = 2, y = 5 FOR d IN myView FILTER d.array[? x..y FILTER CURRENT.foo == 'bar' AND BooSt(CURRENT.bar == 'baz', 2.001)] RETURN d)",
      expected, &ctx);
}

// RANGE

TEST_F(IResearchFilterNestedTest, NestedFilterMatchRange) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 5};

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue value(2, 5);
  ctx.vars.emplace("x", value);

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2..5 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
  assertFilterSuccess(
      vocbase(),
      R"(LET x = 2..5 FOR d IN myView FILTER d.array[? x FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, NestedFilterMatchReversedRange) {
  irs::Or expected;
  auto &filter = expected.add<irs::ByNestedFilter>();
  auto &[parent, child, match, mergeType] = *filter.mutable_options();

  const auto parentField = mangleNested("array");
  const auto fooField = parentField + mangleStringIdentity(".foo");
  const auto barField = parentField + mangleStringIdentity(".bar");

  parent = makeByColumnExistence(parentField);
  child = std::make_unique<irs::Or>();
  makeAnd(
      static_cast<irs::Or &>(*child),
      {{std::string_view{fooField}, std::string_view{"bar"}, irs::kNoBoost},
       {std::string_view{barField}, std::string_view{"baz"}, irs::kNoBoost}});
  mergeType = irs::sort::MergeType::kSum;
  match = irs::Match{2, 1};

  ExpressionContextMock ctx;

  assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 2..1 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      expected, &ctx);
}

TEST_F(IResearchFilterNestedTest, NestedFilterMatchTooMany) {

  ExpressionContextMock ctx;

  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967297 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967295 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967296 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 4294967296..4294967297 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
}

// FAILING TESTS

TEST_F(IResearchFilterNestedTest, testParseFailingCases) {

  ExpressionContextMock ctx;

  arangodb::aql::AqlValue valueX{arangodb::aql::AqlValueHintInt{-140}};
  arangodb::aql::AqlValue valueY{arangodb::aql::AqlValueHintInt{-40}};
  arangodb::aql::AqlValue valueZ{arangodb::aql::AqlValueHintInt{-0}};
  ctx.vars.emplace("x", valueX);
  ctx.vars.emplace("y", valueY);
  ctx.vars.emplace("z", valueZ);

  // wrong ranges
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? 'range' FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? -1 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? -1 FILTER CURRENT.foo == 'bar' AND CURRENT.bar == RAND()] RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? "range" FILTER CURRENT.foo ==
        'bar' AND CURRENT.bar == 'baz'] RETURN d)");

  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER d.array[? -1..5 FILTER CURRENT.foo == 'bar'
        AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
  assertFilterExecutionFail(
      vocbase(),
      R"(let x = -140 FOR d IN myView FILTER d.array[? x..5
                     FILTER
          CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
  assertFilterExecutionFail(
      vocbase(),
      R"(let x = -140 FOR d IN myView FILTER d.array[? 1..x
                       FILTER
            CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
  assertFilterExecutionFail(
      vocbase(),
      R"(let x = -140, y = -40 FOR d IN myView FILTER d.array[? x..y FILTER
          CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
  assertFilterExecutionFail(
      vocbase(),
      R"(let x = -140, y = -40 FOR d IN myView FILTER d.array[? y..x FILTER
          CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
  assertFilterExecutionFail(
      vocbase(),
      R"(let x = -140, y = -40 FOR d IN myView FILTER d.array[? y..x FILTER
          CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
  assertFilterExecutionFail(
      vocbase(),
      R"(let x = -140, z = -0 FOR d IN myView FILTER d.array[? x..z FILTER
          CURRENT.foo == 'bar' AND CURRENT.bar == 'baz'] RETURN d)",
      &ctx);
}

#if USE_ENTERPRISE
#include "tests/IResearch/IResearchFilterNestedTestEE.h"
#endif

} // namespace
