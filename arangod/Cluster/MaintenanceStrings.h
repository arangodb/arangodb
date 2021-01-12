////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CLUSTER_MAINTENANCE_STRINGS_H
#define ARANGODB_CLUSTER_MAINTENANCE_STRINGS_H

namespace arangodb {
namespace maintenance {

constexpr char const* ACTIONS = "actions";
constexpr char const* AGENCY = "agency";
constexpr char const* CACHE_ENABLED = "cacheEnabled";
constexpr char const* COLLECTION = "collection";
constexpr char const* CREATE_COLLECTION = "CreateCollection";
constexpr char const* CREATE_DATABASE = "CreateDatabase";
constexpr char const* DATABASE = "database";
constexpr char const* DROP_COLLECTION = "DropCollection";
constexpr char const* DROP_DATABASE = "DropDatabase";
constexpr char const* DROP_INDEX = "DropIndex";
constexpr char const* ENSURE_INDEX = "EnsureIndex";
constexpr char const* FIELDS = "fields";
constexpr char const* FOLLOWER_ID = "followerId";
constexpr char const* FOLLOWERS_TO_DROP = "followersToDrop";
constexpr char const* GLOB_UID = "globallyUniqueId";
constexpr char const* ID = "id";
constexpr char const* INDEX = "index";
constexpr char const* INDEXES = "indexes";
constexpr char const* KEY = "key";
constexpr char const* LOCAL_LEADER = "localLeader";
constexpr char const* NAME = "name";
constexpr char const* OBJECT_ID = "objectId";
constexpr char const* OLD_CURRENT_COUNTER = "oldCurrentCounter";
constexpr char const* OP = "op";
constexpr char const* PHASE_ONE = "phaseOne";
constexpr char const* PHASE_TWO = "phaseTwo";
constexpr char const* PLAN_RAFT_INDEX = "planRaftIndex";
constexpr char const* RESIGN_SHARD_LEADERSHIP = "ResignShardLeadership";
constexpr char const* SCHEMA = "schema";
constexpr char const* SELECTIVITY_ESTIMATE = "selectivityEstimate";
constexpr char const* SERVERS = "servers";
constexpr char const* SERVER_ID = "serverId";
constexpr char const* SHARD = "shard";
constexpr char const* SHARDS = "shards";
constexpr char const* SHARD_VERSION = "shardVersion";
constexpr char const* SYNCHRONIZE_SHARD = "SynchronizeShard";
constexpr char const* TAKEOVER_SHARD_LEADERSHIP = "TakeoverShardLeadership";
constexpr char const* THE_LEADER = "theLeader";
constexpr char const* UNDERSCORE = "_";
constexpr char const* UPDATE_COLLECTION = "UpdateCollection";
constexpr char const* WAIT_FOR_SYNC = "waitForSync";
constexpr char const* LEADER_NOT_YET_KNOWN = "NOT_YET_TOUCHED";

}  // namespace maintenance
}  // namespace arangodb

#endif
