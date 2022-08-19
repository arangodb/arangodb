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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateAgencyHandler.h"

#include "Agency/AgencyComm.h"
#include "Agency/AgencyPaths.h"
#include "Agency/AgencyStrings.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceStrings.h"

#include "Basics/StaticStrings.h"

namespace arangodb::replication2::replicated_state::document {

// TODO this would do with an AgencyCache, there is no real need for
// ClusterFeature
DocumentStateAgencyHandler::DocumentStateAgencyHandler(
    GlobalLogIdentifier gid, ArangodServer& server,
    ClusterFeature& clusterFeature)
    : _gid(std::move(gid)), _server(server), _clusterFeature(clusterFeature) {}

auto DocumentStateAgencyHandler::getCollectionPlan(
    std::string const& collectionId) -> std::shared_ptr<VPackBuilder> {
  auto builder = std::make_shared<VPackBuilder>();
  auto path = cluster::paths::aliases::plan()
                  ->collections()
                  ->database(_gid.database)
                  ->collection(collectionId);
  _clusterFeature.agencyCache().get(*builder, path);

  ADB_PROD_ASSERT(!builder->isEmpty())
      << "Could not get collection from plan " << path->str();

  return builder;
}

auto DocumentStateAgencyHandler::reportShardInCurrent(
    std::string const& collectionId, std::string const& shardId,
    std::shared_ptr<velocypack::Builder> const& properties) -> Result {
  VPackBuilder localShard;
  {
    VPackObjectBuilder ob(&localShard);

    localShard.add(StaticStrings::Error, VPackValue(false));
    localShard.add(StaticStrings::ErrorMessage, VPackValue(std::string()));
    localShard.add(StaticStrings::ErrorNum, VPackValue(0));
    localShard.add(maintenance::SERVERS, VPackSlice::emptyArraySlice());
    localShard.add(StaticStrings::FailoverCandidates,
                   VPackSlice::emptyArraySlice());
  }

  AgencyOperation op(consensus::CURRENT_COLLECTIONS + _gid.database + "/" +
                         collectionId + "/" + shardId,
                     AgencyValueOperationType::SET, localShard.slice());
  AgencyPrecondition pr(consensus::PLAN_COLLECTIONS + _gid.database + "/" +
                            collectionId + "/shards/" + shardId,
                        AgencyPrecondition::Type::VALUE,
                        VPackSlice::emptyArraySlice());

  AgencyComm comm(_server);
  AgencyWriteTransaction currentTransaction(op, pr);
  AgencyCommResult r = comm.sendTransactionWithFailover(currentTransaction);
  return r.asResult();
}

}  // namespace arangodb::replication2::replicated_state::document
