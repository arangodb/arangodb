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

#include "RestAgencyHandler.h"

#include <thread>

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/Agent.h"
#include "Basics/StaticStrings.h"
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

RestAgencyHandler::RestAgencyHandler(GeneralRequest* request,
                                     GeneralResponse* response, Agent* agent)
    : RestBaseHandler(request, response), _agent(agent) {}

bool RestAgencyHandler::isDirect() const { return false; }

inline RestStatus RestAgencyHandler::reportErrorEmptyRequest() {
  LOG_TOPIC(WARN, Logger::AGENCY)
      << "Empty request to public agency interface.";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyHandler::reportTooManySuffices() {
  LOG_TOPIC(WARN, Logger::AGENCY)
      << "Too many suffixes. Agency public interface takes one path.";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyHandler::reportUnknownMethod() {
  LOG_TOPIC(WARN, Logger::AGENCY) << "Public REST interface has no method "
                                  << _request->suffixes()[0];
  generateError(rest::ResponseCode::NOT_FOUND, 405);
  return RestStatus::DONE;
}

void RestAgencyHandler::redirectRequest(std::string const& leaderId) {
  try {
    std::string url = Endpoint::uriForm(_agent->config().poolAt(leaderId)) +
                      _request->requestPath();
    _response->setResponseCode(rest::ResponseCode::TEMPORARY_REDIRECT);
    _response->setHeaderNC(StaticStrings::Location, url);
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Sending 307 redirect to " << url;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::AGENCY) << e.what() << " " << __FILE__ << ":"
                                    << __LINE__;
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  e.what());
  }
}

RestStatus RestAgencyHandler::handleTransient() {

  // Must be a POST request
  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  }

  // Convert to velocypack
  query_t query;
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << e.what() << " " << __FILE__ << ":" << __LINE__;
    Builder body;
    body.openObject();
    body.add("message", VPackValue(e.what()));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // Need Array input
  if (!query->slice().isArray()) {
    Builder body;
    body.openObject();
    body.add(
      "message", VPackValue("Expecting array of arrays as body for writes"));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }
  
  // Empty request array
  if (query->slice().length() == 0) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue("Empty request."));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }
  
  // Leadership established?
  if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue("No leader"));
    body.close();
    generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
    return RestStatus::DONE;
  }
  
  trans_ret_t ret;

  try {
    ret = _agent->transient(query);
  } catch (std::exception const& e) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue(e.what()));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // We're leading and handling the request
  if (ret.accepted) {  

    generateResult(
      (ret.failed==0) ?
      rest::ResponseCode::OK : rest::ResponseCode::PRECONDITION_FAILED,
      ret.result->slice());
      
  } else {            // Redirect to leader
    if (_agent->leaderID() == NO_LEADER) {
      Builder body;
      body.openObject();
      body.add("message", VPackValue("No leader"));
      body.close();
      generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
      return RestStatus::DONE;
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
  }

  return RestStatus::DONE;
  
}

RestStatus RestAgencyHandler::handleStores() {
  if (_request->requestType() == rest::RequestType::GET) {
    Builder body;
    {
      VPackObjectBuilder b(&body);
      {
        _agent->executeLocked([&]() {
          body.add(VPackValue("spearhead"));
          { 
            VPackArrayBuilder bb(&body);
            _agent->spearhead().dumpToBuilder(body);
          }
          body.add(VPackValue("read_db"));
          { 
            VPackArrayBuilder bb(&body);
            _agent->readDB().dumpToBuilder(body);
          }
          body.add(VPackValue("transient"));
          { 
            VPackArrayBuilder bb(&body);
            _agent->transient().dumpToBuilder(body);
          }
        });
      }
    }
    generateResult(rest::ResponseCode::OK, body.slice());
  } else {
    generateError(rest::ResponseCode::BAD, 400);
  }
  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::handleStore() {

  if (_request->requestType() == rest::RequestType::POST) {

    auto query = _request->toVelocyPackBuilderPtr();
    arangodb::consensus::index_t index = 0;

    try {
      index = query->slice().getUInt();
    } catch (...) {
      index = _agent->lastCommitted().second;
    }
    
    try {
      query_t builder = _agent->buildDB(index);
      generateResult(rest::ResponseCode::OK, builder->slice());
    } catch (...) {
      generateError(rest::ResponseCode::BAD, 400);
    }
    
  } else {
    generateError(rest::ResponseCode::BAD, 400);
  }
  
  return RestStatus::DONE;
  
}

RestStatus RestAgencyHandler::handleWrite() {

  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  }
  
  query_t query;

  // Convert to velocypack
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << e.what() << " " << __FILE__ << ":" << __LINE__;
    Builder body;
    body.openObject();
    body.add("message", VPackValue(e.what()));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // Need Array input
  if (!query->slice().isArray()) {
    Builder body;
    body.openObject();
    body.add(
      "message", VPackValue("Expecting array of arrays as body for writes"));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // Empty request array
  if (query->slice().length() == 0) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue("Empty request."));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // Leadership established?
  if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue("No leader"));
    body.close();
    generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
    return RestStatus::DONE;
  }

  // Do write
  write_ret_t ret;
  try {
    ret = _agent->write(query);
  } catch (std::exception const& e) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Malformed write query " << query;
    Builder body;
    body.openObject();
    body.add("message",
             VPackValue(std::string("Malformed write query") + e.what()));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // We're leading and handling the request
  if (ret.accepted) {  

    bool found;
    std::string call_mode = _request->header("x-arangodb-agency-mode", found);
    if (!found) {
      call_mode = "waitForCommitted";
    }
    
    size_t errors = 0;
    Builder body;
    body.openObject();
    Agent::raft_commit_t result = Agent::raft_commit_t::OK;
    
    if (call_mode != "noWait") {
      // Note success/error
      body.add("results", VPackValue(VPackValueType::Array));
      for (auto const& index : ret.indices) {
        body.add(VPackValue(index));
        if (index == 0) {
          errors++;
        }
      }
      body.close();
      
      // Wait for commit of highest except if it is 0?
      if (!ret.indices.empty() && call_mode == "waitForCommitted") {
        arangodb::consensus::index_t max_index = 0;
        try {
          max_index =
            *std::max_element(ret.indices.begin(), ret.indices.end());
        } catch (std::exception const& e) {
          LOG_TOPIC(WARN, Logger::AGENCY)
            << e.what() << " " << __FILE__ << ":" << __LINE__;
        }
        
        if (max_index > 0) {
          result = _agent->waitFor(max_index);
        }
        
      }
    }
    
    body.close();

    if (result == Agent::raft_commit_t::UNKNOWN) {
      generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
    } else if (result == Agent::raft_commit_t::TIMEOUT) {
      generateResult(rest::ResponseCode::REQUEST_TIMEOUT, body.slice());
    } else {
      if (errors > 0) { // Some/all requests failed
        generateResult(rest::ResponseCode::PRECONDITION_FAILED, body.slice());
      } else {          // All good
        generateResult(rest::ResponseCode::OK, body.slice());
      }
    }
    
  } else {            // Redirect to leader
    if (_agent->leaderID() == NO_LEADER) {
      Builder body;
      body.openObject();
      body.add("message", VPackValue("No leader"));
      body.close();
      generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
      return RestStatus::DONE;
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
  }

  return RestStatus::DONE;
  
}



RestStatus RestAgencyHandler::handleTransact() {

  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  }
  
  query_t query;

  // Convert to velocypack
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << e.what() << " " << __FILE__ << ":" << __LINE__;
    Builder body;
    body.openObject();
    body.add("message", VPackValue(e.what()));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // Need Array input
  if (!query->slice().isArray()) {
    Builder body;
    body.openObject();
    body.add(
      "message", VPackValue("Expecting array of arrays as body for writes"));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // Empty request array
  if (query->slice().length() == 0) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue("Empty request."));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // Leadership established?
  if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue("No leader"));
    body.close();
    generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
    return RestStatus::DONE;
  }

  // Do write
  trans_ret_t ret;
  try {
    ret = _agent->transact(query);
  } catch (std::exception const& e) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Malformed write query " << query;
    Builder body;
    body.openObject();
    body.add(
      "message", VPackValue(std::string("Malformed write query") + e.what()));
    body.close();
    generateResult(rest::ResponseCode::BAD, body.slice());
    return RestStatus::DONE;
  }

  // We're leading and handling the request
  if (ret.accepted) {  

    // Wait for commit of highest except if it is 0?
    if (ret.maxind > 0) {
      _agent->waitFor(ret.maxind);
    }
    generateResult(
      (ret.failed==0) ?
        rest::ResponseCode::OK : rest::ResponseCode::PRECONDITION_FAILED,
      ret.result->slice());
    
  } else {            // Redirect to leader
    if (_agent->leaderID() == NO_LEADER) {
      Builder body;
      body.openObject();
      body.add("message", VPackValue("No leader"));
      body.close();
      generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
      return RestStatus::DONE;
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
  }

  return RestStatus::DONE;
  
}


inline RestStatus RestAgencyHandler::handleInquire() {

  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  }
  
  query_t query;

  // Get query from body
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& e) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << e.what() << " " << __FILE__ << ":" << __LINE__;
    generateError(rest::ResponseCode::BAD, 400);
    return RestStatus::DONE;
  }
  
  // Leadership established?
  if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
    Builder body;
    body.openObject();
    body.add("message", VPackValue("No leader"));
    body.close();
    generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
    return RestStatus::DONE;
  }
  
  inquire_ret_t ret;
  try {
    ret = _agent->inquire(query);
  } catch (std::exception const& e) {
    generateError(
      rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, e.what());
    return RestStatus::DONE;
  }
  
  if (ret.accepted) {  // I am leading

    generateResult(rest::ResponseCode::OK, ret.result->slice());
    
  } else {  // Redirect to leader
    
    if (_agent->leaderID() == NO_LEADER) {
      Builder body;
      body.openObject();
      body.add("message", VPackValue("No leader"));
      body.close();
      generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
      return RestStatus::DONE;
      
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
    
    return RestStatus::DONE;
     
  }
  
  return RestStatus::DONE;
  
}

