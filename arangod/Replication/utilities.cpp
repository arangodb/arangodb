////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "utilities.h"

#include <string>
#include <unordered_map>

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Mutex.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/Syncer.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/UrlHelper.h"

struct TRI_vocbase_t;

namespace {
/// @brief handle the state response of the leader
arangodb::Result handleLeaderStateResponse(arangodb::replutils::Connection const& connection,
                                           arangodb::replutils::LeaderInfo& leader,
                                           arangodb::velocypack::Slice const& slice,
                                           char const* context) {
  using arangodb::Result;
  using arangodb::velocypack::Slice;

  // note: context can be a nullptr

  std::string const endpointString = " from endpoint '" + leader.endpoint + "'";

  // process "state" section
  Slice const state = slice.get("state");
  if (!state.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("state section is missing in response") + endpointString);
  }

  // state."lastLogTick"
  Slice tick = state.get("lastLogTick");
  if (!tick.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("lastLogTick is missing in response") + endpointString);
  }

  TRI_voc_tick_t const lastLogTick =
      arangodb::basics::VelocyPackHelper::stringUInt64(tick);
  if (lastLogTick == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("lastLogTick is 0 in response") + endpointString);
  }

  // process "server" section
  Slice const server = slice.get("server");
  if (!server.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("server section is missing in response") + endpointString);
  }

  // server."version"
  Slice const version = server.get("version");
  if (!version.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("server version is missing in response") + endpointString);
  }

  // server."serverId"
  Slice const serverId = server.get("serverId");
  if (!serverId.isString()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("server id is missing in response") + endpointString);
  }

  // validate all values we got
  std::string const leaderIdString(serverId.copyString());
  arangodb::ServerId const leaderId{arangodb::basics::StringUtils::uint64(leaderIdString)};
  if (leaderId.empty()) {
    // invalid leader id
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("invalid server id in response") + leaderIdString);
  }

  if (leaderIdString == connection.localServerId()) {
    // leader and replica are the same instance. this is not supported.
    return Result(TRI_ERROR_REPLICATION_LOOP,
                  std::string("got same server id (") +
                      connection.localServerId() + ")" + endpointString +
                      " as the local applier server's id");
  }

  // server."engine"
  if (Slice engine = server.get("engine"); engine.isString()) {
    // the attribute "engine" is optional, as it was introduced later
    leader.engine = engine.copyString();
  } else {
    leader.engine = "unknown";
  }

  std::string const versionString(version.copyString());
  auto v = arangodb::rest::Version::parseVersionString(versionString);
  int major = v.first;
  int minor = v.second;

  if (major != 3) {
    // we can connect to 3.x only
    return Result(TRI_ERROR_REPLICATION_LEADER_INCOMPATIBLE,
                  std::string("got incompatible leader version") +
                      endpointString + ": '" + versionString + "'");
  }

  leader.majorVersion = major;
  leader.minorVersion = minor;
  leader.serverId = leaderId;
  leader.lastLogTick = lastLogTick;

  if (context == nullptr) {
    LOG_TOPIC("6c920", INFO, arangodb::Logger::REPLICATION)
      << "connected to leader at " << leader.endpoint 
      << ", id " << leader.serverId.id() 
      << ", version " << leader.majorVersion << "." << leader.minorVersion 
      << ", last log tick " << leader.lastLogTick
      << ", engine " << leader.engine;
  } else {
    LOG_TOPIC("6c921", INFO, arangodb::Logger::REPLICATION)
      << "connected to leader at " << leader.endpoint 
      << ", version " << leader.majorVersion << "." << leader.minorVersion 
      << ": " << context;
  }

  return {};
}
}  // namespace

