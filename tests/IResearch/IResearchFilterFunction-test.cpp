//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "common.h"
#include "ExpressionContextMock.h"
#include "StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "IResearch/AqlHelper.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/AqlFunctionFeature.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"
#include "search/term_filter.hpp"
#include "search/all_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/phrase_filter.hpp"

NS_LOCAL

std::string mangleBool(std::string name) {
  arangodb::iresearch::kludge::mangleBool(name);
  return name;
}

std::string mangleNull(std::string name) {
  arangodb::iresearch::kludge::mangleNull(name);
  return name;
}

std::string mangleNumeric(std::string name) {
  arangodb::iresearch::kludge::mangleNumeric(name);
  return name;
}

std::string mangleString(std::string name, std::string suffix) {
  arangodb::iresearch::kludge::mangleAnalyzer(name);
  name += suffix;
  return name;
}

std::string mangleType(std::string name) {
  arangodb::iresearch::kludge::mangleType(name);
  return name;
}

std::string mangleAnalyzer(std::string name) {
  arangodb::iresearch::kludge::mangleAnalyzer(name);
  return name;
}

std::string mangleStringIdentity(std::string name) {
  arangodb::iresearch::kludge::mangleStringField(
    name,
    arangodb::iresearch::IResearchAnalyzerFeature::identity()
  );
  return name;
}

void assertExpressionFilter(
    std::string const& queryString,
    std::string const& refName = "d"
) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    nullptr,
    std::make_shared<arangodb::velocypack::Builder>(),
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* ast = query.ast();
  REQUIRE(ast);

  auto* root = ast->root();
  REQUIRE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    REQUIRE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  REQUIRE(filterNode);

  // find referenced variable
  auto* allVars = ast->variables();
  REQUIRE(allVars);
  arangodb::aql::Variable* ref = nullptr;
  for (auto entry : allVars->variables(true)) {
    if (entry.second == refName) {
      ref = allVars->getVariable(entry.first);
      break;
    }
  }
  REQUIRE(ref);

  // supportsFilterCondition
  {
    arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, ref };
    CHECK((arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode)));
  }

  // iteratorForCondition
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      {},
      {},
      {},
      arangodb::transaction::Options()
    );

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    irs::Or expected;
    expected.add<arangodb::iresearch::ByExpression>().init(
      *dummyPlan,
      *ast,
      *filterNode->getMember(0) // 'd.a.b[_REFERENCE_('c')]
    );

    irs::Or actual;
    arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
    CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
    CHECK((expected == actual));
  }
}

void assertFilter(
    bool parseOk,
    bool execOk,
    std::string const& queryString,
    irs::filter const& expected,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* ast = query.ast();
  REQUIRE(ast);

  auto* root = ast->root();
  REQUIRE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    REQUIRE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  REQUIRE(filterNode);

  // find referenced variable
  auto* allVars = ast->variables();
  REQUIRE(allVars);
  arangodb::aql::Variable* ref = nullptr;
  for (auto entry : allVars->variables(true)) {
    if (entry.second == refName) {
      ref = allVars->getVariable(entry.first);
      break;
    }
  }
  REQUIRE(ref);

  // optimization time
  {
    arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, ref };
    CHECK((parseOk == arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode)));
  }

  // execution time
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      {},
      {},
      {},
      arangodb::transaction::Options()
    );

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    irs::Or actual;
    arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, exprCtx, ref };
    CHECK((execOk == arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
    CHECK((!execOk || expected == actual));
  }
}

