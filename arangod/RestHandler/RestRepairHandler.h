////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB3_RESTREPAIRHANDLER_H
#define ARANGODB3_RESTREPAIRHANDLER_H

#include "Agency/AgencyComm.h"
#include "Cluster/ClusterRepairs.h"
#include "GeneralServer/AsyncJobManager.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {
namespace rest {
class AsyncJobManager;
}

namespace rest_repair {

enum class JobStatus { todo, finished, pending, failed, missing };

inline char const* toString(JobStatus jobStatus) {
  return jobStatus == JobStatus::todo
             ? "todo"
             : jobStatus == JobStatus::pending
                   ? "pending"
                   : jobStatus == JobStatus::finished
                         ? "finished"
                         : jobStatus == JobStatus::failed
                               ? "failed"
                               : jobStatus == JobStatus::missing ? "missing"
                                                                 : "n/a";
}
}

class RestRepairHandler : public arangodb::RestBaseHandler {
 public:
  RestRepairHandler(GeneralRequest* request, GeneralResponse* response);

  char const* name() const final { return "RestDemoHandler"; }

  bool isDirect() const override {
    // TODO does this do what I think it does?
    return false;
  }

  RestStatus execute() override;

  // TODO maybe implement cancel() ?

 private:
  bool _pretendOnly = true;

  bool pretendOnly();

  RestStatus repairDistributeShardsLike();

  Result executeRepairOperations(
      DatabaseID const& databaseId, CollectionID const& collectionId,
      std::list<cluster_repairs::RepairOperation> const& list);

  template <std::size_t N>
  cluster_repairs::ResultT<std::array<cluster_repairs::VPackBufferPtr, N>>
  getFromAgency(std::array<std::string const, N> const& agencyKeyArray);

  cluster_repairs::ResultT<cluster_repairs::VPackBufferPtr> getFromAgency(
      std::string const& agencyKey);

  cluster_repairs::ResultT<rest_repair::JobStatus> getJobStatusFromAgency(
      std::string const& jobId);

  cluster_repairs::ResultT<bool> jobFinished(std::string const& jobId);

  bool repairCollection(
      DatabaseID const& databaseId, CollectionID const& collectionId,
      std::list<cluster_repairs::RepairOperation> const& repairOperations,
      VPackBuilder& response);

  bool repairAllCollections(
      VPackSlice const& planCollections,
      std::map<CollectionID, cluster_repairs::ResultT<std::list<
                                 cluster_repairs::RepairOperation>>> const&
          repairOperationsByCollection,
      VPackBuilder& response);

  cluster_repairs::ResultT<std::string> static getDbAndCollectionName(
      VPackSlice planCollections, CollectionID const& collectionId);

  void addErrorDetails(VPackBuilder& builder, int errorNumber);

  cluster_repairs::ResultT<bool> checkReplicationFactor(
      DatabaseID const& databaseId, CollectionID const& collectionId);
};
}

#endif  // ARANGODB3_RESTREPAIRHANDLER_H