namespace arangodb {
namespace replutils {

std::string const ReplicationUrl = "/_api/replication";

Connection::Connection(Syncer* syncer, ReplicationApplierConfiguration const& applierConfig)
    : _endpointString{applierConfig._endpoint},
      _localServerId{basics::StringUtils::itoa(ServerIdFeature::getId().id())},
      _clientInfo{applierConfig._clientInfoString} {
  std::unique_ptr<httpclient::GeneralClientConnection> connection;
  std::unique_ptr<Endpoint> endpoint{Endpoint::clientFactory(_endpointString)};
  if (endpoint != nullptr) {
    connection.reset(httpclient::GeneralClientConnection::factory(
        applierConfig._server, endpoint, applierConfig._requestTimeout,
        applierConfig._connectTimeout, static_cast<size_t>(applierConfig._maxConnectRetries),
        static_cast<uint32_t>(applierConfig._sslProtocol)));
  }

  if (connection != nullptr) {
    std::string retryMsg =
        std::string("retrying failed HTTP request for endpoint '") +
        applierConfig._endpoint + std::string("' for replication applier");
    std::string databaseName = applierConfig._database;
    if (!databaseName.empty()) {
      retryMsg +=
          std::string(" in database '") + databaseName + std::string("'");
    }

    httpclient::SimpleHttpClientParams params(applierConfig._requestTimeout, false);
    params.setMaxRetries(2);
    params.setRetryWaitTime(2 * 1000 * 1000);  // 2s
    params.setRetryMessage(retryMsg);

    std::string username = applierConfig._username;
    std::string password = applierConfig._password;
    if (!username.empty()) {
      params.setUserNamePassword("/", username, password);
    } else {
      params.setJwt(applierConfig._jwt);
    }
    params.setMaxPacketSize(applierConfig._maxPacketSize);
    params.setLocationRewriter(syncer, &(syncer->rewriteLocation));
    _client.reset(new httpclient::SimpleHttpClient(connection, params));
  }
}

bool Connection::valid() const { return (_client != nullptr); }

std::string const& Connection::endpoint() const { return _endpointString; }

std::string const& Connection::localServerId() const { return _localServerId; }

std::string const& Connection::clientInfo() const { return _clientInfo; }

void Connection::setAborted(bool value) {
  if (_client) {
    _client->setAborted(value);
  }
}

bool Connection::isAborted() const {
  if (_client) {
    return _client->isAborted();
  }
  return true;
}

ProgressInfo::ProgressInfo(Setter s) : _setter{s} {}

void ProgressInfo::set(std::string const& msg) {
  std::lock_guard<std::mutex> guard(_mutex);
  _setter(msg);
}

constexpr double BatchInfo::DefaultTimeout;

/// @brief send a "start batch" command
/// @param patchCount try to patch count of this collection
///        only effective with the incremental sync (optional)
Result BatchInfo::start(replutils::Connection const& connection,
                        replutils::ProgressInfo& progress, replutils::LeaderInfo& leader,
                        SyncerId const& syncerId, char const* context,
                        std::string const& patchCount) {
  // TODO make sure all callers verify not child syncer
  if (!connection.valid()) {
    return {TRI_ERROR_INTERNAL};
  }

  double const now = TRI_microtime();
  id = 0;

  // SimpleHttpClient automatically add database prefix
  std::string const url = [&]() {
    using namespace url;
    std::string const path{ReplicationUrl + "/batch"};
    QueryParameters parameters;
    parameters.add("serverId", connection.localServerId());
    if (syncerId.value != 0) {
      parameters.add("syncerId", syncerId.toString());
    }
    if (!connection.clientInfo().empty()) {
      parameters.add("clientInfo", connection.clientInfo());
    }
    if (!leader.serverId.isSet()) {
      // if we haven't fetched the leader state yet, fetch it now.
      // in the ideal case, this can save us one request
      parameters.add("state", "true");
    }
    return Location(Path{path}, Query{parameters}, std::nullopt).toString();
  }();

  VPackBuilder b;
  {
    VPackObjectBuilder guard(&b, true);
    guard->add("ttl", VPackValue(ttl));
    if (!patchCount.empty()) {
      guard->add("patchCount", VPackValue(patchCount));
    }
  }
  std::string const body = b.toJson();

  progress.set("sending batch start command to url " + url);
  // send request
  std::unique_ptr<httpclient::SimpleHttpResult> response;
  connection.lease([&](httpclient::SimpleHttpClient* client) {
    auto headers = replutils::createHeaders();
    response.reset(client->retryRequest(rest::RequestType::POST, url,
                                        body.c_str(), body.size(), headers));
  });

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url, connection);
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());

  if (r.fail()) {
    return r;
  }

  VPackSlice slice = builder.slice();
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "start batch response is not an object");
  }
 
  if (!leader.serverId.isSet()) {
    // if we don't have any info about the leader state yet, fetch the
    // info from the response now and update our state
    if (VPackSlice state = slice.get("state"); state.isObject()) {
      r = ::handleLeaderStateResponse(connection, leader, state, context);
      if (r.fail()) {
        return r;
      }
    }
  } 

  std::string const batchId =
      basics::VelocyPackHelper::getStringValue(slice, "id", "");
  if (batchId.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "start batch id is missing in response");
  }
  id = basics::StringUtils::uint64(batchId);

  std::string const lastTick =
      basics::VelocyPackHelper::getStringValue(slice, "lastTick", "");
  if (lastTick.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "start batch lastTick is missing in response");
  }

  leader.lastLogTick = basics::StringUtils::uint64(lastTick);
  updateTime = now;

  return Result();
}

/// @brief send an "extend batch" command
Result BatchInfo::extend(replutils::Connection const& connection,
                         replutils::ProgressInfo& progress, SyncerId const syncerId) {
  if (id == 0) {
    return Result();
  } else if (!connection.valid()) {
    return {TRI_ERROR_INTERNAL};
  }

  double const now = TRI_microtime();

  if (now <= updateTime + ttl * 0.25) {
    // no need to extend the batch yet - only extend it if a quarter of its ttl
    // is already over
    return Result();
  }

  std::string const url = [&]() {
    using namespace url;
    std::string const path{ReplicationUrl + "/batch/" + basics::StringUtils::itoa(id)};
    QueryParameters parameters;
    parameters.add("serverId", connection.localServerId());
    if (syncerId.value != 0) {
      parameters.add("syncerId", syncerId.toString());
    }
    if (!connection.clientInfo().empty()) {
      parameters.add("clientInfo", connection.clientInfo());
    }
    return Location(Path{path}, Query{parameters}, std::nullopt).toString();
  }();
  std::string const body = "{\"ttl\":" + basics::StringUtils::itoa(ttl) + "}";
  progress.set("sending batch extend command to url " + url);

  // send request
  std::unique_ptr<httpclient::SimpleHttpResult> response;
  connection.lease([&](httpclient::SimpleHttpClient* client) {
    if (id == 0) {
      return;
    }
    auto headers = replutils::createHeaders();
    response.reset(client->request(rest::RequestType::PUT, url, body.c_str(), body.size(), headers));
  });

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url, connection);
  }

  updateTime = now;

  return Result();
}

