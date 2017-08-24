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
#include "StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/SystemDatabaseFeature.h"
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

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"
#include "search/term_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/phrase_filter.hpp"

NS_LOCAL

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);
DEFINE_FACTORY_DEFAULT(TestAttribute);

struct TestTermAttribute: public irs::term_attribute {
 public:
  DECLARE_FACTORY_DEFAULT();

  virtual ~TestTermAttribute() {}
  void clear() override { value_ = irs::bytes_ref::nil; }
  virtual const irs::bytes_ref& value() const override { return value_; }

  iresearch::bytes_ref value_;
};

DEFINE_FACTORY_DEFAULT(TestTermAttribute);

class TestAnalyzer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();

  TestAnalyzer(): irs::analysis::analyzer(TestAnalyzer::type()), _term(_attrs.emplace<TestTermAttribute>()) { _attrs.emplace<TestAttribute>(); }
  virtual irs::attribute_store const& attributes() const NOEXCEPT override { return _attrs; }
  static ptr make(irs::string_ref const& args) { if (args.null()) throw std::exception(); if (args.empty()) return nullptr; PTR_NAMED(TestAnalyzer, ptr); return ptr; }
  virtual bool next() override { if (_data.empty()) return false; _term->value_ = irs::bytes_ref(_data.c_str(), 1); _data = irs::bytes_ref(_data.c_str() + 1, _data.size() - 1); return true; }
  virtual bool reset(irs::string_ref const& data) override { _data = irs::ref_cast<irs::byte_type>(data); return true; }

 private:
  irs::attribute_store _attrs;
  irs::bytes_ref _data;
  irs::attribute_store::ref<TestTermAttribute>& _term;
};

DEFINE_ANALYZER_TYPE_NAMED(TestAnalyzer, "TestAnalyzer");
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

void assertFilterSuccess(std::string const& queryString, irs::filter const& expected) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  std::shared_ptr<arangodb::velocypack::Builder> bindVars;
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     bindVars, options,
     arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* root = query.ast()->root();
  REQUIRE(root);
  auto* filterNode = root->getMember(1);
  REQUIRE(filterNode);

  irs::Or actual;
  CHECK((arangodb::iresearch::FilterFactory::filter(nullptr, *filterNode)));
  CHECK((arangodb::iresearch::FilterFactory::filter(&actual, *filterNode)));
  CHECK(expected == actual);
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

void assertFilterFail(std::string const& queryString) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     nullptr, nullptr,
     arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* root = query.ast()->root();
  REQUIRE(root);
  auto* filterNode = root->getMember(1);
  REQUIRE(filterNode);

  irs::Or actual;
  CHECK((!arangodb::iresearch::FilterFactory::filter(nullptr, *filterNode)));
  CHECK((!arangodb::iresearch::FilterFactory::filter(&actual, *filterNode)));
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
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
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

    auto* analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

    analyzers->emplace("test_analyzer", "TestAnalyzer", "abc"); // cache analyzer

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

  // invalid attribute access
  assertFilterFail("FOR d IN VIEW myView FILTER d in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d[*] in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d.a[*] in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER [] in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER ['d'] in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 'd.a' in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER null in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER true in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER false in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4 in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4.5 in [1,2,3] RETURN d");

  // not a value in array
  assertFilterFail("FOR d IN collection FILTER d.a in ['1',['2'],'3'] RETURN d");
  // not a constant in array
  assertFilterFail("FOR d IN collection FILTER d.a in ['1', d, '3'] RETURN d");

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in 4..5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b['c'].e.f in 4..5 RETURN d", expected);
  }

  // numeric floating range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.5);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in 4.5..5.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b['c.e.f'] in 4.5..5.0 RETURN d", expected);
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

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in 4..5.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b'].c.e['f'] in 4..5.0 RETURN d", expected);
  }

  // string range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("4");
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("5");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in '4'..'5' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b.c.e.f'] in '4'..'5' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b.c.e.f'] in '4'..'5' RETURN d", expected);
  }

  // string range, attribute offset
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("4");
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("5");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f[4] in '4'..'5' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b.c.e.f'][4] in '4'..'5' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a']['b.c.e.f[4]'] in '4'..'5' RETURN d", expected);
  }

  // boolean range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleBool("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in false..true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.e.f in false..true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b['c.e.f'] in false..true RETURN d", expected);
  }

  // boolean range, attribute offset
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleBool("[100].a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d[100].a.b.c.e.f in false..true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d[100]['a'].b.c.e.f in false..true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d[100]['a'].b['c.e.f'] in false..true RETURN d", expected);
  }

  // null range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleNull("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f in null..null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a.b.c.e.f'] in null..null RETURN d", expected);
  }

  // null range
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleNull("a[100].b.c[1].e[32].f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a[100].b.c[1].e[32].f in null..null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a[100].b.c[1].e[32].f'] in null..null RETURN d", expected);
  }

  // invalid attribute access
  assertFilterFail("FOR d IN VIEW myView FILTER d in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER [] in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER ['d'] in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 'd.a' in 4..5 RETURN d");
  assertFilterFail("for d in view myview filter d[*] in 4..5 return d");
  assertFilterFail("for d in view myview filter d.a[*] in 4..5 return d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4 in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4.3 in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER null in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER true in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER false in 4..5 RETURN d");

  // invalid heterogeneous ranges
  assertFilterFail("FOR d IN VIEW myView FILTER d.a in 'a'..4 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d.a in 1..null RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d.a in false..5.5 RETURN d");

  // inverted 'in' node node
  assertFilterFail("FOR d IN VIEW myView FILTER 4..5 in d.a RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER [1,2,'3'] in d.a RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4 in d.a RETURN d");

  // invalid range (supported by AQL)
  assertFilterFail("FOR d IN VIEW myView FILTER d.a in 1..4..5 RETURN d");
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

  // invalid attribute access
  assertFilterFail("for d in view myview filter d[*] not in [1,2,3] return d");
  assertFilterFail("for d in view myview filter d.a[*] not in [1,2,3] return d");
  assertFilterFail("FOR d IN VIEW myView FILTER d not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER [] not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER ['d'] not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 'd.a' not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER null not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER true not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER false not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4 not in [1,2,3] RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4.5 not in [1,2,3] RETURN d");

  // not a value in array
  assertFilterFail("FOR d IN collection FILTER d.a not in ['1',['2'],'3'] RETURN d");

  // not a constant in array
  assertFilterFail("FOR d IN collection FILTER d.a not in ['1', d, '3'] RETURN d");

  // numeric range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in 4..5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b.c.e.f'] not in 4..5 RETURN d", expected);
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

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[4].c.e.f not in 4..5 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b[4].c.e.f'] not in 4..5 RETURN d", expected);
  }

  // numeric floating range
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.5);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in 4.5..5.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c.e.f not in 4.5..5.0 RETURN d", expected);
  }

  // numeric floating range, attribute offset
  {
    irs::numeric_token_stream minTerm; minTerm.reset(4.5);
    irs::numeric_token_stream maxTerm; maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[3].b[1].c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN collection FILTER d.a[3].b[1].c.e.f not in 4.5..5.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a[3]['b'][1].c.e.f not in 4.5..5.0 RETURN d", expected);
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

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in 4..5.0 RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c['e'].f not in 4..5.0 RETURN d", expected);
  }

  // string range
  {
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("4");
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("5");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in '4'..'5' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'].c.e.f not in '4'..'5' RETURN d", expected);
  }

  // string range, attribute offset
  {
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_range>();
    range.field(mangleStringIdentity("a.b[3].c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("4");
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("5");

    assertFilterSuccess("FOR d IN collection FILTER d.a.b[3].c.e.f not in '4'..'5' RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a['b'][3].c.e.f not in '4'..'5' RETURN d", expected);
  }

  // boolean range
  {
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_range>();
    range.field(mangleBool("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in false..true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.e.f not in false..true RETURN d", expected);
  }

  // boolean range, attribute offset
  {
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_range>();
    range.field(mangleBool("a.b.c.e.f[1]"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_false());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f[1] not in false..true RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d['a'].b.c.e.f[1] not in false..true RETURN d", expected);
  }

  // null range
  {
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_range>();
    range.field(mangleNull("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e.f not in null..null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c['e'].f not in null..null RETURN d", expected);
  }

  // null range, attribute offset
  {
    irs::Or expected;
    auto& range = expected.add<irs::Not>().filter<irs::Or>().add<irs::by_range>();
    range.field(mangleNull("a.b.c.e[3].f"));
    range.include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null());
    range.include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c.e[3].f not in null..null RETURN d", expected);
    assertFilterSuccess("FOR d IN collection FILTER d.a.b.c['e'][3].f not in null..null RETURN d", expected);
  }

  // invalid attribute access
  assertFilterFail("FOR d IN VIEW myView FILTER d not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d[*] not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d.a[*] not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER [] not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 'd.a' not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4 not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER 4.3 not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER null not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER true not in 4..5 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER false not in 4..5 RETURN d");

  // not invalid heterogeneous ranges
  assertFilterFail("FOR d IN VIEW myView FILTER d.a not in 'a'..4 RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d.a not in 1..null RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER d.a not in false..5.5 RETURN d");

  // invalid range (supported by AQL)
  assertFilterFail("FOR d IN VIEW myView FILTER d.a not in 1..4..5 RETURN d");
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

  // invalid attribute access
  assertFilterFail("FOR d IN collection FILTER k.a == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[*] == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[*] == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER '1' == d RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a == {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} == d.a RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a == 1..2 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 1..2 == d.a RETURN d");

  // invalid equality (supported by AQL)
  assertFilterFail("FOR d IN collection FILTER 2 == d.a.b.c.numeric == 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric == 2 == 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 3 == 2 == d.a.b.c.numeric RETURN d");
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

  // invalid attribute access
  assertFilterFail("FOR d IN collection FILTER ['d'] != '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER [] != '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER k.a != '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d != '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[*] != '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[*] != '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER '1' != d RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a != {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} != d.a RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a != 1..2 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 1..2 != d.a RETURN d");

  // invalid inequality (supported by AQL)
  assertFilterFail("FOR d IN collection FILTER 2 != d.a.b.c.numeric != 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 2 == d.a.b.c.numeric != 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric != 2 != 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric != 2 == 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 3 != 2 != d.a.b.c.numeric RETURN d");
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

  // complex boolean attribute, numeric
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

  // complex boolean attribute, numeric
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

  // invalid attribute access
  assertFilterFail("FOR d IN collection FILTER [] >= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER ['d'] >= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER k.a >= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d >= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[*] >= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[*] >= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER '1' <= d RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a >= {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} <= d.a RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a >= 1..2 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 1..2 <= d.a RETURN d");

  // invalid comparison (supported by AQL)
  assertFilterFail("FOR d IN collection FILTER 2 >= d.a.b.c.numeric >= 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric >= 2 >= 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric >= 2 >= 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 3 >= 2 >= d.a.b.c.numeric RETURN d");
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

  // complex boolean attribute, floating
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

  // complex boolean attribute, numeric
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

  // invalid attribute access
  assertFilterFail("FOR d IN collection FILTER [] > '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER ['d'] > '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER k.a > '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d > '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[*] > '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[*] > '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER '1' < d RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a > {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} < d.a RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a > 1..2 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 1..2 < d.a RETURN d");

  // invalid comparison (supported by AQL)
  assertFilterFail("FOR d IN collection FILTER 2 > d.a.b.c.numeric > 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric > 2 > 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric > 2 > 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 3 > 2 > d.a.b.c.numeric RETURN d");
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

  // complex boolean attribute, numeric
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

  // complex boolean attribute with offset, numeric
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

  // invalid attribute access
  assertFilterFail("FOR d IN collection FILTER []  <= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER ['d'] <= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER k.a <= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d <= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[*] <= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[*] <= '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER '1' >= d RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a <= {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} >= d.a RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a <= 1..2 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 1..2 >= d.a RETURN d");

  // invalid comparison (supported by AQL)
  assertFilterFail("FOR d IN collection FILTER 2 <= d.a.b.c.numeric <= 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric <= 2 <= 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric <= 2 <= 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 3 <= 2 <= d.a.b.c.numeric RETURN d");
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

  // invalid attribute access
  assertFilterFail("FOR d IN collection FILTER [] < '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER ['d'] < '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER k.a < '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d < '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[*] < '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[*] < '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER '1' > d RETURN d");

  // unsupported node types
  assertFilterFail("FOR d IN collection FILTER d.a < {} RETURN d");
  assertFilterFail("FOR d IN collection FILTER {} > d.a RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a < 1..2 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 1..2 > d.a RETURN d");

  // invalid comparison (supported by AQL)
  assertFilterFail("FOR d IN collection FILTER 2 < d.a.b.c.numeric < 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric < 2 < 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a.b.c.numeric < 2 < 3 RETURN d");
  assertFilterFail("FOR d IN collection FILTER 3 < 2 < d.a.b.c.numeric RETURN d");
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

  // invalid unary not usage
  assertFilterFail("FOR d IN collection FILTER not d == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER not d[*] == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER not d.a[*] == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER not [] == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER not d.a == '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a == not '1' RETURN d");
  assertFilterFail("FOR d IN collection FILTER '1' == not d.a RETURN d");
  assertFilterFail("FOR d IN collection FILTER not '1' == not d.a RETURN d");
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

    assertFilterFail("FOR d IN collection FILTER d.a.b.c < '1' and not d.c.b.a == '2' RETURN d");
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

    assertFilterFail("FOR d IN collection FILTER d.a[*].b > 15 and d.a[*].b < 40 RETURN d");
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

    assertFilterFail("FOR d IN collection FILTER d.a[*].b > 15 and d.a[*].b < 40 RETURN d");
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

    assertFilterFail("FOR d IN collection FILTER d.a[*].b.c >= 15 and d.a.b.c <= 40 RETURN d");
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

    assertFilterFail("FOR d IN collection FILTER d.a.b[*] > 15 and d.a.b.c <= 40 RETURN d");
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
}

SECTION("Value") {
  // string value == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER '1' RETURN d", expected);
  }

  // true value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER true RETURN d", expected);
  }

  // string empty value == false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER '' RETURN d", expected);
  }

  // false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER false RETURN d", expected);
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

  // zero numeric value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER 0 RETURN d", expected);
  }

  // zero floating value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER 0.0 RETURN d", expected);
  }

  // non zero floating value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 0.1 RETURN d", expected);
  }

  // Array == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER [] RETURN d", expected);
  }

  // Range == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 1..2 RETURN d", expected);
  }

  // Object == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER {} RETURN d", expected);
  }

  // reference
  assertFilterFail("FOR d IN collection FILTER d RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[1] RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[1] RETURN d");
  assertFilterFail("FOR d IN collection FILTER d[*] RETURN d");
  assertFilterFail("FOR d IN collection FILTER d.a[*] RETURN d");
}

