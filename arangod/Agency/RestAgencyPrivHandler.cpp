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
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

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

RestAgencyPrivHandler::RestAgencyPrivHandler(GeneralRequest* request,
                                             GeneralResponse* response,
                                             Agent* agent)
    : RestBaseHandler(request, response), _agent(agent) {}

bool RestAgencyPrivHandler::isDirect() const { return false; }

inline RestHandler::status RestAgencyPrivHandler::reportErrorEmptyRequest() {
  LOG_TOPIC(WARN, Logger::AGENCY) << "Empty request to agency!";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestHandler::status::DONE;
}

inline RestHandler::status RestAgencyPrivHandler::reportTooManySuffices() {
  LOG_TOPIC(WARN, Logger::AGENCY)
      << "Agency handles a single suffix: vote, log or configure";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestHandler::status::DONE;
}

inline RestHandler::status RestAgencyPrivHandler::reportBadQuery(
    std::string const& message) {
  generateError(rest::ResponseCode::BAD, 400, message);
  return RestHandler::status::DONE;
}

inline RestHandler::status RestAgencyPrivHandler::reportMethodNotAllowed() {
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  return RestHandler::status::DONE;
}

inline RestHandler::status RestAgencyPrivHandler::reportGone() {
  generateError(rest::ResponseCode::GONE, 410);
  return RestHandler::status::DONE;
}

RestHandler::status RestAgencyPrivHandler::execute() {
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
      std::string id;  // leaderId for appendEntries, cadidateId for requestVote
      arangodb::consensus::index_t prevLogIndex, leaderCommit;
      if (_request->suffix()[0] == "appendEntries") {  // appendEntries
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        if (readValue("term", term) && readValue("leaderId", id) &&
            readValue("prevLogIndex", prevLogIndex) &&
            readValue("prevLogTerm", prevLogTerm) &&
            readValue("leaderCommit", leaderCommit)) {  // found all values
          bool ret = _agent->recvAppendEntriesRPC(
              term, id, prevLogIndex, prevLogTerm, leaderCommit,
              _request->toVelocyPackBuilderPtr(&opts));
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
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        if (readValue("term", term) && readValue("agencyId", id)) {
          priv_rpc_ret_t ret = _agent->requestVote(
              term, id, 0, 0, _request->toVelocyPackBuilderPtr(&opts));
          result.add("term", VPackValue(ret.term));
          result.add("voteGranted", VPackValue(ret.success));
        } else {
          return reportBadQuery();  // bad query
        }
      } else if (_request->suffix()[0] == "activate") {  // notify
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        arangodb::velocypack::Options options;
        query_t everything;
        try {
          everything = _request->toVelocyPackBuilderPtr(&options);
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
        
      } else if (_request->suffix()[0] == "gossip") {
        if (_request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        arangodb::velocypack::Options options;
        query_t query = _request->toVelocyPackBuilderPtr(&options);
        try {
          query_t ret = _agent->gossip(query);
          result.add("id", ret->slice().get("id"));
          result.add("endpoint", ret->slice().get("endpoint"));
          result.add("pool", ret->slice().get("pool"));
        } catch (std::exception const& e) {
          return reportBadQuery(e.what());
        }
      } else if (_request->suffix()[0] == "activeAgents") {
        if (_request->requestType() != rest::RequestType::GET) {
          return reportMethodNotAllowed();
        }
        if (_agent->leaderID() != NO_LEADER) {
          result.add("active",
                     _agent->config().activeAgentsToBuilder()->slice());
        }
      } else if (_request->suffix()[0] == "inform") {
        arangodb::velocypack::Options options;
        query_t query = _request->toVelocyPackBuilderPtr(&options);
        try {
          _agent->notify(query);
        } catch (std::exception const& e) {
          return reportBadQuery(e.what());
        }
      } else {
        generateError(rest::ResponseCode::NOT_FOUND, 404);  // nothing else here
        return RestHandler::status::DONE;
      }
    }
    result.close();
    VPackSlice s = result.slice();
    generateResult(rest::ResponseCode::OK, s);
  } catch (...) {
    // Ignore this error
  }
  return RestHandler::status::DONE;
}
