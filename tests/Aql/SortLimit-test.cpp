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

#include "gtest/gtest.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

// test setup
#include "IResearch/common.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

extern const char* ARGV0;  // defined in main.cpp

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

class SortLimitTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  std::unique_ptr<TRI_vocbase_t> vocbase;
  std::vector<arangodb::velocypack::Builder> insertedDocs;

  SortLimitTest()
      : engine(server),
        server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
    arangodb::ClusterEngine::Mocking = true;
    arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called

    // setup required application features
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    system = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                             0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature

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

    vocbase = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    CreateCollection();
  }

  ~SortLimitTest() {
    vocbase.reset();
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
  }

  std::string sorterType(TRI_vocbase_t& vocbase, std::string const& queryString,
                         std::string rules = "") {
    auto options = arangodb::velocypack::Parser::fromJson(
        "{\"optimizer\": {\"rules\": [" + rules + "]}}");
    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

    auto result = query.explain();
    VPackSlice nodes = result.data->slice().get("nodes");
    EXPECT_TRUE(nodes.isArray());

    std::string strategy;
    for (auto const& it : VPackArrayIterator(nodes)) {
      if (!it.get("type").isEqualString("SortNode")) {
        continue;
      }

      EXPECT_TRUE(strategy.empty());
      strategy = it.get("strategy").copyString();
    }

    EXPECT_TRUE(!strategy.empty());
    return strategy;
  }

  bool verifyExpectedResults(TRI_vocbase_t& vocbase, std::string const& queryString,
                             std::vector<size_t> const& expected,
                             std::string rules = "") {
    auto options = arangodb::velocypack::Parser::fromJson(
        "{\"optimizer\": {\"rules\": [" + rules + "]}}");
    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);
    std::shared_ptr<arangodb::aql::SharedQueryState> ss = query.sharedState();
    arangodb::aql::QueryResult result;

    while (true) {
      auto state = query.execute(arangodb::QueryRegistryFeature::registry(), result);
      if (state == arangodb::aql::ExecutionState::WAITING) {
        ss->waitForAsyncResponse();
      } else {
        break;
      }
    }

    EXPECT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

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
  }

  // create collection0, insertedDocs[0, 999]
  void CreateCollection() {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase->createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs;
    size_t total = 1000;
    for (size_t i = 0; i < total; i++) {
      docs.emplace_back(arangodb::velocypack::Parser::fromJson(
          "{ \"valAsc\": " + std::to_string(i) +
          ", \"valDsc\": " + std::to_string(total - 1 - i) +
          ", \"mod\": " + std::to_string(i % 100) + "}"));
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(insertedDocs.size() == total);
  }
};

TEST_F(SortLimitTest, CheckSimpleLimitSortedAscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedAscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckSimpleLimitSortedAscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {999, 998, 997, 996, 995,
                                  994, 993, 992, 991, 990};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedAscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {989, 988, 987, 986, 985,
                                  984, 983, 982, 981, 980};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckSimpleLimitSortedDscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc DESC LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {999, 998, 997, 996, 995,
                                  994, 993, 992, 991, 990};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedDscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc DESC LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {989, 988, 987, 986, 985,
                                  984, 983, 982, 981, 980};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckSimpleLimitSortedDscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc DESC LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedDscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc DESC LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetCompoundSort) {
  std::string query =
      "FOR d IN testCollection0 SORT d.mod, d.valAsc LIMIT 2, 5 RETURN  d";
  std::vector<size_t> expected = {200, 300, 400, 500, 600};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetCompoundSortAgain) {
  std::string query =
      "FOR d IN testCollection0 SORT d.mod, d.valAsc LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {1,   101, 201, 301, 401,
                                  501, 601, 701, 801, 901};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckInterloperFilterMovedUp) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc FILTER d.mod == 0 LIMIT 0, 10 "
      "RETURN d";
  std::vector<size_t> expected = {0,   100, 200, 300, 400,
                                  500, 600, 700, 800, 900};
  EXPECT_TRUE(sorterType(*vocbase, query) == "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckInterloperFilterNotMoved) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc FILTER d.mod == 0 LIMIT 0, 10 "
      "RETURN d";
  std::string rules = "\"-move-filters-up\", \"-move-filters-up-2\"";
  std::vector<size_t> expected = {0,   100, 200, 300, 400,
                                  500, 600, 700, 800, 900};
  EXPECT_TRUE(sorterType(*vocbase, query, rules) == "standard");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected, rules));
}

TEST_F(SortLimitTest, CheckInterloperEnumerateList) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc FOR e IN 1..10 FILTER e == 1 "
      "LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_TRUE(sorterType(*vocbase, query) == "standard");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}
