////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RestSupervisionStateHandler.h"

#include "Agency/AgencyComm.h"
#include "Cluster/ResultT.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestSupervisionStateHandler::RestSupervisionStateHandler(application_features::ApplicationServer& server,
                                                         GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestSupervisionStateHandler::~RestSupervisionStateHandler() {}

RestStatus RestSupervisionStateHandler::execute() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  AgencyCommResult const& result = AgencyComm().getValues("Target");
  if (result.successful()) {

    VPackBuilder bodyBuilder;
    //try {
      LOG_DEVEL << result.slice().toJson();
      VPackSlice target = result.slice().at(0).get({"arango", "Target"});

      {
        VPackObjectBuilder ob(&bodyBuilder);
        bodyBuilder.add("ToDo", target.get("ToDo"));
        bodyBuilder.add("Pending", target.get("Pending"));
        bodyBuilder.add("Finished", target.get("Finished"));
        bodyBuilder.add("Failed", target.get("Failed"));
      }


      resetResponse(rest::ResponseCode::OK);
      _response->setPayload(bodyBuilder.slice(), true);
      return RestStatus::DONE;

    //} catch(...) {}
  } else {
    LOG_DEVEL << "failed to get agency value";
  }

  generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR);
  return RestStatus::DONE;
}
