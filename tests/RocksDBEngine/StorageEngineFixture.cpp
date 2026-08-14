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

#include "RocksDBEngine/StorageEngineFixture.h"

#include "Replication2/Version.h"

namespace arangodb::tests {

std::unique_ptr<StorageEngineFixtureSuite> makeStartedSuite(bool timeTravel) {
  auto suite = std::make_unique<StorageEngineFixtureSuite>(timeTravel);
  suite->serverState.setRole(ServerState::ROLE_SINGLE);

  using ::testing::Return;
  using ::testing::ReturnRef;
  ON_CALL(suite->dumpLimits, limits())
      .WillByDefault(ReturnRef(suite->limitsOptions));
  ON_CALL(suite->flush, isEnabled()).WillByDefault(Return(true));
  ON_CALL(suite->logProvider, options())
      .WillByDefault(Return(suite->logSettings));
  ON_CALL(suite->dbProvider, defaultReplicationVersion())
      .WillByDefault(Return(replication::Version::ONE));

  suite->engine.start();
  return suite;
}

void stopSuite(std::unique_ptr<StorageEngineFixtureSuite>& suite) {
  suite->server.beginShutdown();
  suite->engine.beginShutdown();
  suite->engine.stop();
  suite->engine.unprepare();
  suite.reset();
}

StorageEngineFixtureSuite::~StorageEngineFixtureSuite() {
  while (!scheduler.queueEmpty()) {
    scheduler.runOnce();
  }
}

}  // namespace arangodb::tests
