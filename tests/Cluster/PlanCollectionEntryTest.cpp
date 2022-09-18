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

#include "Cluster/Utils/PlanCollectionEntry.h"
#include "VocBase/Properties/PlanCollection.h"

#include "Logger/LogMacros.h"
#include "Inspection/VPack.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {
class PlanCollectionEntryTest : public ::testing::Test {
 protected:
};

TEST_F(PlanCollectionEntryTest, default_values) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  PlanCollectionEntry entry{col};

  auto builder = entry.toVPackDeprecated();
  LOG_DEVEL << builder.toJson();
}

}
