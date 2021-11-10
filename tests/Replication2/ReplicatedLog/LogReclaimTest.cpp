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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <Basics/Exceptions.h>

#include <utility>

#include "Replication2/ReplicatedLog/types.h"
#include "TestHelper.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

TEST_F(ReplicatedLogTest, reclaim_leader_after_term_change) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{2});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader(LogConfig{2, 2, false}, "leader",
                                        LogTerm{1}, {follower});

  auto idx = leader->insert(LogPayload::createFromString("payload"), false,
                            LogLeader::doNotTriggerAsyncReplication);
  auto f = leader->waitFor(idx).then([&](futures::Try<WaitForResult>&& quorum) {
    EXPECT_TRUE(quorum.hasException());
    try {
      quorum.throwIfFailed();
    } catch (basics::Exception const& ex) {
      EXPECT_EQ(ex.code(), TRI_ERROR_REPLICATION_LEADER_CHANGE);
    } catch (...) {
      ADD_FAILURE() << "unexpected exception";
    }

    return leaderLog->getLeader();
  });

  leaderLog->becomeLeader("leader", LogTerm{2}, {follower}, 1);
  ASSERT_TRUE(f.isReady());
  auto newLeader = std::move(f).get();

  ASSERT_NE(newLeader, nullptr);
}

TEST_F(ReplicatedLogTest, reclaim_follower_after_term_change) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{2});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader(LogConfig{2, 2, false}, "leader",
                                        LogTerm{1}, {follower});

  auto idx = leader->insert(LogPayload::createFromString("payload"), false,
                            LogLeader::doNotTriggerAsyncReplication);
  auto f =
      follower->waitFor(idx).then([&](futures::Try<WaitForResult>&& quorum) {
        EXPECT_TRUE(quorum.hasException());
        try {
          quorum.throwIfFailed();
        } catch (basics::Exception const& ex) {
          EXPECT_EQ(ex.code(), TRI_ERROR_REPLICATION_LEADER_CHANGE);
        } catch (std::exception const& e) {
          ADD_FAILURE() << "unexpected exception: " << e.what();
        } catch (...) {
          ADD_FAILURE() << "unexpected exception";
        }

        return leaderLog->getLeader();
      });

  followerLog->becomeLeader("leader", LogTerm{2}, {follower}, 1);
  ASSERT_TRUE(f.isReady());
  auto newLeader = std::move(f).get();

  ASSERT_NE(newLeader, nullptr);
}
