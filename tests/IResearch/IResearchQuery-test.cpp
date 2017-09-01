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
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Transaction/UserTransaction.h"
#include "Transaction/StandaloneContext.h"
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
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "tests/Basics/icu-helper.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "IResearch/VelocyPackHelper.h"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

NS_LOCAL

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

TEST_CASE("IResearchQueryTest", "[iresearch][iresearch-query]") {
  IResearchQuerySetup s;
  UNUSED(s);

SECTION("SelectAll") {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  arangodb::LogicalCollection* logicalCollection1{};
  arangodb::LogicalCollection* logicalCollection2{};

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection2));
  }

  // add view
  auto logicalView = vocbase.createView(createJson->slice(), 0);
  REQUIRE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
      "}}"
    );
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::vector<arangodb::ManagedDocumentResult> insertedDocs(2*42);

  // populate view with the data
  {
      arangodb::OperationOptions opt;
      TRI_voc_tick_t tick;

      arangodb::transaction::UserTransaction trx(
        arangodb::transaction::StandaloneContext::Create(&vocbase),
        EMPTY, EMPTY, EMPTY, 
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));

      size_t i = 0;

      // insert into collection_1
      for (; i < insertedDocs.size()/2; ++i) {
        auto const doc = arangodb::velocypack::Parser::fromJson("{ \"key\": " + std::to_string(i) + "}");
        auto const res = logicalCollection1->insert(&trx, doc->slice(), insertedDocs[i], opt, tick, false);
        CHECK(res.ok());
      }

      // insert into collection_2
      for (; i < insertedDocs.size(); ++i) {
        auto const doc = arangodb::velocypack::Parser::fromJson("{ \"key\": " + std::to_string(i) + "}");
        auto const res = logicalCollection1->insert(&trx, doc->slice(), insertedDocs[i], opt, tick, false);
        CHECK(res.ok());
      }

      CHECK((trx.commit().ok()));
      view->sync();
  }

  // unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // key ASC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT d.key ASC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // key DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT d.key DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // TFIDF() ASC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT TFIDF(d) RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // TFIDF() DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT TFIDF(d) DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // BM25() ASC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT BM25(d) RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // BM25() DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT BM25(d) DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // TFIDF() ASC, key ASC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT TFIDF(d), d.key ASC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // TFIDF ASC, key DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView SORT TFIDF(d), d.key DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }
}

// ==, !=, <, <=, >, >=, range
SECTION("StringTerm") {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  arangodb::LogicalCollection* logicalCollection1{};
  arangodb::LogicalCollection* logicalCollection2{};

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection2));
  }

  // add view
  auto logicalView = vocbase.createView(createJson->slice(), 0);
  REQUIRE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
      "}}"
    );
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;
    TRI_voc_tick_t tick;

    arangodb::transaction::UserTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));

    // insert into collections
    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder->slice();
      REQUIRE(root.isArray());

      size_t i = 0;

      arangodb::LogicalCollection* collections[] {
        logicalCollection1, logicalCollection2
      };

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res = collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt, tick, false);
        CHECK(res.ok());
        ++i;
      }
    }

    CHECK((trx.commit().ok()));
    view->sync();
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                ==
  // -----------------------------------------------------------------------------

  // missing term
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name == 'invalid_value' RETURN d"
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

  // inavlid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name == 0 RETURN d"
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

  // inavlid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name == null RETURN d"
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

  // inavlid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name == false RETURN d"
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

  // inavlid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name == true RETURN d"
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

  // d.name == 'A', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name == 'A' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.same == 'same', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.same == 'xyz' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.duplicated == 'abcd' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.duplicated == 'abcd', name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.duplicated == 'abcd' SORT d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator actualDocs(result);
    CHECK(expectedDocs.size() == actualDocs.size());

    for (auto expectedDoc = expectedDocs.rbegin(), end = expectedDocs.rend(); expectedDoc != end; ++expectedDoc) {
      CHECK(actualDocs.valid());
      auto actualDoc = actualDocs.value();
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      actualDocs.next();
    }
    CHECK(!actualDocs.valid());
  }

  // d.duplicated == 'abcd', TFIDF() ASC, name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.duplicated == 'abcd' SORT TFIDF(d) ASC, d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator actualDocs(result);
    CHECK(expectedDocs.size() == actualDocs.size());

    for (auto expectedDoc = expectedDocs.rbegin(), end = expectedDocs.rend(); expectedDoc != end; ++expectedDoc) {
      CHECK(actualDocs.valid());
      auto actualDoc = actualDocs.value();
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      actualDocs.next();
    }
    CHECK(!actualDocs.valid());
  }

  // d.same == 'same', BM25() ASC, TFIDF() ASC, seq DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                !=
  // -----------------------------------------------------------------------------

  // missing term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name != 'invalid_term' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // existing duplicated term, unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.same != 'xyz' RETURN d"
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

  // existing unique term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    expectedDocs.erase("C");

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name != 'C' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // missing term, seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name != 'invalid_term' SORT d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // existing duplicated term, TFIDF() ASC, BM25() ASC, seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const fieldSlice = docSlice.get("duplicated");
      if (!fieldSlice.isNone() && "abcd" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.duplicated != 'abcd' SORT TFIDF(d) ASC, BM25(d) ASC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(resultIt.size() == expectedDocs.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 <
  // -----------------------------------------------------------------------------

  // d.name < 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key >= "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name < 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name < '!' (less than min term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name < '!' RETURN d"
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

  // d.name < '~' (less than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const nameSlice = docSlice.get("name");
      if (arangodb::iresearch::getStringRef(nameSlice) >= "~") {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name < '~' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                <=
  // -----------------------------------------------------------------------------

  // d.name <= 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key > "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name <= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name <= '!' (less than min term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name <= '!' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());

    auto const actualDoc = resultIt.value();
    auto const resolved = actualDoc.resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[27].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name <= '~' (less than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name <= '~' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 >
  // -----------------------------------------------------------------------------

  // d.name > 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name > '~' (greater than max term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > '~' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > '!' (greater than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const nameSlice = docSlice.get("name");
      if (arangodb::iresearch::getStringRef(nameSlice) <= "!") {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > '!' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                >=
  // -----------------------------------------------------------------------------

  // d.name > 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name >= '~' (greater or equal than max term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= '~' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());

    auto const actualDoc = resultIt.value();
    auto const resolved = actualDoc.resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[26].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name >= '!' (greater or equal than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= '!' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                      Range (>, <)
  // -----------------------------------------------------------------------------

  // d.name > 'H' AND d.name < 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H" || key >= "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > 'H' AND d.name < 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name > 'S' AND d.name < 'N' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > 'S' AND d.name < 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > 'H' AND d.name < 'H' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > 'H' AND d.name < 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > '!' AND d.name < '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name <= "!" || name >= "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > '!' AND d.name < '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>=, <)
  // -----------------------------------------------------------------------------

  // d.name >= 'H' AND d.name < 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H" || key >= "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= 'H' AND d.name < 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name >= 'S' AND d.name < 'N' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= 'S' AND d.name < 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name >= 'H' AND d.name < 'H' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= 'H' AND d.name < 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name >= '!' AND d.name < '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name < "!" || name >= "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= '!' AND d.name < '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>, <=)
  // -----------------------------------------------------------------------------

  // d.name >= 'H' AND d.name <= 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H" || key > "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > 'H' AND d.name <= 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name > 'S' AND d.name <= 'N' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > 'S' AND d.name <= 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > 'H' AND d.name <= 'H' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > 'H' AND d.name <= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name <= "!" || name > "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name > '!' AND d.name <= '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // d.name >= 'H' AND d.name <= 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H" || key > "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= 'H' AND d.name <= 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name >= 'S' AND d.name <= 'N' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= 'S' AND d.name <= 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name >= 'H' AND d.name <= 'H' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= 'H' AND d.name <= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[7].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name > '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name < "!" || name > "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name >= '!' AND d.name <= '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // d.name >= 'H' AND d.name <= 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H" || key > "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name IN 'H'..'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name >= 'S' AND d.name <= 'N' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name IN 'S'..'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name >= 'H' AND d.name <= 'H' , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name IN 'H'..'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[7].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name > '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name < "!" || name > "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name IN '!'..'~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }
}

SECTION("NumericTerm") {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  arangodb::LogicalCollection* logicalCollection1{};
  arangodb::LogicalCollection* logicalCollection2{};

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection2));
  }

  // add view
  auto logicalView = vocbase.createView(createJson->slice(), 0);
  REQUIRE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
      "}}"
    );
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;
    TRI_voc_tick_t tick;

    arangodb::transaction::UserTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));

    // insert into collections
    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder->slice();
      REQUIRE(root.isArray());

      size_t i = 0;

      arangodb::LogicalCollection* collections[] {
        logicalCollection1, logicalCollection2
      };

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res = collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt, tick, false);
        CHECK(res.ok());
        ++i;
      }
    }

    CHECK((trx.commit().ok()));
    view->sync();
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                ==
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq == '0' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq == true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq == false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // invalid type
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq == null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // missing term
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq == -1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.value == 90.564, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs {
      { 12, &insertedDocs[12] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == 90.564 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value == -32.5, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs {
      { 16, &insertedDocs[16] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value == -32.5 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq == 2, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs {
      { 2, &insertedDocs[2] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq == 2 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq == 2.0, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs {
      { 2, &insertedDocs[2] }
    };

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq == 2.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value == 100.0, TFIDF() ASC, BM25() ASC, d.seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<ptrdiff_t>();
      if (value != 100) {
        continue;
      }
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER 100.0 == d.value SORT BM25(d) ASC, TFIDF(d) ASC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                !=
  // -----------------------------------------------------------------------------

  // missing term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq != -1 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // existing duplicated term, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isNone() && valueSlice.getNumber<ptrdiff_t>() == 100) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != 100 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // existing unique term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }
    expectedDocs.erase("C");

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq != 2.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // missing term, seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value != -1 SORT d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // existing duplicated term, TFIDF() ASC, BM25() ASC, seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isNone() && 123 == valueSlice.getNumber<ptrdiff_t>()) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER 123 != d.value SORT TFIDF(d) ASC, BM25(d) ASC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(resultIt.size() == expectedDocs.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 <
  // -----------------------------------------------------------------------------

  // d.seq < 7, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key >= 7) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq < 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq < 0 (less than min term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq < 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq < 31 (less than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key >= 31) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq < 31 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value < 0, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() >= 0) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < 0 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value < 95, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() >= 95) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value < 95 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                <=
  // -----------------------------------------------------------------------------

  // d.seq <= 7, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key > 7) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq <= 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq <= 0 (less or equal than min term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq <= 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[0].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.seq <= 31 (less or equal than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key > 31) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq <= 31 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value <= 0, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() > 0) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= 0 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value <= 95, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() > 95) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value <= 95 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 >
  // -----------------------------------------------------------------------------

  // d.seq > 7, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq > 31 (greater than max term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 31 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq > 0 (less or equal than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 0) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 0 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value > 0, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() <= 0) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 0 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value > 95, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() <= 95) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 95 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                >=
  // -----------------------------------------------------------------------------

  // d.seq >= 7, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key < 7) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq >= 31 (greater than max term), unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 31 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[31].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.seq >= 0 (less or equal than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 0 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= 0, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() < 0) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 0 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value > 95, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() < 95) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 95 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                      Range (>, <)
  // -----------------------------------------------------------------------------

  // d.seq > 7 AND d.name < 18, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7 || key >= 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 7 AND d.seq < 18 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq > 7 AND d.seq < 18, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7 || key >= 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 7.1 AND d.seq < 17.9 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq > 18 AND d.seq < 7 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 18 AND d.seq < 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq > 7 AND d.seq < 7.0 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 7 AND d.seq < 7.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq > 0 AND d.seq < 31 , TFIDF() ASC, BM25() ASC, d.name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 0 || key >= 31) {
        continue;
      }
      expectedDocs.emplace(arangodb::iresearch::getStringRef(docSlice.get("name")), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 0 AND d.seq < 31 SORT tfidf(d), BM25(d), d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value > 90.564 AND d.value < 300, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value <= 90.564 || value >= 300) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 90.564 AND d.value < 300 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value > -32.5 AND d.value < 50, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value <= -32.5 || value >= 50) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > -32.5 AND d.value < 50 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>=, <)
  // -----------------------------------------------------------------------------

  // d.seq >= 7 AND d.seq < 18, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key < 7 || key >= 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7 AND d.seq < 18 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq > 7.1 AND d.seq <= 17.9, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7 || key >= 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7.1 AND d.seq <= 17.9 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq >= 18 AND d.seq < 7 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 18 AND d.seq < 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq >= 7 AND d.seq < 7.0 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7 AND d.seq < 7.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq >= 0 AND d.seq < 31 , TFIDF() ASC, BM25() ASC, d.name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key >= 31) {
        continue;
      }
      expectedDocs.emplace(arangodb::iresearch::getStringRef(docSlice.get("name")), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 0 AND d.seq < 31 SORT tfidf(d), BM25(d), d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= 90.564 AND d.value < 300, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value < 90.564 || value >= 300) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 90.564 AND d.value < 300 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= -32.5 AND d.value < 50, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value < -32.5 || value >= 50) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= -32.5 AND d.value < 50 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>, <=)
  // -----------------------------------------------------------------------------

  // d.seq > 7 AND d.seq <= 18, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7 || key > 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 7 AND d.seq <= 18 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq > 7 AND d.seq <= 17.9, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7 || key >= 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 7.1 AND d.seq <= 17.9 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq > 18 AND d.seq <= 7 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 18 AND d.seq <= 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq > 7 AND d.seq <= 7.0 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 7 AND d.seq <= 7.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq > 0 AND d.seq <= 31 , TFIDF() ASC, BM25() ASC, d.name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 0 || key > 31) {
        continue;
      }
      expectedDocs.emplace(arangodb::iresearch::getStringRef(docSlice.get("name")), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq > 0 AND d.seq <= 31 SORT tfidf(d), BM25(d), d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value > 90.564 AND d.value <= 300, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value <= 90.564 || value > 300) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > 90.564 AND d.value <= 300 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value > -32.5 AND d.value <= 50, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value <= -32.5 || value > 50) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value > -32.5 AND d.value <= 50 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // d.seq >= 7 AND d.seq <= 18, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key < 7 || key > 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7 AND d.seq <= 18 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq >= 7.1 AND d.seq <= 17.9, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7 || key >= 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7.1 AND d.seq <= 17.9 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq >= 18 AND d.seq <= 7 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 18 AND d.seq <= 7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq >= 7.0 AND d.seq <= 7.0 , unordered
  // will be optimized to d.seq == 7.0
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7.0 AND d.seq <= 7.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[7].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.seq > 7 AND d.seq <= 7.0 , unordered
  // there will be EMPTY_NODE in execution plan,
  // filter condition will be replaced with the strange 'true' value (why not 'false')
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 7 AND d.seq <= 7.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq >= 0 AND d.seq <= 31 , TFIDF() ASC, BM25() ASC, d.name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key > 31) {
        continue;
      }
      expectedDocs.emplace(arangodb::iresearch::getStringRef(docSlice.get("name")), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq >= 0 AND d.seq <= 31 SORT tfidf(d), BM25(d), d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= 90.564 AND d.value <= 300, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value < 90.564 || value > 300) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= 90.564 AND d.value <= 300 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= -32.5 AND d.value <= 50, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value < -32.5 || value > 50) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value >= -32.5 AND d.value <= 50 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // d.seq >= 7 AND d.seq <= 18, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key < 7 || key > 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq IN 7..18 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq >= 7.1 AND d.seq <= 17.9, unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key <= 7 || key >= 18) {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq IN 7.1..17.9 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.seq >= 18 AND d.seq <= 7 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq IN 18..7 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq >= 7 AND d.seq <= 7.0 , unordered
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq IN 7..7.0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(arangodb::velocypack::Slice(insertedDocs[7].vpack()) == resolved);

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.seq >= 0 AND d.seq <= 31 , TFIDF() ASC, BM25() ASC, d.name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      if (key > 31) {
        continue;
      }
      expectedDocs.emplace(arangodb::iresearch::getStringRef(docSlice.get("name")), &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.seq IN 0..31 SORT tfidf(d), BM25(d), d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= 90.564 AND d.value <= 300, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value < 90.564 || value > 300) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value IN 90.564..300 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= -32.5 AND d.value <= 50, BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone()) {
        continue;
      }
      auto const value = valueSlice.getNumber<double>();
      if (value < -32.5 || value > 50) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.value IN -32.5..50 SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->second->vpack()) == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }
}

