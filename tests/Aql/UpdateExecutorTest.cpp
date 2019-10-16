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

class UpdateExecutorTest : public testing::TestWithParam<size_t> {
 protected:
  mocks::MockAqlServer server{};
  TRI_vocbase_t& vocbase{server.getSystemDatabase()};

  UpdateExecutorTest() {}

  void SetUp() override {
    SCOPED_TRACE("Setup");
    auto info = VPackParser::fromJson(R"({"name":"UnitTestCollection"})");
    auto collection = vocbase.createCollection(info->slice());
    ASSERT_NE(collection.get(), nullptr) << "Failed to create collection";
    size_t numDocs = GetParam();
    // Insert Documents
    std::string insertQuery =
        R"aql(FOR i IN 1..)aql" + basics::StringUtils::itoa(numDocs) +
        R"aql( INSERT {value: i, sortValue: i} INTO UnitTestCollection)aql";
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

TEST_P(UpdateExecutorTest, update_all) {
  std::string query = R"aql(FOR doc IN UnitTestCollection UPDATE doc WITH {value: 'foo'} IN UnitTestCollection)aql";
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

TEST_P(UpdateExecutorTest, update_only_even) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
      FILTER doc.sortValue % 2 == 0
      UPDATE doc WITH {value: 'foo'} IN UnitTestCollection
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

TEST_P(UpdateExecutorTest, update_all_but_skip) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    SORT doc.sortValue
    UPDATE doc WITH {value: 'foo'} IN UnitTestCollection
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

TEST_P(UpdateExecutorTest, update_all_return_old) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    UPDATE doc WITH {value: 'foo'} IN UnitTestCollection
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

TEST_P(UpdateExecutorTest, update_all_return_new) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    UPDATE doc WITH {value: 'foo'} IN UnitTestCollection
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

TEST_P(UpdateExecutorTest, update_all_return_old_and_new) {
  std::string query = R"aql(
    FOR doc IN UnitTestCollection
    UPDATE doc WITH {value: 'foo'} IN UnitTestCollection
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

INSTANTIATE_TEST_CASE_P(UpdateExecutorTestRunner, UpdateExecutorTest,
                        ::testing::Values(1, 999, 1000, 1001, 2001));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb