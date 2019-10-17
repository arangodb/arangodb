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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Mocks/Servers.h"
#include "QueryHelper.h"
#include "gtest/gtest.h"

#include "Aql/Query.h"
#include "RestServer/QueryRegistryFeature.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "../IResearch/IResearchQueryCommon.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class InsertExecutorTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase;

  std::string const collectionName = "UnitTestCollection";

 public:
  InsertExecutorTest() : vocbase(server.getSystemDatabase()) {}

  void SetUp() override {
    SCOPED_TRACE("SetUp");
    auto info = VPackParser::fromJson("{\"name\": \"" + collectionName + "\"}");
    auto collection = vocbase.createCollection(info->slice());
    ASSERT_NE(collection.get(), nullptr) << "Failed to create collection";
  }
};

class InsertExecutorTestCount : public InsertExecutorTest,
                                public ::testing::WithParamInterface<size_t> {};

class InsertExecutorTestCounts
    : public InsertExecutorTest,
      public ::testing::WithParamInterface<std::vector<size_t>> {};

INSTANTIATE_TEST_CASE_P(InsertExecutorTestInstance, InsertExecutorTestCount,
                        testing::Values(1, 100, 999, 1000, 1001));

INSTANTIATE_TEST_CASE_P(
    InsertExecutorTestInstance, InsertExecutorTestCounts,
    testing::Values(std::vector<size_t>{1}, std::vector<size_t>{100},
                    std::vector<size_t>{999}, std::vector<size_t>{1000},
                    std::vector<size_t>{1001}, std::vector<size_t>{1, 100, 1000, 1000, 900},
                    std::vector<size_t>{10, 10, 10, 10, 10, 100, 100, 10, 100,
                                        1000, 1000, 900, 10, 100}));

TEST_P(InsertExecutorTestCount, insert_without_return) {
  std::string query = std::string("FOR i IN 1..") + std::to_string(GetParam()) +
                      " INSERT { value: i } INTO " + collectionName;

  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  std::string checkQuery = "FOR i IN " + collectionName + " RETURN i.value";
  VPackBuilder builder;
  builder.openArray();
  for (size_t i = 1; i <= GetParam(); i++) {
    builder.add(VPackValue(i));
  }
  builder.close();
  AssertQueryHasResult(vocbase, checkQuery, builder.slice());
}

TEST_P(InsertExecutorTestCount, insert_with_return) {
  auto const bindParameters = VPackParser::fromJson("{ }");

  std::string query = std::string("FOR i IN 1..") + std::to_string(GetParam()) +
                      " INSERT { value: i } INTO " + collectionName +
                      " RETURN NEW";
  auto result = arangodb::tests::executeQuery(vocbase, query, bindParameters);
  TRI_ASSERT(result.data->slice().isArray());
  TRI_ASSERT(result.data->slice().length() == GetParam());

  std::string checkQuery = "FOR i IN " + collectionName + " RETURN i";
  AssertQueryHasResult(vocbase, checkQuery, result.data->slice());
}

TEST_P(InsertExecutorTestCounts, insert_multiple_without_return) {
  VPackBuilder builder;
  builder.openArray();

  std::vector<size_t> param = GetParam();
  for (auto i : param) {
    std::string query = std::string("FOR i IN 1..") + std::to_string(i) +
                        " INSERT { value: i } INTO " + collectionName;

    AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

    for (size_t j = 1; j <= i; j++) {
      builder.add(VPackValue(j));
    }
  }

  builder.close();
  std::string checkQuery = "FOR i IN " + collectionName + " RETURN i.value";
  AssertQueryHasResult(vocbase, checkQuery, builder.slice());
}

TEST_P(InsertExecutorTestCounts, insert_multiple_with_return) {
  auto const bindParameters = VPackParser::fromJson("{ }");

  VPackBuilder builder;
  builder.openArray();

  std::vector<size_t> param = GetParam();

  for (auto i : param) {
    std::string query = std::string("FOR i IN 1..") + std::to_string(i) +
                        " INSERT { value: i } INTO " + collectionName +
                        " RETURN NEW ";
    auto result = arangodb::tests::executeQuery(vocbase, query, bindParameters);
    ASSERT_TRUE(result.ok());
    builder.add(VPackArrayIterator(result.data->slice()));
  }

  builder.close();
  std::string checkQuery = "FOR i IN " + collectionName + " RETURN i";
  AssertQueryHasResult(vocbase, checkQuery, builder.slice());
}

// OLD is a keyword, but only sometimes. In particular in insert queries it isnt.
TEST_F(InsertExecutorTest, insert_return_old) {
  std::string query = std::string("FOR i IN 1..1 INSERT { value: i } INTO ") +
                      collectionName + " RETURN OLD";

  AssertQueryFailsWith(vocbase, query, 1203);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
