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

#include "StateMachineTestHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <Replication2/Mocks/ReplicatedLogMetricsMock.h>
#include "Replication2/Mocks/PersistedLog.h"

void arangodb::TestLogEntry::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add("payload", velocypack::Value(payload));
}

auto arangodb::TestLogEntry::fromVelocyPack(arangodb::velocypack::Slice slice)
    -> arangodb::TestLogEntry {
  return TestLogEntry(slice.get("payload").copyString());
}

#include "Replication2/ReplicatedState/AbstractStateMachine.tpp"

template struct replicated_state::AbstractStateMachine<TestLogEntry>;

auto StateMachineTest::createReplicatedLog()
    -> std::shared_ptr<replication2::replicated_log::ReplicatedLog> {
  auto persisted = std::make_shared<test::MockLog>(LogId{0});
  auto core = std::make_unique<replicated_log::LogCore>(persisted);
  auto metrics = std::make_shared<ReplicatedLogMetricsMock>();
  return std::make_shared<replicated_log::ReplicatedLog>(std::move(core), metrics, LoggerContext(Logger::REPLICATION2));
}
