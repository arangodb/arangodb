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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Mocks/Servers.h"
#include "QueryHelper.h"
#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

static const std::string GetAllDocs =
    R"aql(FOR doc IN UnitTestCollection SORT doc.sortValue RETURN doc.value)aql";

enum UpsertType { UPDATE, REPLACE };

class UpsertExecutorTest : public ::testing::TestWithParam<UpsertType> {
 protected:
  mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase{server.getSystemDatabase()};

  void SetUp() override {
    SCOPED_TRACE("Setup");
    auto info = VPackParser::fromJson(R"({"name":"UnitTestCollection"})");
    auto collection = vocbase.createCollection(info->slice());
    ASSERT_NE(collection.get(), nullptr) << "Failed to create collection";
    // Insert Documents
    std::string insertQuery =
        R"aql(INSERT {_key: "testee", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection)aql";
    SCOPED_TRACE(insertQuery);
    AssertQueryHasResult(vocbase, insertQuery, VPackSlice::emptyArraySlice());
    auto expected = VPackParser::fromJson(R"([1])");
    AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
  }

  void AssertNotChanged() {
    auto expected = VPackParser::fromJson(R"([1])");
    AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
  }

  std::string const action() const {
    if (GetParam() == UpsertType::UPDATE) {
      return "UPDATE";
    }
    return "REPLACE";
  }
};

TEST_P(UpsertExecutorTest, basic) {
  std::string query = R"aql(
      UPSERT {_key: "testee"}
      INSERT {value: "invalid"})aql" +
                      action() + R"aql({value: 2}
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

TEST_P(UpsertExecutorTest, option_ignoreErrors_default) {
  // This should trigger a duplicate key error
  std::string query = R"aql(
      UPSERT {value: "thiscannotbefound"}
      INSERT {_key: "testee", value: 2}
      )aql" + action() +
                      R"aql( {value: 2}
      INTO UnitTestCollection)aql";
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
  AssertNotChanged();
}

TEST_P(UpsertExecutorTest, option_ignoreErrors_true) {
  // This should trigger a duplicate key error
  std::string query = R"aql(
      UPSERT {value: "thiscannotbefound"}
      INSERT {_key: "testee", value: 2}
      )aql" + action() +
                      R"aql( {value: 2}
      INTO UnitTestCollection
      OPTIONS {ignoreErrors: true})aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertNotChanged();
}

TEST_P(UpsertExecutorTest, option_ignoreErrors_false) {
  // This should trigger a duplicate key error
  std::string query = R"aql(
      UPSERT {value: "thiscannotbefound"}
      INSERT {_key: "testee", value: 2}
      )aql" + action() +
                      R"aql( {value: 2}
      INTO UnitTestCollection
      OPTIONS {ignoreErrors: false})aql";
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
  AssertNotChanged();
}