void assertFilterSuccess(
    std::string const& queryString,
    irs::filter const& expected,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  return assertFilter(true, true, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterExecutionFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  irs::Or expected;
  return assertFilter(true, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  irs::Or expected;
  return assertFilter(false, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterParseFail(
    std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    nullptr,
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  CHECK(TRI_ERROR_NO_ERROR != parseResult.code);
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchFilterSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchFilterSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true);
    features.emplace_back(new arangodb::DatabaseFeature(&server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::ViewTypesFeature(&server), false); // required for IResearchFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(&server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_NONDETERM_",
      ".",
      false, // fake non-deterministic
      false, // fake can throw
      true,
      [](arangodb::aql::Query*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_FORWARD_",
      ".",
      true, // fake deterministic
      false, // fake can throw
      true,
      [](arangodb::aql::Query*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    analyzers->emplace("test_analyzer", "TestCharAnalyzer", "abc"); // cache analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchFilterSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(&server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
  }
}; // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchFilterFunctionTest", "[iresearch][iresearch-filter]") {
  IResearchFilterSetup s;
  UNUSED(s);

SECTION("AttributeAccess") {
  // attribute access, non empty object
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{ \"a\": { \"b\": \"1\" } }");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x={} FOR d IN collection FILTER x.a.b RETURN d", expected, &ctx);
  }

  // attribute access, empty object
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x={} FOR d IN collection FILTER x.a.b RETURN d", expected, &ctx);
  }

  assertExpressionFilter("FOR d IN collection FILTER d RETURN d"); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c RETURN d"); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER d.a.b[TO_STRING('c')] RETURN d"); // no reference to `d`

  // nondeterministic expression -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a.b[_NONDETERM_('c')] RETURN d");
}

SECTION("ValueReference") {
  // string value == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER '1' RETURN d", expected);
  }

  // string reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue("abc")));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x='abc' FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // string empty value == false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER '' RETURN d", expected);
  }

  // empty string reference false
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue("")));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x='' FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // true value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER true RETURN d", expected);
  }

  // boolean reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{true})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=true FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER false RETURN d", expected);
  }

  // boolean reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=false FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // null == value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER null RETURN d", expected);
  }

  // non zero numeric value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 1 RETURN d", expected);
  }

  // non zero numeric reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=1 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // zero numeric value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER 0 RETURN d", expected);
  }

  // zero numeric reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=0 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // zero floating value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER 0.0 RETURN d", expected);
  }

  // zero floating reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=0.0 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // non zero floating value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 0.1 RETURN d", expected);
  }

  // non zero floating reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=0.1 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // Array == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER [] RETURN d", expected);
  }

  // Array reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[]");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(obj->slice())));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=[] FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // Range == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 1..2 RETURN d", expected);
  }

  // Range reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(1, 1)));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=1..1 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // Object == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER {} RETURN d", expected);
  }

  // Object reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(obj->slice())));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x={} FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // numeric expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET numVal=2 FOR d IN collection FILTER numVal-2 RETURN d", expected, &ctx);
  }

  // boolean expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET numVal=2 FOR d IN collection FILTER ((numVal+1) < 2) RETURN d", expected, &ctx);
  }

  // null expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::empty>();
    root.add<irs::all>();

    assertFilterSuccess("LET nullVal=null FOR d IN collection FILTER (nullVal && true) RETURN d", expected, &ctx);
  }

  // self-reference
  assertExpressionFilter("FOR d IN collection FILTER d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[1] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[1] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] RETURN d");
}

SECTION("SystemFunctions") {
  // scalar
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=1 FOR d IN collection FILTER TO_STRING(x) RETURN d", expected, &ctx); // reference
  }

  // scalar
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=0 FOR d IN collection FILTER TO_BOOL(x) RETURN d", expected, &ctx); // reference
  }

  // nondeterministic expression : wrap it
  assertExpressionFilter("FOR d IN VIEW myView FILTER RAND() RETURN d");
}