// ==, !=, <, <=, >, >=, range
SECTION("BooleanTerm") {
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
      CHECK((res.successful()));
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
        "\"testCollection0\": { \"includeAllFields\": true, \"nestListValues\": true },"
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
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const valueSlice = docSlice.get("value");
      if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value == false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value == false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const keySlice = docSlice.get("seq");
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const keySlice = docSlice.get("seq");
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const keySlice = docSlice.get("seq");
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const keySlice = docSlice.get("seq");
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const keySlice = docSlice.get("seq");
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value != true, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isBoolean() && valueSlice.getBoolean()) {
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value != false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isBoolean() && !valueSlice.getBoolean()) {
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value != false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isBoolean() && !valueSlice.getBoolean()) {
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value <= false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value <= true, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value > false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= false, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= false, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= true AND d.value <= true, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
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
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.value >= true AND d.value <= true, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.value >= false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice();
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
      CHECK(expectedDoc->second == resolved);
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }
}

// FIXME TODO
// SECTION("Range") { }
// SECTION("Prefix") { }
// SECTION("Phrase") { }
// SECTION("Tokens") { }

SECTION("Exists") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto* collection = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs {
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -6, \"value\": null }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -5, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -4, \"value\": \"abc\" }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -3, \"value\": 3.14 }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
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

    irs::utf8_path resource;
    resource/=irs::string_ref(IResearch_test_resource_dir);
    resource/=irs::string_ref("simple_sequential.json");

    auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder->slice();
    REQUIRE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      collection->cid(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      CHECK((res.successful()));
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
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view->getImplementation());
    REQUIRE((false == !impl));

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, \"nestListValues\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
      "}}"
    );
    CHECK((impl->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((2 == impl->linkCount()));
    impl->sync();
  }

  // test non-existent (any)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.missing) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (any) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['missing']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (bool)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.name, 'type', 'bool') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (bool) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['name'], 'type', 'bool') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (boolean)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.name, 'type', 'boolean') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (boolean) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['name'], 'type', 'boolean') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (numeric)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.name, 'type', 'numeric') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (numeric) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['name'], 'type', 'numeric') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (null)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.name, 'type', 'null') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (null) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['name'], 'type', 'null') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (string)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.seq, 'type', 'string') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (string) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['seq'], 'type', 'string') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (analyzer)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.seq, 'analyzer', 'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (analyzer) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['seq'], 'analyzer', 'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (array)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value[2]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (array) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'][2]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (object)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value.d) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-existent (object) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value']['d']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (any)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (any) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (bool)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[1].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value, 'type', 'bool') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (bool) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[1].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'], 'type', 'bool') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (boolean)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[1].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value, 'type', 'boolean') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (boolean) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[1].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'], 'type', 'boolean') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (numeric)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[3].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value, 'type', 'numeric') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (numeric) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[3].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'], 'type', 'numeric') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (null)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value, 'type', 'null') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (null) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'], 'type', 'null') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (string)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[2].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value, 'type', 'string') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (string) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[2].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'], 'type', 'string') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (analyzer)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[2].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value, 'analyzer', 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (analyzer) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[2].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'], 'analyzer', 'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (array)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[4].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value[1]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (array) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[4].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value'][1]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (object)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[5].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.value.b) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test existent (object) via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[5].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d['value']['b']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }
}