TEST_P(UpsertExecutorTest, option_keepNull_default) {
  std::string query = R"aql(
      UPSERT {_key: "testee"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {value: null}
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  std::string testQuery =
      R"aql(FOR x IN UnitTestCollection FILTER x._key == "testee" RETURN HAS(x, "value"))aql";
  auto expected = VPackParser::fromJson(R"([true])");
  AssertQueryHasResult(vocbase, testQuery, expected->slice());
}

TEST_P(UpsertExecutorTest, option_keepNull_true) {
  std::string query = R"aql(
      UPSERT {_key: "testee"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {value: null}
      INTO UnitTestCollection
      OPTIONS {keepNull: true})aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  std::string testQuery =
      R"aql(FOR x IN UnitTestCollection FILTER x._key == "testee" RETURN HAS(x, "value"))aql";
  auto expected = VPackParser::fromJson(R"([true])");
  AssertQueryHasResult(vocbase, testQuery, expected->slice());
}

TEST_P(UpsertExecutorTest, option_keepNull_false) {
  std::string query = R"aql(
      UPSERT {_key: "testee"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {value: null}
      INTO UnitTestCollection
      OPTIONS {keepNull: false})aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  std::string testQuery =
      R"aql(FOR x IN UnitTestCollection FILTER x._key == "testee" RETURN HAS(x, "value"))aql";
  if (GetParam() == UpsertType::UPDATE) {
    auto expected = VPackParser::fromJson(R"([false])");
    AssertQueryHasResult(vocbase, testQuery, expected->slice());
  } else {
    // Replace will not honor keepNull
    auto expected = VPackParser::fromJson(R"([true])");
    AssertQueryHasResult(vocbase, testQuery, expected->slice());
  }
}

TEST_P(UpsertExecutorTest, option_mergeObjects_default) {
  std::string query = R"aql(
      UPSERT {_key: "testee"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {nestedObject: {foo: "bar"} }
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  std::string testQuery =
      R"aql(FOR x IN UnitTestCollection FILTER x._key == "testee" RETURN x.nestedObject)aql";
  if (GetParam() == UpsertType::UPDATE) {
    auto expected = VPackParser::fromJson(R"([{"foo": "bar", "value": 1}])");
    AssertQueryHasResult(vocbase, testQuery, expected->slice());
  } else {
    // Replace will never merge
    auto expected = VPackParser::fromJson(R"([{"foo": "bar"}])");
    AssertQueryHasResult(vocbase, testQuery, expected->slice());
  }
}

TEST_P(UpsertExecutorTest, option_mergeObjects_true) {
  std::string query = R"aql(
      UPSERT {_key: "testee"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {nestedObject: {foo: "bar"} }
      INTO UnitTestCollection
      OPTIONS {mergeObjects: true})aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  std::string testQuery =
      R"aql(FOR x IN UnitTestCollection FILTER x._key == "testee" RETURN x.nestedObject)aql";
  if (GetParam() == UpsertType::UPDATE) {
    auto expected = VPackParser::fromJson(R"([{"foo": "bar", "value": 1}])");
    AssertQueryHasResult(vocbase, testQuery, expected->slice());
  } else {
    // Replace will never merge
    auto expected = VPackParser::fromJson(R"([{"foo": "bar"}])");
    AssertQueryHasResult(vocbase, testQuery, expected->slice());
  }
}

TEST_P(UpsertExecutorTest, option_mergeObjects_false) {
  std::string query = R"aql(
      UPSERT {_key: "testee"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {nestedObject: {foo: "bar"} }
      INTO UnitTestCollection
      OPTIONS {mergeObjects: false})aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  std::string testQuery =
      R"aql(FOR x IN UnitTestCollection FILTER x._key == "testee" RETURN x.nestedObject)aql";
  auto expected = VPackParser::fromJson(R"([{"foo": "bar"}])");
  AssertQueryHasResult(vocbase, testQuery, expected->slice());
}

// TODO: In current implementaiton we search for exact match of _key, _rev which is not found.
//       So we actually do insert, this needs to be fixed althoug this seems to be no production case
TEST_P(UpsertExecutorTest, DISABLED_option_ignoreRevs_default) {
  std::string query = R"aql(
      UPSERT {_key: "testee", _rev: "12345"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {value: 2}
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

// TODO: In current implementaiton we search for exact match of _key, _rev which is not found.
//       So we actually do insert, this needs to be fixed althoug this seems to be no production case
TEST_P(UpsertExecutorTest, DISABLED_option_ignoreRevs_true) {
  std::string query = R"aql(
      UPSERT {_key: "testee", _rev: "12345"}
      INSERT {value: "invalid"}
      UPDATE {value: 2}
      INTO UnitTestCollection
      OPTIONS {ignoreRevs: true} )aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}
// TODO: In current implementaiton we search for exact match of _key, _rev which is not found.
//       So we actually do insert, this needs to be fixed althoug this seems to be no production case
TEST_P(UpsertExecutorTest, DISABLED_option_ignoreRevs_false) {
  std::string query = R"aql(
      UPSERT {_key: "testee", _rev: "12345"}
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {value: 2}
      INTO UnitTestCollection
      OPTIONS {ignoreRevs: false} )aql";
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_ARANGO_CONFLICT);
  AssertNotChanged();
}

INSTANTIATE_TEST_CASE_P(UpsertExecutorTestBasics, UpsertExecutorTest,
                        ::testing::Values(UpsertType::UPDATE, UpsertType::REPLACE));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb