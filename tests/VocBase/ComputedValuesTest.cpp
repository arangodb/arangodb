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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Exceptions.h"
#include "Mocks/Servers.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "VocBase/ComputedValues.h"
#include "VocBase/LogicalCollection.h"

#include "gtest/gtest.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include <memory>
#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::tests;

class ComputedValuesTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    server = std::make_unique<mocks::MockAqlServer>();
  }
  static void TearDownTestCase() { server.reset(); }
  void TearDown() override {
    auto& vocbase = server->getSystemDatabase();
    auto c = vocbase.lookupCollection("test");
    if (c != nullptr) {
      vocbase.dropCollection(c->id(), false);
    }
  }

 protected:
  static inline std::unique_ptr<mocks::MockAqlServer> server;
};

TEST_F(ComputedValuesTest, createComputedValuesFromEmptyObject) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  // cannot create ComputedValues from an Object slice
  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, velocypack::Slice::emptyObjectSlice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesFromNone) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  // when creating computed values from a None slice, we get no
  // error, but a nullptr back
  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, velocypack::Slice::noneSlice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(nullptr, res.get());
}

TEST_F(ComputedValuesTest, createComputedValuesFromEmptyArray) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  // when creating computed values from an empty Array slice, we get no
  // error, but a nullptr back
  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, velocypack::Slice::emptyArraySlice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(nullptr, res.get());
}

TEST_F(ComputedValuesTest, createComputedValuesFromGarbledObject) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("foo", velocypack::Value(true));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesFromObjectInvalidName) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Slice::emptyArraySlice());
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value("true"));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesFromObjectEmptyName) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value(""));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value("true"));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesFromObjectMissingName) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value("true"));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesFromObjectMissingExpression) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("overwrite", velocypack::Value("true"));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesFromObjectInvalidExpression) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;

  auto expressions = {
      "",
      "RETURN",
      "LET a = 1",
      "LET a = 1 RETURN a",
      "RETURN (RETURN 1)",
      "RETURN (FOR i IN 1..10 RETURN i)",
      "RETURN TOKENS('Lörem ipsüm, DOLOR SIT Ämet.', 'text_de')"};

  for (auto const& expression : expressions) {
    velocypack::Builder b;
    b.openArray();
    b.openObject();
    b.add("name", velocypack::Value("foo"));
    b.add("expression", velocypack::Value(expression));
    b.add("overwrite", velocypack::Value("true"));
    b.close();
    b.close();

    auto res = ComputedValues::buildInstance(
        vocbase, shardKeys, b.slice(),
        arangodb::transaction::OperationOriginTestCase{});
    ASSERT_FALSE(res.ok());
    ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  }
}

