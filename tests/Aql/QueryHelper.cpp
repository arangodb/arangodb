////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "QueryHelper.h"
#include "gtest/gtest.h"

#include "../IResearch/IResearchQueryCommon.h"
#include "Aql/QueryResult.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {
// Check whether there exists some None value inside this slice, recursively.
auto vpackHasNoneRecursive(VPackSlice slice) -> bool {
  slice = slice.resolveExternals();
  if (slice.isNone()) {
    return true;
  }

  if (slice.isArray()) {
    auto iter = VPackArrayIterator(slice);
    return std::any_of(iter.begin(), iter.end(),
                       [](auto slice) { return vpackHasNoneRecursive(slice); });
  }
  if (slice.isObject()) {
    auto iter = VPackObjectIterator(slice);
    return std::any_of(iter.begin(), iter.end(), [](auto pair) {
      return vpackHasNoneRecursive(pair.key) || vpackHasNoneRecursive(pair.value);
    });
  }

  return false;
}
}

void arangodb::tests::aql::AssertQueryResultToSlice(QueryResult const& result,
                                                    VPackSlice expected) {
  ASSERT_TRUE(expected.isArray()) << "Invalid input";
  ASSERT_TRUE(result.ok()) << "Reason: " << result.errorNumber() << " => "
                           << result.errorMessage();
  auto resultSlice = result.data->slice();
  ASSERT_TRUE(resultSlice.isArray());
  EXPECT_FALSE(vpackHasNoneRecursive(result.data->slice()));
  ASSERT_EQ(expected.length(), resultSlice.length()) << resultSlice.toJson();
  for (VPackValueLength i = 0; i < expected.length(); ++i) {
    EXPECT_TRUE(basics::VelocyPackHelper::equal(resultSlice.at(i), expected.at(i), false))
        << "Line " << i << ": " << resultSlice.at(i).toJson()
        << " (found) != " << expected.at(i).toJson();
  }
}

void arangodb::tests::aql::AssertQueryHasResult(TRI_vocbase_t& database,
                                                std::string const& query,
                                                VPackSlice expected) {
  auto const bindParameters = VPackParser::fromJson("{ }");
  SCOPED_TRACE("Query: " + query);
  auto queryResult = arangodb::tests::executeQuery(database, query, bindParameters);
  AssertQueryResultToSlice(queryResult, expected);
}

void arangodb::tests::aql::AssertQueryFailsWith(TRI_vocbase_t& database,
                                                std::string const& query,
                                                ErrorCode errorNumber) {
  auto const bindParameters = VPackParser::fromJson("{ }");
  SCOPED_TRACE("Query: " + query);
  auto queryResult = arangodb::tests::executeQuery(database, query, bindParameters);
  EXPECT_FALSE(queryResult.ok()) << "Should yield error number " << errorNumber;
  EXPECT_EQ(queryResult.errorNumber(), errorNumber)
      << "Returned message: " << queryResult.errorMessage();
}
