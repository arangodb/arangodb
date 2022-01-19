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

#include "FakeReplicatedLog.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

auto TestReplicatedLog::becomeFollower(ParticipantId const& id, LogTerm term,
                                       ParticipantId leaderId)
    -> std::shared_ptr<DelayedFollowerLog> {
  auto ptr = ReplicatedLog::becomeFollower(id, term, std::move(leaderId));
  return std::make_shared<DelayedFollowerLog>(ptr);
}

auto TestReplicatedLog::becomeLeader(
    ParticipantId const& id, LogTerm term,
    std::vector<std::shared_ptr<replicated_log::AbstractFollower>> const&
        follower,
    std::size_t writeConcern) -> std::shared_ptr<replicated_log::LogLeader> {
  LogConfig config;
  config.writeConcern = writeConcern;
  config.waitForSync = false;
  config.softWriteConcern = writeConcern;
  config.replicationFactor = follower.size();
  return becomeLeader(config, id, term, follower);
}
