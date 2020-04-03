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

#include "Aql/ExecutionBlock.h"
#include "Basics/StringUtils.h"

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
//       So we actually do insert, this needs to be fixed although this seems to be no production case
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
//       So we actually do insert, this needs to be fixed although this seems to be no production case
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
//       So we actually do insert, this needs to be fixed although this seems to be no production case
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

TEST_P(UpsertExecutorTest, insert_not_found) {
  std::string query = R"aql(
      UPSERT {_key: "keyNotThere"}
      INSERT {value: 2, sortValue: 2})aql" +
                      action() + R"aql({value: "invalid"}
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([1, 2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

TEST_P(UpsertExecutorTest, insert_on_value) {
  std::string query = R"aql(
      UPSERT {value: 2}
      INSERT {value: 2, sortValue: 2})aql" +
                      action() + R"aql({value: "invalid"}
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([1, 2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

TEST_P(UpsertExecutorTest, upsert_on_value) {
  std::string query = R"aql(
      UPSERT {value: 1}
      INSERT {value: "invalid"})aql" +
                      action() + R"aql({value: 2}
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

TEST_P(UpsertExecutorTest, alternate_insert_update) {
  std::string query = R"aql(
      FOR i IN 20..30
      UPSERT {value: 2}
      INSERT {value: 2, sortValue: i})aql" +
                      action() + R"aql({value: i, sortValue: i}
      INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  // The idea of this query is, that we first insert an unknown value 2.
  // In the next iteration we are supposed to find this value 2.
  // and overwrite it by something else.
  // This will in turn allow to insert a new document with value 2.
  auto expected = VPackParser::fromJson(R"([1, 21, 23, 25, 27, 29, 2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

INSTANTIATE_TEST_CASE_P(UpsertExecutorTestBasics, UpsertExecutorTest,
                        ::testing::Values(UpsertType::UPDATE, UpsertType::REPLACE));

/*
 * SECTION: Integration tests
 */

class UpsertExecutorIntegrationTest
    : public testing::TestWithParam<std::tuple<UpsertType, size_t>> {
 protected:
  mocks::MockAqlServer server{};
  TRI_vocbase_t& vocbase{server.getSystemDatabase()};

  void SetUp() override {
    SCOPED_TRACE("Setup");
    ASSERT_EQ(ExecutionBlock::ProductionDefaultBatchSize, ExecutionBlock::DefaultBatchSize);

    ExecutionBlock::setDefaultBatchSize(100);

    auto info = VPackParser::fromJson(R"({"name":"UnitTestCollection"})");
    auto collection = vocbase.createCollection(info->slice());
    ASSERT_NE(collection.get(), nullptr) << "Failed to create collection";
    // Insert Documents
    std::string insertQuery =
        R"aql(FOR i IN 1..)aql" + std::to_string(numDocs()) +
        R"aql( INSERT {_key: TO_STRING(i), value: i, sortValue: i} INTO UnitTestCollection)aql";
    SCOPED_TRACE(insertQuery);
    AssertQueryHasResult(vocbase, insertQuery, VPackSlice::emptyArraySlice());
    VPackBuilder expected;
    {
      VPackArrayBuilder a{&expected};
      for (size_t i = 1; i <= numDocs(); ++i) {
        expected.add(VPackValue(i));
      }
    }
    AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
  }

  void TearDown() override {
    ExecutionBlock::setDefaultBatchSize(ExecutionBlock::ProductionDefaultBatchSize);
  }

  size_t numDocs() const {
    auto const& [type, numDocs] = GetParam();
    return numDocs;
  }

  UpsertType type() const {
    auto const& [type, numDocs] = GetParam();
    return type;
  }

  std::string const action() const {
    if (type() == UpsertType::UPDATE) {
      return "UPDATE";
    }
    return "REPLACE";
  }
};

// This is disallowed in the parser (TRI_ERROR_QUERY_PARSE)
TEST_P(UpsertExecutorIntegrationTest, DISABLED_upsert_all) {
  std::string query = R"aql(
      FOR doc IN UnitTestCollection
      UPSERT doc 
      INSERT {value: "invalid"}
      )aql" + action() +
                      R"aql( {value: "foo"} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_all_by_key) {
  std::string query = R"aql(FOR doc IN 1..)aql" + basics::StringUtils::itoa(numDocs()) +
                      R"aql( UPSERT {_key: TO_STRING(doc)} 
                             INSERT {value: "invalid"} )aql" +
                      action() + R"aql( {value: 'foo'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_all_by_id) {
  std::string query =
      R"aql(FOR doc IN 1..)aql" + basics::StringUtils::itoa(numDocs()) +
      R"aql( UPSERT {_id: CONCAT("UnitTestCollection/", TO_STRING(doc)) } 
                             INSERT {value: "invalid"} )aql" +
      action() + R"aql( {value: 'foo'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_only_even) {
  std::string query = R"aql(
    FOR sortValue IN 1..)aql" +
                      basics::StringUtils::itoa(numDocs()) +
                      R"aql(
      FILTER sortValue % 2 == 0
      UPSERT {sortValue}
      INSERT {value: "invalid"} )aql" +
                      action() + R"aql(
      {value: 'foo', sortValue} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      if (i % 2 == 0) {
        expected.add(VPackValue("foo"));
      } else {
        expected.add(VPackValue(i));
      }
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_all_but_skip) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    UPSERT {_key: doc._key}
    INSERT {value: 'invalid'} )aql" +
                      action() + R"aql(
    {value: 'foo', sortValue: doc.sortValue } IN UnitTestCollection
    LIMIT 526, null
    RETURN 1
  )aql";
  VPackBuilder expectedUpdateResponse;
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    VPackArrayBuilder b{&expectedUpdateResponse};

    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
      if (i > 526) {
        expectedUpdateResponse.add(VPackValue(1));
      }
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_all_return_old) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    UPSERT {_key: doc._key}
    INSERT {value: 'invalid'} )aql" +
                      action() + R"aql(
    {value: 'foo', sortValue: doc.sortValue } IN UnitTestCollection
    RETURN OLD.value
  )aql";
  VPackBuilder expectedUpdateResponse;
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    VPackArrayBuilder b{&expectedUpdateResponse};

    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
      expectedUpdateResponse.add(VPackValue(i));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_all_return_new) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    UPSERT {_key: doc._key}
    INSERT {value: 'invalid'} )aql" +
                      action() + R"aql(
    {value: 'foo', sortValue: doc.sortValue } IN UnitTestCollection
    RETURN NEW.value
  )aql";

  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};

    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, expected.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_all_return_old_and_new) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    UPSERT {_key: doc._key}
    INSERT {value: 'invalid'} )aql" +
                      action() + R"aql(
    {value: 'foo', sortValue: doc.sortValue } IN UnitTestCollection
    RETURN {old: OLD.value, new: NEW.value}
  )aql";

  VPackBuilder expectedUpdateResponse;
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    VPackArrayBuilder b{&expectedUpdateResponse};

    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
      {
        VPackObjectBuilder c{&expectedUpdateResponse};
        expectedUpdateResponse.add("old", VPackValue(i));
        expectedUpdateResponse.add("new", VPackValue("foo"));
      }
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_handling_old_attributes) {
  std::string query = R"aql(
      FOR doc IN UnitTestCollection
      UPSERT {_key: doc._key}
      INSERT {value: "invalid"} )aql" +
                      action() + R"aql(
      {foo: 'foo'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    if (type() == UpsertType::UPDATE) {
      for (size_t i = 1; i <= numDocs(); ++i) {
        expected.add(VPackValue(i));
      }
    } else {
      for (size_t i = 1; i <= numDocs(); ++i) {
        expected.add(VPackSlice::nullSlice());
      }
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_in_subquery_multi_access) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    LET updated = (UPSERT {_key: doc._key}
      INSERT {value: "invalid"} )aql" +
                      action() + R"aql(
      {value: 'foo'} IN UnitTestCollection)
    RETURN updated
  )aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue(i));
    }
  }
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION);
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_in_subquery) {
  std::string query = R"aql(
    FOR x IN ["foo", "bar"]
    FILTER x != "foo" /* The storage engine mock does NOT support multiple edits */
    LET updated = (
      FOR doc IN UnitTestCollection
        UPSERT {_key: doc._key} 
        INSERT {value: "invalid"})aql" +
                      action() +
                      R"aql( {value: x} IN UnitTestCollection
    )
    RETURN updated
  )aql";
  // Both subqueries do not return anything
  auto expectedUpdateResponse = VPackParser::fromJson(R"([[]])");
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("bar"));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse->slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_in_subquery_with_outer_skip) {
  std::string query = R"aql(
    FOR x IN 1..2
      LET updated = (
        FILTER x < 2
        FOR doc IN UnitTestCollection
          UPSERT {_key: doc._key} 
          INSERT {value: "invalid"})aql" +
                      action() +
                      R"aql( {value: "foo"} IN UnitTestCollection)
    LIMIT 1, null
    RETURN updated
  )aql";
  VPackBuilder expectedUpdateResponse;
  VPackBuilder expected;
  {
    // [ [] ]
    VPackArrayBuilder b{&expectedUpdateResponse};
    VPackArrayBuilder c{&expectedUpdateResponse};

    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_in_subquery_with_inner_skip) {
  std::string query = R"aql(
    FOR x IN 1..2
    LET updated = (
      FILTER x < 2
      FOR doc IN UnitTestCollection
        UPSERT {_key: doc._key} 
        INSERT {value: "invalid"})aql" +
                      action() +
                      R"aql(  {value: CONCAT('foo', TO_STRING(x))} IN UnitTestCollection
        LIMIT 526, null
      RETURN 1
    )
    RETURN LENGTH(updated)
  )aql";

  VPackBuilder expectedUpdateResponse;
  {
    VPackArrayBuilder a{&expectedUpdateResponse};
    if (numDocs() < 526) {
      expectedUpdateResponse.add(VPackValue(0));
    } else {
      expectedUpdateResponse.add(VPackValue(numDocs() - 526));
    }
    // Second subquery is fully filtered
    expectedUpdateResponse.add(VPackValue(0));
  }

  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo1"));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_all_insert_by_key) {
  std::string from = basics::StringUtils::itoa(numDocs() + 1);
  std::string to = basics::StringUtils::itoa(numDocs() + numDocs());
  // The keys are out-of-range for the existing ones, so we trigger INSERT case
  std::string query = R"aql(FOR doc IN )aql" + from + R"aql(..)aql" + to +
                      R"aql( UPSERT {_key: TO_STRING(doc)} 
                             INSERT {_key: TO_STRING(doc), value: "foo", sortValue: doc} )aql" +
                      action() + R"aql( {value: 'invalid'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    // Keep first untouched
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue(i));
    }
    // INSERT as many as we already have with foo
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_first_update_then_insert) {
  std::string from = basics::StringUtils::itoa(1);
  std::string to = basics::StringUtils::itoa(numDocs() + numDocs());
  // We start with updates, then hit end of keyrange and insert
  std::string query = R"aql(FOR doc IN )aql" + from + R"aql(..)aql" + to +
                      R"aql( UPSERT {_key: TO_STRING(doc)} 
                             INSERT {_key: TO_STRING(doc), value: "foo", sortValue: doc} )aql" +
                      action() + R"aql( {value: 'bar'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    // Updates result in BAR
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("bar"));
    }
    // INSERT as many as we already have with foo
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_first_insert_then_update) {
  std::string from = basics::StringUtils::itoa(numDocs() + numDocs());
  std::string to = basics::StringUtils::itoa(1);
  // We start with insert, then hit end of keyrange and start with updates.
  // We iterate downwards
  std::string query = R"aql(FOR doc IN )aql" + from + R"aql(..)aql" + to +
                      R"aql( UPSERT {_key: TO_STRING(doc)} 
                             INSERT {_key: TO_STRING(doc), value: "foo", sortValue: doc} )aql" +
                      action() + R"aql( {value: 'bar'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    // Updates result in BAR
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("bar"));
    }
    // INSERT as many as we already have with foo
    for (size_t i = 1; i <= numDocs(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(UpsertExecutorIntegrationTest, upsert_alternate_insert_upsert) {
  std::string from = basics::StringUtils::itoa(1);
  std::string to = basics::StringUtils::itoa(numDocs() + numDocs());
  // We alternate between inserts and updates
  // If number is disible by two, we divide it by two. (in key range = update)
  // If not, we divide, round floor and add 1000 (out of key range = insert)
  std::string query = R"aql(FOR preMod IN )aql" + from + R"aql(..)aql" + to +
                      R"aql(
                             LET doc = (preMod % 2 == 0) ? (preMod / 2) : (floor(preMod / 2) + 2000)
                             UPSERT {_key: TO_STRING(doc)} 
                             INSERT {_key: TO_STRING(doc), value: "foo", sortValue: preMod} )aql" +
                      action() + R"aql( {value: 'bar', sortValue: preMod} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    // Updates result in BAR
    for (size_t i = 1; i <= numDocs() * 2; ++i) {
      if (i % 2 == 0) {
        expected.add(VPackValue("bar"));
      } else {
        expected.add(VPackValue("foo"));
      }
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

INSTANTIATE_TEST_CASE_P(UpsertExecutorIntegration, UpsertExecutorIntegrationTest,
                        ::testing::Combine(::testing::Values(UpsertType::UPDATE, UpsertType::REPLACE),
                                           ::testing::Values(1, 101)));

/* This works as well, but takes considerably more time to pass.
INSTANTIATE_TEST_CASE_P(UpsertExecutorIntegration, UpsertExecutorIntegrationTest,
                        ::testing::Combine(::testing::Values(UpsertType::UPDATE, UpsertType::REPLACE),
                                           ::testing::Values(1, 999, 1000, 1001, 2001)));
*/

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
