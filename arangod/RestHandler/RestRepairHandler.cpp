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

#include <list>
#include <valarray>
#include <arangod/GeneralServer/AsyncJobManager.h>
#include <arangod/Cluster/ServerState.h>
#include "RestRepairHandler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::rest_repair;
using namespace arangodb::cluster_repairs;

// TODO Fix logging for production (remove [tg], reduce loglevels, rewrite messages etc)


RestRepairHandler::RestRepairHandler(
  GeneralRequest *request, GeneralResponse *response
)
  : RestBaseHandler(request, response) {
}


RestStatus RestRepairHandler::execute() {
  if (SchedulerFeature::SCHEDULER->isStopping()) {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_SHUTTING_DOWN);
    return RestStatus::FAIL;
  }

  // TODO Allow GET and implement it as a pretend-only version

  if (_request->requestType() != rest::RequestType::POST) {
    generateError(
      rest::ResponseCode::METHOD_NOT_ALLOWED,
      (int) rest::ResponseCode::METHOD_NOT_ALLOWED
    );

    return RestStatus::FAIL;
  }

  std::vector<std::string> const &suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(
      rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
      "Bad parameter: expected 'distributeShardsLike', got none"
    );

    return RestStatus::FAIL;
  }

  if (suffixes[0] != "distributeShardsLike") {
    generateError(
      rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
      "Bad parameter: expected 'distributeShardsLike', got '" + suffixes[0] + "'"
    );

    return RestStatus::FAIL;
  }

  return repairDistributeShardsLike();
}


RestStatus
RestRepairHandler::repairDistributeShardsLike() {
  if (ServerState::instance()->isSingleServer()) {
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "RestRepairHandler::repairDistributeShardsLike: "
    << "No ClusterInfo instance";

    generateError(rest::ResponseCode::BAD,
      TRI_ERROR_HTTP_BAD_PARAMETER, "Only useful in cluster mode.");

    return RestStatus::FAIL;
  }

  try {
    rest::ResponseCode responseCode = rest::ResponseCode::OK;

    ClusterInfo* clusterInfo = ClusterInfo::instance();
    if (clusterInfo == nullptr) {
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::repairDistributeShardsLike: "
      << "No ClusterInfo instance";
      generateError(rest::ResponseCode::SERVER_ERROR,
        TRI_ERROR_HTTP_SERVER_ERROR);

      return RestStatus::FAIL;
    }

    VPackSlice plan = clusterInfo->getPlan()->slice();

    VPackSlice planCollections = plan.get("Collections");

    ResultT<VPackBufferPtr> healthResult = getFromAgency("Supervision/Health");

    if (healthResult.fail()) {
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::repairDistributeShardsLike: "
      << "Failed to fetch server health result";
      generateError(rest::ResponseCode::SERVER_ERROR,
        healthResult.errorNumber(),
        healthResult.errorMessage());

      return RestStatus::FAIL;
    }

    VPackSlice supervisionHealth(healthResult.get()->data());

    // TODO assert replicationFactor < #DBServers before calling repairDistributeShardsLike()
    // This has to be done per collection...

    ResultT<std::map<
        CollectionID,
        ResultT<std::list<RepairOperation>>
    >>
      repairOperationsByCollectionResult
      = DistributeShardsLikeRepairer::repairDistributeShardsLike(
        planCollections,
        supervisionHealth
      );

    if (repairOperationsByCollectionResult.fail()) {
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::repairDistributeShardsLike: "
      << "Error during preprocessing: "
      << "[" << repairOperationsByCollectionResult.errorNumber() << "] "
      << repairOperationsByCollectionResult.errorMessage();
      generateError(rest::ResponseCode::SERVER_ERROR,
        repairOperationsByCollectionResult.errorNumber(),
        repairOperationsByCollectionResult.errorMessage()
      );

      return RestStatus::FAIL;
    }
    std::map<CollectionID, ResultT<std::list<RepairOperation>>>&
      repairOperationsByCollection = repairOperationsByCollectionResult.get();

    VPackBuilder response;
    response.add(VPackValue(VPackValueType::Object));

    if (repairOperationsByCollection.empty()) {
      response.add("message", VPackValue("Nothing to do."));
    }
    else {
      std::stringstream message;
      message
        << "Repairing "
        << repairOperationsByCollection.size()
        << " collections";

      bool allCollectionsSucceeded = true;

      response.add("collections", VPackValue(VPackValueType::Object));

      for(auto const& it : repairOperationsByCollection) {
        CollectionID collectionId = it.first;
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
          success = repairCollection(repairOperationsResult.get(), response);
        }
        else {
          response.add(StaticStrings::ErrorMessage, VPackValue(repairOperationsResult.errorMessage()));
          success = false;
        }
        response.add(StaticStrings::Error, VPackValue(! success));

        allCollectionsSucceeded = success && allCollectionsSucceeded;

        response.close();
      }

      response.close();

    }

    response.close();

    generateOk(responseCode, response);

    return RestStatus::DONE;
  }
  catch (Exception &e) {
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "RestRepairHandler::repairDistributeShardsLike: "
    << "Caught exception: " << e.message();
    generateError(rest::ResponseCode::SERVER_ERROR,
      e.code());

    return RestStatus::FAIL;
  }

}


bool RestRepairHandler::repairCollection(
  std::list<RepairOperation> repairOperations,
  VPackBuilder &response
) {
  bool success = true;

  response.add("PlannedOperations", VPackValue(velocypack::ValueType::Array));

  for (auto const& op : repairOperations) {
    RepairOperationToVPackVisitor
      addToVpack(response);

    boost::apply_visitor(addToVpack, op);
  }

  response.close();

  Result result = executeRepairOperations(repairOperations);
  if (result.fail()) {
    success = false;
    response.add(StaticStrings::ErrorMessage, VPackValue(result.errorMessage()));
  }

  return success;
}

ResultT<bool>
RestRepairHandler::jobFinished(std::string const& jobId) {
  LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
  << "RestRepairHandler::executeRepairOperations: "
  << "Fetching job info of " << jobId;
  ResultT<JobStatus> jobStatus
    = getJobStatusFromAgency(jobId);

  if (jobStatus.ok()) {
    LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
    << "RestRepairHandler::executeRepairOperations: "
    << "Job status is: "
    << toString(jobStatus.get());

    switch (jobStatus.get()) {
      case JobStatus::todo:
      case JobStatus::pending:
        break;

      case JobStatus::finished:
        return true;
        break;

      case JobStatus::failed:
        LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::executeRepairOperations: "
        << "Job " << jobId << " failed, aborting";

        return Result(TRI_ERROR_CLUSTER_REPAIRS_JOB_FAILED);

      case JobStatus::missing:
        LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::executeRepairOperations: "
        << "Job " << jobId << " went missing, aborting";

        return Result(TRI_ERROR_CLUSTER_REPAIRS_JOB_DISAPPEARED);
    }

  }
  else {
    LOG_TOPIC(INFO, arangodb::Logger::CLUSTER)
    << "RestRepairHandler::executeRepairOperations: "
    << "Failed to get job status: "
    << "[" << jobStatus.errorNumber() << "] "
    << jobStatus.errorMessage();

    return jobStatus;
  }

  return false;
}