SECTION("NaryOr") {
  // FIXME TODO
}

SECTION("NaryAnd") {
  // FIXME TODO
}

SECTION("UnsupportedUserFunctions") {
  assertFilterFail("FOR d IN VIEW myView FILTER ir::unknownFunction() RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER ir::unknownFunction1(d) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER ir::unknownFunction2(d, 'quick') RETURN d");
}

SECTION("UnsupportedSystemFunctions") {
  // FIXME TODO
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
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, d) RETURN d");
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

    // invalid 2nd argument
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'Analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'ANALYZER') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'foo') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, d) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, null) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 123) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 123.5) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, true) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, false) RETURN d");
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

  // invalid 3rd argument
  assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'type', d) RETURN d");
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
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', d) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', null) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 123) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', 123.5) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', true) RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER exists(d.name, 'analyzer', false) RETURN d");
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
}

SECTION("Phrase") {
  // wrong number of arguments
  assertFilterParseFail("FOR d IN VIEW myView FILTER phrase() RETURN d");

  // without offset, default analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("name")).push_back("quick");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['name'], 'quick') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phRase(d.name, 'quick') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phRase(d['name'], 'quick') RETURN d", expected);

    // invalid attribute access
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d, 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d[*], 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.a.b[*].c, 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase('d.name', 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(123, 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(123.5, 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(null, 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(true, 'quick') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(false, 'quick') RETURN d");
  }

  // field with simple offset
  // without offset, default analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("[42]")).push_back("quick");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[42], 'quick') RETURN d", expected);
  }

  // with offset, default analyzer
  // quick brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("name")).push_back("quick").push_back("brown");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0.0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 0.1, 'brown') RETURN d", expected);

    // wrong offset argument
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', '0', 'brown') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', null, 'brown') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', true, 'brown') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', false, 'brown') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', d.name, 'brown') RETURN d");
  }

  // with offset, complex name, default analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("obj.name")).push_back("quick").push_back("brown", 5);

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj'].name, 'quick', 5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, 'quick', 5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, 'quick', 5.0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj['name'], 'quick', 5.0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, 'quick', 5.5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['name'], 'quick', 5.5, 'brown') RETURN d", expected);
  }

  // with offset, complex name with offset, default analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("obj[3].name[1]")).push_back("quick").push_back("brown", 5);

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj'][3].name[1], 'quick', 5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], 'quick', 5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], 'quick', 5.0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3]['name'][1], 'quick', 5.0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj[3].name[1], 'quick', 5.5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj'][3]['name'][1], 'quick', 5.5, 'brown') RETURN d", expected);
  }

  // with offset, complex name, default analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("[5].obj.name[100]")).push_back("quick").push_back("brown", 5);

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5]['obj'].name[100], 'quick', 5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], 'quick', 5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], 'quick', 5.0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj['name'][100], 'quick', 5.0, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5].obj.name[100], 'quick', 5.5, 'brown') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d[5]['obj']['name'][100], 'quick', 5.5, 'brown') RETURN d", expected);
  }

  // multiple offsets, complex name, default analyzer
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("obj.properties.id.name"))
          .push_back("quick")
          .push_back("brown", 3)
          .push_back("fox", 2)
          .push_back("jumps");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, 'jumps') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d['obj']['properties']['id']['name'], 'quick', 3, 'brown', 2, 'fox', 0, 'jumps') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.0, 'brown', 2, 'fox', 0.5, 'jumps') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj['properties'].id.name, 'quick', 3.0, 'brown', 2, 'fox', 0.5, 'jumps') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.0, 'brown', 2.0, 'fox', 0.5, 'jumps') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties['id'].name, 'quick', 3.0, 'brown', 2.0, 'fox', 0.5, 'jumps') RETURN d", expected);
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.5, 'brown', 2.0, 'fox', 0.5, 'jumps') RETURN d", expected);

    // wrong value
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, d.brown, 2, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2, 2, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2.5, 2, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, null, 2, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, true, 2, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, false, 2, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, d) RETURN d");

    // wrong offset argument
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', '2', 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', null, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', true, 'fox', 0, 'jumps') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', false, 'fox', 0, 'jumps') RETURN d");
  }

  // without offset, custom analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("name", "test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 'test_analyzer') RETURN d", expected);

    // invalid attribute access
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase('d.name', 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(123, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(123.5, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(null, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(true, 'quick', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(false, 'quick', 'test_analyzer') RETURN d");
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
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', d.name, 'brown', 'test_analyzer') RETURN d");
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
    assertFilterSuccess("FOR d IN VIEW myView FILTER phrase(d.obj.name, 'quick', 5.6, 'brown', 'test_analyzer') RETURN d", expected);
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
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, d.brown, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 2.5, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, null, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, true, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, false, 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, d, 'test_analyzer') RETURN d");

    // wrong offset argument
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', '2', 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', null, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', true, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', false, 'fox', 0, 'jumps', 'test_analyzer') RETURN d");
  }

  // wrong analylzer
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', ['d']) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', [d]) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', d) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3.0) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 'invalidAnalyzer') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', d) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', 3) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', 3.0) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', null) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER phrase(d.name, 'quick', 3, 'brown', 'invalidAnalyzer') RETURN d");
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

  // invalid scoring limit
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', '1024') RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', true) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', false) RETURN d");
  assertFilterFail("FOR d IN VIEW myView FILTER starts_with(d.name, 'abc', null) RETURN d");
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