SECTION("UnsupportedUserFunctions") {
//  FIXME need V8 context up and running to execute user functions
//  assertFilterFail("FOR d IN VIEW myView FILTER ir::unknownFunction() RETURN d", &ExpressionContextMock::EMPTY);
//  assertFilterFail("FOR d IN VIEW myView FILTER ir::unknownFunction1(d) RETURN d", &ExpressionContextMock::EMPTY);
//  assertFilterFail("FOR d IN VIEW myView FILTER ir::unknownFunction2(d, 'quick') RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("Exists") {
  // field only
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("name").prefix_match(true);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d['name']) RETURN d", expected);
  }

  // field with simple offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("[42]").prefix_match(true);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d[42]) RETURN d", expected);
  }

  // complex field
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("obj.prop.name").prefix_match(true);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.obj.prop.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d['obj']['prop']['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.obj.prop.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d['obj'].prop.name) RETURN d", expected);
  }

  // complex field with offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("obj.prop[3].name").prefix_match(true);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d['obj']['prop'][3]['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d['obj'].prop[3].name) RETURN d", expected);
  }

  // complex field with offset
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("index", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("obj.prop[3].name").prefix_match(true);

    assertFilterSuccess("LET index=2 FOR d IN VIEW myView FILTER exists(d.obj.prop[index+1].name) RETURN d", expected, &ctx);
    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d['obj']['prop'][3]['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d['obj'].prop[3].name) RETURN d", expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("a.b.c.e[4].f[5].g[3].g.a").prefix_match(true);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", &ctx);
  }

  // invalid attribute access
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d[*]) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.a.b[*]) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists('d.name') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(123) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(123.5) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(false) RETURN d");

  // field + type
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleType("name")).prefix_match(true);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'type') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'type') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'Type') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'TYPE') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'invalid') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER exists(d.name, d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, null) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 123) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 123.5) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, true) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, false) RETURN d");
  }

  // field + analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleAnalyzer("name")).prefix_match(true);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'analyzer') RETURN d", expected);
  }

  // invalid 2nd argument
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'Analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'ANALYZER') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'foo') RETURN d");
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER exists(d.name, d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 123) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 123.5) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, false) RETURN d");

  // field + analyzer as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("analyz"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleAnalyzer("name")).prefix_match(true);

    assertFilterSuccess("LET anl='analyz' FOR d IN VIEW myView FILTER exists(d.name, CONCAT(anl,'er')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET anl='analyz' FOR d IN VIEW myView FILTER eXists(d.name, CONCAT(anl,'er')) RETURN d", expected, &ctx);
  }

  // field + analyzer as invalid expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    assertFilterExecutionFail("LET anl='analyz' FOR d IN VIEW myView FILTER exists(d.name, anl) RETURN d", &ctx);
    assertFilterExecutionFail("LET anl='analyz' FOR d IN VIEW myView FILTER eXists(d.name, anl) RETURN d", &ctx);
  }

  // field + type + string
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleStringIdentity("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'string') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'type', 'string') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'String') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'STRING') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'invalid') RETURN d");
  }

  // field + type + string as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("ty"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue("stri"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleStringIdentity("name")).prefix_match(false);

    assertFilterSuccess("LET anl='ty' LET type='stri' FOR d IN VIEW myView FILTER exists(d.name, CONCAT(anl,'pe'), CONCAT(type,'ng')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET anl='ty' LET type='stri' FOR d IN VIEW myView FILTER eXists(d.name, CONCAT(anl,'pe'), CONCAT(type,'ng')) RETURN d", expected, &ctx);
  }

  // field + type + numeric
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNumeric("obj.name")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.obj.name, 'type', 'numeric') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.obj.name, 'type', 'numeric') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.obj.name, 'type', 'Numeric') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.obj.name, 'type', 'NUMERIC') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.obj.name, 'type', 'foo') RETURN d");
  }

  // field + type + numeric as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("ty"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue("nume"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNumeric("name")).prefix_match(false);

    assertFilterSuccess("LET anl='ty' LET type='nume' FOR d IN VIEW myView FILTER exists(d.name, CONCAT(anl,'pe'), CONCAT(type,'ric')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET anl='ty' LET type='nume' FOR d IN VIEW myView FILTER eXists(d.name, CONCAT(anl,'pe'), CONCAT(type,'ric')) RETURN d", expected, &ctx);
  }

  // field + type + bool
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleBool("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'bool') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'type', 'bool') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'Bool') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'BOOL') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'asdfasdfa') RETURN d");
  }

  // field + type + boolean
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleBool("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'boolean') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'type', 'boolean') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'Boolean') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'BOOLEAN') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'asdfasdfa') RETURN d");
  }

  // field + type + boolean as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("ty"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue("boo"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleBool("name")).prefix_match(false);

    assertFilterSuccess("LET anl='ty' LET type='boo' FOR d IN VIEW myView FILTER exists(d.name, CONCAT(anl,'pe'), CONCAT(type,'lean')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET anl='ty' LET type='boo' FOR d IN VIEW myView FILTER eXists(d.name, CONCAT(anl,'pe'), CONCAT(type,'lean')) RETURN d", expected, &ctx);
  }

  // field + type + null
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNull("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'null') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'type', 'null') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'Null') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'NULL') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'asdfasdfa') RETURN d");
  }

  // field + type + null as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("ty"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue("nu"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNull("name")).prefix_match(false);

    assertFilterSuccess("LET anl='ty' LET type='nu' FOR d IN VIEW myView FILTER exists(d.name, CONCAT(anl,'pe'), CONCAT(type,'ll')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET anl='ty' LET type='nu' FOR d IN VIEW myView FILTER eXists(d.name, CONCAT(anl,'pe'), CONCAT(type,'ll')) RETURN d", expected, &ctx);
  }

  // field + type + invalid expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("ty"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    assertFilterExecutionFail("LET anl='ty' LET type='boo' FOR d IN VIEW myView FILTER exists(d.name, CONCAT(anl,'pe'), type) RETURN d", &ctx);
    assertFilterExecutionFail("LET anl='ty' LET type='boo' FOR d IN VIEW myView FILTER eXists(d.name, CONCAT(anl,'pe'), type) RETURN d", &ctx);
  }

  // invalid 3rd argument
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 123) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 123.5) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', false) RETURN d");

  // field + type + analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleString("name", "test_analyzer")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'analyzer', 'test_analyzer') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 'foo') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 'invalid') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', '') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', null) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 123) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 123.5) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', true) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', false) RETURN d");
  }

  // field + type + analyzer as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("analyz"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue("test_"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleString("name", "test_analyzer")).prefix_match(false);

    assertFilterSuccess("LET anl='analyz' LET type='test_' FOR d IN VIEW myView FILTER exists(d.name, CONCAT(anl,'er'), CONCAT(type,'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET anl='analyz' LET type='test_' FOR d IN VIEW myView FILTER eXists(d.name, CONCAT(anl,'er'), CONCAT(type,'analyzer')) RETURN d", expected, &ctx);
  }

  // field + type + analyzer via []
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleString("name", "test_analyzer")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d['name'], 'analyzer', 'test_analyzer') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', 'foo') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', 'invalid') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', '') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', null) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', 123) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', 123.5) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', true) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d['name'], 'analyzer', false) RETURN d");
  }

  // field + type + identity analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleStringIdentity("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER eXists(d.name, 'analyzer', 'identity') RETURN d", expected);
  }

  // invalid number of arguments
  assertFilterParseFail("FOR d IN VIEW myView FILTER exists() RETURN d");
  assertFilterParseFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', 'null', d) RETURN d");
  assertFilterParseFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 'test_analyzer', false) RETURN d");

  // non-deterministic arguments
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d[RAND() ? 'name' : 'x']) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, RAND() > 2 ? 'type' : 'analyzer') RETURN d");
}

