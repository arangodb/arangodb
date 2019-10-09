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

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

// test setup
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/TraverserEngineRegistry.h"
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

class SortLimitTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::FIXME, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  std::unique_ptr<TRI_vocbase_t> vocbase;
  std::vector<arangodb::velocypack::Builder> insertedDocs;

  SortLimitTest() : server() {
    arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
    arangodb::ClusterEngine::Mocking = true;
    arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);

    vocbase = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    CreateCollection();
  }

  ~SortLimitTest() {
    vocbase.reset();
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

    EXPECT_FALSE(strategy.empty());
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
      }
    }

    return true;
  }

  // create collection0, insertedDocs[0, 999]
  void CreateCollection() {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase->createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs;
    size_t total = 1000;
    for (size_t i = 0; i < total; i++) {
      docs.emplace_back(arangodb::velocypack::Parser::fromJson(
          "{ \"valAsc\": " + std::to_string(i) +
          ", \"valDsc\": " + std::to_string(total - 1 - i) +
          ", \"mod\": " + std::to_string(i % 100) + "}"));
    }

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_EQ(insertedDocs.size(), total);
  }
};

TEST_F(SortLimitTest, CheckSimpleLimitSortedAscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedAscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckSimpleLimitSortedAscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {999, 998, 997, 996, 995,
                                  994, 993, 992, 991, 990};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedAscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {989, 988, 987, 986, 985,
                                  984, 983, 982, 981, 980};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckSimpleLimitSortedDscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc DESC LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {999, 998, 997, 996, 995,
                                  994, 993, 992, 991, 990};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedDscInInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc DESC LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {989, 988, 987, 986, 985,
                                  984, 983, 982, 981, 980};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckSimpleLimitSortedDscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc DESC LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetSortedDscInReverseInsertionOrder) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valDsc DESC LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetCompoundSort) {
  std::string query =
      "FOR d IN testCollection0 SORT d.mod, d.valAsc LIMIT 2, 5 RETURN  d";
  std::vector<size_t> expected = {200, 300, 400, 500, 600};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckLimitWithOffsetCompoundSortAgain) {
  std::string query =
      "FOR d IN testCollection0 SORT d.mod, d.valAsc LIMIT 10, 10 RETURN d";
  std::vector<size_t> expected = {1,   101, 201, 301, 401,
                                  501, 601, 701, 801, 901};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckInterloperFilterMovedUp) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc FILTER d.mod == 0 LIMIT 0, 10 "
      "RETURN d";
  std::vector<size_t> expected = {0,   100, 200, 300, 400,
                                  500, 600, 700, 800, 900};
  EXPECT_EQ(sorterType(*vocbase, query), "constrained-heap");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}

TEST_F(SortLimitTest, CheckInterloperFilterNotMoved) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc FILTER d.mod == 0 LIMIT 0, 10 "
      "RETURN d";
  std::string rules = "\"-move-filters-up\", \"-move-filters-up-2\"";
  std::vector<size_t> expected = {0,   100, 200, 300, 400,
                                  500, 600, 700, 800, 900};
  EXPECT_EQ(sorterType(*vocbase, query, rules), "standard");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected, rules));
}

TEST_F(SortLimitTest, CheckInterloperEnumerateList) {
  std::string query =
      "FOR d IN testCollection0 SORT d.valAsc FOR e IN 1..10 FILTER e == 1 "
      "LIMIT 0, 10 RETURN d";
  std::vector<size_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(sorterType(*vocbase, query), "standard");
  EXPECT_TRUE(verifyExpectedResults(*vocbase, query, expected));
}
