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

struct IResearchQuerySetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQuerySetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

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
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchQuerySetup() {
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
}

// FIXME TODO
// SECTION("Range") { }
// SECTION("Prefix") { }
// SECTION("Phrase") { }
// SECTION("Tokens") { }
// SECTION("Exists") { }
// SECTION("Not") { }
// SECTION("In") { }
// SECTION("Value") { }
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
