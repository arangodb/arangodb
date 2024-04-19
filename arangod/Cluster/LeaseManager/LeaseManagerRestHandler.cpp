////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "LeaseManagerRestHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Network/NetworkFeature.h"
#include "Cluster/LeaseManager/AbortLeaseInformation.h"
#include "Cluster/LeaseManager/AbortLeaseInformationInspector.h"
#include "Cluster/LeaseManager/LeaseManager.h"
#include "Inspection/VPack.h"

using namespace arangodb;
using namespace arangodb::cluster;

LeaseManagerRestHandler::LeaseManagerRestHandler(ArangodServer& server,
                                                 GeneralRequest* request,
                                                 GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus LeaseManagerRestHandler::execute() {
  switch (request()->requestType()) {
    case RequestType::GET:
      return executeGet();
    case RequestType::DELETE_REQ: {
      bool parseSuccess = false;
      VPackSlice const body = this->parseVPackBody(parseSuccess);
      if (!parseSuccess) {
        // error message generated in parseVPackBody
        generateError(Result(TRI_ERROR_BAD_PARAMETER,
                             "Failed to Parse Body, invalid JSON."));
        return RestStatus::DONE;
      }
      auto info = velocypack::deserialize<AbortLeaseInformation>(body);
      return executeDelete(std::move(info));
    }
    default:
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      return RestStatus::DONE;
  }

}

RestStatus LeaseManagerRestHandler::executeGet() {
  auto& networkFeature = server().getFeature<NetworkFeature>();
  auto& leaseManager = networkFeature.leaseManager();
  auto builder = leaseManager.leasesToVPack();
  generateOk(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}

RestStatus LeaseManagerRestHandler::executeDelete(AbortLeaseInformation info) {
  auto& networkFeature = server().getFeature<NetworkFeature>();
  auto& leaseManager = networkFeature.leaseManager();
  leaseManager.abortLeasesForServer(std::move(info));
  // This API can only return 200, leases are guaranteed to be aborted.
  generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
  return RestStatus::DONE;
}