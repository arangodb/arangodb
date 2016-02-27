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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/ArangoServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestAgencyPrivHandler.h"

#include "Agency/Agent.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Logger.h"

using namespace arangodb;

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::consensus;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

extern ArangoServer* ArangoInstance;


RestAgencyPrivHandler::RestAgencyPrivHandler(HttpRequest* request, Agent* agent)
    : RestBaseHandler(request), _agent(agent) {
}

bool RestAgencyPrivHandler::isDirect() const { return false; }

inline HttpHandler::status_t RestAgencyPrivHandler::reportErrorEmptyRequest () {
  LOG(WARN) << "Empty request to agency!";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyPrivHandler::reportTooManySuffices () {
  LOG(WARN) << "Agency handles a single suffix: vote, log or configure";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyPrivHandler::unknownMethod () {
  LOG(WARN) << "Too many suffixes. Agency public interface takes one path.";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyPrivHandler::execute() {
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    arangodb::velocypack::Options opts;

    if (_request->suffix().size() == 0) { // empty request
      reportErrorEmptyRequest();
    } else if (_request->suffix().size() > 1) { // request too long
      reportTooManySuffices();
    } else {
    	if        (_request->suffix()[0] == "appendEntries") {  // replication
        _agent->appendEntries (_request->headers(), _request->toVelocyPack());
    	} else if (_request->suffix()[0] ==   "requestVote") {  // election
        _agent->vote (_request->headers(), _request->toVelocyPack());
      } else {
        generateError(HttpResponse::NOT_FOUND,404); // nothing 
        return HttpHandler::status_t(HANDLER_DONE);
      }
        
    }
      
    result.close();
    VPackSlice s = result.slice();
    generateResult(s);
  } catch (...) {
    // Ignore this error
  }
  return HttpHandler::status_t(HANDLER_DONE);
}


