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
#include "Cluster/ClusterRepairs.h"
#include <velocypack/velocypack-aliases.h>

#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
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

    std::shared_ptr<VPackBuffer<uint8_t>> vpack
      = getFromAgency("Supervision/Health");

    VPackSlice supervisionHealth(vpack->data());

    // TODO assert replicationFactor < #DBServers before calling repairDistributeShardsLike()

    DistributeShardsLikeRepairer repairer;
    std::list<RepairOperation> repairOperations
      = repairer.repairDistributeShardsLike(
        planCollections,
        supervisionHealth
      );

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

      // TODO execute operations
      for (auto const& op : repairOperations) {
        LOG_TOPIC(INFO, arangodb::Logger::CLUSTER) // TODO for debugging, remove later
        << "[tg] op type = " << op.which();

        switch(op.which()) {
          case 0: {
            MoveShardOperation msop = boost::get<MoveShardOperation>(op);
            response.add(VPackValue(msop.collection));
          }
            break;
          case 1: {
            AgencyWriteTransaction wtrx = boost::get<AgencyWriteTransaction>(op);
            response.add(VPackValue(wtrx.toJson()));
          }
            break;
          default:
            THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
        }
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
      TRI_ERROR_HTTP_SERVER_ERROR);

    return RestStatus::FAIL;
  }

}

class ExecuteRepairOperationVisitor
  : boost::static_visitor<AgencyWriteTransaction> {
  AgencyWriteTransaction operator()(MoveShardOperation& op) {
    uint64_t jobId = ClusterInfo::instance()->uniqid();
    std::shared_ptr<VPackBuffer<uint8_t>> vpackTodo = op.toVpackTodo(jobId);

    std::string const agencyKey = "Target/ToDo/" + jobId;

    return AgencyWriteTransaction {
      AgencyOperation {
        agencyKey,
        AgencyValueOperationType::SET,
        VPackSlice(vpackTodo->data())
      },
      AgencyPrecondition {}
    };
  }

  AgencyWriteTransaction operator()(AgencyWriteTransaction& wtrx) {
    return wtrx;
  }
};

void RestRepairHandler::executeRepairOperations(
  std::list<RepairOperation> repairOperations
) {
  // TODO Maybe wait if there are *any* jobs that were created from repairDistributeShardsLike?
  AgencyComm comm;
  for (auto const& op : repairOperations) {
    AgencyWriteTransaction wtrx
      = boost::apply_visitor(ExecuteRepairOperationVisitor(), op);

    AgencyCommResult result
      = comm.sendTransactionWithFailover(wtrx);
    if (! result.successful()) {
      // TODO return error
      return;
    }

//    // TODO on move shard operations we have to wait for the job to finish.
//    // It probably doesn't make sense for fixServerOrderTransactions
//    std::shared_ptr<VPackBuffer<uint8_t>> vpack
//      = getFromAgency("Target/Finished/" + jobId);
//
//    VPackSlice finished(vpack->data());


  }
}

std::shared_ptr<VPackBuffer<uint8_t>>
RestRepairHandler::getFromAgency(std::string const &agencyKey) {
  AgencyComm agency;

  AgencyCommResult result = agency.getValues(agencyKey);

  if (!result.successful()) {
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
    << "RestRepairHandler::getFromAgency: "
    << "Getting value from agency failed with: " << result.errorMessage();
    generateError(rest::ResponseCode::SERVER_ERROR,
      TRI_ERROR_HTTP_SERVER_ERROR);

    return nullptr;
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

  return builder.steal();
}
