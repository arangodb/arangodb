////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "../Mocks/Servers.h"
#include "Basics/StringUtils.h"
#include "QueryHelper.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {

namespace aql {

static const std::string GetAllDocs =
    R"aql(FOR doc IN UnitTestCollection SORT doc.sortValue RETURN doc.value)aql";

/*
 * SECTION: ExecutionBlock tests
 */

class ReplaceExecutorTest : public testing::Test {
 protected:
  mocks::MockAqlServer server{};
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
};

TEST_F(ReplaceExecutorTest, basic) {
  std::string query = R"aql(REPLACE "testee" WITH {value: 2} INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

TEST_F(ReplaceExecutorTest, option_ignoreErrors_default) {
  std::string query = R"aql(REPLACE "unkown" WITH {value: 2} INTO UnitTestCollection)aql";
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  AssertNotChanged();
}

TEST_F(ReplaceExecutorTest, option_ignoreErrors_true) {
  std::string query = R"aql(REPLACE "unkown" WITH {value: 2} INTO UnitTestCollection OPTIONS {ignoreErrors: true})aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertNotChanged();
}

TEST_F(ReplaceExecutorTest, option_ignoreErrors_false) {
  std::string query = R"aql(REPLACE "unkown" WITH {value: 2} INTO UnitTestCollection OPTIONS {ignoreErrors: false})aql";
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  AssertNotChanged();
}

TEST_F(ReplaceExecutorTest, option_ignoreRevs_default) {
  std::string query = R"aql(REPLACE {_key: "testee", _rev: "12345"} WITH {value: 2} INTO UnitTestCollection)aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

TEST_F(ReplaceExecutorTest, option_ignoreRevs_true) {
  std::string query = R"aql(REPLACE {_key: "testee", _rev: "12345"} WITH {value: 2} INTO UnitTestCollection OPTIONS {ignoreRevs: true} )aql";
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());

  auto expected = VPackParser::fromJson(R"([2])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());
}

TEST_F(ReplaceExecutorTest, option_ignoreRevs_false) {
  std::string query = R"aql(REPLACE {_key: "testee", _rev: "12345"} WITH {value: 2} INTO UnitTestCollection OPTIONS {ignoreRevs: false} )aql";
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_ARANGO_CONFLICT);
  AssertNotChanged();
}

/*
 * SECTION: Integration tests
 */

class ReplaceExecutorIntegrationTest : public testing::TestWithParam<size_t> {
 protected:
  mocks::MockAqlServer server{};
  TRI_vocbase_t& vocbase{server.getSystemDatabase()};

  void SetUp() override {
    SCOPED_TRACE("Setup");
    auto info = VPackParser::fromJson(R"({"name":"UnitTestCollection"})");
    auto collection = vocbase.createCollection(info->slice());
    ASSERT_NE(collection.get(), nullptr) << "Failed to create collection";
    size_t numDocs = GetParam();
    // Insert Documents
    std::string insertQuery =
        R"aql(FOR i IN 1..)aql" + basics::StringUtils::itoa(numDocs) +
        R"aql( INSERT {_key: TO_STRING(i), value: i, sortValue: i} INTO UnitTestCollection)aql";
    SCOPED_TRACE(insertQuery);
    AssertQueryHasResult(vocbase, insertQuery, VPackSlice::emptyArraySlice());
    VPackBuilder expected;
    {
      VPackArrayBuilder a{&expected};
      for (size_t i = 1; i <= GetParam(); ++i) {
        expected.add(VPackValue(i));
      }
    }
    AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
  }
};

TEST_P(ReplaceExecutorIntegrationTest, replace_all) {
  std::string query = R"aql(FOR doc IN UnitTestCollection REPLACE doc WITH {value: 'foo'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_all_by_key) {
  std::string query =
      R"aql(FOR doc IN 1..)aql" + basics::StringUtils::itoa(GetParam()) +
      R"aql( REPLACE TO_STRING(doc) WITH {value: 'foo'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_all_by_id) {
  std::string query =
      R"aql(FOR doc IN 1..)aql" + basics::StringUtils::itoa(GetParam()) +
      R"aql( REPLACE CONCAT("UnitTestCollection/", TO_STRING(doc)) WITH {value: 'foo'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue(i));
    }
  }
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_only_even) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
      FILTER doc.sortValue % 2 == 0
      REPLACE doc WITH {value: 'foo', sortValue: doc.sortValue} IN UnitTestCollection
  )aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
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

