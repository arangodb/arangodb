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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/QueryString.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/StandaloneContext.h"
#include "Graph/Cursors/DBServerEdgeCursor.h"
#include "Mocks/MockQuery.h"

// using namespace arangodb;
// using namespace arangodb::aql;
using namespace arangodb::graph;

namespace arangodb::tests {

class DBServerEdgeCursorTest : public ::testing::Test {};

TEST_F(DBServerEdgeCursorTest, bla) {
  auto queryString = arangodb::aql::QueryString(std::string_view("RETURN 1"));

  auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(
      server.getSystemDatabase(), transaction::OperationOriginTestCase{});
  auto fakeQuery = std::make_shared<mocks::MockQuery>(ctx, queryString);
  traverser::TraverserOptions opts{*fakeQuery};
  auto lookupInfo = std::vector<BaseOptions::LookupInfo>{};
  auto cursor = DBServerEdgeCursor(nullptr, nullptr, lookupInfo);
  // cursor.rearm("A", 0);
}

}  // namespace arangodb::tests
