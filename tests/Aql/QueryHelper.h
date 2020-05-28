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

#include <string>

struct TRI_vocbase_t;

namespace arangodb {
// forward declarations
namespace velocypack {
class Slice;
}

namespace aql {
struct QueryResult;
}

namespace tests {

namespace aql {

/// @brief Tests if the given QueryResult matches the given expected value
///        Expected is required to be an Array (maybe empty). As AQL can only
///        return a cursor, that is transformed into an Array.
///        Ordering matters. Also asserts that query was successful.
void AssertQueryResultToSlice(arangodb::aql::QueryResult const& result,
                              arangodb::velocypack::Slice expected);

/// @brief Tests if executing the given query on the given database results in the given expected value
///        Expected is required to be an Array (maybe empty). As AQL can only
///        return a cursor, that is transformed into an Array.
///        Ordering matters. Also asserts that query was successful.
void AssertQueryHasResult(TRI_vocbase_t& database, std::string const& queryString,
                          arangodb::velocypack::Slice expected);

/// @brief Tests if executing the given query on the given database results in the given error
///        Requires the query to error. Testing of no-error (TRI_ERROR_NO_ERROR) is not possible.
void AssertQueryFailsWith(TRI_vocbase_t& database, std::string const& query, int errorNumber);

}  // namespace aql
}  // namespace tests

}  // namespace arangodb