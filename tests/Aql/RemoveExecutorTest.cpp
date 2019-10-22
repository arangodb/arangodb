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

class RemoveExecutorTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase;

  std::string const collectionName = "UnitTestCollection";
  std::string const allDocumentsQuery =
      std::string("FOR d IN " + collectionName + " RETURN d");

 public:
  RemoveExecutorTest() : vocbase(server.getSystemDatabase()) {}

  void SetUp() override {
    SCOPED_TRACE("SetUp");
    auto info = VPackParser::fromJson("{\"name\": \"" + collectionName + "\"}");
    auto collection = vocbase.createCollection(info->slice());
    ASSERT_NE(collection.get(), nullptr) << "Failed to create collection";

    std::string createQuery =
        "FOR i IN 1..1000 INSERT { _key: TO_STRING(i), value: i, sortvalue: i "
        "} IN " +
        collectionName;
    AssertQueryHasResult(vocbase, createQuery, VPackSlice::emptyArraySlice());
  }
};

class RemoveExecutorTestPatterns
    : public RemoveExecutorTest,
      public ::testing::WithParamInterface<std::pair<size_t, size_t>> {
 public:
  size_t const nDocs;
  std::string const nDocsString;
  size_t const rDocs;
  std::string const rDocsString;

  RemoveExecutorTestPatterns()
      : nDocs(GetParam().first),
        nDocsString(std::to_string(GetParam().first)),
        rDocs(GetParam().second),
        rDocsString(std::to_string(GetParam().second)) {}

  void SetUp() override {
    SCOPED_TRACE("SetUp");
    auto info = VPackParser::fromJson("{\"name\": \"" + collectionName + "\"}");
    auto collection = vocbase.createCollection(info->slice());
    ASSERT_NE(collection.get(), nullptr) << "Failed to create collection";

    std::string createQuery = "FOR i IN 1.." + nDocsString +
                              " INSERT { _key: TO_STRING(i), "
                              "value: i, sortvalue: i "
                              "} IN " +
                              collectionName;
    AssertQueryHasResult(vocbase, createQuery, VPackSlice::emptyArraySlice());
  }
};

TEST_F(RemoveExecutorTest, remove_non_existent_assert_error) {
  std::string query = std::string("REMOVE { _key: \"invalidFoo\"} IN " + collectionName);

  AssertQueryFailsWith(vocbase, query, 1202);
}

TEST_F(RemoveExecutorTest, remove_non_existent_ignore_error) {
  auto const expected = VPackParser::fromJson("[ ]");
  std::string query = std::string("REMOVE { _key: \"invalidFoo\"} IN " + collectionName +
                                  " OPTIONS { ignoreErrors: true }");

  AssertQueryHasResult(vocbase, query, expected->slice());
}

TEST_P(RemoveExecutorTestPatterns, remove_all_without_return) {
  std::string query =
      std::string("FOR d IN " + collectionName + " REMOVE d IN " + collectionName);

  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  AssertQueryHasResult(vocbase, allDocumentsQuery, VPackSlice::emptyArraySlice());
}

TEST_P(RemoveExecutorTestPatterns, remove_all_with_return) {
  auto const bindParameters = VPackParser::fromJson("{ }");
  std::string allQuery = std::string("FOR d IN " + collectionName + " RETURN d");

  auto allDocs = executeQuery(vocbase, allQuery, bindParameters);
  ASSERT_TRUE(allDocs.ok());

  std::string query =
      std::string("FOR d IN " + collectionName + " REMOVE d IN " +
                  collectionName + " RETURN OLD");
  AssertQueryHasResult(vocbase, query, allDocs.data->slice());

  AssertQueryHasResult(vocbase, allDocumentsQuery, VPackSlice::emptyArraySlice());
}

