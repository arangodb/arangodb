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

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchFeature.h"
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
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/AqlFunctionFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"

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

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);

struct TestTermAttribute: public irs::term_attribute {
 public:
  void value(irs::bytes_ref const& value) {
    value_ = value;
  }
};

class TestAnalyzer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();

  static ptr make(irs::string_ref const& args) {
    if (args.null()) throw std::exception();
    if (args.empty()) return nullptr;
    PTR_NAMED(TestAnalyzer, ptr);
    return ptr;
  }

  TestAnalyzer() : irs::analysis::analyzer(TestAnalyzer::type()) {
    _attrs.emplace(_term);
    _attrs.emplace(_attr);
  }

  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }

  virtual bool next() override {
    if (_data.empty()) {
      return false;
    }

    _term.value(irs::bytes_ref(_data.c_str(), 1));
    _data = irs::bytes_ref(_data.c_str() + 1, _data.size() - 1);
    return true;
  }

  virtual bool reset(irs::string_ref const& data) override {
    _data = irs::ref_cast<irs::byte_type>(data);
    return true;
  }

 private:
  irs::attribute_view _attrs;
  irs::bytes_ref _data;
  TestTermAttribute _term;
  TestAttribute _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(TestAnalyzer, "TestCharAnalyzer");
REGISTER_ANALYZER(TestAnalyzer);

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
    false, &vocbase, arangodb::aql::QueryString(queryString),
    nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
    arangodb::transaction::UserTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
    );

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    irs::Or expected;
    expected.add<arangodb::iresearch::ByExpression>().init(
      *dummyPlan,
      *ast,
      *filterNode->getMember(0), // 'd.a.b[_REFERENCE_('c')]
      trx,
      ExpressionContextMock::EMPTY
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
    std::string const& refName = "d") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     nullptr, options,
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
    CHECK((parseOk == arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode)));
  }

  // iteratorForCondition
  {
    arangodb::transaction::UserTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
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
    std::string const& refName = "d") {
  return assertFilter(true, true, queryString, expected, exprCtx, refName);
}

void assertFilterExecutionFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::string const& refName = "d") {
  irs::Or expected;
  return assertFilter(true, false, queryString, expected, exprCtx, refName);
}

void assertFilterFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::string const& refName = "d") {
  irs::Or expected;
  return assertFilter(false, false, queryString, expected, exprCtx, refName);
}

void assertFilterParseFail(std::string const& queryString) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     nullptr, nullptr,
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

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature

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
      false,
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
      false,
      [](arangodb::aql::Query*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    auto* analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

    analyzers->emplace("test_analyzer", "TestCharAnalyzer", "abc"); // cache analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchFilterSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(&server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::FeatureCacheFeature::reset();
  }
}; // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchFilterTest", "[iresearch][iresearch-filter]") {
  IResearchFilterSetup s;
  UNUSED(s);

SECTION("BinaryIn") {
  // simple attribute
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] in ['1','2','3'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d[1] in ['1','2','3'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a[1]")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a[1]")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a[1]")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a[1] in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'][1] in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].e.f in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name with offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c[412].e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c[412].e.f")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c[412].e.f")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'][412].e.f in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[412].e.f in ['1','2','3'] RETURN d", expected);
  }

  // heterogeneous array values
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("quick.brown.fox")).term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      CHECK(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess("FOR d IN collection FILTER d.quick.brown.fox in ['1',null,true,false,2] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.quick['brown'].fox in ['1',null,true,false,2] RETURN d", expected);
  }

  // empty array
  {
    irs::Or expected;
    auto& root = expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER d.quick.brown.fox in [] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['quick'].brown.fox in [] RETURN d", expected);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("3");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in ['1','2','3'] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in ['1','2','3'] RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in ['1','2','3'] RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in ['1','2','3'] RETURN d", &ctx);
  }

  // reference in array
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    // not a constant in array
    assertFilterSuccess(
      "LET c=2 FOR d IN collection FILTER d.a.b.c.e.f in ['1', c, '3'] RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // array as reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess("LET x=['1', 2, '3'] FOR d IN collection FILTER d.a.b.c.e.f in x RETURN d", expected, &ctx);
  }

  // nondeterministic value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', RAND(), '3' ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        CHECK(irs::Or::type() == root.type());
        CHECK(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          CHECK(expected == *begin);
        }

        CHECK(root.end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', d, '3' ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        CHECK(irs::Or::type() == root.type());
        CHECK(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          CHECK(expected == *begin);
        }

        CHECK(root.end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', d.e, d.a.b.c.e.f ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        CHECK(irs::Or::type() == root.type());
        CHECK(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        CHECK(root.end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', 1+d.b, '3' ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        CHECK(irs::Or::type() == root.type());
        CHECK(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          CHECK(expected == *begin);
        }

        CHECK(root.end() == ++begin);
      }
    }
  }

  // nondeterministic attribute access in value
  {
    std::string const queryString = "FOR d IN collection FILTER 4 in [ 1, d.a[_NONDETERM_('abc')], 4 ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        CHECK(irs::Or::type() == root.type());
        CHECK(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          CHECK(irs::empty() == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          CHECK(irs::all() == *++begin);
        }

        CHECK(root.end() == ++begin);
      }
    }
  }

  // self-reference in value
  {
    std::string const queryString = "FOR d IN collection FILTER 4 in [ 1, d.b.a, 4 ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        CHECK(irs::Or::type() == root.type());
        CHECK(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          CHECK(irs::empty() == *begin);
        }

        // 2nd filter
        {
          irs::numeric_token_stream stream;
          stream.reset(4.0);
          auto& term = stream.attributes().get<irs::term_attribute>();
          CHECK(stream.next());

          irs::by_term expected;
          expected.field(mangleNumeric("b.a")).term(term->value());
          CHECK(expected == *++begin);
        }

        // 3rd filter
        {
          CHECK(irs::all() == *++begin);
        }

        CHECK(root.end() == ++begin);
      }
    }
  }

  assertExpressionFilter("FOR d IN collection FILTER 4 in [ 1, 1+d.b, 4 ] RETURN d");

  // heterogeneous references and expression in array
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
      "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR d IN collection FILTER d.a.b.c.e.f in ['1', strVal, boolVal, numVal+1, nullVal] RETURN d",
      expected,
      &ctx // expression context
    );
  }

  assertExpressionFilter("FOR d IN VIEW myView FILTER [1,2,'3'] in d.a RETURN d");

  // non-deterministic expression name in array
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] in ['1','2','3'] RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN VIEW myView FILTER d in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER d[*] in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER d.a[*] in [1,2,3] RETURN d");

  // no reference provided
  assertFilterExecutionFail("LET x={} FOR d IN VIEW myView FILTER d.a in [1,x.a,3] RETURN d", &ExpressionContextMock::EMPTY);

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER [] in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER ['d'] in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 'd.a' in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER null in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER true in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER false in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4.5 in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 1..2 in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY); // by some reason arangodb evaluates it to false
  }

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 in [1,2,3,4] RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // not a value in array
  assertFilterFail("FOR d IN collection FILTER d.a in ['1',['2'],'3'] RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a in ['1', {\"abc\": \"def\"},'3'] RETURN d");

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b['c'].e.f in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric floating range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in 4.5..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b['c.e.f'] in 4.5..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric int-float range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in 4..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c.e['f'] in 4..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(102.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f in c..c+100 RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100]['b'].c[1].e.f in c..c+100 RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(102.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in 2..102 RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in 2..102 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in 2..102 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in 2..102 RETURN d", &ctx);
  }

  // string range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b.c.e.f'] in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b.c.e.f'] in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f[4] in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b.c.e.f'][4] in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b.c.e.f[4]'] in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f[4] in '4a'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b.c.e.f[4]'] in '4'..'5av' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a['b.c.e.f'][4] in 'a4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // string expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f in TO_STRING(c)..TO_STRING(c+2) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f in TO_STRING(c)..TO_STRING(c+2) RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in '2'..'4' RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in '2'..'4' RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in '2'..'4' RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in '2'..'4' RETURN d", &ctx);
  }

  // boolean range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.e.f in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b['c.e.f'] in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // boolean range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("[100].a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d[100].a.b.c.e.f in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d[100]['a'].b.c.e.f in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d[100]['a'].b['c.e.f'] in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // boolean expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f in TO_BOOL(c)..IS_NULL(TO_BOOL(c-2)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f in TO_BOOL(c)..TO_BOOL(c-2) RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in true..false RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in true..false RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in true..false RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in false..true RETURN d", &ctx);
  }

  // null range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a.b.c.e.f'] in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // null range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e[32].f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a[100].b.c[1].e[32].f in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a[100].b.c[1].e[32].f'] in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // null expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a[100].b.c[1].e.f in c..null RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a[100].b.c[1]['e'].f in c..null RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in null..null RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in null..null RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in null..null RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in null..null RETURN d", &ctx);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a in 'a'..4 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a in 1..null RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a in false..5.5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a in 1..4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a in 'false'..1 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a in 0..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a in null..true RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // range as reference
  {
    arangodb::aql::AqlValue value(1,3);
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(1,3));

    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(3.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET x=1..3 FOR d IN collection FILTER d.a.b.c.e.f in x RETURN d", expected, &ctx);
  }

  // non-deterministic expression name in range
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] in 4..5 RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] in _NONDETERM_(4)..5 RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN VIEW myView FILTER d in 4..5 RETURN d");
  assertExpressionFilter("for d in VIEW myView filter d[*] in 4..5 return d");
  assertExpressionFilter("for d in VIEW myView filter d.a[*] in 4..5 return d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER d.a in d.b..5 RETURN d");
  assertFilterExecutionFail("LET x={} FOR d IN VIEW myView FILTER 4..5 in x.a RETURN d", &ExpressionContextMock::EMPTY); // no reference to x
  assertFilterExecutionFail("LET x={} FOR d IN VIEW myView FILTER 4 in x.a RETURN d", &ExpressionContextMock::EMPTY); // no reference to x
  assertExpressionFilter("for d in VIEW myView filter 4..5 in d.a return d"); // self-reference
  assertExpressionFilter("FOR d IN VIEW myView FILTER 4 in d.b..5 RETURN d"); // self-reference

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER [] in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER ['d'] in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 'd.a' in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER null in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER true in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER false in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4.3 in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY); // ArangoDB feature
  }

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 in 4..4+1 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }
}