SECTION("Phrase") {
  // wrong number of arguments
  assertFilterParseFail("FOR d IN VIEW myView FILTER phrase() RETURN d");

  // without offset, custom analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("name", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phRase(d.name, 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phRase(d['name'], 'quick', 'test_analyzer') RETURN d", expected);

    // invalid attribute access
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d[*], 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.a.b[*].c, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase('d.name', 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(123, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(123.5, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(null, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(true, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(false, 'quick', 'test_analyzer') RETURN d");

    // invalid input
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 1, \"abc\" ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ 1, \"abc\" ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, true, 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], false, 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, null, 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], null, 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 3.14, 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], 1234, 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, { \"a\": 7, \"b\": \"c\" }, 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], { \"a\": 7, \"b\": \"c\" }, 'test_analyzer') RETURN d");
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("a.b.c.e[4].f[5].g[3].g.a", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", &ctx);
  }

  // field with simple offset
  // without offset, custom analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("[42]", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[42], 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[42], [ 'quick' ], 'test_analyzer') RETURN d", expected);
  }

  // without offset, custom analyzer, expressions
  // quick
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("value", arangodb::aql::AqlValue("qui"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue("test_"));

    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("name", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d.name, CONCAT(value,'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d['name'], CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d.name, [ CONCAT(value,'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d['name'], [ CONCAT(value, 'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d.name, CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d['name'], CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d.name, [ CONCAT(value, 'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d['name'], [ CONCAT(value, 'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
  }

  // without offset, custom analyzer, invalid expressions
  // quick
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("value", arangodb::aql::AqlValue("qui"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));

    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d.name, CONCAT(value,'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d.name, [ CONCAT(value,'ck') ], analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d['name'], [ CONCAT(value, 'ck') ], analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d.name, CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d.name, [ CONCAT(value, 'ck') ], analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d['name'], [ CONCAT(value, 'ck') ], analyzer) RETURN d", &ctx);
  }

  // with offset, custom analyzer
  // quick brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("name", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b").push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 0.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 0.5, 'brown' ], 'test_analyzer') RETURN d", expected);

    // wrong offset argument
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', '0', 'brown', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', null, 'brown', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', true, 'brown', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', false, 'brown', 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', d.name, 'brown', 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', '0', 'brown' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', null, 'brown' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', true, 'brown' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', false, 'brown' ], 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', d.name, 'brown' ], 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
  }

  // with offset, complex name, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj.name", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 5).push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['name'], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj['name'], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, 'quick', 5.6, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['name'], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['name'], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj['name'], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, [ 'quick', 5.6, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['name'], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
  }

  // with offset, complex name with offset, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj[3].name[1]", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 5).push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj'][3].name[1], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3]['name'][1], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj'][3]['name'][1], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj'][3].name[1], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3]['name'][1], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj'][3]['name'][1], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
  }

  // with offset, complex name, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("[5].obj.name[100]", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 5).push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5]['obj'].name[100], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj['name'][100], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5]['obj']['name'][100], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5]['obj'].name[100], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj['name'][100], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5]['obj']['name'][100], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
  }

  // multiple offsets, complex name, custom analyzer
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj.properties.id.name", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 3).push_back("r").push_back("o").push_back("w").push_back("n");
    phrase.push_back("f", 2).push_back("o").push_back("x");
    phrase.push_back("j").push_back("u").push_back("m").push_back("p").push_back("s");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj['properties'].id.name, 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['properties']['id']['name'], 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj['properties'].id.name, [ 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['properties']['id']['name'], [ 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps'] , 'test_analyzer') RETURN d", expected);

    // wrong value
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, d.brown, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2.5, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, null, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, true, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, false, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, d, 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, d.brown, 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 2, 2, 'fox', 0, 'jumps'] , 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 2.5, 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, null, 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, true, 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, false, 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2, 'fox', 0, d ], 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);

    // wrong offset argument
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', '2', 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', null, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', true, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', false, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', '2', 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', null, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', true, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', false, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d");
  }

  // multiple offsets, complex name, custom analyzer, expressions
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj.properties.id.name", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 3).push_back("r").push_back("o").push_back("w").push_back("n");
    phrase.push_back("f", 2).push_back("o").push_back("x");
    phrase.push_back("j").push_back("u").push_back("m").push_back("p").push_back("s");

    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input", arangodb::aql::AqlValue("bro"));

    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', offset+1, CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, 'fox', offset-2, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj['properties'].id.name, 'quick', 3.6, CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', offset+0.5, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', offset+1, CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, 'fox', offset-2, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj['properties'].id.name, [ 'quick', 3.6, CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', offset+0.5, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
  }

  // multiple offsets, complex name, custom analyzer, invalid expressions
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input", arangodb::aql::AqlValue("bro"));

    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', TO_BOOL(offset+1), CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', TO_STRING(2), 'fox', 0, 'jumps', 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps', TO_BOOL('test_analyzer')) RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', TO_BOOL(3.6), 'brown', 2, 'fox', offset-2, 'jumps', 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(offset+1), CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, 'brown', TO_STRING(2), 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps' ], TO_BOOL('test_analyzer')) RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(3.6), 'brown', 2, 'fox', offset-2, 'jumps' ], 'test_analyzer') RETURN d", &ctx);
  }

  // invalid analyzer
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick', [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick', false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick', null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3.14) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick', 1234) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick', { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick', 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ 'quick' ], [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ 'quick' ], false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ 'quick' ], null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], 3.14) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ 'quick' ], 1234) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ 'quick' ], { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d['name'], [ 'quick' ], 'invalid_analyzer') RETURN d");

  // wrong analylzer
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', ['d']) RETURN d");
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', [d]) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3.0) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', 3) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', 3.0) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', 'invalidAnalyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], ['d']) RETURN d");
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], [d]) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], 3) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], 3.0) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick' ], 'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 3) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 3.0) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 'invalidAnalyzer') RETURN d");

  // non-deterministic arguments
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d[RAND() ? 'name' : 0], 'quick', 0, 'brown', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, RAND() ? 'quick' : 'slow', 0, 'brown', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0, RAND() ? 'brown' : 'red', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0, 'brown', RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d[RAND() ? 'name' : 0], [ 'quick', 0, 'brown' ], 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ RAND() ? 'quick' : 'slow', 0, 'brown' ], 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 0, RAND() ? 'brown' : 'red' ], 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, [ 'quick', 0, 'brown' ], RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
}

