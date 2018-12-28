////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "common.h"

#include "StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Transaction/Methods.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0; // defined in main.cpp

NS_LOCAL

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchQueryOrSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryOrSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
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

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    analyzers->emplace(
      "test_analyzer",
      "TestAnalyzer",
      "abc",
      irs::flags{ irs::frequency::type(), irs::position::type() } // required for PHRASE
    ); // cache analyzer

    analyzers->emplace(
      "test_csv_analyzer",
      "TestDelimAnalyzer",
      ","
    ); // cache analyzer

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchQueryOrSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
  }
}; // IResearchQuerySetup

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchQueryTestOr", "[iresearch][iresearch-query]") {
  IResearchQueryOrSetup s;
  UNUSED(s);

  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

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
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(createJson->slice())
  );
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "\"collection_1\": { \"analyzers\": [ \"test_analyzer\", \"identity\" ], \"includeAllFields\": true, \"trackListPositions\": true, \"storeValues\":\"id\" },"
      "\"collection_2\": { \"analyzers\": [ \"test_analyzer\", \"identity\" ], \"includeAllFields\": true, \"storeValues\":\"id\" }"
      "}}"
    );
    CHECK((view->properties(updateJson->slice(), true).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;
    TRI_voc_tick_t tick;

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
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
      auto root = builder.slice();
      REQUIRE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[] {
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
    CHECK(view->commit().ok());
  }

  // d.name == 'A' OR d.name == 'Q', d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      if (keySlice.isNone()) {
        continue;
      }
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key != "A" && key != "Q") {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'A' OR d.name == 'Q' SORT d.seq DESC RETURN d"
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

  // d.name == 'X' OR d.same == 'xyz', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'X' OR d.same == 'xyz' SORT BM25(d) DESC, TFIDF(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    // Check 1st (the most relevant doc)
    // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc", "prefix":"bateradsfsfasdf" }
    {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDocs[23]->vpack()), resolved, true)));
      expectedDocs.erase(23);
    }

    // Check the rest of documents
    auto expectedDoc = expectedDocs.rbegin();
    for (resultIt.next(); resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true)));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.name == 'K' OR d.value <= 100 OR d.duplicated == abcd, TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[10].vpack()), // {"name":"K","seq":10,"same":"xyz","value":12,"duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz","duplicated":"abcd","prefix":"abcy" }
      arangodb::velocypack::Slice(insertedDocs[26].vpack()), // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[4].vpack()),  // {"name":"E","seq":4,"same":"xyz","value":100,"duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz","value":100,"duplicated":"abcd","prefix":"abcd" }
      arangodb::velocypack::Slice(insertedDocs[16].vpack()), // {"name":"Q","seq":16,"same":"xyz", "value":-32.5, "duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[15].vpack()), // {"name":"P","seq":15,"same":"xyz","value":50,"prefix":"abde"}
      arangodb::velocypack::Slice(insertedDocs[14].vpack()), // {"name":"O","seq":14,"same":"xyz","value":0 }
      arangodb::velocypack::Slice(insertedDocs[13].vpack()), // {"name":"N","seq":13,"same":"xyz","value":1,"duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[12].vpack()), // {"name":"M","seq":12,"same":"xyz","value":90.564 }
      arangodb::velocypack::Slice(insertedDocs[11].vpack()), // {"name":"L","seq":11,"same":"xyz","value":95 }
      arangodb::velocypack::Slice(insertedDocs[9].vpack()),  // {"name":"J","seq":9,"same":"xyz","value":100 }
      arangodb::velocypack::Slice(insertedDocs[8].vpack()),  // {"name":"I","seq":8,"same":"xyz","value":100,"prefix":"bcd" }
      arangodb::velocypack::Slice(insertedDocs[6].vpack()),  // {"name":"G","seq":6,"same":"xyz","value":100 }
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz","value":12,"prefix":"abcde"}
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'K' OR d.value <= 100 OR d.duplicated == 'abcd' SORT TFIDF(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    // Check the documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      resultIt.next();
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(*expectedDoc, resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // d.name == 'A' OR d.name == 'Q' OR d.same != 'xyz', d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      if (keySlice.isNone()) {
        continue;
      }
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key != "A" && key != "Q") {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'A' OR d.name == 'Q' OR d.same != 'xyz' SORT d.seq DESC RETURN d"
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

  // d.name == 'F' OR EXISTS(d.duplicated), BM25(d) DESC, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      if (keySlice.isNone()) {
        continue;
      }
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key != "F" && docSlice.get("duplicated").isNone()) {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'F' OR EXISTS(d.duplicated) SORT BM25(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    // Check 1st (the most relevant doc)
    // {"name":"F","seq":5,"same":"xyz", "value":1234 }
    {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDocs[5]->vpack()), resolved, true)));
      expectedDocs.erase(5);
    }

    // Check the rest of documents
    auto expectedDoc = expectedDocs.rbegin();
    for (resultIt.next(); resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true)));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs {
      // The most relevant document (satisfied both search conditions)
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}

      // FIXME TODO check why this section is now more relevant
      // The least relevant documents (contain non-unique term 'abcy' in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }

      // Less relevant documents (satisfied STARTS_WITH condition only, has unqiue term in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[25].vpack()), // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }
/*
      // The least relevant documents (contain non-unique term 'abcy' in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
*/
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, 'abc') SORT TFIDF(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), BM25(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs {
      // The most relevant document (satisfied both search conditions)
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}

      // FIXME TODO check why this section is now more relevant
      // The least relevant documents (contain non-unique term 'abcy' in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }

      // Less relevant documents (satisfied STARTS_WITH condition only, has unqiue term in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[25].vpack()), // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }
/*
      // The least relevant documents (contain non-unique term 'abcy' in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
*/
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, 'abc') SORT BM25(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), BM25(d) DESC, d.seq DESC, LIMIT 3
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs {
      // The most relevant document (satisfied both search conditions)
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
/*
      // Less relevant documents (satisfied STARTS_WITH condition only, has unqiue term in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[25].vpack()), // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
*/
      // FIXME TODO check why this section is now appeared and the above section disapeared
      // Less relevant documents (satisfied STARTS_WITH condition only, has unqiue term in 'prefix' field)
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, 'abc') SORT BM25(d) DESC, d.seq DESC LIMIT 3 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // STARTS_WITH(d['prefix'], 'abc') OR EXISTS(d.duplicated) OR d.value < 100 OR d.name >= 'Z', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      arangodb::velocypack::Slice(insertedDocs[25].vpack()), // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" ,
      arangodb::velocypack::Slice(insertedDocs[26].vpack()), // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}
/*
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
*/
      // FIXME TODO check why the above section changed order the to following:
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }

      arangodb::velocypack::Slice(insertedDocs[23].vpack()), // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc", "prefix":"bateradsfsfasdf" }
      arangodb::velocypack::Slice(insertedDocs[18].vpack()), // {"name":"S","seq":18,"same":"xyz", "duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[16].vpack()), // {"name":"Q","seq":16,"same":"xyz", "value":-32.5, "duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[15].vpack()), // {"name":"P","seq":15,"same":"xyz","value":50, "prefix":"abde"},
      arangodb::velocypack::Slice(insertedDocs[14].vpack()), // {"name":"O","seq":14,"same":"xyz","value":0 },
      arangodb::velocypack::Slice(insertedDocs[13].vpack()), // {"name":"N","seq":13,"same":"xyz","value":1, "duplicated":"vczc"},
      arangodb::velocypack::Slice(insertedDocs[12].vpack()), // {"name":"M","seq":12,"same":"xyz","value":90.564 },
      arangodb::velocypack::Slice(insertedDocs[11].vpack()), // {"name":"L","seq":11,"same":"xyz","value":95 }
      arangodb::velocypack::Slice(insertedDocs[10].vpack()), // {"name":"K","seq":10,"same":"xyz","value":12, "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[7].vpack()),  // {"name":"H","seq":7,"same":"xyz", "value":123, "duplicated":"vczc"},
      arangodb::velocypack::Slice(insertedDocs[4].vpack()),  // {"name":"E","seq":4,"same":"xyz", "value":100, "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[2].vpack()),  // {"name":"C","seq":2,"same":"xyz", "value":123, "duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[1].vpack()),  // {"name":"B","seq":1,"same":"xyz", "value":101, "duplicated":"vczc"}
    };

    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc') OR EXISTS(d.duplicated) OR d.value < 100 OR d.name >= 'Z' SORT TFIDF(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // PHRASE(d['duplicated'], 'v', 2, 'z', 'test_analyzer') OR STARTS_WITH(d['prefix'], 'abc') OR d.value > 100 OR d.name >= 'Z', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      arangodb::velocypack::Slice(insertedDocs[25].vpack()), // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" ,
      arangodb::velocypack::Slice(insertedDocs[26].vpack()), // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[23].vpack()), // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc", "prefix":"bateradsfsfasdf" }
      arangodb::velocypack::Slice(insertedDocs[18].vpack()), // {"name":"S","seq":18,"same":"xyz", "duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[16].vpack()), // {"name":"Q","seq":16,"same":"xyz", "value":-32.5, "duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[13].vpack()), // {"name":"N","seq":13,"same":"xyz","value":1, "duplicated":"vczc"},
      arangodb::velocypack::Slice(insertedDocs[7].vpack()),  // {"name":"H","seq":7,"same":"xyz", "value":123, "duplicated":"vczc"},
      arangodb::velocypack::Slice(insertedDocs[2].vpack()),  // {"name":"C","seq":2,"same":"xyz", "value":123, "duplicated":"vczc"}
      arangodb::velocypack::Slice(insertedDocs[1].vpack()),  // {"name":"B","seq":1,"same":"xyz", "value":101, "duplicated":"vczc"}
/*
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
*/
      // FIXME TODO check why the above section changed order the to following:
      arangodb::velocypack::Slice(insertedDocs[31].vpack()), // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
      arangodb::velocypack::Slice(insertedDocs[30].vpack()), // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
      arangodb::velocypack::Slice(insertedDocs[20].vpack()), // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
      arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }

      arangodb::velocypack::Slice(insertedDocs[15].vpack()), // {"name":"P","seq":15,"same":"xyz","value":50, "prefix":"abde"},
      arangodb::velocypack::Slice(insertedDocs[14].vpack()), // {"name":"O","seq":14,"same":"xyz","value":0 },
      arangodb::velocypack::Slice(insertedDocs[12].vpack()), // {"name":"M","seq":12,"same":"xyz","value":90.564 },
      arangodb::velocypack::Slice(insertedDocs[11].vpack()), // {"name":"L","seq":11,"same":"xyz","value":95 }
      arangodb::velocypack::Slice(insertedDocs[10].vpack()), // {"name":"K","seq":10,"same":"xyz","value":12, "duplicated":"abcd"}
    };

    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 1, 'z'), 'test_analyzer') OR STARTS_WITH(d['prefix'], 'abc') OR d.value < 100 OR d.name >= 'Z' SORT TFIDF(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }
    CHECK((i == expected.size()));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------