SECTION("BinaryNotIn") {
  // simple attribute
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a not in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] not in ['1','2','3'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d[1] not in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c.e.f not in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].e.f not in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name, offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c[323].e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c[323].e.f")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c[323].e.f")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[323].e.f not in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c[323].e.f not in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'][323].e.f not in ['1','2','3'] RETURN d", expected);
  }

  // heterogeneous array values
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("quick.brown.fox")).term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      CHECK(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess("FOR d IN collection FILTER d.quick.brown.fox not in ['1',null,true,false,2] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.quick['brown'].fox not in ['1',null,true,false,2] RETURN d", expected);
  }

  // empty array
  {
    irs::Or expected;
    auto& root = expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER d.quick.brown.fox not in [] RETURN d", expected);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("3");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in ['1','2','3'] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in ['1','2','3'] RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in ['1','2','3'] RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in ['1','2','3'] RETURN d", &ctx);
  }

  // array as reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess("LET x=['1', 2, '3'] FOR d IN collection FILTER d.a.b.c.e.f not in x RETURN d", expected, &ctx);
  }

  // reference in array
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    // not a constant in array
    assertFilterSuccess(
      "LET c=2 FOR d IN collection FILTER d.a.b.c.e.f not in ['1', c, '3'] RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // nondeterministic value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', RAND(), '3' ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        CHECK(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        CHECK(andNode);
        CHECK(irs::And::type() == andNode->type());
        CHECK(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          CHECK(expected == *begin);
        }

        CHECK(andNode->end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', d.a, '3' ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        CHECK(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        CHECK(andNode);
        CHECK(irs::And::type() == andNode->type());
        CHECK(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          CHECK(expected == *begin);
        }

        CHECK(andNode->end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', 1+d.a, '3' ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        CHECK(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        CHECK(andNode);
        CHECK(irs::And::type() == andNode->type());
        CHECK(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          CHECK(expected == *begin);
        }

        CHECK(andNode->end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', d.e, d.a.b.c.e.f ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        CHECK(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        CHECK(andNode);
        CHECK(irs::And::type() == andNode->type());
        CHECK(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          CHECK(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        CHECK(andNode->end() == ++begin);
      }
    }
  }

  // nondeterministic attribute access in value
  {
    std::string const queryString = "FOR d IN collection FILTER 4 not in [ 1, d.a[_NONDETERM_('abc')], 4 ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());
        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        CHECK(irs::Not::type() == notNode.type());
        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        CHECK(andNode);
        CHECK(irs::And::type() == andNode->type());
        CHECK(3 == andNode->size());
        auto begin = andNode->begin();

        // 1st filter
        {
          CHECK(irs::empty() == *begin);
        }

        // 2nd filter
        {
          ++begin;
          CHECK(arangodb::iresearch::ByExpression::type() == begin->type());
          CHECK(nullptr != dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          CHECK(irs::all() == *++begin);
        }

        CHECK(andNode->end() == ++begin);
      }
    }
  }

  // self-reference in value
  {
    std::string const queryString = "FOR d IN collection FILTER 4 not in [ 1, d.b.a, 4 ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(
       false, &vocbase, arangodb::aql::QueryString(queryString),
       nullptr, options,
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));

      {
        CHECK(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        CHECK(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        CHECK(andNode);
        CHECK(irs::And::type() == andNode->type());
        CHECK(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          CHECK(irs::empty() == *begin);
        }

        // 2nd filter
        {
          irs::numeric_token_stream stream;
          stream.reset(4.0);
          auto& term = stream.attributes().get<irs::term_attribute>();
          CHECK(stream.next());

          irs::by_term expected;
          expected.field(mangleNumeric("b.a")).term(term->value());
          CHECK(expected == *++begin);
        }

        // 3rd filter
        {
          CHECK(irs::all() == *++begin);
        }

        CHECK(andNode->end() == ++begin);
      }
    }
  }

  assertExpressionFilter("FOR d IN collection FILTER 4 not in [ 1, 1+d.b, 4 ] RETURN d");

  // heterogeneous references and expression in array
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
      "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR d IN collection FILTER d.a.b.c.e.f not in ['1', strVal, boolVal, numVal+1, nullVal] RETURN d",
      expected,
      &ctx // expression context
    );
  }

  assertExpressionFilter("FOR d IN VIEW myView FILTER [1,2,'3'] not in d.a RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN VIEW myView FILTER d not in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER d[*] not in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER d.a[*] not in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER 4 not in [1,d,3] RETURN d");

  // no reference provided
  assertFilterExecutionFail("LET x={} FOR d IN VIEW myView FILTER d.a not in [1,x.a,3] RETURN d", &ExpressionContextMock::EMPTY);

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 not in [1,2,3,4] RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER [] not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER ['d'] not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 'd.a' not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER null not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER true not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER false not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4.5 not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 1..2 not in [1,2,3] RETURN d", expected, &ExpressionContextMock::EMPTY); // by some reason arangodb evaluates it to true
  }

  // not a value in array
  assertFilterFail("FOR d IN collection FILTER d.a not in ['1',['2'],'3'] RETURN d");

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b.c.e.f'] not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b[4].c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[4].c.e.f not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b[4].c.e.f'] not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric floating range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in 4.5..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c.e.f not in 4.5..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric floating range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[3].b[1].c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a[3].b[1].c.e.f not in 4.5..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a[3]['b'][1].c.e.f not in 4.5..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric int-float range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in 4..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c['e'].f not in 4..5.0 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // numeric expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(102.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in c..c+100 RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in c..c+100 LIMIT 100 RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100]['b'].c[1].e.f not in c..c+100 RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(102.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in 2..102 RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in 2..102 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in 2..102 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in 2..102 RETURN d", &ctx);
  }

  // string range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c.e.f not in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b[3].c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[3].c.e.f not in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][3].c.e.f not in '4'..'5' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // string expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in TO_STRING(c)..TO_STRING(c+2) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f not in TO_STRING(c)..TO_STRING(c+2) RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in '2'..'4' RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in '2'..'4' RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in '2'..'4' RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in '2'..'4' RETURN d", &ctx);
  }

  // boolean range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.e.f not in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // boolean range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[1]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f[1] not in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.e.f[1] not in false..true RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // boolean expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in TO_BOOL(c)..IS_NULL(TO_BOOL(c-2)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f not in TO_BOOL(c)..TO_BOOL(c-2) RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in true..false RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in true..false RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in true..false RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in false..true RETURN d", &ctx);
  }

  // null range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c['e'].f not in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // null range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[3].f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e[3].f not in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c['e'][3].f not in null..null RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // null expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a[100].b.c[1].e.f not in c..null RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a[100].b.c[1]['e'].f not in c..null RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in null..null RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in null..null RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in null..null RETURN d", &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in null..null RETURN d", &ctx);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a not in 'a'..4 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a not in 1..null RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a not in false..5.5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a not in 1..4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a not in 'false'..1 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a not in 0..true RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER d.a not in null..true RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // range as reference
  {
    arangodb::aql::AqlValue value(1,3);
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(1,3));

    irs::numeric_token_stream minTerm; minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(3.0);
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET x=1..3 FOR d IN collection FILTER d.a.b.c.e.f not in x RETURN d", expected, &ctx);
  }

  // non-deterministic expression name in range
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] not in 4..5 RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] not in _NONDETERM_(4)..5 RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN VIEW myView FILTER d not in 4..5 RETURN d");
  assertExpressionFilter("for d in VIEW myView FILTER d[*] not in 4..5 RETURN d");
  assertExpressionFilter("for d in VIEW myView FILTER d.a[*] not in 4..5 RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER d.a not in d.b..5 RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER 4..5 not in d.a RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER [1,2,'3'] not in d.a RETURN d");
  assertExpressionFilter("FOR d IN VIEW myView FILTER 4 not in d.a RETURN d");
  assertFilterExecutionFail("LET x={} FOR d IN VIEW myView FILTER 4..5 not in x.a RETURN d", &ExpressionContextMock::EMPTY); // no reference to x
  assertFilterExecutionFail("LET x={} FOR d IN VIEW myView FILTER 4 in not x.a RETURN d", &ExpressionContextMock::EMPTY); // no reference to x
  assertExpressionFilter("for d in VIEW myView filter 4..5 not in d.a return d"); // self-reference
  assertExpressionFilter("FOR d IN VIEW myView FILTER 4 not in d.b..5 RETURN d"); // self-reference

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER [] not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER ['d'] not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 'd.a' not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER null not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER true not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER false not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4.3 not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY); // ArangoDB feature
  }

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 not in 4..5 RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN VIEW myView FILTER 4 not in 4..4+1 RETURN d", expected, &ExpressionContextMock::EMPTY);
  }
}

