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

#include "Inspection/VPack.h"
#include "VocBase/Properties/KeyGeneratorProperties.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {
class KeyGeneratorPropertiesTest : public ::testing::Test {};

TEST_F(KeyGeneratorPropertiesTest, traditional) {
  VPackBuilder builder;
  builder.openObject();
  builder.add("type", "traditional");
  builder.close();

  KeyGeneratorProperties res;

  auto status = velocypack::deserializeWithStatus(builder.slice(), res);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(std::holds_alternative<TraditionalKeyGeneratorProperties>(res));
  EXPECT_FALSE(std::holds_alternative<AutoIncrementGeneratorProperties>(res));
}

}  // namespace arangodb::tests
