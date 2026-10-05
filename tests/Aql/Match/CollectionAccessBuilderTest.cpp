////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/Match/CollectionAccessBuilder.h"
#include "Aql/Match/PatternTypes.h"
#include "Basics/Exceptions.h"
#include "Basics/ErrorCode.h"

using namespace arangodb::aql::match;

TEST(CollectionAccessBuilderTest, requireCollectionNameReturnsCollection) {
  auto ds = DataSource::collection("vc");
  EXPECT_EQ("vc", CollectionAccessBuilder::requireCollectionName(ds));
}

TEST(CollectionAccessBuilderTest, requireCollectionNameRejectsBindParameter) {
  auto ds = DataSource::bindParameter("@@vc");
  try {
    (void)CollectionAccessBuilder::requireCollectionName(ds);
    FAIL() << "expected unresolved bind parameter to throw";
  } catch (arangodb::basics::Exception const& ex) {
    EXPECT_EQ(TRI_ERROR_INTERNAL, ex.code());
  }
}
