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
  LOG_TOPIC(ERR, arangodb::Logger::CLUSTER) // TODO for debugging only
  << "[tg] RestRepairHandler::execute()";

  if (SchedulerFeature::SCHEDULER->isStopping()) {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_SHUTTING_DOWN);
    return RestStatus::FAIL;
  }

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

  // TODO Instantiate job: Use SchedulerFeature::SCHEDULER->post(lambda)

  return repairDistributeShardsLike();
}


RestStatus
RestRepairHandler::repairDistributeShardsLike() {
  LOG_TOPIC(ERR, arangodb::Logger::CLUSTER) // TODO for debugging only
  << "[tg] RestRepairHandler::repairDistributeShardsLike()";

  if (ServerState::instance()->isSingleServer()) {
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "RestRepairHandler::repairDistributeShardsLike: "
    << "No ClusterInfo instance";

    generateError(rest::ResponseCode::BAD,
      TRI_ERROR_HTTP_BAD_PARAMETER, "Only useful in cluster mode.");

    return RestStatus::FAIL;
  }

  try {
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

    DistributeShardsLikeRepairer repairer;
    ResultT<std::list<RepairOperation>> repairOperationsResult
      = repairer.repairDistributeShardsLike(
        planCollections,
        supervisionHealth
      );

    if (repairOperationsResult.fail()) {
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::repairDistributeShardsLike: "
      << "Error during preprocessing: "
      << "[" << repairOperationsResult.errorNumber() << "] "
      << repairOperationsResult.errorMessage();
      generateError(rest::ResponseCode::SERVER_ERROR,
        repairOperationsResult.errorNumber(),
        repairOperationsResult.errorMessage()
      );

      return RestStatus::FAIL;
    }
    std::list<RepairOperation>& repairOperations = repairOperationsResult.get();

    VPackBuilder response;
    response.add(VPackValue(VPackValueType::Object));


    if (repairOperations.empty()) {
      response.add("message", VPackValue("Nothing to do."));
    }
    else {
      std::stringstream message;
      message
        << "Executing "
        << repairOperations.size()
        << " operations";
      response.add("message", VPackValue(message.str()));

      // TODO this is only for debugging:
      response.add("Operations", VPackValue(VPackValueType::Array));

      for (auto const& op : repairOperations) {
        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO for debugging, remove later
        << "[tg] op type = "
        << getTypeAsString(op);

        std::stringstream stringstream;
        stringstream << op;
        response.add(VPackValue(stringstream.str()));
      }

      // TODO this is only for debugging
      response.close();

      executeRepairOperations(repairOperations);
    }

    response.close();

    generateOk(rest::ResponseCode::OK, response);


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


// TODO For debugging, remove later. At least, this shouldn't stay here.
std::ostream& operator<<(std::ostream& ostream, AgencyWriteTransaction const& trx) {
  VPackOptions optPretty = VPackOptions::Defaults;
  optPretty.prettyPrint = true;

  VPackBuilder builder;

  trx.toVelocyPack(builder);

  ostream << builder.slice().toJson(&optPretty);

  return ostream;
}

Result
RestRepairHandler::executeRepairOperations(
  std::list<RepairOperation> repairOperations
) {
  // TODO Maybe wait if there are *any* jobs that were created from repairDistributeShardsLike?
  AgencyComm comm;
  for (auto& op : repairOperations) {
    auto visitor = RepairOperationToTransactionVisitor();
    auto pair =
      boost::apply_visitor(visitor, op);

    AgencyWriteTransaction &wtrx = pair.first;
    boost::optional<uint64_t> waitForJobId = pair.second;

    LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO set to DEBUG
    << "RestRepairHandler::executeRepairOperations: "
    << "Sending transaction to the agency";

    LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO set to TRACE
    << "RestRepairHandler::executeRepairOperations: "
    << "Transaction is: "
    << wtrx;

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
      LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO set to DEBUG
      << "RestRepairHandler::executeRepairOperations: "
      << "Waiting for job " << waitForJobId.get();
      bool previousJobFinished = false;
      std::string jobId = std::to_string(waitForJobId.get());

      // TODO the loop is too long, try to refactor it
      while(! previousJobFinished) {
        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO set to TRACE
        << "RestRepairHandler::executeRepairOperations: "
        << "Fetching job info of " << jobId;
        ResultT<JobStatus> jobStatus
          = getJobStatusFromAgency(jobId);

        if (jobStatus.ok()) {
          LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO set to DEBUG
          << "RestRepairHandler::executeRepairOperations: "
          << "Job status is: "
          << (
            jobStatus.get() == JobStatus::todo ? "todo" :
            jobStatus.get() == JobStatus::pending ? "pending" :
            jobStatus.get() == JobStatus::finished ? "finished" :
            jobStatus.get() == JobStatus::failed ? "failed" :
            jobStatus.get() == JobStatus::missing ? "missing" :
            "n/a"
          );

          switch (jobStatus.get()) {
            case JobStatus::todo:
            case JobStatus::pending:
              break;

            case JobStatus::finished:
              previousJobFinished = true;
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

        } else
        {
          LOG_TOPIC(INFO, arangodb::Logger::CLUSTER)
          << "RestRepairHandler::executeRepairOperations: "
          << "Failed to get job status: "
          << "[" << jobStatus.errorNumber() << "] "
          << jobStatus.errorMessage();

          return jobStatus;
        }

        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO set to TRACE
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
RestRepairHandler::getFromAgencyArray(std::array<std::string const, N> const& agencyKeyArray) {
  std::array<VPackBufferPtr, N> resultArray;

  AgencyComm agency;

  // TODO The new code with the new getValues method is untested!
  AgencyCommResult result = agency.getValues(agencyKeyArray);

  for(size_t i = 0; i < N; i++) {
    std::string const& agencyKey = agencyKeyArray[i];

    if (!result.successful()) {
      LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
      << "RestRepairHandler::getFromAgency: "
      << "Getting value from agency failed with: " << result.errorMessage();
      generateError(rest::ResponseCode::SERVER_ERROR,
        result.errorCode(),
        result.errorMessage());

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


template <std::size_t N>
ResultT<std::array<VPackBufferPtr, N>>
RestRepairHandler::getFromAgencyOld(std::array<std::string const, N> const& agencyKeyArray) {
  std::array<VPackBufferPtr, N> resultArray;

  AgencyComm agency;

  for(size_t i = 0; i < N; i++) {
    std::string const& agencyKey = agencyKeyArray[i];
    AgencyCommResult result = agency.getValues(agencyKey);

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

template <std::size_t N>
ResultT<std::array<VPackBufferPtr, N>>
RestRepairHandler::getFromAgencyNew(std::array<std::string const, N> const& agencyKeyArray) {
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

  // TODO The new code with transactions is untested!
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

template <std::size_t N>
ResultT<std::array<VPackBufferPtr, N>>
RestRepairHandler::getFromAgency(std::array<std::string const, N> const& agencyKeyArray) {
  auto oldR = getFromAgencyOld(agencyKeyArray);
  auto newR = getFromAgencyNew(agencyKeyArray);
  auto arrayR = getFromAgencyArray(agencyKeyArray);

  // TODO this method is a proxy for debugging

  LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
  << "[tg] RestRepairHandler::getFromAgency: old   = "
  << oldR;
  LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
  << "[tg] RestRepairHandler::getFromAgency: array = "
  << arrayR;
  LOG_TOPIC(WARN, arangodb::Logger::CLUSTER)
  << "[tg] RestRepairHandler::getFromAgency: new_  = "
  << newR;

  TRI_ASSERT(oldR.ok() && newR.ok() && arrayR.ok());
  TRI_ASSERT(oldR.get().size() == newR.get().size() && oldR.get().size() == arrayR.get().size());

  std::array<VPackBufferPtr, N>& oldA = oldR.get();
  std::array<VPackBufferPtr, N>& newA = newR.get();
  std::array<VPackBufferPtr, N>& arrayA = arrayR.get();

  for(size_t i = 0; i < oldA.size(); i++) {
    std::string oldJS = VPackSlice(oldA[i]->data()).toJson();
    std::string newJS = VPackSlice(newA[i]->data()).toJson();
    std::string arrayJS = VPackSlice(arrayA[i]->data()).toJson();
    TRI_ASSERT(oldJS == newJS);
    TRI_ASSERT(oldJS == arrayJS);
  }

  return newR;
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
