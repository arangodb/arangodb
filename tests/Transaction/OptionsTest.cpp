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
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Transaction/Options.h"

using namespace arangodb;

namespace {

velocypack::Builder serialize(transaction::Options const& options) {
  velocypack::Builder builder;
  builder.openObject();
  options.toVelocyPack(builder);
  builder.close();
  return builder;
}

}  // namespace

TEST(TransactionOptionsTest, ReadTimestampSurvivesVelocyPackRoundTrip) {
  transaction::Options options;
  options.readTimestamp = 1234;

  transaction::Options restored;
  restored.fromVelocyPack(serialize(options).slice());

  ASSERT_TRUE(restored.readTimestamp.has_value());
  EXPECT_EQ(*restored.readTimestamp, 1234u);
}

// "no read timestamp" is the absence of the attribute, not a sentinel value, so
// a receiver cannot mistake it for a request to read at some point in time.
TEST(TransactionOptionsTest, UnsetReadTimestampIsNotSerialized) {
  transaction::Options options;
  ASSERT_FALSE(options.readTimestamp.has_value());

  EXPECT_TRUE(serialize(options).slice().get("readTimestamp").isNone());
}
