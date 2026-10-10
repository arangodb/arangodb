////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Ast.h"
#include "Aql/CollectOptions.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/VariableGenerator.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include "../Mocks/Servers.h"

using namespace arangodb::aql;

namespace arangodb::tests::aql {

class AggregateVarInfoTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  std::shared_ptr<arangodb::aql::Query> fakedQuery;
  Ast ast;

 public:
  AggregateVarInfoTest()
      : fakedQuery(server.createFakeQuery()), ast(*fakedQuery.get()) {}

  std::vector<AggregateVarInfo> parse(std::string_view json) {
    auto builder = velocypack::Parser::fromJson(json.data(), json.size());
    return AggregateVarInfo::fromVelocyPack(&ast, builder->slice());
  }
};

// The deserializer does not look at the aggregator type, so a placeholder is
// enough to cover an aggregate call with more than one argument.
TEST_F(AggregateVarInfoTest, reads_multiple_in_variables) {
  auto result = parse(R"([{
    "outVariable": {"id": 1, "name": "hi"},
    "inVariables": [{"id": 2, "name": "2"}, {"id": 3, "name": "3"}],
    "type": "MULTI_ARG"
  }])");

  ASSERT_EQ(1, result.size());
  EXPECT_EQ("MULTI_ARG", result[0].type);
  EXPECT_EQ(1, result[0].outVar->id);
  ASSERT_EQ(2, result[0].inVars.size());
  EXPECT_EQ(2, result[0].inVars[0]->id);
  EXPECT_EQ(3, result[0].inVars[1]->id);
}

TEST_F(AggregateVarInfoTest, reads_single_in_variable) {
  auto result = parse(R"([{
    "outVariable": {"id": 1, "name": "mn"},
    "inVariables": [{"id": 2, "name": "2"}],
    "type": "MIN"
  }])");

  ASSERT_EQ(1, result.size());
  ASSERT_EQ(1, result[0].inVars.size());
  EXPECT_EQ(2, result[0].inVars[0]->id);
}

// Coordinators are upgraded after DB servers, so a new DB server must still
// understand the single "inVariable" that an old coordinator writes.
TEST_F(AggregateVarInfoTest, reads_legacy_single_in_variable) {
  auto result = parse(R"([{
    "outVariable": {"id": 1, "name": "mn"},
    "inVariable": {"id": 2, "name": "2"},
    "type": "MIN"
  }])");

  ASSERT_EQ(1, result.size());
  EXPECT_EQ("MIN", result[0].type);
  EXPECT_EQ(1, result[0].outVar->id);
  ASSERT_EQ(1, result[0].inVars.size());
  EXPECT_EQ(2, result[0].inVars[0]->id);
}

// COUNT/LENGTH have their input optimized away, so an old coordinator writes
// no "inVariable" at all
TEST_F(AggregateVarInfoTest, reads_legacy_without_in_variable) {
  auto result = parse(R"([{
    "outVariable": {"id": 1, "name": "n"},
    "type": "LENGTH"
  }])");

  ASSERT_EQ(1, result.size());
  EXPECT_EQ("LENGTH", result[0].type);
  EXPECT_TRUE(result[0].inVars.empty());
}

TEST_F(AggregateVarInfoTest, reads_empty_in_variables) {
  auto result = parse(R"([{
    "outVariable": {"id": 1, "name": "n"},
    "inVariables": [],
    "type": "LENGTH"
  }])");

  ASSERT_EQ(1, result.size());
  EXPECT_TRUE(result[0].inVars.empty());
}

TEST_F(AggregateVarInfoTest, reads_several_aggregates) {
  auto result = parse(R"([
    {"outVariable": {"id": 1, "name": "n"}, "type": "LENGTH"},
    {"outVariable": {"id": 2, "name": "mn"},
     "inVariable": {"id": 3, "name": "3"}, "type": "MIN"},
    {"outVariable": {"id": 4, "name": "hi"},
     "inVariables": [{"id": 5, "name": "5"}, {"id": 6, "name": "6"}],
     "type": "MULTI_ARG"}
  ])");

  ASSERT_EQ(3, result.size());
  EXPECT_TRUE(result[0].inVars.empty());
  EXPECT_EQ(1, result[1].inVars.size());
  EXPECT_EQ(2, result[2].inVars.size());
}

TEST_F(AggregateVarInfoTest, rejects_non_array) {
  EXPECT_ANY_THROW(parse(R"({"outVariable": {"id": 1, "name": "n"}})"));
}

}  // namespace arangodb::tests::aql
