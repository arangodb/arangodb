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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/TestHelper.h"

#include <Basics/Exceptions.h>
#include <Replication2/ReplicatedLog.h>

#include <gtest/gtest.h>

using namespace arangodb::replication2;

struct LogManagerTest : LogTestBase {};


TEST_F(LogManagerTest, simple_test) {
  auto [leader, localProxy] = addLogInstance("leader");
  leader->becomeLeader(LogTerm{1}, {localProxy}, 1);
  auto local = _manager->getPersistedLogById(localProxy->getLogId());


  auto idx = leader->insert(LogPayload{"first entry"});
  ASSERT_TRUE(_executor->hasPendingActions());
  auto f = leader->waitFor(idx);
  TRI_ASSERT(!f.isReady());
  leader->runAsyncStep();
  _executor->executeAllActions();
  TRI_ASSERT(f.isReady());

  auto entry = std::optional<LogEntry>{};
  auto iter = local->read(LogIndex{0});
  entry = iter->next();
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->logTerm(), LogTerm{1});
  EXPECT_EQ(entry->logIndex(), LogIndex{1});
  EXPECT_EQ(entry->logPayload().dummy, "first entry");

  entry = iter->next();
  ASSERT_FALSE(entry.has_value());
}
