////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/LeaseManager.h"
#include "Cluster/ClusterTypes.h"

using namespace arangodb;
using namespace arangodb::cluster;

class LeaseManagerTest : public ::testing::Test {};

TEST_F(LeaseManagerTest, test_can_require_a_lease) {
  LeaseManager leaseManager;
  PeerState leaseIsFor{
      .serverId = "PRMR_1"
  };
  auto guardOne = leaseManager.requireLease(leaseIsFor);
  auto guardTwo = leaseManager.requireLease(leaseIsFor);
  ASSERT_NE(guardOne.id(), guardTwo.id());
}