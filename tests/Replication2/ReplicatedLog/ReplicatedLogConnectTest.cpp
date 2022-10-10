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
#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"
#include "Replication2/Mocks/ReplicatedLogMetricsMock.h"

using namespace arangodb;
using namespace arangodb::replication2;

namespace arangodb::replication2::test {
struct FakeFollowerFactory : replicated_log::IAbstractFollowerFactory {
  auto constructFollower(const ParticipantId& id)
      -> std::shared_ptr<replicated_log::AbstractFollower> override {
    return std::shared_ptr<replicated_log::AbstractFollower>();
  }
};

}  // namespace arangodb::replication2::test

struct ReplicatedLogTest2 : ::testing::Test {
  std::shared_ptr<test::FakeStorageEngineMethodsContext> storageContext =
      std::make_shared<test::FakeStorageEngineMethodsContext>(
          12, LogId{12}, std::make_shared<test::ThreadAsyncExecutor>());
  std::unique_ptr<replicated_state::IStorageEngineMethods> methods =
      storageContext->getMethods();
  std::unique_ptr<replicated_log::LogCore> core =
      std::make_unique<replicated_log::LogCore>(*methods);
  std::shared_ptr<ReplicatedLogMetricsMock> logMetricsMock =
      std::make_shared<ReplicatedLogMetricsMock>();
  std::shared_ptr<ReplicatedLogGlobalSettings> optionsMock =
      std::make_shared<ReplicatedLogGlobalSettings>();
  LoggerContext loggerContext = LoggerContext(Logger::REPLICATION2);
  agency::ServerInstanceReference const myself = {"SELF", RebootId{1}};
  std::shared_ptr<replicated_log::IAbstractFollowerFactory> followerFactory =
      std::make_shared<test::FakeFollowerFactory>();
};

TEST_F(ReplicatedLogTest2, foo_bar) {
  auto log = std::make_shared<replicated_log::ReplicatedLog>(
      std::move(core), logMetricsMock, optionsMock, followerFactory,
      loggerContext, myself);
}
