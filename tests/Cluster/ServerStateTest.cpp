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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/ServerState.h"

using namespace arangodb;

TEST(ServerStateTest, IsCoordinatorId) {
  auto goodValues = {
      "CRDN-513eef6a-7601-4349-b074-532c3e7c6da3",
      "CRDN-5729b9a3-85a4-4128-a9f5-ec4a9c191e06",
      "CRDN-eea7b6ec-bcae-409c-afd2-32b33e7f45da",
  };

  for (auto v : goodValues) {
    EXPECT_TRUE(ServerState::isCoordinatorId(v));
  }

  auto badValues = {
      "",
      "C",
      "CR",
      "CRD",
      "CRDN",
      "CRDN-",
      "crdn-x",
      "Crdn-1234",
      " CRDN-999999999999999999999999999999999999999999",
      "CRDN-abcdef",
      "CRDN-1234",
      "CRDN-x",
      "CRDN-999999999999999999999999999999999999999999",
      "CRDN-\r\n\t",
      "CRDN-abcdef",
      "CRDN-513eef6a-7601-4349-b074-532c3e7c6da3-xyz",
      "CRDN-5729b9a3-85a4-4128-a9f5-ec4a9c191e061",
      "CRDN-1eea7b6ec-bcae-409c-afd2-32b33e7f45da",
      "PRMR-513eef6a-7601-4349-b074-532c3e7c6da3",
      "PRMR-616c101c-a82d-4cf1-b4e7-15cf9f84af58",
      "AGNT-b168326c-c546-483f-bb55-66ff7923c964",
  };

  for (auto v : badValues) {
    EXPECT_FALSE(ServerState::isCoordinatorId(v));
  }
}

TEST(ServerStateTest, IsDBServerId) {
  auto goodValues = {
      "PRMR-513eef6a-7601-4349-b074-532c3e7c6da3",
      "PRMR-616c101c-a82d-4cf1-b4e7-15cf9f84af58",
      "PRMR-d4614e67-9f7e-4154-b7b9-323b5464d56c",
  };

  for (auto v : goodValues) {
    EXPECT_TRUE(ServerState::isDBServerId(v));
  }

  auto badValues = {
      "",
      "P",
      "PR",
      "PRM",
      "PRMR",
      "PRMR-",
      "prmr-x",
      "prmr-1234",
      "PRMR-1234",
      "PRMR-x",
      "PRMR-999999999999999999999999999999999999999999",
      "PRMR-\r\n\t",
      "PRMR-abcdef",
      " PRMR-999999999999999999999999999999999999999999",
      "PRMR-513eef6a-7601-4349-b074-532c3e7c6da3-xyz",
      "PRMR-513eef6a-7601-4349-b074-532c3e7c6da31",
      "PRMR-1513eef6a-7601-4349-b074-532c3e7c6da3",
      "CRDN-513eef6a-7601-4349-b074-532c3e7c6da3-xyz",
      "CRDN-5729b9a3-85a4-4128-a9f5-ec4a9c191e061",
      "CRDN-1eea7b6ec-bcae-409c-afd2-32b33e7f45da",
      "AGNT-b168326c-c546-483f-bb55-66ff7923c964",
  };

  for (auto v : badValues) {
    EXPECT_FALSE(ServerState::isDBServerId(v));
  }
}
