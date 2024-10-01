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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb::maintenance {

constexpr char const* ACTIONS = "actions";
constexpr char const* AGENCY = "agency";
constexpr char const* COLLECTION = "collection";
constexpr char const* CREATE_COLLECTION = "CreateCollection";
constexpr char const* CREATE_DATABASE = "CreateDatabase";
constexpr char const* DATABASE = "database";
constexpr char const* DROP_COLLECTION = "DropCollection";
constexpr char const* DROP_DATABASE = "DropDatabase";
constexpr char const* DROP_INDEX = "DropIndex";
constexpr char const* ENSURE_INDEX = "EnsureIndex";
constexpr char const* FIELDS = "fields";
constexpr char const* FORCED_RESYNC = "forcedResync";
constexpr char const* FOLLOWERS_TO_DROP = "followersToDrop";
constexpr char const* FOLLOWER_ID = "followerId";
constexpr char const* GROUP_ID = "groupId";
constexpr char const* ID = "id";
constexpr char const* INDEX = "index";
constexpr char const* INDEXES = "indexes";
constexpr char const* KEY = "key";
constexpr char const* LEADER_NOT_YET_KNOWN = "NOT_YET_TOUCHED";
constexpr char const* LOCAL_LEADER = "localLeader";
constexpr char const* NAME = "name";
constexpr char const* OLD_CURRENT_COUNTER = "oldCurrentCounter";
constexpr char const* OP = "op";
constexpr char const* PHASE_ONE = "phaseOne";
constexpr char const* PHASE_TWO = "phaseTwo";
constexpr char const* PLAN_RAFT_INDEX = "planRaftIndex";
constexpr char const* PLAN_ID = "planId";
constexpr char const* REPLICATED_LOG_ID = "replicatedLogId";
constexpr char const* REPLICATED_LOG_SPEC = "replicatedLogSpec";
constexpr char const* REPLICATED_STATE_ID = "replicatedStateId";
constexpr char const* RESIGN_SHARD_LEADERSHIP = "ResignShardLeadership";
constexpr char const* SELECTIVITY_ESTIMATE = "selectivityEstimate";
constexpr char const* SERVERS = "servers";
constexpr char const* SERVER_ID = "serverId";
constexpr char const* SHARD = "shard";
constexpr char const* SHARDS = "shards";
constexpr char const* SHARD_VERSION = "shardVersion";
constexpr char const* SYNCHRONIZE_SHARD = "SynchronizeShard";
constexpr char const* SYNC_BY_REVISION = "syncByRevision";
constexpr char const* TAKEOVER_SHARD_LEADERSHIP = "TakeoverShardLeadership";
constexpr char const* THE_LEADER = "theLeader";
constexpr char const* UNDERSCORE = "_";
constexpr char const* UPDATE_COLLECTION = "UpdateCollection";
constexpr char const* UPDATE_REPLICATED_LOG = "UpdateReplicatedLog";
constexpr char const* WAIT_FOR_SYNC = "waitForSync";

}  // namespace arangodb::maintenance
