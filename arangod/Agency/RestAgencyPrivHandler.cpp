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

#include "RestAgencyPrivHandler.h"

#include "Agency/Agent.h"

#include <typeinfo>

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

using namespace arangodb;

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::consensus;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestAgencyPrivHandler::RestAgencyPrivHandler(GeneralRequest* request,
                                             GeneralResponse* response,
                                             Agent* agent)
    : RestBaseHandler(request, response), _agent(agent) {}

bool RestAgencyPrivHandler::isDirect() const { return false; }

inline RestStatus RestAgencyPrivHandler::reportErrorEmptyRequest() {
  LOG_TOPIC(WARN, Logger::AGENCY) << "Empty request to agency!";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyPrivHandler::reportTooManySuffices() {
  LOG_TOPIC(WARN, Logger::AGENCY)
      << "Agency handles a single suffix: vote, log or configure";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyPrivHandler::reportBadQuery(
    std::string const& message) {
  generateError(rest::ResponseCode::BAD, 400, message);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyPrivHandler::reportMethodNotAllowed() {
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyPrivHandler::reportGone() {
  generateError(rest::ResponseCode::GONE, 410);
  return RestStatus::DONE;
}

RestStatus RestAgencyPrivHandler::execute() {
  try {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("id", VPackValue(_agent->id()));
    result.add("endpoint", VPackValue(_agent->endpoint()));

    auto const& suffixes = _request->suffixes();

    if (suffixes.empty()) {  // empty request
      return reportErrorEmptyRequest();
    } else if (suffixes.size() > 1) {  // request too long
      return reportTooManySuffices();
    } else {
      term_t term = 0;
      term_t prevLogTerm = 0;
      std::string id;  // leaderId for appendEntries, cadidateId for requestVote
      arangodb::consensus::index_t prevLogIndex, leaderCommit;
      if (suffixes[0] == "appendEntries") {  // appendEntries
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        int64_t senderTimeStamp = 0;
        readValue("senderTimeStamp", senderTimeStamp);  // ignore if not given
        if (readValue("term", term) && readValue("leaderId", id) &&
            readValue("prevLogIndex", prevLogIndex) &&
            readValue("prevLogTerm", prevLogTerm) &&
            readValue("leaderCommit", leaderCommit)) {  // found all values
          bool ret = _agent->recvAppendEntriesRPC(
              term, id, prevLogIndex, prevLogTerm, leaderCommit,
              _request->toVelocyPackBuilderPtr());
          result.add("success", VPackValue(ret));
          result.add("senderTimeStamp", VPackValue(senderTimeStamp));
        } else {
          return reportBadQuery();  // bad query
        }
      } else if (suffixes[0] == "requestVote") {  // requestVote
        int64_t timeoutMult = 1;
        readValue("timeoutMult", timeoutMult);
        if (readValue("term", term) && readValue("candidateId", id) &&
            readValue("prevLogIndex", prevLogIndex) &&
            readValue("prevLogTerm", prevLogTerm)) {
          priv_rpc_ret_t ret =
              _agent->requestVote(term, id, prevLogIndex, prevLogTerm, nullptr,
                                  timeoutMult);
          result.add("term", VPackValue(ret.term));
          result.add("voteGranted", VPackValue(ret.success));
        }
      } else if (suffixes[0] == "notifyAll") {  // notify
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        if (readValue("term", term) && readValue("agencyId", id)) {
          priv_rpc_ret_t ret = _agent->requestVote(
              term, id, 0, 0, _request->toVelocyPackBuilderPtr(), -1);
          result.add("term", VPackValue(ret.term));
          result.add("voteGranted", VPackValue(ret.success));
        } else {
          return reportBadQuery();  // bad query
        }
      } else if (suffixes[0] == "activate") {  // notify
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        query_t everything;
        try {
          everything = _request->toVelocyPackBuilderPtr();
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, Logger::AGENCY)
            << "Failure getting activation body:" <<  e.what();
        }
        try {
          query_t res = _agent->activate(everything);
          for (auto const& i : VPackObjectIterator(res->slice())) {
            result.add(i.key.copyString(),i.value);
          }
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, Logger::AGENCY) << "Activation failed: " << e.what();
        }
        
      } else if (suffixes[0] == "gossip") {
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        query_t query = _request->toVelocyPackBuilderPtr();
        try {
          query_t ret = _agent->gossip(query);
          for (auto const& obj : VPackObjectIterator(ret->slice())) {
            result.add(obj.key.copyString(), obj.value);
          }
        } catch (std::exception const& e) {
          return reportBadQuery(e.what());
        }
      } else if (suffixes[0] == "measure") {
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        auto query = _request->toVelocyPackBuilderPtr();
        try {
          _agent->reportMeasurement(query);
        } catch (std::exception const& e) {
          return reportBadQuery(e.what());
        }
      } else if (suffixes[0] == "activeAgents") {
        if (_request->requestType() != rest::RequestType::GET) {
          return reportMethodNotAllowed();
        }
        if (_agent->leaderID() != NO_LEADER) {
          result.add("active",
                     _agent->config().activeAgentsToBuilder()->slice());
        }
      } else if (suffixes[0] == "inform") {
        query_t query = _request->toVelocyPackBuilderPtr();
        try {
          _agent->notify(query);
        } catch (std::exception const& e) {
          return reportBadQuery(e.what());
        }
      } else {
        generateError(rest::ResponseCode::NOT_FOUND, 404);  // nothing else here
        return RestStatus::DONE;
      }
    }
    result.close();
    VPackSlice s = result.slice();
    generateResult(rest::ResponseCode::OK, s);
  } catch (...) {
    // Ignore this error
  }
  return RestStatus::DONE;
}
