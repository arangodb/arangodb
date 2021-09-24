////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Mocks/Servers.h"

#include "Auth/UserManager.h"
#include "GeneralServer/AuthenticationFeature.h"

namespace arangodb {
namespace tests {
namespace auth_info_test {

namespace {
//static char const* FailureOnLoadDB = "UserManager::performDBLookup";
}

class UserManagerClusterTest : public ::testing::Test {
 public:
  mocks::MockCoordinator _server{};

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

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST_F(UserManagerClusterTest, regression_forgotten_update) {
  /* The following order of events did lead to a missing update:
   * 1. um->triggerLocalReload();
   * 2. um->triggerGlobalReload();
   * 3. heartbeat
   * 4. um->loadFromDB();
   *
   * 1. and 2. moved internal versions forward two times
   * 3. heartbeat resets one of the movings
   * 4. Does not perform the actual load, as the heartbeat reset indicates everything is okay.
   */

  TRI_AddFailurePointDebugging(FailureOnLoadDB);
  auto guard = arangodb::scopeGuard(
      []() noexcept { TRI_RemoveFailurePointDebugging(FailureOnLoadDB); });

  auto um = userManager();
  // If for some reason this EXPECT ever triggers, we can
  // inject either the AgencyValue into the UserManager
  // or vice-versa. This is just an assertion that we
  // expect everything to start at default (1).
  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());

  um->triggerLocalReload();
  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());

  um->triggerGlobalReload();
  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());

  /*
   * This is the correct test here, the heartbeat has
   * a side-effect, but that is simply untestable in the
   * current design. So the only thing i could
   * fall back to is to assert that the UserVersions in the agency
   * do stay aligned.
   *
   * this->simulateOneHeartbeat();
   */
  // This needs to trigger a reload from DB
  try {
    um->userExists("unknown user");
    FAIL();
  } catch (arangodb::basics::Exception const& e) {
    // This Execption indicates that we got past the version
    // checks and would contact DBServer now.
    // This is not under test here, we only want to test that we load
    // the plan
    EXPECT_EQ(e.code(), TRI_ERROR_DEBUG);
  } catch (...) {
    FAIL();
  }
}

TEST_F(UserManagerClusterTest, cacheRevalidationShouldKeepVersionsInLine) {
  TRI_AddFailurePointDebugging(FailureOnLoadDB);
  auto guard = arangodb::scopeGuard(
      []() noexcept { TRI_RemoveFailurePointDebugging(FailureOnLoadDB); });

  auto um = userManager();
  // If for some reason this EXPECT ever triggers, we can
  // inject either the AgencyValue into the UserManager
  // or vice-versa. This is just an assertion that we
  // expect everything to start at default (1).
  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());

  try {
    // This needs to trigger a reload from DB
    // Internally it will do local, global, and loadFromDB.
    um->triggerCacheRevalidation();
    FAIL();
  } catch (arangodb::basics::Exception const& e) {
    // This Execption indicates that we got past the version
    // checks and would contact DBServer now.
    // This is not under test here, we only want to test that we load
    // the plan
    EXPECT_EQ(e.code(), TRI_ERROR_DEBUG);
  } catch (...) {
    FAIL();
  }
  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());
}

TEST_F(UserManagerClusterTest, triggerLocalReloadShouldNotUpdateClusterVersion) {
  TRI_AddFailurePointDebugging(FailureOnLoadDB);
  auto guard = arangodb::scopeGuard(
      []() noexcept { TRI_RemoveFailurePointDebugging(FailureOnLoadDB); });

  auto um = userManager();
  // If for some reason this EXPECT ever triggers, we can
  // inject either the AgencyValue into the UserManager
  // or vice-versa. This is just an assertion that we
  // expect everything to start at default (1).
  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());

  uint64_t versionBefore = getAgencyUserVersion();

  um->triggerLocalReload();
  EXPECT_EQ(versionBefore, getAgencyUserVersion());

  /*
   * This is the correct test here, see above
   *
   * this->simulateOneHeartbeat();
   */
  // This needs to trigger a reload from DB
  try {
    um->userExists("unknown user");
    FAIL();
  } catch (arangodb::basics::Exception const& e) {
    // This Execption indicates that we got past the version
    // checks and would contact DBServer now.
    // This is not under test here, we only want to test that we load
    // the plan
    EXPECT_EQ(e.code(), TRI_ERROR_DEBUG);
  } catch (...) {
    FAIL();
  }
}

TEST_F(UserManagerClusterTest, triggerGlobalReloadShouldUpdateClusterVersion) {
  TRI_AddFailurePointDebugging(FailureOnLoadDB);
  auto guard = arangodb::scopeGuard(
      []() noexcept { TRI_RemoveFailurePointDebugging(FailureOnLoadDB); });

  auto um = userManager();
  // If for some reason this EXPECT ever triggers, we can
  // inject either the AgencyValue into the UserManager
  // or vice-versa. This is just an assertion that we
  // expect everything to start at default (1).
  EXPECT_EQ(um->globalVersion(), getAgencyUserVersion());

  uint64_t versionBefore = getAgencyUserVersion();

  um->triggerGlobalReload();
  // The version in the Agency needs to be increased
  EXPECT_LT(versionBefore, getAgencyUserVersion());

  /*
   * This is the correct test here, see above
   *
   * this->simulateOneHeartbeat();
   */
  // This needs to trigger a reload from DB
  try {
    um->userExists("unknown user");
    FAIL();
  } catch (arangodb::basics::Exception const& e) {
    // This Execption indicates that we got past the version
    // checks and would contact DBServer now.
    // This is not under test here, we only want to test that we load
    // the plan
    EXPECT_EQ(e.code(), TRI_ERROR_DEBUG);
  } catch (...) {
    FAIL();
  }
}

#endif

}  // namespace auth_info_test
}  // namespace tests
}  // namespace arangodb
