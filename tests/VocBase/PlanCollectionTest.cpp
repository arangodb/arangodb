////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Exceptions.h"
#include "VocBase/Properties/PlanCollection.h"
#include <velocypack/Builder.h>

namespace arangodb {
namespace tests {

class PlanCollectionUserAPITest : public ::testing::Test {};

TEST_F(PlanCollectionUserAPITest, test_requires_some_input) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  EXPECT_THROW(PlanCollection::fromCreateAPIBody(body.slice()),
               arangodb::basics::Exception);
}

TEST_F(PlanCollectionUserAPITest, test_minimal_user_input) {
  std::string colName = "test";
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue(colName));
  }
  auto testee = PlanCollection::fromCreateAPIBody(body.slice());
  EXPECT_EQ(testee.name, colName);
  // Test Default values
  EXPECT_FALSE(testee.waitForSync);
  EXPECT_FALSE(testee.isSystem);

  // TODO: this is just rudimentary
  // does not test internals yet
  EXPECT_TRUE(testee.schema.slice().isObject());
  EXPECT_TRUE(testee.keyOptions.slice().isObject());
}
}  // namespace tests
}  // namespace arangodb
