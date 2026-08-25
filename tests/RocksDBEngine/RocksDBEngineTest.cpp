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

#include "RocksDBEngine/StorageEngineFixture.h"

using namespace arangodb;
using namespace arangodb::tests;

TEST_F(StorageEngineFixture, CanConstruct) {
  EXPECT_EQ(engine().kEngineName, "rocksdb");
}

// Own suite, not the shared one - it's already started by TEST_F time.
TEST(RocksDBEngineRecoveryTest, StateSequenceAndRecoveryDoneOnce) {
  using ::testing::Return;
  using ::testing::ReturnRef;

  StorageEngineFixtureSuite suite;
  suite.serverState.setRole(ServerState::ROLE_SINGLE);

  ON_CALL(suite.dumpLimits, limits())
      .WillByDefault(ReturnRef(suite.limitsOptions));
  ON_CALL(suite.flush, isEnabled()).WillByDefault(Return(true));
  ON_CALL(suite.logProvider, options())
      .WillByDefault(Return(suite.logSettings));
  ON_CALL(suite.dbProvider, defaultReplicationVersion())
      .WillByDefault(Return(replication::Version::ONE));

  RocksDBEngine::cleanupStaleRecoveryHelpers();

  EXPECT_EQ(suite.engine.engineState(), EngineState::kPreRecovery);

  EXPECT_CALL(suite.dbProvider, recoveryDone()).Times(1);

  suite.engine.prepare();
  suite.engine.start();

  EXPECT_EQ(suite.engine.engineState(), EngineState::kRunning);

  suite.server.beginShutdown();
  suite.engine.beginShutdown();
  suite.engine.stop();
  suite.engine.unprepare();
}
