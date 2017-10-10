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

#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Transaction/UserTransaction.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "ApplicationFeatures/JemallocFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "tests/Basics/icu-helper.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

NS_LOCAL

struct TestTermAttribute: public irs::term_attribute {
 public:
  void value(irs::bytes_ref const& value) {
    value_ = value;
  }
};

class TestDelimAnalyzer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();

  static ptr make(irs::string_ref const& args) {
    if (args.null()) throw std::exception();
    if (args.empty()) return nullptr;
    PTR_NAMED(TestDelimAnalyzer, ptr, args);
    return ptr;
  }

  TestDelimAnalyzer(irs::string_ref const& delim)
    : irs::analysis::analyzer(TestDelimAnalyzer::type()),
      _delim(irs::ref_cast<irs::byte_type>(delim)) {
    _attrs.emplace(_term);
  }

  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }

  virtual bool next() override {
    if (_data.empty()) {
      return false;
    }

    size_t i = 0;

    for (size_t count = _data.size(); i < count; ++i) {
      auto data = irs::ref_cast<char>(_data);
      auto delim = irs::ref_cast<char>(_delim);

      if (0 == strncmp(&(data.c_str()[i]), delim.c_str(), delim.size())) {
        _term.value(irs::bytes_ref(_data.c_str(), i));
        _data = irs::bytes_ref(_data.c_str() + i + (std::max)(size_t(1), _delim.size()), _data.size() - i - (std::max)(size_t(1), _delim.size()));
        return true;
      }
    }

    _term.value(_data);
    _data = irs::bytes_ref::nil;
    return true;
  }

  virtual bool reset(irs::string_ref const& data) override {
    _data = irs::ref_cast<irs::byte_type>(data);
    return true;
  }

 private:
  irs::attribute_view _attrs;
  irs::bytes_ref _delim;
  irs::bytes_ref _data;
  TestTermAttribute _term;
};

DEFINE_ANALYZER_TYPE_NAMED(TestDelimAnalyzer, "TestDelimAnalyzer");
REGISTER_ANALYZER(TestDelimAnalyzer);

arangodb::aql::QueryResult executeQuery(
    TRI_vocbase_t& vocbase,
    const std::string& queryString
) {
  std::shared_ptr<arangodb::velocypack::Builder> bindVars;
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
    false, &vocbase, arangodb::aql::QueryString(queryString),
    bindVars, options,
    arangodb::aql::PART_MAIN
  );

  return query.execute(arangodb::QueryRegistryFeature::QUERY_REGISTRY);
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

extern const char* ARGV0; // defined in main.cpp

struct IResearchQuerySetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQuerySetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();
    IcuInitializer::setup(ARGV0); // initialize ICU, required for Utf8Helper which is using by optimizer

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::JemallocFeature(&server), false); // required for DatabasePathFeature
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(&server), true);
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
    analyzers->emplace("test_csv_analyzer", "TestDelimAnalyzer", ","); // cache analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchQuerySetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(&server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
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
}; // IResearchQuerySetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchQueryTestBooleanTerm", "[iresearch][iresearch-query]") {
  IResearchQuerySetup s;
  UNUSED(s);

// ==, !=, <, <=, >, >=, range
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  arangodb::LogicalView* view{};
  std::vector<arangodb::velocypack::Builder> insertedDocs;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto* collection = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs {
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -7 }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -6, \"value\": false}"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -5, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -4, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -3, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -2, \"value\": false}"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -1, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 0, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 1, \"value\": false}")
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      collection->cid(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));

    for (auto& entry: docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      CHECK((res.successful()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    CHECK((trx.commit().ok()));
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
    auto* collection = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs {
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 2, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 3, \"value\": false}"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 4, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 5, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 6, \"value\": false}"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 7, \"value\": false}"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": 8 }")
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      collection->cid(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));

    for (auto& entry: docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      REQUIRE((res.successful()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    CHECK((trx.commit().ok()));
  }

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"iresearch\" }");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));

    view = logicalView.get();
    REQUIRE(nullptr != view);
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view->getImplementation());
    REQUIRE((false == !impl));

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, \"trackListPositions\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
      "}}"
    );
    CHECK((impl->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((2 == impl->linkCount()));
    impl->sync();
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                ==
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == 'true' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == 'false' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == 1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value == true, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      REQUIRE(docSlice.isObject());
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      REQUIRE(keySlice.isNumber());
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value == false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value == false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == false SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                !=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;

    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const keySlice = docSlice.get("seq");
      auto const fieldSlice = docSlice.get("value");

      if (!fieldSlice.isNone() && (fieldSlice.isString() && "true"  == arangodb::iresearch::getStringRef(fieldSlice))) {
        continue;
      }

      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != 'true' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;

    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const keySlice = docSlice.get("seq");
      auto const fieldSlice = docSlice.get("value");

      if (!fieldSlice.isNone() && (fieldSlice.isString() && "false"  == arangodb::iresearch::getStringRef(fieldSlice))) {
        continue;
      }

      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != 'false' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;

    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const keySlice = docSlice.get("seq");
      auto const fieldSlice = docSlice.get("value");

      if (!fieldSlice.isNone() && (fieldSlice.isNumber() && 0. == fieldSlice.getNumber<double>())) {
        continue;
      }

      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const keySlice = docSlice.get("seq");
      auto const fieldSlice = docSlice.get("value");

      if (!fieldSlice.isNone() && (fieldSlice.isNumber() && 1. == fieldSlice.getNumber<double>())) {
        continue;
      }

      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != 1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const keySlice = docSlice.get("seq");
      auto const fieldSlice = docSlice.get("value");

      if (!fieldSlice.isNone() && fieldSlice.isNull()) {
        continue;
      }

      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value != true, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");

      if (!valueSlice.isNone() && (valueSlice.isBoolean() && valueSlice.getBoolean())) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value != false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");

      if (!valueSlice.isNone() && (valueSlice.isBoolean() && !valueSlice.getBoolean())) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value != false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");

      if (!valueSlice.isNone() && (valueSlice.isBoolean() && !valueSlice.getBoolean())) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != false SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 <
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < 'true' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < 'false' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < 1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value < true, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value < false, unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value < true, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < true SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                <=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= 'true' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= 'false' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= 1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value <= true, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value <= false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value <= true, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= true SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 >
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 'true' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 'false' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value > true, unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value > false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value > false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > false SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                >=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 'true' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 'false' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value >= true, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= false SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                      Range (>, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 'false' and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 0 and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > null and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // empty range
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > true and d.value < false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value > false AND d.value < true
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > false and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value > true AND d.value < true
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > true and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>=, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 'false' and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 0 and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= null and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // empty range
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= true and d.value < false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value >= true AND d.value < true
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= true and d.value < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value >= false AND d.value < true, BM25(d), TFIDF(d), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= false AND d.value < true SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 'false' and d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 0 and d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > null and d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value > false AND d.value <= false
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > false and d.value <= false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // empty range
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > true and d.value <= false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value > true AND d.value <= true
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > true and d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value > false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > false AND d.value <= true SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 'false' and d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 0 and d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= null and d.value <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // empty range
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= true and d.value <= false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value >= false AND d.value <= false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= false and d.value <= false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= true AND d.value <= true, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= true AND d.value <= true SORT d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= false AND d.value <= true SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // empty range
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value IN true..false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.value >= false AND d.value <= false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value IN false..false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= true AND d.value <= true, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value IN true..true SORT d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value IN false..true SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second, resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
