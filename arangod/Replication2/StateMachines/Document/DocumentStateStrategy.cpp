////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "DocumentStateStrategy.h"

#include <velocypack/Builder.h>

#include "Agency/AgencyStrings.h"
#include "Basics/ResultT.h"
#include "Basics/StringUtils.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/CreateCollection.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/MaintenanceStrings.h"
#include "Cluster/ServerState.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb::replication2::replicated_state::document;

DocumentStateAgencyHandler::DocumentStateAgencyHandler(ArangodServer& server,
                                                       AgencyCache& agencyCache)
    : _server(server), _agencyCache(agencyCache){};

auto DocumentStateAgencyHandler::getCollectionPlan(
    std::string const& database, std::string const& collectionId)
    -> std::shared_ptr<VPackBuilder> {
  auto builder = std::make_shared<VPackBuilder>();
  auto path = cluster::paths::aliases::plan()
                  ->collections()
                  ->database(database)
                  ->collection(collectionId);
  _agencyCache.get(*builder, path);

  // TODO better error handling
  // Maybe check the cache again after some time or log additional information
  TRI_ASSERT(!builder->isEmpty());

  return builder;
}

auto DocumentStateAgencyHandler::reportShardInCurrent(
    std::string const& database, std::string const& collectionId,
    std::string const& shardId,
    std::shared_ptr<velocypack::Builder> const& properties) -> Result {
  auto participants = properties->slice().get(maintenance::SHARDS).get(shardId);

  VPackBuilder localShard;
  {
    VPackObjectBuilder ob(&localShard);

    localShard.add(StaticStrings::Error, VPackValue(false));
    localShard.add(StaticStrings::ErrorMessage, VPackValue(std::string()));
    localShard.add(StaticStrings::ErrorNum, VPackValue(0));
    localShard.add(maintenance::SERVERS, participants);
    localShard.add(StaticStrings::FailoverCandidates, participants);
  }

  AgencyOperation op(consensus::CURRENT_COLLECTIONS + database + "/" +
                         collectionId + "/" + shardId,
                     AgencyValueOperationType::SET, localShard.slice());
  AgencyPrecondition pr(consensus::PLAN_COLLECTIONS + database + "/" +
                            collectionId + "/shards/" + shardId,
                        AgencyPrecondition::Type::VALUE, participants);

  AgencyComm comm(_server);
  AgencyWriteTransaction currentTransaction(op, pr);
  AgencyCommResult r = comm.sendTransactionWithFailover(currentTransaction);
  return r.asResult();
}

DocumentStateShardHandler::DocumentStateShardHandler(
    MaintenanceFeature& maintenanceFeature)
    : _maintenanceFeature(maintenanceFeature){};

auto DocumentStateShardHandler::stateIdToShardId(LogId logId) -> std::string {
  return fmt::format("s{}", logId);
}

auto DocumentStateShardHandler::createLocalShard(
    GlobalLogIdentifier const& gid, std::string const& collectionId,
    std::shared_ptr<velocypack::Builder> const& properties)
    -> ResultT<std::string> {
  auto shardId = stateIdToShardId(gid.id);

  // For the moment, use the shard information to get the leader
  auto participants = properties->slice().get(maintenance::SHARDS).get(shardId);
  TRI_ASSERT(participants.isArray());
  auto leaderId = participants[0].toString();
  auto serverId = ServerState::instance()->getId();
  auto shouldBeLeading = leaderId == serverId;

  maintenance::ActionDescription actionDescriptions(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::CREATE_COLLECTION},
          {maintenance::COLLECTION, collectionId},
          {maintenance::SHARD, shardId},
          {maintenance::DATABASE, gid.database},
          {maintenance::SERVER_ID, std::move(serverId)},
          {maintenance::THE_LEADER,
           shouldBeLeading ? "" : std::move(leaderId)}},
      shouldBeLeading ? maintenance::LEADER_PRIORITY
                      : maintenance::FOLLOWER_PRIORITY,
      false, properties);

  maintenance::CreateCollection collectionCreator(_maintenanceFeature,
                                                  actionDescriptions);
  bool work = collectionCreator.first();
  if (work) {
    return ResultT<std::string>::error(
        TRI_ERROR_INTERNAL, fmt::format("Cannot create shard ID {}", shardId));
  }

  return ResultT<std::string>::success(std::move(shardId));
}