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

  std::string const collectionName = "testCollection";

 public:
  InsertExecutorTest() : vocbase(server.getSystemDatabase()) {}

  void SetUp() override {
    auto createJson =
        velocypack::Parser::fromJson(R"({ "name": "testCollection", "type": 2 })");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(collection.get(), nullptr);
  }
};

class InsertExecutorTestCount : public InsertExecutorTest,
                                public ::testing::WithParamInterface<size_t> {};

class InsertExecutorTestCounts
    : public InsertExecutorTest,
      public ::testing::WithParamInterface<std::vector<size_t>> {};

INSTANTIATE_TEST_CASE_P(InsertExecutorTestInstance, InsertExecutorTestCount,
                        testing::Values(1, 100, 999, 1000, 1001));

INSTANTIATE_TEST_CASE_P(InsertExecutorTestInstance, InsertExecutorTestCounts,
                        testing::Values(std::vector<size_t>{1}, std::vector<size_t>{100},
                                        std::vector<size_t>{999}, std::vector<size_t>{1000},
                                        std::vector<size_t>{1001},
                                        std::vector<size_t>{1, 100, 1000, 1000, 900}));

TEST_P(InsertExecutorTestCount, insert) {
  std::string query = std::string("FOR i IN 1..") + std::to_string(GetParam()) +
                      " INSERT { value: i } INTO testCollection";
  auto const expected = VPackParser::fromJson("[]");

  AssertQueryHasResult(server, query, expected->slice());

  std::string checkQuery = "FOR i IN testCollection RETURN i.value";
  VPackBuilder builder;
  builder.openArray();
  for (size_t i = 1; i <= GetParam(); i++) {
    builder.add(VPackValue(i));
  }
  builder.close();
  AssertQueryHasResult(server, checkQuery, builder.slice());
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