TEST_F(ComputedValuesTest,
       createComputedValuesFromObjectInvalidBindParameters) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;

  auto expressions = {"RETURN @document", "RETURN @foo",
                      "LET a = @doc RETURN a", "LET a = @doc RETURN @doc"};

  for (auto const& expression : expressions) {
    velocypack::Builder b;
    b.openArray();
    b.openObject();
    b.add("name", velocypack::Value("foo"));
    b.add("expression", velocypack::Value(expression));
    b.add("overwrite", velocypack::Value("true"));
    b.close();
    b.close();

    auto res = ComputedValues::buildInstance(
        vocbase, shardKeys, b.slice(),
        arangodb::transaction::OperationOriginTestCase{});
    ASSERT_FALSE(res.ok());
    ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesFromObjectMissingOverwrite) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesFromObjectSimple) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  auto cv = res.get();
  ASSERT_TRUE(cv->mustComputeValuesOnInsert());
  ASSERT_TRUE(cv->mustComputeValuesOnUpdate());
  ASSERT_TRUE(cv->mustComputeValuesOnReplace());

  {
    velocypack::Builder b;
    cv->toVelocyPack(b);

    ASSERT_TRUE(b.slice().isArray());
    ASSERT_EQ(1, b.slice().length());

    auto s = b.slice().at(0);
    ASSERT_EQ("foo", s.get("name").stringView());
    ASSERT_EQ("RETURN 1", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_TRUE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());

    s = s.get("computeOn");
    ASSERT_TRUE(s.isArray());
    ASSERT_EQ(3, s.length());
    ASSERT_EQ("insert", s[0].stringView());
    ASSERT_EQ("update", s[1].stringView());
    ASSERT_EQ("replace", s[2].stringView());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesInvalidComputeOn) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.add("computeOn", velocypack::Value("insert"));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesInvalidComputeOn2) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("test"));
  b.close();
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesInvalidComputeOn3) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value(""));
  b.close();
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesEmptyComputeOn) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.add("computeOn", velocypack::Slice::emptyArraySlice());
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createComputedValuesComputeOnInsert) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN @doc"));
  b.add("overwrite", velocypack::Value(false));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("insert"));
  b.close();
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  auto cv = res.get();
  ASSERT_TRUE(cv->mustComputeValuesOnInsert());
  ASSERT_FALSE(cv->mustComputeValuesOnUpdate());
  ASSERT_FALSE(cv->mustComputeValuesOnReplace());

  {
    velocypack::Builder b;
    cv->toVelocyPack(b);

    ASSERT_TRUE(b.slice().isArray());
    ASSERT_EQ(1, b.slice().length());

    auto s = b.slice().at(0);
    ASSERT_EQ("foo", s.get("name").stringView());
    ASSERT_EQ("RETURN @doc", s.get("expression").stringView());
    ASSERT_FALSE(s.get("overwrite").getBoolean());
    ASSERT_TRUE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());

    s = s.get("computeOn");
    ASSERT_TRUE(s.isArray());
    ASSERT_EQ(1, s.length());
    ASSERT_EQ("insert", s[0].stringView());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesComputeOnUpdate) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1 + 1"));
  b.add("overwrite", velocypack::Value(true));
  b.add("keepNull", velocypack::Value(false));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("update"));
  b.close();
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  auto cv = res.get();
  ASSERT_FALSE(cv->mustComputeValuesOnInsert());
  ASSERT_TRUE(cv->mustComputeValuesOnUpdate());
  ASSERT_FALSE(cv->mustComputeValuesOnReplace());

  {
    velocypack::Builder b;
    cv->toVelocyPack(b);

    ASSERT_TRUE(b.slice().isArray());
    ASSERT_EQ(1, b.slice().length());

    auto s = b.slice().at(0);
    ASSERT_EQ("foo", s.get("name").stringView());
    ASSERT_EQ("RETURN 1 + 1", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_FALSE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());

    s = s.get("computeOn");
    ASSERT_TRUE(s.isArray());
    ASSERT_EQ(1, s.length());
    ASSERT_EQ("update", s[0].stringView());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesComputeOnReplace) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("a b"));
  b.add("expression", velocypack::Value("RETURN 'testi'"));
  b.add("overwrite", velocypack::Value(true));
  b.add("keepNull", velocypack::Value(false));
  b.add("failOnWarning", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("replace"));
  b.close();
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  auto cv = res.get();
  ASSERT_FALSE(cv->mustComputeValuesOnInsert());
  ASSERT_FALSE(cv->mustComputeValuesOnUpdate());
  ASSERT_TRUE(cv->mustComputeValuesOnReplace());

  {
    velocypack::Builder b;
    cv->toVelocyPack(b);

    ASSERT_TRUE(b.slice().isArray());
    ASSERT_EQ(1, b.slice().length());

    auto s = b.slice().at(0);
    ASSERT_EQ("a b", s.get("name").stringView());
    ASSERT_EQ("RETURN 'testi'", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_FALSE(s.get("keepNull").getBoolean());
    ASSERT_TRUE(s.get("failOnWarning").getBoolean());

    s = s.get("computeOn");
    ASSERT_TRUE(s.isArray());
    ASSERT_EQ(1, s.length());
    ASSERT_EQ("replace", s[0].stringView());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesComputeOnMultiple) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("insert"));
  b.close();
  b.close();
  b.openObject();
  b.add("name", velocypack::Value("bar"));
  b.add("expression", velocypack::Value("RETURN 2"));
  b.add("overwrite", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("replace"));
  b.close();
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  auto cv = res.get();
  ASSERT_TRUE(cv->mustComputeValuesOnInsert());
  ASSERT_FALSE(cv->mustComputeValuesOnUpdate());
  ASSERT_TRUE(cv->mustComputeValuesOnReplace());

  velocypack::Builder bActual;
  cv->toVelocyPack(bActual);

  ASSERT_TRUE(bActual.slice().isArray());
  ASSERT_EQ(2, bActual.slice().length());

  {
    auto s = bActual.slice().at(0);
    ASSERT_EQ("foo", s.get("name").stringView());
    ASSERT_EQ("RETURN 1", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_TRUE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());
  }

  {
    auto s = bActual.slice().at(1);
    ASSERT_EQ("bar", s.get("name").stringView());
    ASSERT_EQ("RETURN 2", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_TRUE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesComputeOnMultiple2) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;
  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("insert"));
  b.close();
  b.close();
  b.openObject();
  b.add("name", velocypack::Value("bar"));
  b.add("expression", velocypack::Value("RETURN 2"));
  b.add("overwrite", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("replace"));
  b.close();
  b.close();
  b.openObject();
  b.add("name", velocypack::Value("qux"));
  b.add("expression", velocypack::Value("RETURN 3"));
  b.add("overwrite", velocypack::Value(true));
  b.add(velocypack::Value("computeOn"));
  b.openArray();
  b.add(velocypack::Value("update"));
  b.close();
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_TRUE(res.ok());
  auto cv = res.get();
  ASSERT_TRUE(cv->mustComputeValuesOnInsert());
  ASSERT_TRUE(cv->mustComputeValuesOnUpdate());
  ASSERT_TRUE(cv->mustComputeValuesOnReplace());

  velocypack::Builder bActual;
  cv->toVelocyPack(bActual);

  ASSERT_TRUE(bActual.slice().isArray());
  ASSERT_EQ(3, bActual.slice().length());

  {
    auto s = bActual.slice().at(0);
    ASSERT_EQ("foo", s.get("name").stringView());
    ASSERT_EQ("RETURN 1", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_TRUE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());
  }

  {
    auto s = bActual.slice().at(1);
    ASSERT_EQ("bar", s.get("name").stringView());
    ASSERT_EQ("RETURN 2", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_TRUE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());
  }

  {
    auto s = bActual.slice().at(2);
    ASSERT_EQ("qux", s.get("name").stringView());
    ASSERT_EQ("RETURN 3", s.get("expression").stringView());
    ASSERT_TRUE(s.get("overwrite").getBoolean());
    ASSERT_TRUE(s.get("keepNull").getBoolean());
    ASSERT_FALSE(s.get("failOnWarning").getBoolean());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesComputeOnSystemAttributes) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;

  auto fields = {"_id", "_key", "_rev", "_from", "_to"};

  for (auto const& field : fields) {
    velocypack::Builder b;
    b.openArray();
    b.openObject();
    b.add("name", velocypack::Value(field));
    b.add("expression", velocypack::Value("RETURN 1"));
    b.add("overwrite", velocypack::Value(true));
    b.close();
    b.close();

    auto res = ComputedValues::buildInstance(
        vocbase, shardKeys, b.slice(),
        arangodb::transaction::OperationOriginTestCase{});
    ASSERT_FALSE(res.ok());
    ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesComputeOnShardKeys) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys{"foo", "bar", "baz"};

  for (auto const& field : shardKeys) {
    velocypack::Builder b;
    b.openArray();
    b.openObject();
    b.add("name", velocypack::Value(field));
    b.add("expression", velocypack::Value("RETURN 1"));
    b.add("overwrite", velocypack::Value(true));
    b.close();
    b.close();

    auto res = ComputedValues::buildInstance(
        vocbase, shardKeys, b.slice(),
        arangodb::transaction::OperationOriginTestCase{});
    ASSERT_FALSE(res.ok());
    ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  }
}

