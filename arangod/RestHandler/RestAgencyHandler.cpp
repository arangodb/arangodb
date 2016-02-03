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

#include "Rest/AnyServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestAgencyHandler.h"

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

extern AnyServer* ArangoInstance;


RestAgencyHandler::RestAgencyHandler(HttpRequest* request, Agent* agent)
    : RestBaseHandler(request), _agent(agent) {
}

bool RestAgencyHandler::isDirect() const { return false; }

HttpHandler::status_t RestAgencyHandler::execute() {
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));


    // Empty request
    if (_request->suffix().size() == 0) {
    	LOG(WARN) << "Empty request to agency. Must ask for vote, log or "
    			"configure";
    	generateError(HttpResponse::NOT_FOUND,404);
    	return status_t(HANDLER_DONE);
    } else if (_request->suffix().size() > 1) { // path size >= 2
    	LOG(WARN) << "Agency handles a single suffix: vote, log or configure";
    	generateError(HttpResponse::NOT_FOUND,404);
    	return status_t(HANDLER_DONE);
    } else {
    	if (_request->suffix()[0].compare("vote") == 0) { //vote4me
    		LOG(WARN) << "Vote request";
    		bool found;
    		char const* termStr = _request->value("term", found);
    		if (!found) {							  // For now: don't like this
    			LOG(WARN) << "No term specified";  // should be handled by
				generateError(HttpResponse::BAD,400); // Agent rather than Rest
    			return status_t(HANDLER_DONE);        // handler.
    		}
    		char const* idStr = _request->value("id", found);
    		if (!found) {
    			LOG(WARN) << "No id specified";
				generateError(HttpResponse::BAD,400);
    			return status_t(HANDLER_DONE);
    		}
			if (_agent->vote(std::stoul(idStr), std::stoul(termStr))) {
				result.add("vote", VPackValue("YES"));
			} else {
				result.add("vote", VPackValue("NO"));
			}
		} /*else {



    	} else if (_request->suffix()[0].compare("log") == 0) { // log replication
    	} else if (_request->suffix()[0].compare("configure") == 0) {} // cluster conf*/
    }

    result.close();
    VPackSlice s = result.slice();
    generateResult(s);
  } catch (...) {
    // Ignore this error
  }
  return status_t(HANDLER_DONE);
}
