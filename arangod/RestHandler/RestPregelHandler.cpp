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

#include "Pregel/Conductor.h"
#include "Pregel/Execution.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::pregel;

RestPregelHandler::RestPregelHandler(GeneralRequest* request, GeneralResponse* response)
: RestVocbaseBaseHandler(request, response) {}

RestHandler::status RestPregelHandler::execute() {
  
  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
  parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);
  
  if (!parseSuccess) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_NOT_IMPLEMENTED, "illegal request for /_api/pregel");
  } else if (_request->requestType() == rest::RequestType::POST) {
    
    VPackSlice s(parsedBody->start());
    
    std::vector<std::string> const& suffix = _request->suffix();
  
    if (suffix.size() != 1) {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_HTTP_NOT_FOUND);
      return status::DONE;
    } else if (suffix[0] == "finishedGSS") {
      finishedGSS(s);
    } else if (suffix[0] == "nextGSS") {
      nextGSS(s);
    }
    VPackBuilder result;
    result.add(VPackValue("thanks"));
    generateResult(rest::ResponseCode::OK, result.slice());
    
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_NOT_IMPLEMENTED, "illegal method for /_api/pregel");
  }
  
  return status::DONE;
}

void RestPregelHandler::nextGSS(VPackSlice &body) {
  VPackSlice executionNum = body.get("en");
  if (executionNum.isInt()) {
    Execution *exe = Conductor::instance()->execution(executionNum.getInt());
    
    if (!exe) {// can happen if gss == 0
      VPackSlice vertex = body.get("vertex");
      VPackSlice edge = body.get("edge");
      VPackSlice algo = body.get("algo");

      exe = new Execution(executionNum.getInt(),
                          _vocbase,
                          vertex.copyString(),
                          edge.copyString(),
                          algo.copyString());
    }
    exe->nextGlobalStep(body);
  }
}

void RestPregelHandler::finishedGSS(VPackSlice &body) {
  VPackSlice executionNum = body.get("en");
  if (executionNum.isInt()) {
    Execution *exe = Conductor::instance()->execution(executionNum.getInt());
    if (exe) {
      exe->finishedGlobalStep(body);
    }
  }
}


void RestPregelHandler::receivedMessages(VPackSlice &body) {
  VPackSlice executionNum = body.get("en");
  if (executionNum.isInt()) {
    Execution *exe = Conductor::instance()->execution(executionNum.getInt());
    
    if (exe) {
      //exe->nextGlobalStep(body);
    }
    
  }
}
