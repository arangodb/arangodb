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

#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestAgencyPrivHandler.h"

#include "Agency/Agent.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <typeinfo>

#include "Logger/Logger.h"

using namespace arangodb;

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::consensus;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestAgencyPrivHandler::RestAgencyPrivHandler(HttpRequest* request, Agent* agent)
    : RestBaseHandler(request), _agent(agent) {}

bool RestAgencyPrivHandler::isDirect() const { return false; }

inline HttpHandler::status_t RestAgencyPrivHandler::reportErrorEmptyRequest() {
  LOG(WARN) << "Empty request to agency!";
  generateError(GeneralResponse::ResponseCode::NOT_FOUND, 404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyPrivHandler::reportTooManySuffices() {
  LOG(WARN) << "Agency handles a single suffix: vote, log or configure";
  generateError(GeneralResponse::ResponseCode::NOT_FOUND, 404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyPrivHandler::reportBadQuery() {
  generateError(GeneralResponse::ResponseCode::BAD, 400);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyPrivHandler::reportMethodNotAllowed() {
  generateError(GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED, 405);
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyPrivHandler::execute() {
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    arangodb::velocypack::Options opts;
    if (_request->suffix().size() == 0) {  // empty request
      return reportErrorEmptyRequest();
    } else if (_request->suffix().size() > 1) {  // request too long
      return reportTooManySuffices();
    } else {
      term_t term = 0;
      term_t prevLogTerm = 0;
      arangodb::consensus::id_t
          id;  // leaderId for appendEntries, cadidateId for requestVote
      arangodb::consensus::index_t prevLogIndex, leaderCommit;
      if (_request->suffix()[0] == "appendEntries") {  // appendEntries
        if (_request->requestType() != GeneralRequest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        if (readValue("term", term) && readValue("leaderId", id) &&
            readValue("prevLogIndex", prevLogIndex) &&
            readValue("prevLogTerm", prevLogTerm) &&
            readValue("leaderCommit", leaderCommit)) {  // found all values
          bool ret = _agent->recvAppendEntriesRPC(
              term, id, prevLogIndex, prevLogTerm, leaderCommit,
              _request->toVelocyPack(&opts));
          result.add("success", VPackValue(ret));
        } else {
          return reportBadQuery();  // bad query
        }
      } else if (_request->suffix()[0] == "requestVote") {  // requestVote
        if (readValue("term", term) && readValue("candidateId", id) &&
            readValue("prevLogIndex", prevLogIndex) &&
            readValue("prevLogTerm", prevLogTerm)) {
          priv_rpc_ret_t ret =
              _agent->requestVote(term, id, prevLogIndex, prevLogTerm, nullptr);
          result.add("term", VPackValue(ret.term));
          result.add("voteGranted", VPackValue(ret.success));
        }
      } else if (_request->suffix()[0] == "notifyAll") {  // notify
        if (_request->requestType() != GeneralRequest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        if (readValue("term", term) && readValue("agencyId", id)) {
          priv_rpc_ret_t ret = _agent->requestVote(
              term, id, 0, 0, _request->toVelocyPack(&opts));
          result.add("term", VPackValue(ret.term));
          result.add("voteGranted", VPackValue(ret.success));
        } else {
          return reportBadQuery();  // bad query
        }
      } else {
        generateError(GeneralResponse::ResponseCode::NOT_FOUND,
                      404);  // nothing else here
        return HttpHandler::status_t(HANDLER_DONE);
      }
    }
    result.close();
    VPackSlice s = result.slice();
    generateResult(GeneralResponse::ResponseCode::OK, s);
  } catch (...) {
    // Ignore this error
  }
  return HttpHandler::status_t(HANDLER_DONE);
}