SECTION("Ternary") {
  // can evaluate expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{3})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=3 FOR d IN collection FILTER x > 2 ? true : false RETURN d", expected, &ctx);
  }

  // can evaluate expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=1 FOR d IN collection FILTER x > 2 ? true : false RETURN d", expected, &ctx);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter("LET x=1 FOR d IN collection FILTER x > 2 ? _NONDETERM_(true) : false RETURN d");

  // can't evaluate expression: no referenced variable in context
  assertFilterExecutionFail("LET x=1 FOR d IN collection FILTER x > 2 ? true : false RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("BinaryEq") {
  // simple attribute, string
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d['a'] RETURN d", expected);
  }

  // simple offset, string
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d[1] == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d[1] RETURN d", expected);
  }

  // complex attribute, string
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleStringIdentity("a.b.c")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute with offset, string
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleStringIdentity("a.b[23].c")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[23].c == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][23].c == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][23].c == '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a.b[23].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a['b'][23].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d['a']['b'][23]['c'] RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleStringIdentity("a.b[23].c")).term("42");

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c == TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23].c == TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d['a']['b'][23].c == TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) == d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) == d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) == d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name with deterministic expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("1");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1' RETURN d", &ctx);
  }

  // complex attribute, true
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleBool("a.b.c")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c == true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true == d.a.b.c RETURN d", expected);
  }

  // complex attribute with offset, true
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleBool("a[1].b.c")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a[1].b.c == true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true == d.a[1].b.c RETURN d", expected);
  }

  // complex attribute, false
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleBool("a.b.c.bool")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool == false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b['c.bool'] == false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false == d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false == d['a'].b['c'].bool RETURN d", expected);
  }

  // expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN VIEW myView FILTER 1 == true RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleBool("a.b[23].c")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c == TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23].c == TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d['a']['b'][23].c == TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) == d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) == d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) == d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleBool("a.b.c.e[4].f[5].g[3].g.a")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER true == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true RETURN d", &ctx);
  }

  // complex attribute, null
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNull("a.b.c.bool")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c.bool == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c.bool == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null == d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null == d['a.b.c.bool'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null == d.a.b.c['bool'] RETURN d", expected);
  }

  // complex attribute with offset, null
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNull("a[1].b[2].c[3].bool")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a[1].b[2].c[3].bool == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a[1]['b'][2].c[3].bool == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'][1]['b'][2].c[3].bool == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null == d.a[1].b[2].c[3].bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null == d['a[1].b[2].c[3].bool'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null == d.a[1].b[2].c[3]['bool'] RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNull("a.b[23].c")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a.b[23].c == (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a['b'][23].c == (c && false) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d['a']['b'][23].c == (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) == d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) == d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) == d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNull("a.b.c.e[4].f[5].g[3].g.a")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER null == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null RETURN d", &ctx);
  }

  // complex attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNumeric("a.b.c.numeric")).term(term->value());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric == 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c.numeric == 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric == 3.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c['numeric'] == 3.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3 == d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 == d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 == d['a.b.c'].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 == d.a['b.c.numeric'] RETURN d", expected);
  }

  // complex attribute with offset, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNumeric("a.b[3].c.numeric")).term(term->value());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[3].c.numeric == 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][3].c.numeric == 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b[3].c.numeric == 3.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b[3].c['numeric'] == 3.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3 == d.a.b[3].c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 == d.a.b[3].c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 == d['a.b[3].c'].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 == d.a['b[3].c.numeric'] RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNumeric("a.b[23].c")).term(term->value());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c == (c + 1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23].c == (c + 1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d['a']['b'][23].c == (c + 1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c + 1.5) == d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c + 1.5) == d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c + 1.5) == d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a")).term(term->value());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5 RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER 42.5 == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5 RETURN d", &ctx);
  }

  // complex range expression
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleBool("a.b.c")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER 3 == 2 == d.a.b.c RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET k={} FOR d IN collection FILTER k.a == '1' RETURN d", expected, &ctx);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] == '1' RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a == _NONDETERM_('1') RETURN d");
  assertExpressionFilter("LET k={} FOR d IN collection FILTER k.a == _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER 3 == (2 == d.a.b.c) RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d == '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] == '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] == '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER '1' == d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a == 1+d.b RETURN d");

  // unsupported node types : fail on parse
  assertFilterFail("FOR d IN collection FILTER d.a == {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} == d.a RETURN d");

  // unsupported node types : fail on execution
  assertFilterExecutionFail("FOR d IN collection FILTER d.a == 1..2 RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN collection FILTER 1..2 == d.a RETURN d", &ExpressionContextMock::EMPTY);

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric == 2 == 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER 2 == d.a.b.c.numeric == 3 RETURN d");
}