Result
RestRepairHandler::executeRepairOperations(
  std::list<RepairOperation> repairOperations
) {
  AgencyComm comm;
  for (auto& op : repairOperations) {
    auto visitor = RepairOperationToTransactionVisitor();
    auto trxJobPair =
      boost::apply_visitor(visitor, op);

    AgencyWriteTransaction &wtrx = trxJobPair.first;
    boost::optional<uint64_t> waitForJobId = trxJobPair.second;

    LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
    << "RestRepairHandler::executeRepairOperations: "
    << "Sending a transaction to the agency";

    AgencyCommResult result
      = comm.sendTransactionWithFailover(wtrx);
    if (! result.successful()) {
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::executeRepairOperations: "
      << "Failed to send transaction to the agency. Error was: "
      << "[" << result.errorCode() << "] "
      << result.errorMessage();

      return Result(result.errorCode(), result.errorMessage());
    }

    // If the transaction posted a job, we wait for it to finish.
    if(waitForJobId) {
      LOG_TOPIC(DEBUG, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::executeRepairOperations: "
      << "Waiting for job " << waitForJobId.get();
      bool previousJobFinished = false;
      std::string jobId = std::to_string(waitForJobId.get());

      while(! previousJobFinished) {
        ResultT<bool> jobFinishedResult = jobFinished(jobId);
        if (jobFinishedResult.fail()) {
          return jobFinishedResult;
        }
        previousJobFinished = jobFinishedResult.get();

        LOG_TOPIC(TRACE, arangodb::Logger::CLUSTER)
        << "RestRepairHandler::executeRepairOperations: "
        << "Sleeping for 1s";
        sleep(1);
      }
    }

  }

  return Result();
}


template <std::size_t N>
ResultT<std::array<VPackBufferPtr, N>>
RestRepairHandler::getFromAgency(std::array<std::string const, N> const& agencyKeyArray) {
  std::array<VPackBufferPtr, N> resultArray;

  AgencyComm agency;

  std::vector<std::string> paths;

  // apply AgencyCommManager::path on every element and copy to vector
  std::transform(
    agencyKeyArray.begin(),
    agencyKeyArray.end(),
    std::back_inserter(paths),
    [](std::string const& key) {
      return AgencyCommManager::path(key);
    }
  );

  AgencyCommResult result = agency.sendTransactionWithFailover(
    AgencyReadTransaction { std::move(paths) }
  );

  for(size_t i = 0; i < N; i++) {
    std::string const& agencyKey = agencyKeyArray[i];

    if (!result.successful()) {
      LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::getFromAgency: "
      << "Getting value from agency failed with: " << result.errorMessage();
      generateError(rest::ResponseCode::SERVER_ERROR,
        result.errorCode(),
        result.errorMessage()
      );

      return ResultT<
        std::array<VPackBufferPtr, N>
      >::error(
        result.errorCode(),
        result.errorMessage()
      );
    }

    std::vector<std::string> agencyPath =
      basics::StringUtils::split(AgencyCommManager::path(agencyKey), '/');

    agencyPath.erase(
      std::remove(
        agencyPath.begin(),
        agencyPath.end(),
        ""
      ),
      agencyPath.end()
    );


    VPackBuilder builder;

    builder.add(result.slice()[0].get(agencyPath));

    resultArray[i] = builder.steal();
  }

  return ResultT<
    std::array<VPackBufferPtr, N>
  >::success(resultArray);
}

ResultT<VPackBufferPtr>
RestRepairHandler::getFromAgency(std::string const &agencyKey) {
  ResultT<std::array<VPackBufferPtr, 1>> rv
    = getFromAgency<1>({agencyKey});

  if (rv.ok()) {
    return ResultT<VPackBufferPtr>::success(rv.get()[0]);
  }
  else {
    return ResultT<VPackBufferPtr>(rv);
  }
}

ResultT<JobStatus>
RestRepairHandler::getJobStatusFromAgency(std::string const &jobId) {
  // As long as getFromAgency doesn't get all values at once, the order here
  // matters: if e.g. finished was checked before pending, this would
  // introduce a race condition which would result in JobStatus::missing
  // despite it being finished.
  auto rv
    = getFromAgency<4>({
      "Target/ToDo/" + jobId,
      "Target/Pending/" + jobId,
      "Target/Finished/" + jobId,
      "Target/Failed/" + jobId,
    });

  if (rv.fail()) {
    return ResultT<JobStatus>(rv);
  }

  VPackSlice todo(rv.get()[0]->data());
  VPackSlice pending(rv.get()[1]->data());
  VPackSlice finished(rv.get()[2]->data());
  VPackSlice failed(rv.get()[3]->data());

  auto isSet = [&jobId](VPackSlice slice) {
    return slice.isObject()
      && slice.hasKey("jobId")
      && slice.get("jobId").copyString() == jobId;
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

ResultT<std::string>
RestRepairHandler::getDbAndCollectionName(
  VPackSlice planCollections, CollectionID collectionID
) {
  for(auto const& db : VPackObjectIterator { planCollections }) {
    std::string dbName = db.key.copyString();
    for(auto const& collection : VPackObjectIterator { db.value }) {
      std::string currentCollectionId = collection.key.copyString();
      if (currentCollectionId == collectionID) {
        return dbName + "/" + collection.value.get("name").copyString();
      }
    }
  }

  // This should never happen:

  LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
  << "RestRepairHandler::getDbAndCollectionName: "
  << "Collection " << collectionID << " not found!";

  TRI_ASSERT(false);

  return Result {
    TRI_ERROR_INTERNAL,
    "Collection not found"
  };
}
