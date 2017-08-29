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

// ==, !=, <=, <, >=, >
SECTION("StringEqual") {
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

    // insert into collection_1
    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder->slice();
      REQUIRE(root.isArray());

      size_t i = 0;
      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res = logicalCollection1->insert(&trx, doc, insertedDocs.back(), opt, tick, false);
        CHECK(res.ok());
      }
    }

    CHECK((trx.commit().ok()));
    view->sync();
  }

  // missing term
  {
    auto queryResult = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.name == 'invalid_value' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
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

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
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

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
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

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
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

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(arangodb::velocypack::Slice(expectedDoc->vpack()) == resolved);
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