SECTION("BinaryNotEq") {
  // simple string attribute
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("a")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d['a'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("[4]")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d[4] != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d[4] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("a.b.c")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d['a'].b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d['a']['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("a.b[23].c")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[23].c != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b[23].c != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][23].c != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][23]['c'] != '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d.a.b[23].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d['a'].b[23].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d['a']['b'][23].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' != d['a']['b'][23]['c'] RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("a.b[23].c")).term("42");

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c != TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23].c != TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d['a']['b'][23].c != TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) != d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) != d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) != d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("1");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != '1' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != '1' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != '1' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != '1' RETURN d", &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleBool("a.b.c")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c != true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c != true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true != d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true != d['a']['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleBool("a.b.c.bool")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool != false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'].bool != false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false != d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false != d['a']['b'].c.bool RETURN d", expected);
  }

  // complex boolean attribute with offset, false
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleBool("a[12].b.c.bool")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a[12].b.c.bool != false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'][12]['b']['c'].bool != false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false != d.a[12].b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false != d['a'][12]['b'].c.bool RETURN d", expected);
  }

  // complex boolean attribute, null
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNull("a.b.c.bool")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool != null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].bool != null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null != d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null != d['a']['b'].c.bool RETURN d", expected);
  }

  // complex boolean attribute with offset, null
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNull("a.b.c[3].bool")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[3].bool != null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'][3].bool != null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null != d.a.b.c[3].bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null != d['a']['b'].c[3].bool RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleBool("a.b[23].c")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c != TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23].c != TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d['a']['b'][23].c != TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) != d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) != d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) != d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleBool("a.b.c.e[4].f[5].g[3].g.a")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != true RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER true != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != true RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != true RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != true RETURN d", &ctx);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNull("a.b[23].c")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a.b[23].c != (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a['b'][23].c != (c && false) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d['a']['b'][23].c != (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) != d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) != d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) != d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNull("a.b.c.e[4].f[5].g[3].g.a")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != null RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER null != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != null RETURN d", &ctx);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNumeric("a.b.c.numeric")).term(term->value());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric != 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c.numeric != 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric != 3.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3 != d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 != d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 != d.a['b']['c'].numeric RETURN d", expected);
  }

  // complex boolean attribute with offset, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNumeric("a.b.c.numeric[1]")).term(term->value());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric[1] != 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c.numeric[1] != 3 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric[1] != 3.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3 != d.a.b.c.numeric[1] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 != d.a.b.c.numeric[1] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 3.0 != d.a['b']['c'].numeric[1] RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNumeric("a.b[23].c")).term(term->value());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c != (c + 1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23].c != (c + 1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d['a']['b'][23].c != (c + 1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c + 1.5) != d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c + 1.5) != d.a['b'][23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c + 1.5) != d['a']['b'][23]['c'] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a")).term(term->value());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != 42.5 RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER 42.5 != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] != 42.5 RETURN d", &ctx);
  }

  // complex range expression
  {
    irs::Or expected;
    expected.add<irs::Not>().filter<irs::by_term>().field(mangleBool("a.b.c")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER 3 != 2 != d.a.b.c RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, reachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET k={} FOR d IN collection FILTER k.a != '1' RETURN d", expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER ['d'] != '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER [] != '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] != '1' RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a != _NONDETERM_('1') RETURN d");
  assertExpressionFilter("LET k={} FOR d IN collection FILTER k.a != _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER 3 != (2 != d.a.b.c) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d != '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] != '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] != '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER '1' != d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER 2 != d.a.b.c.numeric != 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER 2 == d.a.b.c.numeric != 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric != 2 != 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric != 2 == 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a == 1+d.b RETURN d");

  // unsupported node types : fail on parse
  assertFilterFail("FOR d IN collection FILTER d.a != {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} != d.a RETURN d");
  // unsupported node types : fail on execution
  assertFilterExecutionFail("FOR d IN collection FILTER d.a != 1..2 RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN collection FILTER 1..2 != d.a RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("BinaryGE") {
  // simple string attribute
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a >= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] >= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' <= d.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' <= d['a'] RETURN d", expected);
  }

  // simple string offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("[23]"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d[23] >= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' <= d[23] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' <= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' <= d['a']['b'].c RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b[23].c"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[23].c >= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][23]['c'] >= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' <= d.a.b[23].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' <= d['a']['b'][23].c RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b[23].c"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("42");

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c >= TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] >= TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) <= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) <= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("42");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= '42' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '42' <= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= '42' RETURN d", &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true <= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true <= d['a']['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute with offset, true
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c[223]"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[223] >= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'][223] >= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true <= d.a.b.c[223] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true <= d['a']['b']['c'][223] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.bool"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool >= false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c.bool >= false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a['b']['c'].bool RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b[23].c"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c >= TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] >= TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) <= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) <= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= false RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER false <= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= false RETURN d", &ctx);
  }

  // complex boolean attribute, null
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.nil"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.nil >= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'].nil >= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null <= d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null <= d['a']['b'].c.nil RETURN d", expected);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b[23].c.nil"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[23].c.nil >= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][23]['c'].nil >= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null <= d.a.b[23].c.nil RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null <= d['a']['b'][23].c.nil RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b[23].c"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a.b[23].c >= (c && false) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] >= (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) <= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) <= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= null RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER null <= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= null RETURN d", &ctx);
  }

  // complex numeric attribute
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.numeric"))
            .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric >= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c.numeric >= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric >= 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 <= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 <= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 <= d['a']['b']['c'].numeric RETURN d", expected);
  }

  // complex numeric attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c[223].numeric"))
            .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[223].numeric >= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c[223].numeric >= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[223].numeric >= 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 <= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 <= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 <= d['a']['b']['c'][223].numeric RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b[23].c"))
            .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c >= (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] >= (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) <= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) <= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= 42.5 RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER 42.5 <= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] >= 42.5 RETURN d", &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER 3 >= 2 >= d.a.b.c RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET k='' FOR d IN collection FILTER k.a >= '1' RETURN d", expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER [] >= '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER ['d'] >= '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] >= '1' RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a >= _NONDETERM_('1') RETURN d");
  assertExpressionFilter("LET k={} FOR d IN collection FILTER k.a >= _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER 3 >= (2 >= d.a.b.c) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d >= '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] >= '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] >= '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER '1' <= d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER 2 >= d.a.b.c.numeric >= 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric >= 2 >= 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric >= 2 >= 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a >= 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a >= {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} <= d.a RETURN d");
  assertFilterExecutionFail("FOR d IN collection FILTER d.a >= 1..2 RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN collection FILTER 1..2 <= d.a RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("BinaryGT") {
  // simple string attribute
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a > '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] > '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' < d.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' < d['a'] RETURN d", expected);
  }

  // simple string offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("[23]"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d[23] > '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' < d[23] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' < d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' < d['a']['b'].c RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b[23].c"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[23].c > '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][23]['c'] > '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' < d.a.b[23].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' < d['a']['b'][23].c RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b[23].c"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("42");

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c > TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] > TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) < d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) < d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("42");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '42' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '42' < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '42' RETURN d", &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true < d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true < d['a'].b.c RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.bool"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool > false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.bool > false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d['a']['b']['c'].bool RETURN d", expected);
  }

  // complex boolean attribute with, false
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c[223].bool"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[223].bool > false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c[223].bool > false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.a.b.c[223].bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d['a']['b']['c'][223].bool RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b[23].c"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c > TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] > TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) < d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) < d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > false RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER false < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > false RETURN d", &ctx);
  }

  // complex null attribute
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.nil"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.nil > null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.nil > null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d['a'].b.c.nil RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b[23].c"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a.b[23].c > (c && false) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] > (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) < d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) < d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > null RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER null < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > null RETURN d", &ctx);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b[23].c.nil"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[23].c.nil > null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][23]['c'].nil > null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d.a.b[23].c.nil RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d['a']['b'][23].c.nil RETURN d", expected);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.numeric"))
            .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric > 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'].numeric > 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric > 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 < d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 < d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 < d['a']['b'].c.numeric RETURN d", expected);
  }

  // complex numeric attribute, floating
  {
    irs::numeric_token_stream stream;
    stream.reset(13.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.numeric"))
            .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric > 13.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].numeric > 13.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.5 < d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.5 < d['a']['b'].c.numeric RETURN d", expected);
  }

  // complex numeric attribute, integer
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a[1].b.c[223].numeric"))
            .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a[1].b.c[223].numeric > 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'][1]['b'].c[223].numeric > 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a[1].b.c[223].numeric > 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 < d.a[1].b.c[223].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 < d.a[1].b.c[223].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 < d['a'][1]['b']['c'][223].numeric RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b[23].c"))
            .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c > (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] > (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) < d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) < d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(stream);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 42.5 RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER 42.5 < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 42.5 RETURN d", &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER 3 > 2 > d.a.b.c RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET k={} FOR d IN collection FILTER k.a > '1' RETURN d", expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER [] > '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER ['d'] > '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] > '1' RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' > d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a > _NONDETERM_('1') RETURN d");
  assertExpressionFilter("LET k={} FOR d IN collection FILTER k.a > _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER 3 > (2 > d.a.b.c) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d > '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] > '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] > '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER '1' < d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER 2 > d.a.b.c.numeric > 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric > 2 > 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric > 2 > 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a > 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a > {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} < d.a RETURN d");
  assertFilterExecutionFail("FOR d IN collection FILTER d.a > 1..2 RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN collection FILTER 1..2 < d.a RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("BinaryLE") {
  // simple string attribute
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a <= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] <= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' >= d.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' >= d['a'] RETURN d", expected);
  }

  // simple string offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("[23]"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d[23] <= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' >= d[23] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c <= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c <= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' >= d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a[1].b.c[42]"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a[1].b.c[42] <= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'][1]['b'].c[42] <= '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' >= d.a[1].b.c[42] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' >= d['a'][1]['b']['c'][42] RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b[23].c"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("42");

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c <= TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] <= TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) >= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) >= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("42");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= '42' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '42' >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= '42' RETURN d", &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c <= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] <= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true >= d.a['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b[42].c"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[42].c <= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][42]['c'] <= true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true >= d.a.b[42].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true >= d.a['b'][42]['c'] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.bool"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool <= false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.bool <= false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false >= d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false >= d.a['b']['c'].bool RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b[23].c"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c <= TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] <= TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) >= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) >= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= false RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER false >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= false RETURN d", &ctx);
  }

  // complex null attribute
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.nil"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.nil <= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].nil <= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null >= d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null >= d['a']['b']['c'].nil RETURN d", expected);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.nil[1]"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.nil[1] <= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].nil[1] <= null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null >= d.a.b.c.nil[1] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null >= d['a']['b']['c'].nil[1] RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b[23].c"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a.b[23].c <= (c && false) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] <= (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) >= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) >= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= null RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER null >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= null RETURN d", &ctx);
  }

  // complex numeric attribute
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.numeric"))
            .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric <= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'].numeric <= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric <= 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 >= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 >= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 >= d.a['b']['c'].numeric RETURN d", expected);
  }

  // complex numeric attribute with offset
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c[223].numeric"))
            .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[223].numeric <= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'][223].numeric <= 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[223].numeric <= 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 >= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 >= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 >= d.a['b']['c'][223].numeric RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b[23].c"))
            .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c <= (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] <= (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) >= d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) >= d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= 42.5 RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER 42.5 >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= 42.5 RETURN d", &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER 3 <= 2 <= d.a.b.c RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET k={} FOR d IN collection FILTER k.a <= '1' RETURN d", expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER [] <= '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER ['d'] <= '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] <= '1' RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' <= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a <= _NONDETERM_('1') RETURN d");
  assertExpressionFilter("LET k={} FOR d IN collection FILTER k.a <= _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER 3 <= (2 <= d.a.b.c) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d <= '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] <= '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] <= '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER '1' >= d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER 2 <= d.a.b.c.numeric <= 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric <= 2 <= 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric <= 2 <= 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a <= 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a <= {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} >= d.a RETURN d");
  assertFilterExecutionFail("FOR d IN collection FILTER d.a <= 1..2 RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN collection FILTER 1..2 >= d.a RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("BinaryLT") {
  // simple string attribute
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a < '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] < '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("[42]"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d[42] < '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d[42] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] < '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b[42].c"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[42].c < '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][42]['c'] < '1' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b[42].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a']['b'][42]['c'] RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b[23].c"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("42");

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c < TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] < TO_STRING(c+1) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) > d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_STRING(c+1) > d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("42");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < '42' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '42' > d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= '42' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] <= '42' RETURN d", &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] < true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER true > d['a']['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.bool"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.bool < false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].bool < false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false > d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false > d['a'].b.c.bool RETURN d", expected);
  }

  // complex boolean attribute with offset, false
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c[42].bool[42]"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c[42].bool[42] < false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'][42].bool[42] < false RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false > d.a.b.c[42].bool[42] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false > d['a'].b.c[42].bool[42] RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b[23].c"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c < TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] < TO_BOOL(c-41) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) > d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) > d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < false RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER false > d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < false RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < false RETURN d", &ctx);
  }

  // complex null attribute
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.nil"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.nil < null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].nil < null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null > d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null > d['a'].b.c.nil RETURN d", expected);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b[42].c.nil"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[42].c.nil < null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][42]['c'].nil < null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null > d.a.b[42].c.nil RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null > d['a'].b[42].c.nil RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b[23].c"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a.b[23].c < (c && false) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] < (c && true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) > d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER (c && false) > d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleNull("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < null RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER null > d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < null RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < null RETURN d", &ctx);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.numeric"))
            .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric < 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'].numeric < 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.numeric < 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 > d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 > d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 > d['a']['b']['c'].numeric RETURN d", expected);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a[1].b[42].c.numeric"))
            .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("FOR d IN collection FILTER d.a[1].b[42].c.numeric < 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a[1]['b'][42]['c'].numeric < 13 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a[1].b[42].c.numeric < 13.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13 > d.a[1].b[42].c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 > d.a[1].b[42].c.numeric RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 13.0 > d['a'][1]['b'][42]['c'].numeric RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b[23].c"))
            .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a.b[23].c < (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] < (c+1.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) > d.a.b[23].c RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER (c+1.5) > d['a']['b'][23].c RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"))
            .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(stream);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < 42.5 RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER 42.5 > d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < 42.5 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] < 42.5 RETURN d", &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    expected.add<irs::by_range>()
            .field(mangleBool("a.b.c"))
            .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER 3 < 2 < d.a.b.c RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET k={} FOR d IN collection FILTER k.a < '1' RETURN d", expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER [] < '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER ['d'] < '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] < '1' RETURN d");
  assertExpressionFilter("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '1' < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a < _NONDETERM_('1') RETURN d");
  assertExpressionFilter("LET k={} FOR d IN collection FILTER k.a < _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER 3 < (2 < d.a.b.c) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d < '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] < '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] < '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER '1' > d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER 2 < d.a.b.c.numeric < 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric < 2 < 3 RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c.numeric < 2 < 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a < 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a < {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} > d.a RETURN d");
  assertFilterExecutionFail("FOR d IN collection FILTER d.a < 1..2 RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN collection FILTER 1..2 > d.a RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("UnaryNot") {
  // simple attribute, string
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleStringIdentity("a")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER not (d.a == '1') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a'] == '1') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not ('1' == d.a) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not ('1' == d['a']) RETURN d", expected);
  }

  // simple offset, string
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleStringIdentity("[1]")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER not (d[1] == '1') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not ('1' == d[1]) RETURN d", expected);
  }

  // complex attribute, string
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleStringIdentity("a.b.c")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c == '1') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a']['b']['c'] == '1') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not ('1' == d.a.b.c) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not ('1' == d['a']['b']['c']) RETURN d", expected);
  }

  // complex attribute with offset, string
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleStringIdentity("a.b[42].c")).term("1");

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b[42].c == '1') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a']['b'][42]['c'] == '1') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not ('1' == d.a.b[42].c) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not ('1' == d['a']['b'][42]['c']) RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleStringIdentity("a.b[23].c")).term("42");

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d.a.b[23].c == TO_STRING(c+1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d.a['b'][23].c == TO_STRING(c+1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d['a']['b'][23].c == TO_STRING(c+1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (TO_STRING(c+1) == d.a.b[23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (TO_STRING(c+1) == d.a['b'][23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (TO_STRING(c+1) == d['a']['b'][23]['c']) RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>().field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("1");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1') RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not ('1' == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == '1') RETURN d", &ctx);
  }

  // complex attribute, true
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleBool("a.b.c")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c == true) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a'].b.c == true) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (true == d.a.b.c) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (true == d.a['b']['c']) RETURN d", expected);
  }

  // complex attribute, false
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleBool("a.b.c.bool")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c.bool == false) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a'].b.c.bool == false) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (false == d.a.b.c.bool) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (false == d.a['b']['c'].bool) RETURN d", expected);
  }

  // complex attribute with offset, false
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleBool("a[1].b.c.bool")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("FOR d IN collection FILTER not (d.a[1].b.c.bool == false) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a'][1].b.c.bool == false) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (false == d.a[1].b.c.bool) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (false == d.a[1]['b']['c'].bool) RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleBool("a.b[23].c")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d.a.b[23].c == TO_BOOL(c-41)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d.a['b'][23].c == TO_BOOL(c-41)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d['a']['b'][23].c == TO_BOOL(c-41)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (TO_BOOL(c-41) == d.a.b[23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (TO_BOOL(c-41) == d.a['b'][23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (TO_BOOL(c-41) == d['a']['b'][23]['c']) RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleBool("a.b.c.e[4].f[5].g[3].g.a")).term(irs::boolean_token_stream::value_true());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true) RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (true == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == true) RETURN d", &ctx);
  }

  // complex attribute, null
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNull("a.b.c.bool")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c.bool == null) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d.a['b']['c'].bool == null) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (null == d.a.b.c.bool) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (null == d['a']['b']['c'].bool) RETURN d", expected);
  }

  // complex attribute, null
  {
    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNull("a.b.c.bool[42]")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c.bool[42] == null) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d.a['b']['c'].bool[42] == null) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (null == d.a.b.c.bool[42]) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (null == d['a']['b']['c'].bool[42]) RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNull("a.b[23].c")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("LET c=null FOR d IN collection FILTER not (d.a.b[23].c == (c && true)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER not (d.a['b'][23].c == (c && false)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER not (d['a']['b'][23].c == (c && true)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER not ((c && false) == d.a.b[23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER not ((c && false) == d.a['b'][23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=null FOR d IN collection FILTER not ((c && false) == d['a']['b'][23]['c']) RETURN d", expected, &ctx);
  }
  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNull("a.b.c.e[4].f[5].g[3].g.a")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null) RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (null == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == null) RETURN d", &ctx);
  }

  // complex attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNumeric("a.b.c.numeric")).term(term->value());

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c.numeric == 3) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a']['b']['c'].numeric == 3) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c.numeric == 3.0) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (3 == d.a.b.c.numeric) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (3.0 == d.a.b.c.numeric) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (3.0 == d.a['b']['c'].numeric) RETURN d", expected);
  }

  // according to ArangoDB rules, expression : not '1' == false
  {
    irs::Or expected;
    expected.add<irs::by_term>()
            .field(mangleBool("a"))
            .term(irs::boolean_token_stream::value_false());
    assertFilterSuccess("FOR d IN collection FILTER d.a == not '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN collection FILTER not '1' == d.a RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // complex attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNumeric("a.b.c.numeric[42]")).term(term->value());

    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c.numeric[42] == 3) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d['a']['b']['c'].numeric[42] == 3) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (d.a.b.c.numeric[42] == 3.0) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (3 == d.a.b.c.numeric[42]) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (3.0 == d.a.b.c.numeric[42]) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER not (3.0 == d.a['b']['c'].numeric[42]) RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNumeric("a.b[23].c")).term(term->value());

    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d.a.b[23].c == (c + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d.a['b'][23].c == (c + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not (d['a']['b'][23].c == (c + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not ((c + 1.5) == d.a.b[23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not ((c + 1.5) == d.a['b'][23].c) RETURN d", expected, &ctx);
    assertFilterSuccess("LET c=41 FOR d IN collection FILTER not ((c + 1.5) == d['a']['b'][23]['c']) RETURN d", expected, &ctx);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    expected.add<irs::Not>()
            .filter<irs::And>()
            .add<irs::by_term>().field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a")).term(term->value());

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5) RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (42.5 == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] == 42.5) RETURN d", &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER not [] == '1' RETURN d", expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString = "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not (d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] == '1') RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] == '1'
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString = "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER not ('1' < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // '1' < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString = "FOR d IN collection FILTER not (d.a < _NONDETERM_('1')) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d.a < _NONDETERM_('1')
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString = "LET k={} FOR d IN collection FILTER not (k.a < _NONDETERM_('1')) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // k.a < _NONDETERM_('1')
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // expression with self-reference is not supported by IResearch -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString = "FOR d IN collection FILTER not (d.a < 1+d.b) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d.a < 1+d.b
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER not d == '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER not d[*] == '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER not d.a[*] == '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER not d.a == '1' RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER not '1' == not d.a RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER '1' == not d.a RETURN d");
}

SECTION("BinaryOr") {
  // string and string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("b")).term("2");

    assertFilterSuccess("FOR d IN collection FILTER d.a == '1' or d.b == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] == '1' or d.b == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a == '1' or '2' == d.b RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a or d.b == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a or '2' == d.b RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d['a'] or '2' == d.b RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d['a'] or '2' == d['b'] RETURN d", expected);
  }

  // string or string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");
    root.add<irs::by_term>().field(mangleStringIdentity("c.b.a")).term("2");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < '1' or d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] < '1' or d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < '1' or '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b.c or d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b.c or '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a']['b']['c'] or '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a'].b.c or '2' == d.c.b.a RETURN d", expected);
  }

  // string or string or not string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    auto& subRoot = root.add<irs::Or>();
    subRoot.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    subRoot.add<irs::by_term>().field(mangleStringIdentity("a")).term("2");
    root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("b")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a == '1' or '2' == d.a or d.b != '3' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] == '1' or '2' == d['a'] or d.b != '3' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a == '1' or '2' == d.a or '3' != d.b RETURN d", expected);
  }

  // string in or not string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    auto& subRoot = root.add<irs::Or>();
    subRoot.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    subRoot.add<irs::by_term>().field(mangleStringIdentity("a")).term("2");
    root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("b")).term("3");

    assertFilterSuccess("FOR d IN collection FILTER d.a in ['1', '2'] or d.b != '3' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] in ['1', '2'] or d.b != '3' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a in ['1', '2'] or '3' != d.b RETURN d", expected);
  }

  // bool and null
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_range>()
        .field(mangleBool("b.c"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNull("a.b.c")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.b.c > false or d.a.b.c == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['b']['c'] > false or d.a.b.c == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.b.c or d.a.b.c == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.b.c > false or null == d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.b.c or null == d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.b.c or null == d['a']['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d['b']['c'] or null == d['a']['b']['c'] RETURN d", expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm);
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 or d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > 15 or d['a']['b']['c'] < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d['a']['b']['c'] or d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a['b']['c'] or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c > 15.0 or d['a']['b'].c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d['a']['b']['c'] or 40.0 > d.a.b.c RETURN d", expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 or d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c or d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d['a']['b']['c'] or d['a']['b']['c'] < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= 15 or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= 15.0 or d['a']['b'].c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a.b.c or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a.b.c or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d['a']['b'].c or 40.0 > d.a.b.c RETURN d", expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= 15 or d['a']['b']['c'] <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d['a'].b.c or 40 >= d['a'].b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 or d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a.b.c or d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a['b']['c'] or d['a']['b']['c'] <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 or 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a.b.c or 40.0 >= d.a.b.c RETURN d", expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm);
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > 15 or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d['a'].b.c or d['a'].b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15 or 40 >= d['a']['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 or d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15.0 or d.a['b']['c'] <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c or d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 or 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c or 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d['a'].b.c or 40.0 >= d['a']['b']['c'] RETURN d", expected);
  }

  // heterogeneous expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("boolVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());

    assertFilterSuccess(
      "LET boolVal=false FOR d IN collection FILTER d.a.b.c.e.f=='1' OR d.a.b.c.e.f==boolVal RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // heterogeneous expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    CHECK(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("str");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());

    assertFilterSuccess(
      "LET strVal='str' LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f==strVal OR d.a.b.c.e.f==(numVal+1) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // heterogeneous expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("boolVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    assertFilterSuccess(
      "LET boolVal=false LET nullVal=null FOR d IN collection FILTER d.a.b.c.e.f==boolVal OR d.a.b.c.e.f==nullVal RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // noneterministic expression -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    std::string const refName = "d";
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c > _NONDETERM_('15') or d.a.b.c < '40' RETURN d";

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Or>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d.a.b.c > _NONDETERM_(15)
        trx,
        ExpressionContextMock::EMPTY
      );
      root.add<irs::by_range>()
          .field(mangleStringIdentity("a.b.c"))
          .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("40"); // d.a.b.c < 40

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }
}

SECTION("BinaryAnd") {
  // string and string
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("b")).term("2");

    assertFilterSuccess("FOR d IN collection FILTER d.a == '1' and d.b == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'] == '1' and d.b == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a == '1' and '2' == d.b RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a and d.b == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d.a and '2' == d.b RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' == d['a'] and '2' == d['b'] RETURN d", expected);
  }

  // string and string
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");
    root.add<irs::by_term>().field(mangleStringIdentity("c.b.a")).term("2");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < '1' and d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] < '1' and d.c.b['a'] == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c < '1' and d.c.b['a'] == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < '1' and '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b.c and d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a']['b']['c'] and d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b.c and '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a']['b']['c'] and '2' == d.c.b['a'] RETURN d", expected);
  }

  // string and not string
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");
    root.add<irs::Not>()
        .filter<irs::And>()
        .add<irs::by_term>().field(mangleStringIdentity("c.b.a")).term("2");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < '1' and not (d.c.b.a == '2') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c < '1' and not (d.c.b['a'] == '2') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c < '1' and not ('2' == d.c.b.a) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] < '1' and not ('2' == d.c.b['a']) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b.c and not (d.c.b.a == '2') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a['b']['c'] and not (d.c.b.a == '2') RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d.a.b.c and not ('2' == d.c.b.a) RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '1' > d['a'].b.c and not ('2' == d.c.b['a']) RETURN d", expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    std::string const refName = "d";
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c < '1' and not d.c.b.a == '2' RETURN d";

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>()
          .field(mangleStringIdentity("a.b.c"))
          .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("1");
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(1), // not d.c.b.a == '2'
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // bool and null
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleBool("b.c"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNull("a.b.c")).term(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.b.c > false and d.a.b.c == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['b']['c'] > false and d['a']['b']['c'] == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['b']['c'] > false and d['a'].b.c == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.b.c and d.a.b.c == null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.b.c > false and null == d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['b']['c'] > false and null == d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.b.c and null == d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false < d.b.c and null == d['a']['b']['c'] RETURN d", expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm)
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c > 15 and d['a']['b']['c'] < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15 and d['a']['b']['c'] < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c > 15 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d['a'].b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > 15 and 40 > d['a']['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15.0 and d.a['b']['c'] < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > 15.0 and 40.0 > d.a['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and 40.0 > d.a.b.c RETURN d", expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    std::string const refName = "d";
    std::string const queryString = "FOR d IN collection FILTER d.a[*].b > 15 and d.a[*].b < 40 RETURN d";

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d.a[*].b > 15
        trx,
        ExpressionContextMock::EMPTY
      );
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(1), // d.a[*].b < 40
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // numeric range with offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b[42].c"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm)
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[42].c > 15 and d.a.b[42].c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b[42].c > 15 and d['a']['b'][42]['c'] < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][42]['c'] > 15 and d['a']['b'][42]['c'] < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b[42].c > 15 and d.a.b[42].c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b[42].c and d.a.b[42].c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d['a'].b[42].c and d.a.b[42].c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b[42].c > 15 and 40 > d.a.b[42].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][42]['c'] > 15 and 40 > d['a']['b'][42]['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b[42].c and 40 > d.a.b[42].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b[42].c > 15.0 and d.a.b[42].c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][42]['c'] > 15.0 and d.a['b'][42]['c'] < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b[42].c and d.a.b[42].c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b[42].c > 15.0 and 40.0 > d.a.b[42].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'][42]['c'] > 15.0 and 40.0 > d.a['b'][42]['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b[42].c and 40.0 > d.a.b[42].c RETURN d", expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm)
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= 15 and d['a']['b']['c'] < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d['a']['b']['c'] and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a['b']['c'] and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a.b.c and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d['a']['b']['c'] and 40.0 > d.a['b']['c'] RETURN d", expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm)
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= 15 and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d['a']['b']['c'] and d.a['b']['c'] <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15 and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d.a.b.c and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 <= d['a']['b']['c'] and 40 >= d.a['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c >= 15.0 and d['a']['b'].c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a.b.c and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.0 and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c >= 15.0 and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d.a.b.c and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 <= d['a']['b']['c'] and 40.0 >= d.a.b.c RETURN d", expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    std::string const refName = "d";
    std::string const queryString = "FOR d IN collection FILTER d.a[*].b >= 15 and d.a[*].b <= 40 RETURN d";

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d.a[*].b >= 15
        trx,
        ExpressionContextMock::EMPTY
      );
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(1), // d.a[*].b <= 40
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm)
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c > 15 and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d['a']['b']['c'] and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > 15 and 40 >= d['a']['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d['a']['b'].c and 40 >= d.a['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d['a']['b'].c and d['a']['b']['c'] <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d['a']['b'].c and 40.0 >= d.a.b.c RETURN d", expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    std::string const refName = "d";
    std::string const queryString = "FOR d IN collection FILTER d.a[*].b > 15 and d.a[*].b <= 40 RETURN d";

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d.a[*].b >= 15
        trx,
        ExpressionContextMock::EMPTY
      );
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(1), // d.a[*].b <= 40
        trx,
        ExpressionContextMock::EMPTY
      );

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }

  // dynamic complex attribute field in string range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm)
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 15 &&  d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= 40 RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER 15 < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] &&  40 >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 15 &&  d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= 40 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 15 &&  d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= 40 RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > 15 &&  d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= 40 RETURN d", &ctx);
  }

  // string range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("15")
        .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("40");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > '15' and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > '15' and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d.a.b.c and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d['a']['b'].c and d['a']['b']['c'] < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > '15' and '40' > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > '15' and '40' > d['a']['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d.a.b.c and '40' > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d.a.b.c and '40' > d.a['b']['c'] RETURN d", expected);
  }

  // string range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("15")
        .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("40");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c >= '15' and d['a']['b']['c'] < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c >= '15' and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and '40' > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= '15' and '40' > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and '40' > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d['a']['b']['c'] and '40' > d.a['b']['c'] RETURN d", expected);
  }

  // string range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("15")
        .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("40");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d['a']['b'].c and d.a['b']['c'] <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d['a'].b.c and '40' >= d['a']['b'].c RETURN d", expected);
  }

  // string range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("15")
        .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("40");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > '15' and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > '15' and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d['a'].b.c and d['a'].b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > '15' and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > '15' and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' < d['a']['b'].c and '40' >= d['a']['b']['c'] RETURN d", expected);
  }

  // string expression in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c.e.f"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("15")
        .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("40");

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f > TO_STRING(numVal+13) && d.a.b.c.e.f <= TO_STRING(numVal+38) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER TO_STRING(numVal+13) < d.a.b.c.e.f  && d.a.b.c.e.f <= TO_STRING(numVal+38) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // dynamic complex attribute field in string range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("15")
        .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("40");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '15' && d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= '40' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '15' < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] && '40' >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // dynamic complex attribute field in string range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c.e.f[5].g[3].g.a"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("15");
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("40");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e.f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '15' && d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= '40' RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER '15' < d[a].b[c].e.f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] && '40' >= d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '15' &&  d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= '40' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '15' &&  d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= '40' RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '15' &&  d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]  <= '40' RETURN d", &ctx);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("15");
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c >= '15' and d['a']['b'].c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c >= '15' and 40 > d['a']['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c >= '15' and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d['a']['b']['c'] < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c >= '15' and 40.0 > d['a']['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and 40.0 > d.a.b.c RETURN d", expected);
  }

  // heterogeneous expression
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c.e.f"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("15");
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c.e.f"))
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f >= TO_STRING(numVal+13) && d.a.b.c.e.f < (numVal+38) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER TO_STRING(numVal+13) <= d.a.b.c.e.f  && d.a.b.c.e.f < (numVal+38) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // heterogeneous numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.5);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c"))
            .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm)
            .include<irs::Bound::MIN>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.5 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c >= 15.5 and d['a']['b'].c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= 15.5 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.5 <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.5 and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c >= 15.5 and 40 > d['a']['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c >= 15.5 and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.5 <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.5 and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= 15.5 and d['a']['b']['c'] < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.5 <= d.a.b.c and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= 15.5 and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c >= 15.5 and 40.0 > d['a']['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.5 <= d.a.b.c and 40.0 > d.a.b.c RETURN d", expected);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm);
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("40");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c > 15 and d['a']['b'].c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c > 15 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > 15 and '40' >= d['a']['b'].c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > 15.0 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d['a'].b.c and '40' >= d.a.b.c RETURN d", expected);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleBool("a.b.c"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c >= false and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a['b']['c'] and d.a['b']['c'] <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d['a']['b']['c'] and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a['b']['c'] and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= false and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and 40.0 >= d.a.b.c RETURN d", expected);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.5);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleNull("a.b.c"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::null_token_stream::value_null());
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > null and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > null and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d.a.b.c and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d['a']['b']['c'] and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > null and 40.5 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > null and 40.5 >= d.a['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d.a.b.c and 40.5 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d['a']['b']['c'] and 40.5 >= d['a']['b']['c'] RETURN d", expected);
  }

  // range with different references
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("15");
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a['b']['c'] and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c >= '15' and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a['b']['c'] and 40 > d.a['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d['a'].b.c and d['a']['b']['c'] < 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= '15' and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a.b.c and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER '15' <= d.a['b']['c'] and 40.0 > d.a.b.c RETURN d", expected);
  }

  // range with different references
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>(minTerm);
    root.add<irs::by_range>()
        .field(mangleStringIdentity("a.b.c"))
        .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("40");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d['a']['b']['c'] and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15 and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15 and '40' >= d['a']['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15 < d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15.0 and d['a']['b']['c'] <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > 15.0 and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > 15.0 and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER 15.0 < d['a']['b']['c'] and '40' >= d.a.b.c RETURN d", expected);
  }

  // range with different references
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleBool("a.b.c"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a['b']['c'] and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] >= false and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d['a'].b.c and d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c >= false and 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] >= false and 40.0 >= d.a['b']['c'] RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER false <= d.a.b.c and 40.0 >= d.a.b.c RETURN d", expected);
  }

  // range with different references
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.5);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleNull("a.b.c"))
        .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>(irs::null_token_stream::value_null());
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c"))
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > null and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b']['c'] > null and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d.a.b.c and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d['a'].b.c and d.a.b.c <= 40.5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c > null and 40.5 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b']['c'] > null and 40.5 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d.a.b.c and 40.5 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER null < d['a']['b']['c'] and 40.5 >= d.a['b']['c'] RETURN d", expected);
  }

  // boolean expression in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleBool("a.b.c.e.f"))
         .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_true())
         .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f >= (numVal < 13) && d.a.b.c.e.f <= (numVal > 1) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER (numVal < 13) <= d.a.b.c.e.f  && d.a.b.c.e.f <= (numVal > 1) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // boolean and numeric expression in range
  {
    irs::numeric_token_stream maxTerm; maxTerm.reset(3.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::by_range>()
        .field(mangleBool("a.b.c.e.f"))
        .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_true());
    root.add<irs::by_granular_range>()
        .field(mangleNumeric("a.b.c.e.f"))
        .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f >= (numVal < 13) && d.a.b.c.e.f <= (numVal + 1) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER (numVal < 13) <= d.a.b.c.e.f  && d.a.b.c.e.f <= (numVal + 1) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // null expression in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleNull("a.b.c.e.f"))
         .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null())
         .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess(
      "LET nullVal=null FOR d IN collection FILTER d.a.b.c.e.f >= (nullVal && true) && d.a.b.c.e.f <= (nullVal && false) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET nullVal=null FOR d IN collection FILTER (nullVal && false) <= d.a.b.c.e.f  && d.a.b.c.e.f <= (nullVal && true) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // numeric expression in range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.5);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    expected.add<irs::by_granular_range>()
            .field(mangleNumeric("a.b.c.e.f"))
            .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm)
            .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER d.a['b'].c.e.f >= (numVal + 13.5) && d.a.b.c.e.f < (numVal + 38) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER (numVal + 13.5) <= d.a.b.c.e.f  && d.a.b.c.e.f < (numVal + 38) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // noneterministic expression -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    std::string const refName = "d";
    std::string const queryString = "FOR d IN collection FILTER d.a.b.c > _NONDETERM_('15') and d.a.b.c < '40' RETURN d";

    arangodb::aql::Query query(
      false, &vocbase, arangodb::aql::QueryString(queryString),
      nullptr, std::make_shared<arangodb::velocypack::Builder>(),
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
      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase), {}, {}, {}, arangodb::transaction::Options()
      );

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
        *dummyPlan,
        *ast,
        *filterNode->getMember(0)->getMember(0), // d.a.b.c > _NONDETERM_(15)
        trx,
        ExpressionContextMock::EMPTY
      );
      root.add<irs::by_range>()
          .field(mangleStringIdentity("a.b.c"))
          .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("40"); // d.a.b.c < 40

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
      CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
      CHECK((expected == actual));
    }
  }
}

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
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d.name, CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d['name'], CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
  }

  // without offset, custom analyzer, invalid expressions
  // quick
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("value", arangodb::aql::AqlValue("qui"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));

    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d.name, CONCAT(value,'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phrase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d.name, CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN VIEW myView FILTER phRase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
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

    // wrong offset argument
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', '0', 'brown', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', null, 'brown', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', true, 'brown', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', false, 'brown', 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', d.name, 'brown', 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
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

    // wrong value
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, d.brown, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2.5, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, null, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, true, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, false, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, d, 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);

    // wrong offset argument
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', '2', 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', null, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', true, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', false, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
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

  // non-deterministic arguments
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d[RAND() ? 'name' : 0], 'quick', 0, 'brown', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, RAND() ? 'quick' : 'slow', 0, 'brown', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0, RAND() ? 'brown' : 'red', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0, 'brown', RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
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