SECTION("StartsWith") {
  // without scoring limit
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc') RETURN d", expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", &ctx);
  }

  // without scoring limit, name with offset
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name[1]")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d['name'][1], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.name[1], 'abc') RETURN d", expected);
  }

  // without scoring limit, complex name
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj.properties.name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d['obj']['properties']['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.obj['properties']['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.obj['properties'].name, 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.obj.properties.name, 'abc') RETURN d", expected);
  }

  // without scoring limit, complex name with offset
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]']['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]'].name, 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.obj[400].properties[3].name, 'abc') RETURN d", expected);
  }

  // without scoring limit, complex name with offset, prefix as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
  }

  // without scoring limit, complex name with offset, prefix as an expression of invalid type
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));

    assertFilterExecutionFail("LET prefix=false FOR d IN VIEW myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], prefix) RETURN d", &ctx);
    assertFilterExecutionFail("LET prefix=false FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]']['name'], prefix) RETURN d", &ctx);
    assertFilterExecutionFail("LET prefix=false FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]'].name, prefix) RETURN d", &ctx);
    assertFilterExecutionFail("LET prefix=false FOR d IN VIEW myView FILTER starts_with(d.obj[400].properties[3].name, prefix) RETURN d", &ctx);
  }

  // with scoring limit (int)
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name")).term("abc");
    prefix.scored_terms_limit(1024);

    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d['name'], 'abc', 1024) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', 1024) RETURN d", expected);
  }

  // with scoring limit (double)
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name")).term("abc");
    prefix.scored_terms_limit(100);

    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d['name'], 'abc', 100.5) RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', 100.5) RETURN d", expected);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(6);

    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(6);

    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN VIEW myView FILTER starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression of invalid type
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue("ab"));

    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN VIEW myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], prefix, scoringLimit) RETURN d", &ctx);
    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]']['name'], prefix, scoringLimit) RETURN d", &ctx);
    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN VIEW myView FILTER starts_with(d.obj[400]['properties[3]'].name, prefix, scoringLimit) RETURN d", &ctx);
    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN VIEW myView FILTER starts_with(d.obj[400].properties[3].name, prefix, scoringLimit) RETURN d", &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail("FOR d IN VIEW myView FILTER starts_with() RETURN d");
  assertFilterParseFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', 100, 'abc') RETURN d");

  // invalid attribute access
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(['d'], 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with([d], 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d, 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d[*], 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.a[*].c, 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with('d.name', 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(123, 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(123.5, 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(null, 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(true, 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(false, 'abc') RETURN d");

  // invalid value
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 1) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 1.5) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, null) RETURN d");
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER starts_with(d.name, d) RETURN d", &ExpressionContextMock::EMPTY);

  // invalid scoring limit
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', '1024') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', null) RETURN d");
  assertFilterExecutionFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', d) RETURN d", &ExpressionContextMock::EMPTY);

  // non-deterministic arguments
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d[RAND() ? 'name' : 'x'], 'abc') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, RAND() ? 'abc' : 'def') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', RAND() ? 128 : 10) RETURN d");
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