// SECTION("Not") { }
// SECTION("In") { }

SECTION("Value") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto* collection = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs {
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -6, \"value\": null }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -5, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -4, \"value\": \"abc\" }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -3, \"value\": 3.14 }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
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

    irs::utf8_path resource;
    resource/=irs::string_ref(IResearch_test_resource_dir);
    resource/=irs::string_ref("simple_sequential.json");

    auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder->slice();
    REQUIRE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      collection->cid(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      CHECK((res.successful()));
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
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view->getImplementation());
    REQUIRE((false == !impl));

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, \"nestListValues\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
      "}}"
    );
    CHECK((impl->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((2 == impl->linkCount()));
    impl->sync();
  }

  // test empty array (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER [ ] SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-empty array (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER [ 'abc', 'def' ] SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test boolean (false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER false SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test boolean (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER true SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test numeric (false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER 0 SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test numeric (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER 3.14 SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test null
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER null SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test empty object (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER { } SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-empty object (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER { 'a': 123, 'b': 'cde' } SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test empty string (false)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER '' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }

  // test non-empty string (true)
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[0].slice(),
      insertedDocs[1].slice(),
      insertedDocs[2].slice(),
      insertedDocs[3].slice(),
      insertedDocs[4].slice(),
      insertedDocs[5].slice(),
      insertedDocs[6].slice(),
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER 'abc' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
    auto slice = queryResult.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((expected[i++] == resolved));
    }

    CHECK((i == expected.size()));
  }
}

// SECTION("SimpleOr") { }
// SECTION("ComplexOr") { }
// SECTION("SimpleAnd") { }
// SECTION("ComplexAnd") { }
// SECTION("SimpleBoolean") { }
// SECTION("ComplexBoolean") { }
//

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