TEST_P(ReplaceExecutorIntegrationTest, replace_all_but_skip) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    REPLACE doc WITH {value: 'foo'} IN UnitTestCollection
    LIMIT 526, null
    RETURN 1
  )aql";
  VPackBuilder expectedUpdateResponse;
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    VPackArrayBuilder b{&expectedUpdateResponse};

    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("foo"));
      if (i > 526) {
        expectedUpdateResponse.add(VPackValue(1));
      }
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_all_return_old) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    REPLACE doc WITH {value: 'foo'} IN UnitTestCollection
    RETURN OLD.value
  )aql";
  VPackBuilder expectedUpdateResponse;
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    VPackArrayBuilder b{&expectedUpdateResponse};

    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("foo"));
      expectedUpdateResponse.add(VPackValue(i));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_all_return_new) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    REPLACE doc WITH {value: 'foo'} IN UnitTestCollection
    RETURN NEW.value
  )aql";

  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};

    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, expected.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_all_return_old_and_new) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    REPLACE doc WITH {value: 'foo'} IN UnitTestCollection
    RETURN {old: OLD.value, new: NEW.value}
  )aql";

  VPackBuilder expectedUpdateResponse;
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    VPackArrayBuilder b{&expectedUpdateResponse};

    for (size_t i = 1; i <= GetParam(); ++i) {
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

TEST_P(ReplaceExecutorIntegrationTest, replace_remove_old_attributes) {
  std::string query = R"aql(FOR doc IN UnitTestCollection REPLACE doc WITH {foo: 'foo'} IN UnitTestCollection)aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackSlice::nullSlice());
    }
  }
  AssertQueryHasResult(vocbase, query, VPackSlice::emptyArraySlice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_in_subquery_multi_access) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    LET updated = (REPLACE doc WITH {value: 'foo'} IN UnitTestCollection)
    RETURN updated
  )aql";
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue(i));
    }
  }
  AssertQueryFailsWith(vocbase, query, TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION);
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_in_subquery) {
  std::string query = R"aql(
    FOR x IN ["foo", "bar"]
    FILTER x != "foo" /* The storage engine mock does NOT support multiple edits */
    LET updated = (
      FOR doc IN UnitTestCollection
      REPLACE doc WITH {value: x} IN UnitTestCollection
    )
    RETURN updated
  )aql";
  // Both subqueries do not return anything
  auto expectedUpdateResponse = VPackParser::fromJson(R"([[]])");
  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("bar"));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse->slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_in_subquery_with_outer_skip) {
  std::string query = R"aql(
    FOR x IN 1..2
      LET updated = (
        FILTER x < 2
        FOR doc IN UnitTestCollection
        REPLACE doc WITH {value: 'foo'} IN UnitTestCollection)
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
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("foo"));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

TEST_P(ReplaceExecutorIntegrationTest, replace_in_subquery_with_inner_skip) {
  std::string query = R"aql(
    FOR x IN 1..2
    LET updated = (
      FILTER x < 2
      FOR doc IN UnitTestCollection
        REPLACE doc WITH {value: CONCAT('foo', TO_STRING(x))} IN UnitTestCollection
        LIMIT 526, null
      RETURN 1
    )
    RETURN LENGTH(updated)
  )aql";

  VPackBuilder expectedUpdateResponse;
  {
    VPackArrayBuilder a{&expectedUpdateResponse};
    if (GetParam() < 526) {
      expectedUpdateResponse.add(VPackValue(0));
    } else {
      expectedUpdateResponse.add(VPackValue(GetParam() - 526));
    }
    // Second subquery is fully filtered
    expectedUpdateResponse.add(VPackValue(0));
  }

  VPackBuilder expected;
  {
    VPackArrayBuilder a{&expected};
    for (size_t i = 1; i <= GetParam(); ++i) {
      expected.add(VPackValue("foo1"));
    }
  }
  AssertQueryHasResult(vocbase, query, expectedUpdateResponse.slice());
  AssertQueryHasResult(vocbase, GetAllDocs, expected.slice());
}

INSTANTIATE_TEST_CASE_P(ReplaceExecutorIntegration, ReplaceExecutorIntegrationTest,
                        ::testing::Values(1, 999, 1000, 1001, 2001));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb