////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/LogMacros.h"
#include "Rest/Version.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"

using namespace arangodb;

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::consensus;

////////////////////////////////////////////////////////////////////////////////
/// @brief Rest agency handler
////////////////////////////////////////////////////////////////////////////////

RestAgencyHandler::RestAgencyHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request,
                                     GeneralResponse* response, Agent* agent)
    : RestVocbaseBaseHandler(server, request, response), _agent(agent) {}

inline RestStatus RestAgencyHandler::reportErrorEmptyRequest() {
  LOG_TOPIC("46536", WARN, Logger::AGENCY)
      << "Empty request to public agency interface.";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyHandler::reportTooManySuffices() {
  LOG_TOPIC("ef6ae", WARN, Logger::AGENCY)
      << "Too many suffixes. Agency public interface takes one path.";
  generateError(rest::ResponseCode::NOT_FOUND, 404);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyHandler::reportUnknownMethod() {
  LOG_TOPIC("9b810", WARN, Logger::AGENCY)
      << "Public REST interface has no method " << _request->suffixes()[0];
  generateError(rest::ResponseCode::NOT_FOUND, 405);
  return RestStatus::DONE;
}

inline RestStatus RestAgencyHandler::reportMessage(rest::ResponseCode code,
                                                   std::string const& message) {
  LOG_TOPIC("8a454", DEBUG, Logger::AGENCY) << message;
  Builder body;
  {
    VPackObjectBuilder b(&body);
    body.add("message", VPackValue(message));
  }
  generateResult(code, body.slice());
  return RestStatus::DONE;
}

void RestAgencyHandler::redirectRequest(std::string const& leaderId) {
  try {
    std::string url = Endpoint::uriForm(_agent->config().poolAt(leaderId)) +
                      _request->requestPath();
    _response->setResponseCode(rest::ResponseCode::TEMPORARY_REDIRECT);
    _response->setHeaderNC(StaticStrings::Location, url);
    LOG_TOPIC("9b62c", DEBUG, Logger::AGENCY) << "Sending 307 redirect to " << url;
  } catch (std::exception const&) {
    reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE, "No leader");
  }
}

RestStatus RestAgencyHandler::handleTransient() {
  // Must be a POST request
  if (_request->requestType() != rest::RequestType::POST) {
    return reportMethodNotAllowed();
  }

  // Convert to velocypack
  query_t query;
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& e) {
    return reportMessage(rest::ResponseCode::BAD, e.what());
  }

  // Need Array input
  if (!query->slice().isArray()) {
    return reportMessage(rest::ResponseCode::BAD,
                         "Expecting array of arrays as body for writes");
  }

  // Empty request array
  if (query->slice().length() == 0) {
    return reportMessage(rest::ResponseCode::BAD, "Empty request");
  }

  // Leadership established?
  if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
    return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE, "No leader");
  }

  trans_ret_t ret;

  try {
    ret = _agent->transient(query);
  } catch (std::exception const& e) {
    return reportMessage(rest::ResponseCode::BAD, e.what());
  }

  // We're leading and handling the request
  if (ret.accepted) {
    generateResult((ret.failed == 0) ? rest::ResponseCode::OK : rest::ResponseCode::PRECONDITION_FAILED,
                   ret.result->slice());
  } else {  // Redirect to leader
    if (_agent->leaderID() == NO_LEADER) {
      return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE,
                           "No leader");
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
  }

  return RestStatus::DONE;
}


/// Poll from agency starting with start.
/// The result is of shape
/// { accepted:bool, result: { firstIndex, commitIndex, logs:[{index, log}] } }
/// if !accepted, lost leadership
/// else respond
RestStatus RestAgencyHandler::pollIndex(
  index_t const& start, double const& timeout) {
  auto pollResult = _agent->poll(start, timeout);
  
  if (std::get<1>(pollResult)) {
    return waitForFuture(
      std::move(std::get<0>(pollResult)).thenValue([this, start](std::shared_ptr<VPackBuilder>&& rb) {
        VPackSlice res = rb->slice();

        if (res.isObject() && res.hasKey("result")) {

          if (res.hasKey("error")) { // leadership loss
            generateError(
              rest::ResponseCode::SERVICE_UNAVAILABLE,
              TRI_ERROR_HTTP_SERVICE_UNAVAILABLE, "No leader");
            return;
          }

          VPackSlice slice = res.get("result");

          if (slice.hasKey("log")) {
            VPackBuilder builder;
            {
              VPackObjectBuilder ob(&builder);
              builder.add(StaticStrings::Error, VPackValue(false));
              builder.add("code", VPackValue(int(ResponseCode::OK)));
              builder.add(VPackValue("result"));
              VPackObjectBuilder r(&builder);
              if (!slice.get("firstIndex").isNumber()) {
                generateError(
                  rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, "invalid first log index.");
                return;
              } else if (slice.get("firstIndex").getNumber<uint64_t>() > start) {
                generateError(
                  rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_HTTP_SERVER_ERROR, "first log index is greater than requested.");
                return;
              }
              uint64_t firstIndex = slice.get("firstIndex").getNumber<uint64_t>(), i = 0;
              
              builder.add("commitIndex", slice.get("commitIndex"));
              VPackSlice logs = slice.get("log");
              if (start <= firstIndex) {
                builder.add("firstIndex", logs[i].get("index"));
              }
              builder.add(VPackValue("log"));
              VPackArrayBuilder a(&builder);
              if (start <= firstIndex) {
                uint64_t i = start - firstIndex;
                for (; i < logs.length(); ++i) {
                  builder.add(logs[i]);
                }
              }
            }
            generateResult(rest::ResponseCode::OK, std::move(*builder.steal()));
            return;
          } else {
            generateResult(rest::ResponseCode::OK, std::move(*rb->steal()));
            return;
          }
        } else {
          generateError(
            rest::ResponseCode::SERVICE_UNAVAILABLE,
            TRI_ERROR_HTTP_SERVICE_UNAVAILABLE, "No leader");
        }
      })
      .thenError<VPackException>([this](VPackException const& e) {
        generateError(Result{e.errorCode(), e.what()});
      })
      .thenError<std::exception>([this](std::exception const& e) {
        generateError(
          rest::ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR, e.what());
      }));
  } else {
    auto const& leader = std::get<2>(pollResult);
    if (leader == NO_LEADER) {
      generateError(
        rest::ResponseCode::SERVICE_UNAVAILABLE,
        TRI_ERROR_HTTP_SERVICE_UNAVAILABLE, "No leader");
    } else {
      redirectRequest(leader);
    }
    return RestStatus::DONE;
  }
  
}

namespace {
template <class T> static bool readValue(GeneralRequest const& req, char const* name, T& val) {
  bool found = true;
  std::string const& val_str = req.value(name, found);

  if (!found) {
    LOG_TOPIC("f4732", DEBUG, Logger::AGENCY)
      << "Query string " << name << " missing.";
    return false;
  } else {
    if (!arangodb::basics::StringUtils::toNumber(val_str, val)) {
      LOG_TOPIC("f4237", WARN, Logger::AGENCY)
        << "Conversion of query string " << name  << " with " << val_str << " to " << typeid(T).name() << " failed";
      return false;
    }
  }
  return true;
}}

RestStatus RestAgencyHandler::handlePoll() {

  // GET only
  if (_request->requestType() != rest::RequestType::GET) {
    return reportMethodNotAllowed();
  }

  // WARNING ////////////////////////////////////////////////////
  // Leader only
  auto const& leaderId = _agent->leaderID();
  if (!_agent->leading()) {  // Redirect to leader
    if (leaderId == NO_LEADER) {
      return reportMessage(
        rest::ResponseCode::SERVICE_UNAVAILABLE, "No leader");
    } else {
      TRI_ASSERT(leaderId != _agent->id());
      redirectRequest(leaderId);
      return RestStatus::DONE;
    }
  }

  // Get queryString index
  index_t index = 0;
  double timeout = 60.0;

  readValue(*_request, "index", index);
  readValue(*_request, "timeout", timeout);

  return pollIndex(index, timeout);

}


RestStatus RestAgencyHandler::handleStores() {

  if (_request->requestType() == rest::RequestType::GET) {
    Builder body;
    {
      VPackObjectBuilder b(&body);
      {
        _agent->executeLockedRead([&]() {
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
        });
        _agent->executeTransientLocked([&]() {
            body.add(VPackValue("transient"));
            {
              VPackArrayBuilder bb(&body);
              _agent->transient().dumpToBuilder(body);
            }
        });
      }
    }
    generateResult(rest::ResponseCode::OK, body.slice());
    return RestStatus::DONE;
  }

  return reportMethodNotAllowed();
}

RestStatus RestAgencyHandler::handleStore() {
  if (_request->requestType() == rest::RequestType::POST) {
    auto query = _request->toVelocyPackBuilderPtr();
    arangodb::consensus::index_t index = 0;

    try {
      index = query->slice().getUInt();
    } catch (...) {
      index = _agent->lastCommitted();
    }

    try {
      query_t builder = _agent->buildDB(index);
      generateResult(rest::ResponseCode::OK, builder->slice());
    } catch (...) {
      generateError(rest::ResponseCode::BAD, 400);
    }

    return RestStatus::DONE;
  }

  return reportMethodNotAllowed();
}

RestStatus RestAgencyHandler::handleWrite() {

  using namespace std::chrono;
  if (_request->requestType() != rest::RequestType::POST) {
    return reportMethodNotAllowed();
  }

  query_t query;

  // Convert to velocypack
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& e) {
    return reportMessage(rest::ResponseCode::BAD, e.what());
  }

  // Need Array input
  if (!query->slice().isArray()) {
    return reportMessage(rest::ResponseCode::BAD,
                         "Expecting array of arrays as body for writes");
  }

  // Empty request array
  if (query->slice().length() == 0) {
    return reportMessage(rest::ResponseCode::BAD, "Empty request");
  }

  // Leadership established?
  if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
    return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE, "No leader");
  }

  // Start timing
  auto const start = high_resolution_clock::now();

  // Do write
  write_ret_t ret;
  try {
    ret = _agent->write(query);
  } catch (std::exception const& e) {
    return reportMessage(rest::ResponseCode::BAD, e.what());
  }

  // We're leading and handling the request
  if (ret.accepted) {
    bool found;
    std::string call_mode = _request->header("x-arangodb-agency-mode", found);

    if (!found) {
      call_mode = "waitForCommitted";
    }

    size_t precondition_failed = 0;
    size_t forbidden = 0;

    Builder body;
    body.openObject();
    Agent::raft_commit_t result = Agent::raft_commit_t::OK;

    if (call_mode != "noWait") {
      // Note success/error
      body.add("results", VPackValue(VPackValueType::Array));
      for (auto const& index : ret.indices) {
        body.add(VPackValue(index));
      }
      for (auto const& a : ret.applied) {
        switch (a) {
          case APPLIED:
            break;
          case PRECONDITION_FAILED:
            ++precondition_failed;
            break;
          case FORBIDDEN:
            ++forbidden;
            break;
          default:
            break;
        }
      }
      body.close();

      // Wait for commit of highest except if it is 0?
      if (!ret.indices.empty() && call_mode == "waitForCommitted") {
        arangodb::consensus::index_t max_index = 0;
        try {
          max_index = *std::max_element(ret.indices.begin(), ret.indices.end());
        } catch (std::exception const& ex) {
          LOG_TOPIC("ac99c", WARN, Logger::AGENCY) << ex.what();
        }

        if (max_index > 0) {
          result = _agent->waitFor(max_index);
          _agent->commitHist().count(
            duration<float,std::milli>(high_resolution_clock::now()-start).count());
        }
      }
    }

    body.close();

    if (result == Agent::raft_commit_t::UNKNOWN) {
      generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_HTTP_SERVICE_UNAVAILABLE);
    } else if (result == Agent::raft_commit_t::TIMEOUT) {
      generateError(rest::ResponseCode::REQUEST_TIMEOUT, 408);
    } else {
      if (forbidden > 0) {
        generateResult(rest::ResponseCode::FORBIDDEN, body.slice());
      } else if (precondition_failed > 0) {  // Some/all requests failed
        generateResult(rest::ResponseCode::PRECONDITION_FAILED, body.slice());
      } else {  // All good
        generateResult(rest::ResponseCode::OK, body.slice());
      }
    }

  } else {  // Redirect to leader
    if (_agent->leaderID() == NO_LEADER) {
      return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE,
                           "No leader");
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
  }

  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::handleTransact() {
  if (_request->requestType() != rest::RequestType::POST) {
    return reportMethodNotAllowed();
  }

  query_t query;

  // Convert to velocypack
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& e) {
    return reportMessage(rest::ResponseCode::BAD, e.what());
  }

  // Need Array input
  if (!query->slice().isArray()) {
    return reportMessage(rest::ResponseCode::BAD,
                         "Expecting array of arrays as body for writes");
  }

  // Empty request array
  if (query->slice().length() == 0) {
    return reportMessage(rest::ResponseCode::BAD, "Empty request");
  }

  // Leadership established?
  if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
    return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE, "No leader");
  }

  // Do write
  trans_ret_t ret;
  try {
    ret = _agent->transact(query);
  } catch (std::exception const& e) {
    return reportMessage(rest::ResponseCode::BAD, e.what());
  }

  // We're leading and handling the request
  if (ret.accepted) {
    // Wait for commit of highest except if it is 0?
    if (ret.maxind > 0) {
      _agent->waitFor(ret.maxind);
    }
    generateResult((ret.failed == 0) ? rest::ResponseCode::OK : rest::ResponseCode::PRECONDITION_FAILED,
                   ret.result->slice());

  } else {  // Redirect to leader
    if (_agent->leaderID() == NO_LEADER) {
      return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE,
                           "No leader");
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
  }

  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::handleInquire() {
  if (_request->requestType() != rest::RequestType::POST) {
    return reportMethodNotAllowed();
  }

  query_t query;

  // Get query from body
  try {
    query = _request->toVelocyPackBuilderPtr();
  } catch (std::exception const& ex) {
    LOG_TOPIC("78755", DEBUG, Logger::AGENCY) << ex.what();
    generateError(rest::ResponseCode::BAD, 400);
    return RestStatus::DONE;
  }

  // Leadership established?
  if (_agent->leaderID() == NO_LEADER) {
    return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE, "No leader");
  }

  write_ret_t ret;
  try {
    ret = _agent->inquire(query);
  } catch (std::exception const& e) {
    return reportMessage(rest::ResponseCode::SERVER_ERROR, e.what());
  }

  if (ret.accepted) {  // I am leading

    bool found;
    std::string call_mode = _request->header("x-arangodb-agency-mode", found);
    if (!found) {
      call_mode = "waitForCommitted";
    }

    // First possibility: The answer is empty, we have never heard about
    // these transactions. In this case we say so, regardless what the
    // "agency-mode" is.
    // Second possibility: Non-empty answer, but agency-mode is "noWait",
    // then we simply report our findings, too.
    // Third possibility, we actually have a non-empty list of indices,
    // and we need to wait for commit to answer.

    // Handle cases 2 and 3:
    Agent::raft_commit_t result = Agent::raft_commit_t::OK;
    bool allCommitted = true;
    if (!ret.indices.empty()) {
      arangodb::consensus::index_t max_index = 0;
      try {
        max_index = *std::max_element(ret.indices.begin(), ret.indices.end());
      } catch (std::exception const& ex) {
        LOG_TOPIC("58732", WARN, Logger::AGENCY) << ex.what();
      }

      if (max_index > 0) {
        if (call_mode == "waitForCommitted") {
          result = _agent->waitFor(max_index);
        } else {
          allCommitted = _agent->isCommitted(max_index);
        }
      }
    }

    // We can now prepare the result:
    Builder body;
    bool failed = false;
    {
      VPackObjectBuilder b(&body);
      body.add(VPackValue("results"));
      {
        VPackArrayBuilder bb(&body);
        for (auto const& index : ret.indices) {
          body.add(VPackValue(index));
          failed = (failed || index == 0);
        }
      }
      body.add("inquired", VPackValue(true));
      if (!allCommitted) {  // can only happen in agency_mode "noWait"
        body.add("ongoing", VPackValue(true));
      }
    }

    if (result == Agent::raft_commit_t::UNKNOWN) {
      generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_HTTP_SERVICE_UNAVAILABLE);
    } else if (result == Agent::raft_commit_t::TIMEOUT) {
      generateError(rest::ResponseCode::REQUEST_TIMEOUT, 408);
    } else {
      if (failed) {  // Some/all requests failed
        generateResult(rest::ResponseCode::NOT_FOUND, body.slice());
      } else {  // All good (or indeed unknown in case 1)
        generateResult(rest::ResponseCode::OK, body.slice());
      }
    }
  } else {  // Redirect to leader
    if (_agent->leaderID() == NO_LEADER) {
      return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE,
                           "No leader");
    } else {
      TRI_ASSERT(ret.redirect != _agent->id());
      redirectRequest(ret.redirect);
    }
  }

  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::handleRead() {
  if (_request->requestType() == rest::RequestType::POST) {
    query_t query;
    try {
      query = _request->toVelocyPackBuilderPtr();
    } catch (std::exception const& e) {
      return reportMessage(rest::ResponseCode::BAD, e.what());
    }

    if (_agent->size() > 1 && _agent->leaderID() == NO_LEADER) {
      return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE,
                           "No leader");
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
        return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE,
                             "No leader");
      } else {
        redirectRequest(ret.redirect);
      }
    }
    return RestStatus::DONE;
  }

  return reportMethodNotAllowed();
}

RestStatus RestAgencyHandler::handleConfig() {
  LOG_TOPIC("eda22", DEBUG, Logger::AGENCY) << "handleConfig start";
  // Update endpoint of peer
  if (_request->requestType() == rest::RequestType::POST) {
    try {
      _agent->updatePeerEndpoint(_request->toVelocyPackBuilderPtr());
    } catch (std::exception const& e) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, e.what());
      return RestStatus::DONE;
    }
  }
  else if (_request->requestType() == rest::RequestType::PUT) {
    try {
      _agent->updateSomeConfigValues(_request->toVelocyPackBuilderPtr()->slice());
    } catch (std::exception const& e) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, e.what());
      return RestStatus::DONE;
    }
  }

  // Respond with configuration
  auto last = _agent->lastCommitted();
  LOG_TOPIC("55412", DEBUG, Logger::AGENCY) << "handleConfig after lastCommitted";
  Builder body;
  {
    VPackObjectBuilder b(&body);
    body.add("term", Value(_agent->term()));
    body.add("leaderId", Value(_agent->leaderID()));
    body.add("commitIndex", Value(last));
    _agent->lastAckedAgo(body);
    LOG_TOPIC("ddeea", DEBUG, Logger::AGENCY) << "handleConfig after lastAckedAgo";
    body.add("configuration", _agent->config().toBuilder()->slice());
    body.add("engine", VPackValue(EngineSelectorFeature::engineName()));
    body.add("version", VPackValue(ARANGODB_VERSION));
  }

  LOG_TOPIC("76543", DEBUG, Logger::AGENCY) << "handleConfig after before generateResult";
  generateResult(rest::ResponseCode::OK, body.slice());

  LOG_TOPIC("77891", DEBUG, Logger::AGENCY) << "handleConfig after before done";
  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::handleState() {
  bool found;
  std::string const& val_str = _request->value("redirectToLeader", found);
  bool redirectToLeader = found && val_str == "true";

  // Potentially look at leadership:
  if (redirectToLeader) {
    if (_agent->size() > 1) {
      auto leader = _agent->leaderID();
      if (leader == NO_LEADER) {
        return reportMessage(rest::ResponseCode::SERVICE_UNAVAILABLE,
                             "No leader");
      }
      if (leader != _agent->id()) {
        redirectRequest(leader);
        return RestStatus::DONE;
      }
    }
  }
  // Go ahead, either as leader or, if allowed, as follower.

  VPackBuilder body;
  {
    VPackObjectBuilder o(&body);
    _agent->readDB(body);
  }
  auto ctx = std::make_shared<transaction::StandaloneContext>(_vocbase);
  generateResult(rest::ResponseCode::OK, body.slice(), ctx->getVPackOptionsForDump());
  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::reportMethodNotAllowed() {
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
  return RestStatus::DONE;
}

RestStatus RestAgencyHandler::execute() {
  response()->setAllowCompression(true);
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
      } else if (suffixes[0] == "poll") {
        return handlePoll();
      } else if (suffixes[0] == "inquire") {
        return handleInquire();
      } else if (suffixes[0] == "transient") {
        return handleTransient();
      } else if (suffixes[0] == "transact") {
        return handleTransact();
      } else if (suffixes[0] == "config") {
        if (_request->requestType() != rest::RequestType::GET &&
            _request->requestType() != rest::RequestType::PUT &&
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