/// @brief send a "finish batch" command
Result BatchInfo::finish(replutils::Connection const& connection,
                         replutils::ProgressInfo& progress, SyncerId const syncerId) {
  if (id == 0) {
    return Result();
  } else if (!connection.valid()) {
    return {TRI_ERROR_INTERNAL};
  }

  try {
    std::string const url = [&]() {
      using namespace url;
      std::string const path{ReplicationUrl + "/batch/"  + basics::StringUtils::itoa(id)};
      QueryParameters parameters;
      parameters.add("serverId", connection.localServerId());
      if (syncerId.value != 0) {
        parameters.add("syncerId", syncerId.toString());
      }
      if (!connection.clientInfo().empty()) {
        parameters.add("clientInfo", connection.clientInfo());
      }
      return Location(Path{path}, Query{parameters}, std::nullopt).toString();
    }();
    progress.set("sending batch finish command to url " + url);

    // send request
    std::unique_ptr<httpclient::SimpleHttpResult> response;
    connection.lease([&](httpclient::SimpleHttpClient* client) {
      auto headers = replutils::createHeaders();
      response.reset(client->retryRequest(rest::RequestType::DELETE_REQ, url, nullptr, 0, headers));
    });

    if (hasFailed(response.get())) {
      return buildHttpError(response.get(), url, connection);
    }

    id = 0;
    updateTime = 0;
    return Result();
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
}

LeaderInfo::LeaderInfo(ReplicationApplierConfiguration const& /*applierConfig*/) {}

uint64_t LeaderInfo::version() const {
  return majorVersion * 10000 + minorVersion * 100;
}

/// @brief get leader state
Result LeaderInfo::getState(replutils::Connection& connection, bool isChildSyncer, char const* context) {
  if (isChildSyncer) {
    TRI_ASSERT(endpoint.empty());
    TRI_ASSERT(serverId.isSet());
    TRI_ASSERT(majorVersion != 0);
    return Result();
  }

  std::string const url =
      ReplicationUrl + "/logger-state?serverId=" + connection.localServerId();

  std::unique_ptr<httpclient::SimpleHttpResult> response;
  connection.lease([&](httpclient::SimpleHttpClient* client) {
    // store old settings
    size_t maxRetries = client->params().getMaxRetries();
    uint64_t retryWaitTime = client->params().getRetryWaitTime();

    // apply settings that prevent endless waiting here
    client->params().setMaxRetries(1);
    client->params().setRetryWaitTime(500 * 1000);  // 0.5s

    auto headers = replutils::createHeaders();
    response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0, headers));

    // restore old settings
    client->params().setMaxRetries(maxRetries);
    client->params().setRetryWaitTime(retryWaitTime);
  });

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url, connection);
  }

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  Result r = parseResponse(builder, response.get());
  if (r.fail()) {
    return r;
  }

  VPackSlice slice = builder.slice();

  if (!slice.isObject()) {
    LOG_TOPIC("22327", DEBUG, Logger::REPLICATION)
        << "syncer::getLeaderState - state is not an object";
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from leader at ") +
                      endpoint + ": invalid JSON");
  }

  return ::handleLeaderStateResponse(connection, *this, slice, context);
}

std::unordered_map<std::string, std::string> createHeaders() {
  return {{StaticStrings::ClusterCommSource, ServerState::instance()->getId()}};
}

bool hasFailed(httpclient::SimpleHttpResult* response) {
  return (response == nullptr || !response->isComplete() || response->wasHttpError());
}

Result buildHttpError(httpclient::SimpleHttpResult* response,
                      std::string const& url, Connection const& connection) {
  TRI_ASSERT(hasFailed(response));

  if (response == nullptr || !response->isComplete()) {
    std::string errorMsg;
    connection.lease([&errorMsg](httpclient::SimpleHttpClient* client) {
      errorMsg = client->getErrorMessage();
    });
    if (errorMsg.empty() && response != nullptr) {
      errorMsg = "HTTP " + basics::StringUtils::itoa(response->getHttpReturnCode()) +
                 ": " + response->getHttpReturnMessage() + " - " +
                 response->getBody().toString();
    }
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                  std::string("could not connect to leader at ") +
                      connection.endpoint() + " for URL " + url + ": " + errorMsg);
  }

  TRI_ASSERT(response->wasHttpError());
  return Result(TRI_ERROR_REPLICATION_LEADER_ERROR,
                std::string("got invalid response from leader at ") +
                    connection.endpoint() + " for URL " + url + ": HTTP " +
                    basics::StringUtils::itoa(response->getHttpReturnCode()) +
                    ": " + response->getHttpReturnMessage() + " - " +
                    response->getBody().toString());
}

/// @brief parse a velocypack response
Result parseResponse(velocypack::Builder& builder,
                     httpclient::SimpleHttpResult const* response) {
  try {
    velocypack::Parser parser(builder);
    parser.parse(response->getBody().begin(), response->getBody().length());
    return Result();
  } catch (VPackException const& e) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, e.what());
  } catch (...) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
  }
}

}  // namespace replutils
}  // namespace arangodb
