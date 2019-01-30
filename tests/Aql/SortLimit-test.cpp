////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

// test setup
#include "IResearch/StorageEngineMock.h"
#include "IResearch/common.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Aql/QueryOptions.h"
#include "Aql/SortNode.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct SortLimitSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  SortLimitSetup() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);  //
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                     0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
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

    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~SortLimitSetup() {
    system.reset();  // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }
};  // SortLimitSetup

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////
#include <iostream>
TEST_CASE("SortLimit", "[aql][sort-limit]") {
  SortLimitSetup s;
  UNUSED(s);

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  std::vector<arangodb::velocypack::Builder> insertedDocs;

  auto sorterType = [](TRI_vocbase_t& vocbase, std::string const& queryString) -> std::string {
    auto options = arangodb::velocypack::Parser::fromJson("{}");
    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

    auto result = query.explain();
    VPackSlice nodes = result.result->slice().get("nodes");
    CHECK(nodes.isArray());

    std::string strategy;
    for (auto const& it : VPackArrayIterator(nodes)) {
      if (!it.get("type").isEqualString("SortNode")) {
        continue;
      }

      CHECK(strategy.empty());
      strategy = it.get("strategy").copyString();
    }

    CHECK(!strategy.empty());
    return strategy;
  };

  auto verifyExpectedResults =
      [&insertedDocs](TRI_vocbase_t& vocbase, std::string const& query,
                      std::vector<size_t> const& expected) -> bool {
    auto result = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE((TRI_ERROR_NO_ERROR == result.code));
    auto slice = result.result->slice();
    REQUIRE(slice.isArray());

    if (slice.length() != expected.size()) {
      return false;
    }

    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      if (0 != arangodb::basics::VelocyPackHelper::compare(
                   insertedDocs[expected[i++]].slice(), resolved, true)) {
        return false;
      };
    }

    return true;
  };

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs;
    size_t total = 10000;
    for (size_t i = 0; i < total; i++) {
      docs.emplace_back(arangodb::velocypack::Parser::fromJson(
          "{ \"valAsc\": " + std::to_string(i) +
          ", \"valDsc\": " + std::to_string(total - 1 - i) +
          ", \"mod\": " + std::to_string(i % 100) + "}"));
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    CHECK((trx.begin().ok()));

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      CHECK((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    CHECK((trx.commit().ok()));
    CHECK(insertedDocs.size() == total);
  }

  // check simple limit sorted asc in insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valAsc LIMIT 0, 10 RETURN d";
    std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check limit with offset sorted asc in insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valAsc LIMIT 10, 10 RETURN d";
    std::vector<size_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check simple limit sorted asc in reverse insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valDsc LIMIT 0, 10 RETURN d";
    std::vector<size_t> expected = {9999, 9998, 9997, 9996, 9995,
                                    9994, 9993, 9992, 9991, 9990};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check limit with offset sorted asc in reverse insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valDsc LIMIT 10, 10 RETURN d";
    std::vector<size_t> expected = {9989, 9988, 9987, 9986, 9985,
                                    9984, 9983, 9982, 9981, 9980};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check simple limit sorted dsc in insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valAsc DESC LIMIT 0, 10 RETURN d";
    std::vector<size_t> expected = {9999, 9998, 9997, 9996, 9995,
                                    9994, 9993, 9992, 9991, 9990};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check limit with offset sorted dsc in insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valAsc DESC LIMIT 10, 10 RETURN d";
    std::vector<size_t> expected = {9989, 9988, 9987, 9986, 9985,
                                    9984, 9983, 9982, 9981, 9980};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check simple limit sorted dsc in reverse insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valDsc DESC LIMIT 0, 10 RETURN d";
    std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check limit with offset sorted dsc in reverse insertion order
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.valDsc DESC LIMIT 10, 10 RETURN d";
    std::vector<size_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }

  // check limit with offset compound sort
  {
    std::string query =
        "FOR d IN testCollection0 SORT d.mod, d.valAsc LIMIT 10, 10 RETURN "
        "d";
    std::vector<size_t> expected = {1000, 1100, 1200, 1300, 1400,
                                    1500, 1600, 1700, 1800, 1900};
    CHECK(verifyExpectedResults(vocbase, query, expected));
    CHECK(sorterType(vocbase, query) == "constrained-heap");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
