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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Mocks/Servers.h"

#include "Agency/AgencyComm.h"
#include "Auth/UserManagerImpl.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"

namespace arangodb {
namespace tests {
namespace auth_info_test {

#ifdef ARANGODB_ENABLE_FAILURE_TESTS

class UserManagerClusterTest : public ::testing::Test {
 public:
  UserManagerClusterTest() {
    TRI_AddFailurePointDebugging("UserManager::performDBLookup");
    auto& auth = _server.getFeature<AuthenticationFeature>();
    // we are testing the proper implementation of the userManager, not the
    // mock.
    auth.setUserManager(
        std::make_unique<auth::UserManagerImpl>(_server.server()));
    auto* um = auth.userManager();
    um->loadUserCacheAndStartUpdateThread();
  }

  ~UserManagerClusterTest() {
    auto um = _server.getFeature<AuthenticationFeature>().userManager();
    um->shutdown();
    TRI_RemoveFailurePointDebugging("UserManager::performDBLookup");
  }
  mocks::MockCoordinator _server{"CRDN_0001"};

 protected:
  auth::UserManager* userManager() {
    auto um = _server.getFeature<AuthenticationFeature>().userManager();
    TRI_ASSERT(um != nullptr);
    return um;
  }

  void simulateOneHeartbeat() {
    /*
     * NOTE: Sorry i gave up on this. Something that
     * requires the heartbeat is absolutely untestable
     * it does everything at once, and you need to setup
     * a complete functioning world to somehow execute it.
     * Furthermore it has tons of undesired side-effects.
     *
     * All i wanted to do, is let the heartbeat detect
     * the UserVersion and inject it the way it does into
     * the UserManager.
     */
  }

  uint64_t getAgencyUserVersion() {
    // Simply read the UserVersion from the Agency.
    // This is copy pasted from HeartbeatThread.
    auto& cache = _server.getFeature<ClusterFeature>().agencyCache();
    auto [acb, idx] = cache.read(
        std::vector<std::string>{AgencyCommHelper::path("Sync/UserVersion")});
    auto result = acb->slice();
    VPackSlice slice = result[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Sync", "UserVersion"}));
    TRI_ASSERT(slice.isInteger());
    // there is a UserVersion, and it has to be an UINT
    return slice.getUInt();
  }
};

TEST_F(UserManagerClusterTest, cacheRevalidationShouldKeepVersionsInLine) {
  auto um = userManager();

  // If for some reason this EXPECT ever triggers, we can
  // inject either the AgencyValue into the UserManager
  // or vice versa. This is just an assertion that we
  // expect everything to start at default (1).
  auto const firstGlobalVersion = um->globalVersion();
  EXPECT_EQ(firstGlobalVersion, getAgencyUserVersion());

  // This needs to trigger a reload from DB
  // Internally it will do globalReload, setGlobalVersion, and blocks
  // until the internal thread synchronizes the versions (internal and global).
  auto const internalVersionBeforeReload = um->internalVersion();
  um->triggerCacheRevalidation();

  // we returned here, so we expect the global and internal version
  // to be increased and equal
  EXPECT_GT(um->globalVersion(), firstGlobalVersion);
  EXPECT_GT(um->globalVersion(), internalVersionBeforeReload);

  EXPECT_GT(um->internalVersion(), firstGlobalVersion);
  EXPECT_GT(um->internalVersion(), internalVersionBeforeReload);

  EXPECT_EQ(um->globalVersion(), um->internalVersion());

  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());
}

TEST_F(UserManagerClusterTest, triggerGlobalReloadShouldUpdateClusterVersion) {
  auto um = userManager();
  // If for some reason this EXPECT ever triggers, we can
  // inject either the AgencyValue into the UserManager
  // or vice versa. This is just an assertion that we
  // expect everything to start at default (1).
  uint64_t const versionBeforeGlobalReload = getAgencyUserVersion();
  EXPECT_EQ(um->globalVersion(), versionBeforeGlobalReload);

  um->triggerGlobalReload();

  auto const versionAfterGlobalReload = getAgencyUserVersion();

  // The version in the Agency needs to be increased
  EXPECT_LT(versionBeforeGlobalReload, versionAfterGlobalReload);

  // before the heartbeat we internally still have the state of
  // global & internal version being equal, because no-one yet gave the new
  // agency version to the user-manager
  EXPECT_EQ(um->globalVersion(), versionBeforeGlobalReload);
  EXPECT_EQ(um->internalVersion(), versionBeforeGlobalReload);

  // Simulate heart beat
  // This internally will increase the globalVersion and trigger the
  // UpdateThread to start and preload the user cache
  um->setGlobalVersion(versionAfterGlobalReload);

  // but this call to setGlobalVersion is not blocking, so we need to wait here
  // for the internal version to be updated
  auto const start = std::chrono::system_clock::now();
  auto now = std::chrono::system_clock::now();
  while (now - start < std::chrono::seconds(5)) {
    if (versionAfterGlobalReload <= um->internalVersion()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    now = std::chrono::system_clock::now();
  }

  // should now have parity between the internal, global and agency version
  // but it should not have touched the agency version
  EXPECT_EQ(versionAfterGlobalReload, getAgencyUserVersion());
  EXPECT_EQ(um->globalVersion(), versionAfterGlobalReload);
  EXPECT_EQ(um->internalVersion(), versionAfterGlobalReload);
}

#endif

}  // namespace auth_info_test
}  // namespace tests
}  // namespace arangodb
