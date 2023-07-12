////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "DistributeShardsLike.h"

#include "Basics/Exceptions.h"
#include "Basics/ResultT.h"

using namespace arangodb;

DistributeShardsLike::DistributeShardsLike(
    std::function<ResultT<std::vector<ResponsibleServerList>>()>
        getOriginalSharding)
    : _originalShardingProducer{std::move(getOriginalSharding)} {}

Result DistributeShardsLike::planShardsOnServers(
    std::vector<ServerID> availableServers,
    std::unordered_set<ServerID>& serversPlanned) {
  auto nextSharding = _originalShardingProducer();
  if (nextSharding.fail()) {
    return nextSharding.result();
  }

  for (auto const& list : nextSharding.get()) {
    for (auto const& s : list.servers) {
      // Test if server is available:
      if (std::find(availableServers.begin(), availableServers.end(), s) ==
          availableServers.end()) {
        // TODO: Discuss, should we abort on all servers (like here) or should
        // we only abort if the LEADER is not available.
        return {
            TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS,
            "Server: " + s +
                " required to fulfill distributeShardsLike, is not available"};
      }
      // Register Server als planned
      serversPlanned.emplace(s);
    }
  }

  // Sharding Okay, take it
  _shardToServerMapping = std::move(nextSharding.get());
  return {TRI_ERROR_NO_ERROR};
}

#if false
// Precondition for DistributeShardsLike
 auto const otherCidString = basics::VelocyPackHelper::getStringValue(
   info.json, StaticStrings::DistributeShardsLike, StaticStrings::Empty);
if (!otherCidString.empty() &&
   conditions.find(otherCidString) == conditions.end()) {
 // Distribute shards like case.
 // Precondition: Master collection is not moving while we create this
 // collection We only need to add these once for every Master, we cannot
 // add multiple because we will end up with duplicate entries.
 // NOTE: We do not need to add all collections created here, as they will
 // not succeed In callbacks if they are moved during creation. If they are
 // moved after creation was reported success they are under protection by
 // Supervision.
 conditions.emplace(otherCidString);
 if (colToDistributeShardsLike != nullptr) {
   otherCidShardMap = colToDistributeShardsLike->shardIds();
 } else {
   otherCidShardMap =
       getCollection(databaseName, otherCidString)->shardIds();
 }

 auto const dslProtoColPath = paths::root()
                                  ->arango()
                                  ->plan()
                                  ->collections()
                                  ->database(databaseName)
                                  ->collection(otherCidString);
 // The distributeShardsLike prototype collection should exist in the
 // plan...
 precs.emplace_back(AgencyPrecondition(
     dslProtoColPath, AgencyPrecondition::Type::EMPTY, false));
 // ...and should not still be in creation.
 precs.emplace_back(AgencyPrecondition(dslProtoColPath->isBuilding(),
                                       AgencyPrecondition::Type::EMPTY,
                                       true));

 // Any of the shards locked?
 for (auto const& shard : *otherCidShardMap) {
   precs.emplace_back(
       AgencyPrecondition("Supervision/Shards/" + shard.first,
                          AgencyPrecondition::Type::EMPTY, true));
 }

#endif