inline RestStatus RestAgencyHandler::handleRead() {
  if (_request->requestType() == rest::RequestType::POST) {
    query_t query;
    try {
      query = _request->toVelocyPackBuilderPtr();
    } catch (std::exception const& e) {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << e.what() << " " << __FILE__ << ":" << __LINE__;
      generateError(rest::ResponseCode::BAD, 400);
      return RestStatus::DONE;
    }

    if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
      Builder body;
      body.openObject();
      body.add("message", VPackValue("No leader"));
      body.close();
      generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
      return RestStatus::DONE;
    }

    read_ret_t ret = _agent->read(query);

    if (ret.accepted) {  // I am leading
      if (ret.success.size() == 1 && !ret.success.at(0)) {
        generateResult(rest::ResponseCode::I_AM_A_TEAPOT, ret.result->slice());
      } else {
        generateResult(rest::ResponseCode::OK, ret.result->slice());
      }
    } else {  // Redirect to leader
      if (_agent->leaderID() == NO_LEADER) {
        Builder body;
        body.openObject();
        body.add("message", VPackValue("No leader"));
        body.close();
        generateResult(rest::ResponseCode::SERVICE_UNAVAILABLE, body.slice());
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "We don't know who the leader is";
        return RestStatus::DONE;

      } else {
        TRI_ASSERT(ret.redirect != _agent->id());
        redirectRequest(ret.redirect);
      }
      return RestStatus::DONE;
    }
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
    return RestStatus::DONE;
  }
  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::handleConfig() {

  // Update endpoint of peer
  if (_request->requestType() == rest::RequestType::POST) {
    try {
      _agent->updatePeerEndpoint(_request->toVelocyPackBuilderPtr());
    } catch (std::exception const& e) {
      generateError(
        rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, e.what());
      return RestStatus::DONE;
    }
  }

  // Respond with configuration
  auto last = _agent->lastCommitted();
  Builder body;
  {
    VPackObjectBuilder b(&body);
    body.add("term", Value(_agent->term()));
    body.add("leaderId", Value(_agent->leaderID()));
    body.add("lastApplied", Value(last.first));
    body.add("commitIndex", Value(last.second));
    body.add("lastAcked", _agent->lastAckedAgo()->slice());
    body.add("configuration", _agent->config().toBuilder()->slice());
  }
  
  generateResult(rest::ResponseCode::OK, body.slice());
  
  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::handleState() {
  Builder body;
  body.add(VPackValue(VPackValueType::Array));
  for (auto const& i : _agent->state().get()) {
    body.add(VPackValue(VPackValueType::Object));
    body.add("index", VPackValue(i.index));
    body.add("term", VPackValue(i.term));
    body.add("query", VPackSlice(i.entry->data()));
    body.add("clientId", VPackValue(i.clientId));
    body.close();
  }
  body.close();
  generateResult(rest::ResponseCode::OK, body.slice());
  return RestStatus::DONE;
}

inline RestStatus RestAgencyHandler::reportMethodNotAllowed() {
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::execute() {
  try {
    auto const& suffixes = _request->suffixes();
    if (suffixes.empty()) {  // Empty request
      return reportErrorEmptyRequest();
    } else if (suffixes.size() > 1) {  // path size >= 2
      return reportTooManySuffices();
    } else {
      if (suffixes[0] == "write") {
        return handleWrite();
      } else if (suffixes[0] == "read") {
        return handleRead();
      } else if (suffixes[0] == "inquire") {
        return handleInquire();
      } else if (suffixes[0] == "transient") {
        return handleTransient();
      } else if (suffixes[0] == "transact") {
        return handleTransact();
      } else if (suffixes[0] == "config") {
        if (_request->requestType() != rest::RequestType::GET &&
            _request->requestType() != rest::RequestType::POST) {
          return reportMethodNotAllowed();
        }
        return handleConfig();
      } else if (suffixes[0] == "state") {
        if (_request->requestType() != rest::RequestType::GET) {
          return reportMethodNotAllowed();
        }
        return handleState();
      } else if (suffixes[0] == "stores") {
        return handleStores();
      } else if (suffixes[0] == "store") {
        return handleStore();
      } else {
        return reportUnknownMethod();
      }
    }
  } catch (...) {
    // Ignore this error
  }
  return RestStatus::DONE;
}
