////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_REPAIR_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_REPAIR_HANDLER_H

#include "Agency/AgencyComm.h"
#include "Cluster/ClusterRepairs.h"
#include "Basics/ResultT.h"
#include "GeneralServer/AsyncJobManager.h"
#include "RestHandler/RestBaseHandler.h"

namespace arangodb {
namespace rest {
class AsyncJobManager;
}

namespace rest_repair {

enum class JobStatus { todo, finished, pending, failed, missing };

inline char const* toString(JobStatus jobStatus) {
  switch (jobStatus) {
    case JobStatus::todo:
      return "todo";
    case JobStatus::pending:
      return "pending";
    case JobStatus::finished:
      return "finished";
    case JobStatus::failed:
      return "failed";
    case JobStatus::missing:
      return "missing";
    default:
      return "n/a";
  }
}
}  // namespace rest_repair

class RestRepairHandler : public arangodb::RestBaseHandler {
 public:
  RestRepairHandler(application_features::ApplicationServer& server,
                    GeneralRequest* request, GeneralResponse* response);

  char const* name() const override final { return "RestRepairHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }
  RestStatus execute() override;

 private:
  bool _pretendOnly = true;

  bool pretendOnly();

  // @brief Handler for (the currently only) subroute /distributeShardsLike
  // of /_admin/repair. On pretendOnly == true (i.e. GET), calculates all
  // repair operations and returns them. Otherwise (i.e. on POST) calculates
  // all repair operations, executes them and returns them with the result(s).
  RestStatus repairDistributeShardsLike();

  // @brief Executes all operations in `list`. Returns an ok-Result iff all
  // operations executed successfully and a fail-Result otherwise.
  Result executeRepairOperations(DatabaseID const& databaseId, CollectionID const& collectionId,
                                 std::string const& dbAndCollectionName,
                                 std::list<cluster_repairs::RepairOperation> const& list);

  // @brief Gets N values from the agency in a single transaction
  template <std::size_t N>
  ResultT<std::array<cluster_repairs::VPackBufferPtr, N>> getFromAgency(
      std::array<std::string const, N> const& agencyKeyArray);

  // @brief Gets a single value from the agency
  ResultT<cluster_repairs::VPackBufferPtr> getFromAgency(std::string const& agencyKey);

  // @brief Returns the status of the agency job `jobId` (i.e. todo, pending,
  // finished, ...)
  ResultT<rest_repair::JobStatus> getJobStatusFromAgency(std::string const& jobId);

  // @brief Checks if the agency job with id `jobId`is finished.
  ResultT<bool> jobFinished(std::string const& jobId);

  // @brief Executes the operations given by `repairOperations` to repair
  // the collection `collectionId`. Adds information about the planned operation
  // and the result (success or failure and an error message on failure) to
  // `response`.
  // Returns true iff the repairs were successful.
  bool repairCollection(DatabaseID const& databaseId, CollectionID const& collectionId,
                        std::string const& dbAndCollectionName,
                        std::list<cluster_repairs::RepairOperation> const& repairOperations,
                        VPackBuilder& response);

  // @brief Executes the operations given by `repairOperationsByCollection`.
  // Adds information about the planned operation and the result (success or
  // failure and an error message on failure) per collection to `response`.
  // Returns true iff repairs for all collections were successful.
  bool repairAllCollections(
      VPackSlice const& planCollections,
      std::map<CollectionID, ResultT<std::list<cluster_repairs::RepairOperation>>> const& repairOperationsByCollection,
      VPackBuilder& response);

  // @brief Given a collection ID, looks up the name of the containing database
  // and the name of the collection in `planCollections` and returns them as
  // "dbName/collName".
  ResultT<std::string> static getDbAndCollectionName(VPackSlice planCollections,
                                                     CollectionID const& collectionId);

  // @brief Adds the field "errorDetails" with a detailed error message to the
  // open object in builder.
  void addErrorDetails(VPackBuilder& builder, int errorNumber);

  // @brief Answers the question "Is every shard of collectionId replicated to
  // a number of DBServers equal to its replicationFactor?".
  ResultT<bool> checkReplicationFactor(DatabaseID const& databaseId,
                                       CollectionID const& collectionId);

  // @brief Generate an HTTP Response. Like RestBaseHandler::generateOk(),
  // so it adds .error and .code to the object in payload, but allows
  // for .error to be set to true to allow for error responses with payload.
  void generateResult(rest::ResponseCode code, const velocypack::Builder& payload, bool error);
};
}  // namespace arangodb

#endif  // ARANGOD_REST_HANDLER_REST_REPAIR_HANDLER_H
