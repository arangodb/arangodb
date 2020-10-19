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

#include "RestRepairHandler.h"

#include <array>
#include <chrono>
#include <list>
#include <thread>
#include <valarray>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AsyncJobManager.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::rest_repair;
using namespace arangodb::cluster_repairs;

RestRepairHandler::RestRepairHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestRepairHandler::execute() {
  if (server().isStopping()) {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_SHUTTING_DOWN);
    return RestStatus::DONE;
  }

  switch (_request->requestType()) {
    case rest::RequestType::POST:
      _pretendOnly = false;
      break;
    case rest::RequestType::GET:
      _pretendOnly = true;
      break;
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    (int)rest::ResponseCode::METHOD_NOT_ALLOWED);

      return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Bad parameter: expected 'distributeShardsLike', got none");

    return RestStatus::DONE;
  }

  if (suffixes[0] != "distributeShardsLike") {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Bad parameter: expected 'distributeShardsLike', got '" +
                      suffixes[0] + "'");

    return RestStatus::DONE;
  }

  return repairDistributeShardsLike();
}

RestStatus RestRepairHandler::repairDistributeShardsLike() {
  if (ServerState::instance()->isSingleServer()) {
    LOG_TOPIC("aa557", ERR, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::repairDistributeShardsLike: "
        << "Called on single server; this only makes sense in cluster mode";

    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "Only useful in cluster mode.");

    return RestStatus::DONE;
  }

  try {
    rest::ResponseCode responseCode = rest::ResponseCode::OK;

    if (!server().hasFeature<ClusterFeature>()) {
      LOG_TOPIC("b57dc", ERR, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::repairDistributeShardsLike: "
          << "No ClusterInfo instance";
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR);

      return RestStatus::DONE;
    }
    ClusterInfo& clusterInfo = server().getFeature<ClusterFeature>().clusterInfo();
    AgencyCache& agencyCache = server().getFeature<ClusterFeature>().agencyCache();

    auto waitForNewPlan = [&clusterInfo] {
      using namespace std::chrono_literals;
      auto fRes = clusterInfo.fetchAndWaitForPlanVersion(10.0s);
      // Note that get() might throw
      return fRes.get();
    };

    if (auto res = waitForNewPlan(); !res.ok()) {
      generateError(res);
      return RestStatus::DONE;
    }

    auto [b,i] = agencyCache.get("arango/Plan");
    VPackSlice plan = b->slice().get(std::vector<std::string>{AgencyCommHelper::path(),"Plan"});
    VPackSlice planCollections = plan.get("Collections");

    ResultT<VPackBufferPtr> healthResult = getFromAgency("Supervision/Health");

    if (healthResult.fail()) {
      LOG_TOPIC("afb6f", ERR, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::repairDistributeShardsLike: "
          << "Failed to fetch server health result";
      generateError(rest::ResponseCode::SERVER_ERROR,
                    healthResult.errorNumber(), healthResult.errorMessage());

      return RestStatus::DONE;
    }

    VPackSlice supervisionHealth(healthResult.get()->data());

    ResultT<std::map<CollectionID, ResultT<std::list<RepairOperation>>>> repairOperationsByCollectionResult =
        DistributeShardsLikeRepairer::repairDistributeShardsLike(planCollections, supervisionHealth);

    if (repairOperationsByCollectionResult.fail()) {
      LOG_TOPIC("38b6a", ERR, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::repairDistributeShardsLike: "
          << "Error during preprocessing: "
          << "[" << repairOperationsByCollectionResult.errorNumber() << "] "
          << repairOperationsByCollectionResult.errorMessage();
      generateError(rest::ResponseCode::SERVER_ERROR,
                    repairOperationsByCollectionResult.errorNumber(),
                    repairOperationsByCollectionResult.errorMessage());

      return RestStatus::DONE;
    }
    std::map<CollectionID, ResultT<std::list<RepairOperation>>>& repairOperationsByCollection =
        repairOperationsByCollectionResult.get();

    VPackBuilder response;
    response.add(VPackValue(VPackValueType::Object));

    bool errorOccurred = true;

    if (repairOperationsByCollection.empty()) {
      response.add("message", VPackValue("Nothing to do."));
      errorOccurred = false;
    } else {
      std::stringstream message;
      message << "Repairing " << repairOperationsByCollection.size() << " collections";

      response.add("collections", VPackValue(VPackValueType::Object));

      bool allCollectionsSucceeded =
          repairAllCollections(planCollections, repairOperationsByCollection, response);

      if (!allCollectionsSucceeded) {
        responseCode = rest::ResponseCode::SERVER_ERROR;
      }

      errorOccurred = !allCollectionsSucceeded;

      response.close();
    }

    response.close();

    generateResult(responseCode, response, errorOccurred);

    if (auto res = waitForNewPlan(); !res.ok()) {
      LOG_TOPIC("293c5", WARN, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::repairDistributeShardsLike: "
          << "failed to wait for new plan version after successful operation: "
          << res.errorMessage();
      return RestStatus::DONE;
    }
  } catch (basics::Exception const& e) {
    LOG_TOPIC("78521", ERR, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::repairDistributeShardsLike: "
        << "Caught exception: " << e.message();
    generateError(rest::ResponseCode::SERVER_ERROR, e.code());
  }
  return RestStatus::DONE;
}

bool RestRepairHandler::repairAllCollections(
    VPackSlice const& planCollections,
    std::map<CollectionID, ResultT<std::list<RepairOperation>>> const& repairOperationsByCollection,
    VPackBuilder& response) {
  bool allCollectionsSucceeded = true;

  std::unordered_map<CollectionID, DatabaseID> databaseByCollectionId;

  for (auto const& dbIt : VPackObjectIterator(planCollections)) {
    DatabaseID database = dbIt.key.copyString();
    for (auto const& colIt : VPackObjectIterator(dbIt.value)) {
      CollectionID collectionId = colIt.key.copyString();
      databaseByCollectionId[collectionId] = database;
    }
  }

  for (auto const& it : repairOperationsByCollection) {
    CollectionID collectionId = it.first;
    DatabaseID databaseId = databaseByCollectionId.at(collectionId);
    auto repairOperationsResult = it.second;
    auto nameResult = getDbAndCollectionName(planCollections, collectionId);
    if (nameResult.fail()) {
      // This should never happen.
      allCollectionsSucceeded = false;
      response.add(StaticStrings::Error, VPackValue(true));
      response.add(StaticStrings::ErrorMessage, VPackValue(nameResult.errorMessage()));
      continue;
    }

    std::string name = nameResult.get();
    response.add(name, VPackValue(VPackValueType::Object));

    bool success;
    if (repairOperationsResult.ok()) {
      success = this->repairCollection(databaseId, collectionId, name,
                                       repairOperationsResult.get(), response);
    } else {
      response.add(StaticStrings::ErrorMessage,
                   VPackValue(repairOperationsResult.errorMessage()));
      this->addErrorDetails(response, repairOperationsResult.errorNumber());
      success = false;
    }
    response.add(StaticStrings::Error, VPackValue(!success));

    allCollectionsSucceeded = success && allCollectionsSucceeded;

    response.close();
  }

  return allCollectionsSucceeded;
}

bool RestRepairHandler::repairCollection(DatabaseID const& databaseId,
                                         CollectionID const& collectionId,
                                         std::string const& dbAndCollectionName,
                                         std::list<RepairOperation> const& repairOperations,
                                         VPackBuilder& response) {
  bool success = true;

  response.add("PlannedOperations", VPackValue(velocypack::ValueType::Array));

  for (auto const& op : repairOperations) {
    RepairOperationToVPackVisitor addToVPack(response);

    std::visit(addToVPack, op);
  }

  response.close();

  if (!pretendOnly()) {
    Result result = executeRepairOperations(databaseId, collectionId,
                                            dbAndCollectionName, repairOperations);
    if (result.fail()) {
      success = false;
      response.add(StaticStrings::ErrorMessage, VPackValue(result.errorMessage()));
      addErrorDetails(response, result.errorNumber());
    }
  }

  return success;
}

ResultT<bool> RestRepairHandler::jobFinished(std::string const& jobId) {
  LOG_TOPIC("28146", TRACE, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::jobFinished: "
      << "Fetching job info of " << jobId;
  ResultT<JobStatus> jobStatus = getJobStatusFromAgency(jobId);

  if (jobStatus.ok()) {
    LOG_TOPIC("3ec47", DEBUG, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::jobFinished: "
        << "Job status is: " << toString(jobStatus.get());

    switch (jobStatus.get()) {
      case JobStatus::todo:
      case JobStatus::pending:
        break;

      case JobStatus::finished:
        return ResultT<bool>::success(true);
        break;

      case JobStatus::failed:
        LOG_TOPIC("9abf7", ERR, arangodb::Logger::CLUSTER)
            << "RestRepairHandler::jobFinished: "
            << "Job " << jobId << " failed, aborting";

        return ResultT<bool>::error(TRI_ERROR_CLUSTER_REPAIRS_JOB_FAILED);

      case JobStatus::missing:
        LOG_TOPIC("24862", ERR, arangodb::Logger::CLUSTER)
            << "RestRepairHandler::jobFinished: "
            << "Job " << jobId << " went missing, aborting";

        return ResultT<bool>::error(TRI_ERROR_CLUSTER_REPAIRS_JOB_DISAPPEARED);
    }

  } else {
    LOG_TOPIC("36d07", INFO, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::jobFinished: "
        << "Failed to get job status: "
        << "[" << jobStatus.errorNumber() << "] " << jobStatus.errorMessage();

    return ResultT<bool>::error(std::move(jobStatus).result());
  }

  return ResultT<bool>::success(false);
}

Result RestRepairHandler::executeRepairOperations(DatabaseID const& databaseId,
                                                  CollectionID const& collectionId,
                                                  std::string const& dbAndCollectionName,
                                                  std::list<RepairOperation> const& repairOperations) {
  AgencyComm comm(server());

  size_t opNum = 0;
  for (auto& op : repairOperations) {
    opNum += 1;
    auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
    auto visitor = RepairOperationToTransactionVisitor(ci);
    auto trxJobPair = std::visit(visitor, op);

    AgencyWriteTransaction& wtrx = trxJobPair.first;
    std::optional<uint64_t> waitForJobId = trxJobPair.second;

    LOG_TOPIC("6f32d", DEBUG, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::executeRepairOperations: "
        << "Sending a transaction to the agency";

    AgencyCommResult result = comm.sendTransactionWithFailover(wtrx);

    if (!result.successful()) {
      std::stringstream errMsg;
      errMsg << "Failed to send and execute operation. "
             << "Agency error: "
             << "[" << result.errorCode() << "] `" << result.errorMessage()
             << "' during operation#" << opNum << ": " << op;

      LOG_TOPIC("948df", ERR, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::executeRepairOperations: " << errMsg.str();

      return Result(TRI_ERROR_CLUSTER_REPAIRS_OPERATION_FAILED, errMsg.str());
    }

    TRI_IF_FAILURE("RestRepairHandler::executeRepairOperations") {
      std::string failOnSuffix{"---fail_on_operation_nr-"};
      failOnSuffix.append(std::to_string(opNum));
      if (StringUtils::isSuffix(dbAndCollectionName, failOnSuffix)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    // If the transaction posted a job, we wait for it to finish.
    if (waitForJobId) {
      LOG_TOPIC("e6252", DEBUG, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::executeRepairOperations: "
          << "Waiting for job " << waitForJobId.value();
      bool previousJobFinished = false;
      std::string jobId = std::to_string(waitForJobId.value());

      while (!previousJobFinished) {
        ResultT<bool> jobFinishedResult = jobFinished(jobId);
        if (jobFinishedResult.fail()) {
          return std::move(jobFinishedResult).result();
        }
        previousJobFinished = jobFinishedResult.get();

        LOG_TOPIC("daa02", TRACE, arangodb::Logger::CLUSTER)
            << "RestRepairHandler::executeRepairOperations: "
            << "Sleeping for 1s (still waiting for job)";
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      LOG_TOPIC("7a12d", DEBUG, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::executeRepairOperations: "
          << "Waiting for replicationFactor to match";

      bool replicationFactorMatches = false;

      while (!replicationFactorMatches) {
        ResultT<bool> checkReplicationFactorResult =
            checkReplicationFactor(databaseId, collectionId);

        if (checkReplicationFactorResult.fail()) {
          return std::move(checkReplicationFactorResult).result();
        }
        replicationFactorMatches = checkReplicationFactorResult.get();

        LOG_TOPIC("8a25f", TRACE, arangodb::Logger::CLUSTER)
            << "RestRepairHandler::executeRepairOperations: "
            << "Sleeping for 1s (still waiting for replicationFactor to match)";
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  }

  return Result();
}

template <std::size_t N>
ResultT<std::array<VPackBufferPtr, N>> RestRepairHandler::getFromAgency(
    std::array<std::string const, N> const& agencyKeyArray) {
  std::array<VPackBufferPtr, N> resultArray;

  AgencyComm agency(server());

  std::vector<std::string> paths;

  // apply AgencyCommHelper::path on every element and copy to vector
  std::transform(agencyKeyArray.begin(), agencyKeyArray.end(),
                 std::back_inserter(paths), [](std::string const& key) {
                   return AgencyCommHelper::path(key);
                 });

  AgencyCommResult result =
      agency.sendTransactionWithFailover(AgencyReadTransaction{std::move(paths)});

  for (size_t i = 0; i < N; i++) {
    std::string const& agencyKey = agencyKeyArray[i];

    if (!result.successful()) {
      LOG_TOPIC("07263", WARN, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::getFromAgency: "
          << "Getting value from agency failed with: " << result.errorMessage();
      generateError(rest::ResponseCode::SERVER_ERROR, result.errorCode(),
                    result.errorMessage());

      return ResultT<std::array<VPackBufferPtr, N>>::error(result.errorCode(),
                                                           result.errorMessage());
    }

    std::vector<std::string> agencyPath =
        basics::StringUtils::split(AgencyCommHelper::path(agencyKey), '/');

    agencyPath.erase(std::remove(agencyPath.begin(), agencyPath.end(), ""),
                     agencyPath.end());

    VPackBuilder builder;

    builder.add(result.slice()[0].get(agencyPath));

    resultArray[i] = builder.steal();
  }

  return ResultT<std::array<VPackBufferPtr, N>>::success(resultArray);
}

ResultT<VPackBufferPtr> RestRepairHandler::getFromAgency(std::string const& agencyKey) {
  ResultT<std::array<VPackBufferPtr, 1>> rv = getFromAgency<1>({{agencyKey}});

  if (rv.ok()) {
    return ResultT<VPackBufferPtr>::success(rv.get()[0]);
  } else {
    return ResultT<VPackBufferPtr>(std::move(rv).result());
  }
}

ResultT<JobStatus> RestRepairHandler::getJobStatusFromAgency(std::string const& jobId) {
  // As long as getFromAgency doesn't get all values at once, the order here
  // matters: if e.g. finished was checked before pending, this would
  // introduce a race condition which would result in JobStatus::missing
  // despite it being finished.
  auto rv = getFromAgency<4>({{"Target/ToDo/" + jobId, "Target/Pending/" + jobId,
                               "Target/Finished/" + jobId, "Target/Failed/" + jobId}});

  if (rv.fail()) {
    return ResultT<JobStatus>(std::move(rv).result());
  }

  VPackSlice todo(rv.get()[0]->data());
  VPackSlice pending(rv.get()[1]->data());
  VPackSlice finished(rv.get()[2]->data());
  VPackSlice failed(rv.get()[3]->data());

  auto isSet = [&jobId](VPackSlice slice) {
    return slice.isObject() && slice.hasKey("jobId") &&
           slice.get("jobId").copyString() == jobId;
  };

  if (isSet(todo)) {
    return ResultT<JobStatus>::success(JobStatus::todo);
  }
  if (isSet(pending)) {
    return ResultT<JobStatus>::success(JobStatus::pending);
  }
  if (isSet(finished)) {
    return ResultT<JobStatus>::success(JobStatus::finished);
  }
  if (isSet(failed)) {
    return ResultT<JobStatus>::success(JobStatus::failed);
  }

  return ResultT<JobStatus>::success(JobStatus::missing);
}

ResultT<std::string> RestRepairHandler::getDbAndCollectionName(VPackSlice const planCollections,
                                                               CollectionID const& collectionID) {
  for (auto const& db : VPackObjectIterator{planCollections}) {
    std::string dbName = db.key.copyString();
    for (auto const& collection : VPackObjectIterator{db.value}) {
      std::string currentCollectionId = collection.key.copyString();
      if (currentCollectionId == collectionID) {
        return dbName + "/" + collection.value.get("name").copyString();
      }
    }
  }

  // This should never happen:

  LOG_TOPIC("e4397", ERR, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::getDbAndCollectionName: "
      << "Collection " << collectionID << " not found!";

  TRI_ASSERT(false);

  return Result{TRI_ERROR_INTERNAL, "Collection not found"};
}

void RestRepairHandler::addErrorDetails(VPackBuilder& builder, int const errorNumber) {
  std::optional<const char*> errorDetails;

  switch (errorNumber) {
    case TRI_ERROR_CLUSTER_REPAIRS_FAILED:
      // General error
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY:
      errorDetails =
          "Error while collecting repair actions. "
          "There are not enough healthy DBServers to complete the repair "
          "operations. Please try again after getting your unhealthy "
          "DBServer(s) up again.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED:
      errorDetails =
          "Error while collecting repair actions. "
          "Somewhere the replicationFactor is violated, e.g. this collection "
          "has a different replicationFactor or number of DBServers than its "
          "distributeShardsLike prototype. This has to be fixed before this "
          "collection can be repaired.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS:
      errorDetails =
          "Error while collecting repair actions. "
          "Some shard of this collection doesn't have any DBServers. This "
          "should not happen. "
          "Please report this error.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS:
      errorDetails =
          "Error while collecting repair actions. "
          "Mismatching leaders of a shard and its distributeShardsLike "
          "prototype shard, after the leader should already have been fixed. "
          "This should not happen, but it should be safe to try this job "
          "again. "
          "If that does not help, please report this error.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS:
      errorDetails =
          "Error while collecting repair actions. "
          "Mismatching followers of a shard and its distributeShardsLike "
          "prototype shard, after they should already have been fixed. "
          "This should not happen, but it should be safe to try this job "
          "again. "
          "If that does not help, please report this error.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES:
      errorDetails =
          "Error while collecting repair actions. "
          "Unexpected state of distributeShardsLike or "
          "repairingDistributeShardsLike attribute. "
          "This should not happen, but it should be safe to try this job "
          "again. "
          "If that does not help, please report this error.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_SHARDS:
      errorDetails =
          "Error while collecting repair actions. "
          "In this collection, some shard and its distributeShardsLike "
          "prototype have an unequal number of DBServers. This has to be fixed "
          "before this collection can be repaired.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_JOB_FAILED:
      errorDetails =
          "Error during repairs! "
          "Moving a shard failed. Did you do any changes to the affected "
          "collection(s) or the cluster during the repairs? It should be safe "
          "to try this job again. "
          "If that does not help, please report this error.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_JOB_DISAPPEARED:
      errorDetails =
          "Error during repairs! "
          "A job to move a shard disappeared. This should not happen. "
          "Please report this error.";
      break;
    case TRI_ERROR_CLUSTER_REPAIRS_OPERATION_FAILED:
      errorDetails =
          "Error during repairs! "
          "Executing an operation as an agency transaction failed. Did you do "
          "any changes to the affected collection(s) or the cluster during the "
          "repairs? It should be safe to try this job again. "
          "If that does not help, please report this error.";
      break;
    default:
        // Some non-repair related error
        ;
  }

  if (errorDetails.has_value()) {
    builder.add("errorDetails", VPackValue(errorDetails.value()));
  }
}

bool RestRepairHandler::pretendOnly() { return _pretendOnly; }

ResultT<bool> RestRepairHandler::checkReplicationFactor(DatabaseID const& databaseId,
                                                        CollectionID const& collectionId) {
  if (!server().hasFeature<ClusterFeature>()) {
    LOG_TOPIC("1cd7a", ERR, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::checkReplicationFactor: "
        << "No ClusterInfo instance";
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR);

    return ResultT<bool>::error(TRI_ERROR_INTERNAL);
  }
  ClusterInfo& clusterInfo = server().getFeature<ClusterFeature>().clusterInfo();

  std::shared_ptr<LogicalCollection> const collection =
      clusterInfo.getCollection(databaseId, collectionId);
  std::shared_ptr<ShardMap> const shardMap = collection->shardIds();

  for (auto const& it : *shardMap) {
    auto const& shardId = it.first;
    auto const& dbServers = it.second;

    if (dbServers.size() != static_cast<size_t>(collection->replicationFactor())) {
      LOG_TOPIC("97102", DEBUG, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::checkReplicationFactor: "
          << "replicationFactor doesn't match in shard " << shardId
          << " of collection " << databaseId << "/" << collectionId << ": "
          << "replicationFactor is " << collection->replicationFactor()
          << ", but the shard has " << dbServers.size() << " DBServers.";

      return ResultT<bool>::success(false);
    }
  }

  return ResultT<bool>::success(true);
}

void RestRepairHandler::generateResult(rest::ResponseCode code,
                                       const VPackBuilder& payload, bool error) {
  resetResponse(code);

  try {
    VPackBuilder tmp;
    tmp.add(VPackValue(VPackValueType::Object, true));
    tmp.add(StaticStrings::Error, VPackValue(error));
    tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
    tmp.close();

    tmp = VPackCollection::merge(tmp.slice(), payload.slice(), false);

    VPackOptions options(VPackOptions::Defaults);
    options.escapeUnicode = true;
    writeResult(tmp.slice(), options);
  } catch (...) {
    // Building the error response failed
  }
}
