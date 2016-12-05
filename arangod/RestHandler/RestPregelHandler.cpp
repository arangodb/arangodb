////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestPregelHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Rest/HttpRequest.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Pregel/PregelFeature.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Conductor.h"
#include "Pregel/Worker.h"
#include "Pregel/Utils.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::pregel;

RestPregelHandler::RestPregelHandler(GeneralRequest* request, GeneralResponse* response)
: RestVocbaseBaseHandler(request, response) {}

RestStatus RestPregelHandler::execute() {
  try {    
    bool parseSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
    parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);
    VPackSlice body(parsedBody->start());// never nullptr
    
    if (!parseSuccess || !body.isObject()) {
      LOG(ERR) << "Bad request body\n";
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_NOT_IMPLEMENTED, "illegal request for /_api/pregel");
      return RestStatus::DONE;
    }
    if (_request->requestType() != rest::RequestType::POST) {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_NOT_IMPLEMENTED, "illegal method for /_api/pregel");
      return RestStatus::DONE;

    }
  
    std::vector<std::string> const& suffix = _request->suffixes();
    VPackSlice sExecutionNum = body.get(Utils::executionNumberKey);
    if (!sExecutionNum.isInteger()) {
      LOG(ERR) << "Invalid execution number";
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
      return RestStatus::DONE;
    }
    
    uint64_t executionNumber = sExecutionNum.getUInt();
    if (suffix.size() != 1) {
      LOG(ERR) << "Invalid suffix";
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_HTTP_NOT_FOUND);
      return RestStatus::DONE;
    } else if (suffix[0] == Utils::startExecutionPath) {
      IWorker *w = PregelFeature::instance()->worker(executionNumber);
      if (w) {
        LOG(ERR) << "Worker with this execution number already exists.";
        generateError(rest::ResponseCode::BAD,
                      TRI_ERROR_HTTP_FORBIDDEN);
        return RestStatus::DONE;
      }
      w = AlgoRegistry::createWorker(_vocbase, body);
      PregelFeature::instance()->addWorker(w, executionNumber);
    } else if (suffix[0] == Utils::finishedStartupPath) {
      Conductor *exe = PregelFeature::instance()->conductor(executionNumber);
      if (exe) {
        exe->finishedWorkerStartup(body);
      } else {
        LOG(ERR) << "Conductor not found: " << executionNumber;
      }
    } else if (suffix[0] == Utils::prepareGSSPath) {
      IWorker *w = PregelFeature::instance()->worker(executionNumber);
      if (w) {
        w->prepareGlobalStep(body);
      } else {
        LOG(ERR) << "Invalid execution number, worker does not exist.";
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
        return RestStatus::DONE;
      }
    } else if (suffix[0] == Utils::startGSSPath) {
      IWorker *w = PregelFeature::instance()->worker(executionNumber);
      if (w) {
        w->startGlobalStep(body);
      } else {
        LOG(ERR) << "Invalid execution number, worker does not exist.";
        generateError(rest::ResponseCode::NOT_FOUND,
                      TRI_ERROR_HTTP_NOT_FOUND);
        return RestStatus::DONE;
      }
    } else if (suffix[0] == Utils::messagesPath) {
      IWorker *exe = PregelFeature::instance()->worker(executionNumber);
      if (exe) {
        exe->receivedMessages(body);
      }
    } else if (suffix[0] == Utils::finishedGSSPath) {
      Conductor *exe = PregelFeature::instance()->conductor(executionNumber);
      if (exe) {
        exe->finishedGlobalStep(body);
      } else {
        LOG(ERR) << "Conductor not found: " << executionNumber;
      }
    } else if (suffix[0] == Utils::finalizeExecutionPath) {
      IWorker *exe = PregelFeature::instance()->worker(executionNumber);
      if (exe) {
        exe->finalizeExecution(body);
        PregelFeature::instance()->cleanup(executionNumber);
      }
    } else if (suffix[0] == Utils::startRecoveryPath) {
      IWorker *w = PregelFeature::instance()->worker(executionNumber);
      if (!w) {// we will need to create a worker in these cicumstances
        w = AlgoRegistry::createWorker(_vocbase, body);
      }
      w->startRecovery(body);
    } else if (suffix[0] == Utils::finishedRecoveryPath) {
      Conductor *exe = PregelFeature::instance()->conductor(executionNumber);
      if (exe) {
        exe->finishedRecovery(body);
      } else {
        LOG(ERR) << "Conductor not found: " << executionNumber;
      }
    }
    
    VPackSlice result;
    generateResult(rest::ResponseCode::OK, result);
    
  } catch (std::exception const &e) {
    LOG(ERR) << e.what();
  } catch(...) {
    LOG(ERR) << "Exception in pregel REST handler";
  }
    
  return RestStatus::DONE;
}