TEST_F(ComputedValuesTest, createComputedValuesDuplicateNames) {
  auto& vocbase = server->getSystemDatabase();

  std::vector<std::string> shardKeys;

  velocypack::Builder b;
  b.openArray();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 1"));
  b.add("overwrite", velocypack::Value(true));
  b.close();
  b.openObject();
  b.add("name", velocypack::Value("foo"));
  b.add("expression", velocypack::Value("RETURN 2"));
  b.add("overwrite", velocypack::Value(true));
  b.close();
  b.close();

  auto res = ComputedValues::buildInstance(
      vocbase, shardKeys, b.slice(),
      arangodb::transaction::OperationOriginTestCase{});
  ASSERT_FALSE(res.ok());
  ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
}

TEST_F(ComputedValuesTest, createCollectionNoComputedValues) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson("{\"name\":\"test\"}");

  auto c = vocbase.createCollection(b->slice());
  ASSERT_EQ(nullptr, c->computedValues());
}

TEST_F(ComputedValuesTest, createCollectionEmptyComputedValues) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": []}");

  auto c = vocbase.createCollection(b->slice());
  ASSERT_EQ(nullptr, c->computedValues());
}

TEST_F(ComputedValuesTest, createCollectionComputedValuesInsertOverwriteTrue) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"attr\", "
      "\"expression\":\"RETURN 'test'\", \"overwrite\": true}]}");

  auto c = vocbase.createCollection(b->slice());
  auto cv = c->computedValues();
  ASSERT_NE(nullptr, c->computedValues());
  ASSERT_TRUE(cv->mustComputeValuesOnInsert());
  ASSERT_TRUE(cv->mustComputeValuesOnUpdate());
  ASSERT_TRUE(cv->mustComputeValuesOnReplace());

  std::vector<std::string> const EMPTY;
  std::vector<std::string> collections{"test"};
  transaction::Methods trx(
      transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      EMPTY, collections, EMPTY, transaction::Options());

  EXPECT_TRUE(trx.begin().ok());
  auto doc1 =
      velocypack::Parser::fromJson("{\"_key\":\"test1\", \"attr\":\"abc\"}");
  EXPECT_TRUE(trx.insert("test", doc1->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test1",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("test", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());

  auto doc2 = velocypack::Parser::fromJson("{\"_key\":\"test2\"}");
  EXPECT_TRUE(trx.insert("test", doc2->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test2",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("test", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
}

TEST_F(ComputedValuesTest, createCollectionComputedValuesInsertOverwriteFalse) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"attr\", "
      "\"expression\":\"RETURN 'test'\", \"overwrite\": false}]}");

  auto c = vocbase.createCollection(b->slice());
  auto cv = c->computedValues();
  ASSERT_NE(nullptr, c->computedValues());
  ASSERT_TRUE(cv->mustComputeValuesOnInsert());
  ASSERT_TRUE(cv->mustComputeValuesOnUpdate());
  ASSERT_TRUE(cv->mustComputeValuesOnReplace());

  std::vector<std::string> const EMPTY;
  std::vector<std::string> collections{"test"};
  transaction::Methods trx(
      transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      EMPTY, collections, EMPTY, transaction::Options());

  EXPECT_TRUE(trx.begin().ok());
  auto doc1 =
      velocypack::Parser::fromJson("{\"_key\":\"test1\", \"attr\":\"abc\"}");
  EXPECT_TRUE(trx.insert("test", doc1->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test1",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("abc", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
  auto doc2 = velocypack::Parser::fromJson("{\"_key\":\"test2\"}");
  EXPECT_TRUE(trx.insert("test", doc2->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test2",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("test", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
}

TEST_F(ComputedValuesTest, createCollectionComputedValuesUpdateOverwriteTrue) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"attr\", "
      "\"expression\":\"RETURN 'update'\", \"overwrite\": true, "
      "\"computeOn\":[\"update\"]}]}");

  auto c = vocbase.createCollection(b->slice());

  std::vector<std::string> const EMPTY;
  std::vector<std::string> collections{"test"};
  transaction::Methods trx(
      transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      EMPTY, collections, EMPTY, transaction::Options());

  EXPECT_TRUE(trx.begin().ok());
  auto doc1 =
      velocypack::Parser::fromJson("{\"_key\":\"test1\", \"attr\":\"abc\"}");
  EXPECT_TRUE(trx.insert("test", doc1->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test1",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("abc", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());

  auto doc2 =
      velocypack::Parser::fromJson("{\"_key\":\"test1\", \"attr\":\"qux\"}");
  EXPECT_TRUE(trx.update("test", doc2->slice(), OperationOptions()).ok());

  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test1",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("update", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
}

TEST_F(ComputedValuesTest, createCollectionComputedValuesUpdateOverwriteFalse) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"attr\", "
      "\"expression\":\"RETURN 'update'\", \"overwrite\": false, "
      "\"computeOn\":[\"update\"]}]}");

  auto c = vocbase.createCollection(b->slice());

  std::vector<std::string> const EMPTY;
  std::vector<std::string> collections{"test"};
  transaction::Methods trx(
      transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      EMPTY, collections, EMPTY, transaction::Options());

  EXPECT_TRUE(trx.begin().ok());
  auto doc1 =
      velocypack::Parser::fromJson("{\"_key\":\"test1\", \"attr\":\"abc\"}");
  EXPECT_TRUE(trx.insert("test", doc1->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test1",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("abc", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());

  auto doc2 =
      velocypack::Parser::fromJson("{\"_key\":\"test1\", \"attr\":\"qux\"}");
  EXPECT_TRUE(trx.update("test", doc2->slice(), OperationOptions()).ok());

  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test1",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_EQ("qux", doc.get("attr").stringView());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
}

TEST_F(ComputedValuesTest, createCollectionComputedValuesFailOnWarningStatic) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"attr\", "
      "\"expression\":\"RETURN 1 / 0\", \"overwrite\": true, "
      "\"failOnWarning\": true}]}");

  auto c = vocbase.createCollection(b->slice());
  auto cv = c->computedValues();
  ASSERT_EQ(nullptr, c->computedValues());
}

TEST_F(ComputedValuesTest, createCollectionComputedValuesFailOnWarningDynamic) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"attr\", "
      "\"expression\":\"RETURN @doc.value / 0\", \"overwrite\": true, "
      "\"failOnWarning\": true}]}");

  auto c = vocbase.createCollection(b->slice());

  std::vector<std::string> const EMPTY;
  std::vector<std::string> collections{"test"};
  transaction::Methods trx(
      transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      EMPTY, collections, EMPTY, transaction::Options());

  EXPECT_TRUE(trx.begin().ok());
  auto doc = velocypack::Parser::fromJson("{\"value\":42}");
  EXPECT_THROW({ trx.insert("test", doc->slice(), OperationOptions()); },
               basics::Exception);
}

TEST_F(ComputedValuesTest, createCollectionComputedValuesInvalidValuesDynamic) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"value1\", "
      "\"expression\":\"RETURN @doc.value / 0\", \"overwrite\": true, "
      "\"failOnWarning\": false}]}");

  auto c = vocbase.createCollection(b->slice());

  std::vector<std::string> const EMPTY;
  std::vector<std::string> collections{"test"};
  transaction::Methods trx(
      transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      EMPTY, collections, EMPTY, transaction::Options());

  EXPECT_TRUE(trx.begin().ok());
  auto doc = velocypack::Parser::fromJson(
      "{\"_key\":\"test\", \"value1\":42, \"value2\":23}");
  EXPECT_TRUE(trx.insert("test", doc->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_TRUE(doc.get("value1").isNull());
                       EXPECT_EQ(23, doc.get("value2").getNumber<int>());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
}

TEST_F(ComputedValuesTest, insertKeepNullTrue) {
  auto& vocbase = server->getSystemDatabase();
  auto b = velocypack::Parser::fromJson(
      "{\"name\":\"test\", \"computedValues\": [{\"name\":\"attr\", "
      "\"expression\":\"RETURN @doc.value ?: null\", \"overwrite\": true, "
      "\"keepNull\": true}]}");

  auto c = vocbase.createCollection(b->slice());
  auto cv = c->computedValues();
  ASSERT_NE(nullptr, c->computedValues());
  ASSERT_TRUE(cv->mustComputeValuesOnInsert());
  ASSERT_TRUE(cv->mustComputeValuesOnUpdate());
  ASSERT_TRUE(cv->mustComputeValuesOnReplace());

  std::vector<std::string> const EMPTY;
  std::vector<std::string> collections{"test"};
  transaction::Methods trx(
      transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      EMPTY, collections, EMPTY, transaction::Options());

  EXPECT_TRUE(trx.begin().ok());
  auto doc1 =
      velocypack::Parser::fromJson("{\"_key\":\"test1\", \"attr\":null}");
  EXPECT_TRUE(trx.insert("test", doc1->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test1",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_TRUE(doc.get("attr").isNull());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
  auto doc2 = velocypack::Parser::fromJson(
      "{\"_key\":\"test2\", \"attr\":null, \"value\": null}");
  EXPECT_TRUE(trx.insert("test", doc2->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test2",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_TRUE(doc.get("attr").isNull());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
  auto doc3 = velocypack::Parser::fromJson(
      "{\"_key\":\"test3\", \"attr\":null, \"value\": 1}");
  EXPECT_TRUE(trx.insert("test", doc3->slice(), OperationOptions()).ok());
  EXPECT_TRUE(trx.documentFastPathLocal(
                     "test", "test3",
                     [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
                       EXPECT_FALSE(doc.get("attr").isNull());
                       EXPECT_EQ(1, doc.get("attr").getNumber<int>());
                       return true;
                     })
                  .waitAndGet()
                  .ok());
}