TEST_P(RemoveExecutorTestPatterns, remove_every_third_without_return) {
  std::string query =
      std::string("FOR d IN " + collectionName +
                  " FILTER (d.value % 3) == 0 REMOVE d IN " + collectionName);

  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  std::string checkQuery =
      "FOR d IN " + collectionName + " FILTER d.value % 3 == 0 RETURN d.value";
  AssertQueryHasResult(vocbase, checkQuery, VPackSlice::emptyArraySlice());

  // The things we did not remove still have to be there.
  {
    std::string checkQuery =
        "FOR d IN " + collectionName +
        " FILTER (d.value % 3) != 0 SORT d.value RETURN d.value";
    VPackBuilder builder;
    builder.openArray();
    for (size_t i = 1; i <= nDocs; i++) {
      if (i % 3 != 0) {
        builder.add(VPackValue(i));
      }
    }
    builder.close();
    AssertQueryHasResult(vocbase, checkQuery, builder.slice());
  }
}

TEST_P(RemoveExecutorTestPatterns, remove_every_third_with_return) {
  auto const bindParameters = VPackParser::fromJson("{ }");
  std::string allQuery =
      std::string("FOR d IN " + collectionName +
                  " FILTER (d.value % 3) == 0 SORT d.value RETURN d");

  auto allDocs = executeQuery(vocbase, allQuery, bindParameters);
  ASSERT_TRUE(allDocs.ok());

  std::string query = std::string("FOR d IN " + collectionName +
                                  " FILTER (d.value % 3) == 0 REMOVE d IN " +
                                  collectionName + " SORT OLD.value RETURN OLD");
  AssertQueryHasResult(vocbase, query, allDocs.data->slice());

  // The things we did not remove still have to be there.
  {
    std::string checkQuery =
        "FOR d IN " + collectionName +
        " FILTER (d.value % 3) != 0 SORT d.value RETURN d.value";
    VPackBuilder builder;
    builder.openArray();
    for (size_t i = 1; i <= nDocs; i++) {
      if (i % 3 != 0) {
        builder.add(VPackValue(i));
      }
    }
    builder.close();
    AssertQueryHasResult(vocbase, checkQuery, builder.slice());
  }
}

TEST_P(RemoveExecutorTestPatterns, remove_with_key) {
  auto const bindParameters = VPackParser::fromJson("{ }");
  std::string docQuery =
      std::string("FOR d IN " + collectionName + " FILTER d.value <= " + rDocsString +
                  " SORT d.sortvalue RETURN d");

  auto docs = executeQuery(vocbase, docQuery, bindParameters);
  ASSERT_TRUE(docs.ok());

  std::string query = std::string("FOR i IN 1.." + rDocsString +
                                  " REMOVE { _key: TO_STRING(i) } IN " +
                                  collectionName + " RETURN OLD");
  AssertQueryHasResult(vocbase, query, docs.data->slice());
}

TEST_P(RemoveExecutorTestPatterns, remove_with_id) {
  auto const bindParameters = VPackParser::fromJson("{ }");
  std::string allQuery = std::string("FOR d IN " + collectionName + " RETURN d");

  auto allDocs = executeQuery(vocbase, allQuery, bindParameters);
  ASSERT_TRUE(allDocs.ok());

  std::string query = R"aql(FOR d IN )aql" + collectionName +
                      R"aql( REMOVE { _key: d._key } IN )aql" + collectionName +
                      R"aql( RETURN OLD)aql";

  AssertQueryHasResult(vocbase, query, allDocs.data->slice());
}

TEST_P(RemoveExecutorTestPatterns, remove_all_without_return_subquery) {
  auto const expected = VPackParser::fromJson("[[ ]]");
  std::string query =
      std::string("FOR i in 1..1 LET x = (FOR d IN " + collectionName +
                  " REMOVE d IN " + collectionName + ") RETURN x");

  AssertQueryHasResult(vocbase, query, expected->slice());

  std::string checkQuery = "FOR i IN " + collectionName + " RETURN i.value";
  AssertQueryHasResult(vocbase, checkQuery, VPackSlice::emptyArraySlice());
}

INSTANTIATE_TEST_CASE_P(RemoveExecutorTestInstance, RemoveExecutorTestPatterns,
                        testing::Values(std::pair{100, 10}, std::pair{1000, 10},
                                        std::pair{1000, 100}, std::pair{999, 10},
                                        std::pair{1001, 1000}, std::pair{1001, 1001},
                                        std::pair{2001, 1000}, std::pair{2001, 1500},
                                        std::pair{3000, 1000}, std::pair{3000, 2001}));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
