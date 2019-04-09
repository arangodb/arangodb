////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "../Mocks/StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Aql/Ast.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Helpers.h"
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
#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0; // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchQueryOptionsSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryOptionsSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

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
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
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

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_NONDETERM_",
      ".",
      arangodb::aql::Function::makeFlags(
        // fake non-deterministic
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ),
      [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_FORWARD_",
      ".",
      arangodb::aql::Function::makeFlags(
        // fake deterministic
        arangodb::aql::Function::Flags::Deterministic,
        arangodb::aql::Function::Flags::Cacheable,
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ),
      [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchQueryOptionsSetup() {
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
}; // IResearchQueryOptionsSetup

}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchQueryTestOptionsCollections", "[iresearch][iresearch-query]") {
  IResearchQueryOptionsSetup s;
  UNUSED(s);

  // ==, !=, <, <=, >, >=, range
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

  // add collection_3
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_3\" }");
    auto logicalCollection3 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection3));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(createJson->slice())
  );
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
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
        {
          insertedDocs.emplace_back();
          auto const res = collections[0]->insert(&trx, doc, insertedDocs.back(), opt, false);
          CHECK(res.ok());
        }
        {
          insertedDocs.emplace_back();
          auto const res = collections[1]->insert(&trx, doc, insertedDocs.back(), opt, false);
          CHECK(res.ok());
        }
        ++i;
      }
    }

    CHECK((trx.commit().ok()));
    CHECK((TRI_ERROR_NO_ERROR == arangodb::tests::executeQuery(vocbase, "FOR d IN testView SEARCH 1 ==1 OPTIONS { waitForSync: true } RETURN d").code)); // commit
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                              'collections' option
  // -----------------------------------------------------------------------------

  // collection by name
  {
    std::string const query = 
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1' ] }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
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
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // boound option
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ @collectionName ] }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      },
      arangodb::velocypack::Parser::fromJson("{ \"collectionName\" : \"collection_1\" }")
    ));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query,
      arangodb::velocypack::Parser::fromJson("{ \"collectionName\" : \"collection_1\" }")
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
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // boound options
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : @collections }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      },
      arangodb::velocypack::Parser::fromJson("{ \"collections\" : [ \"collection_1\" ] }")
    ));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query,
      arangodb::velocypack::Parser::fromJson("{ \"collections\" : [ \"collection_1\" ] }")
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
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // collection by id
  {
    std::string const query = 
        "FOR d IN testView SEARCH d.name == 'A'"
        " OPTIONS { collections : [ " + std::to_string(logicalCollection2->id()) + " ] }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[1] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
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
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // collection by id as string
  {
    std::string const query = 
        "FOR d IN testView SEARCH d.name == 'A'"
        " OPTIONS { collections : [ '" + std::to_string(logicalCollection2->id()) + "' ] }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[1] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
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
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // multiple collections
  {
    std::string const query = 
        "FOR d IN testView SEARCH d.name == 'A'"
        " OPTIONS { collections : [ '" + std::to_string(logicalCollection2->id()) + "', 'collection_1' ] }"
        " SORT d._id"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    std::map<irs::string_ref, std::vector<arangodb::ManagedDocumentResult const*>> expectedDocs {
      { "A", { &insertedDocs[0], &insertedDocs[1] } }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(2 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto it = expectedDocs.find(key);
      REQUIRE(it != expectedDocs.end());
      REQUIRE(!it->second.empty());
      auto expectedDoc = it->second.begin();
      REQUIRE(expectedDoc != it->second.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice((**expectedDoc).vpack()), resolved, true));
      it->second.erase(expectedDoc);

      if (it->second.empty()) {
        expectedDocs.erase(it);
      }
    }
    CHECK(expectedDocs.empty());
  }

  // empty array means no data
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ ] }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
  }

  // null means "no restrictions"
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : null }"
        " SORT d._id"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    std::map<irs::string_ref, std::vector<arangodb::ManagedDocumentResult const*>> expectedDocs {
      { "A", { &insertedDocs[0], &insertedDocs[1] } }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(2 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto it = expectedDocs.find(key);
      REQUIRE(it != expectedDocs.end());
      REQUIRE(!it->second.empty());
      auto expectedDoc = it->second.begin();
      REQUIRE(expectedDoc != it->second.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice((**expectedDoc).vpack()), resolved, true));
      it->second.erase(expectedDoc);

      if (it->second.empty()) {
        expectedDocs.erase(it);
      }
    }
    CHECK(expectedDocs.empty());
  }

  // join restricted views
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1' ] }"
        " FOR x IN testView SEARCH x.name == 'A' OPTIONS { collections : [ 'collection_2' ] }"
        " RETURN { d, x }";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    std::map<irs::string_ref, std::vector<arangodb::ManagedDocumentResult const*>> expectedDocs {
      { "A", { &insertedDocs[0], &insertedDocs[1] } }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualValue : resultIt) {
      CHECK(actualValue.isObject());

      auto const d = actualValue.get("d");
      CHECK(d.isObject());
      auto const resolvedd = d.resolveExternal();
      auto const x = actualValue.get("x");
      CHECK(x.isObject());
      auto const resolvedx = x.resolveExternal();

      auto const keySliced = resolvedx.get("name");
      auto const keyd = arangodb::iresearch::getStringRef(keySliced);
      auto const keySlicex = resolvedx.get("name");
      auto const keyx = arangodb::iresearch::getStringRef(keySlicex);
      CHECK(keyd == keyx);

      auto it = expectedDocs.find(keyd);
      REQUIRE(it != expectedDocs.end());
      REQUIRE(2 == it->second.size());
      REQUIRE(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(it->second[0]->vpack()), resolvedd, true));
      REQUIRE(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(it->second[1]->vpack()), resolvedx, true));

      expectedDocs.erase(it);
    }
    CHECK(expectedDocs.empty());
  }

  // wrong collection name is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', 'collection_0' ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // wrong collection id is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', 32112312 ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // wrong collection id as string is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', '32112312' ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', null ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', {} ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', true ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', null ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // invalid options type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : true }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // invalid options type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : 1 }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // invalid options type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : {} }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // collection which is not registered witht a view is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ 'collection_1', 'collection_3' ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }
}

TEST_CASE("IResearchQueryTestOptionsWaitForSync", "[iresearch][iresearch-query]") {
  IResearchQueryOptionsSetup s;
  UNUSED(s);

  // ==, !=, <, <=, >, >=, range
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
      "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
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
        auto const res = collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt, false);
        CHECK(res.ok());
        ++i;
      }
    }

    CHECK((trx.commit().ok()));
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                              'waitForSync' option
  // -----------------------------------------------------------------------------

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: null }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: 1 }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: 'true' }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: [] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: {} }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // don't sync
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: false }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
  }

  // do sync, bind parameter
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: @doSync }"
        " RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      },
      arangodb::velocypack::Parser::fromJson("{ \"doSync\" : true }")
    ));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      query,
      arangodb::velocypack::Parser::fromJson("{ \"doSync\" : true }")
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
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
