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
#include "Cluster/ServerState.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/Syncer.h"
#include "RestServer/ServerIdFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

struct TRI_vocbase_t;

namespace {
/// @brief handle the state response of the master
arangodb::Result handleMasterStateResponse(
    arangodb::replutils::Connection& connection,
    arangodb::replutils::MasterInfo& master,
    arangodb::velocypack::Slice const& slice) {
  using arangodb::Result;
  using arangodb::velocypack::Slice;

  std::string const endpointString = " from endpoint '" + master.endpoint + "'";

  // process "state" section
  Slice const state = slice.get("state");
  if (!state.isObject()) {
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        std::string("state section is missing in response") + endpointString);
  }

  // state."lastLogTick"
  Slice const tick = state.get("lastLogTick");
  if (!tick.isString()) {
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        std::string("lastLogTick is missing in response") + endpointString);
  }

  TRI_voc_tick_t const lastLogTick =
      arangodb::basics::VelocyPackHelper::stringUInt64(tick);
  if (lastLogTick == 0) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("lastLogTick is 0 in response") + endpointString);
  }

  // state."running"
  bool running = arangodb::basics::VelocyPackHelper::getBooleanValue(
      state, "running", false);

  // process "server" section
  Slice const server = slice.get("server");
  if (!server.isObject()) {
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        std::string("server section is missing in response") + endpointString);
  }

  // server."version"
  Slice const version = server.get("version");
  if (!version.isString()) {
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        std::string("server version is missing in response") + endpointString);
  }

  // server."serverId"
  Slice const serverId = server.get("serverId");
  if (!serverId.isString()) {
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        std::string("server id is missing in response") + endpointString);
  }

  // validate all values we got
  std::string const masterIdString(serverId.copyString());
  TRI_server_id_t const masterId =
      arangodb::basics::StringUtils::uint64(masterIdString);
  if (masterId == 0) {
    // invalid master id
    return Result(
        TRI_ERROR_REPLICATION_INVALID_RESPONSE,
        std::string("invalid server id in response") + endpointString);
  }

  if (masterIdString == connection.localServerId()) {
    // master and replica are the same instance. this is not supported.
    return Result(TRI_ERROR_REPLICATION_LOOP,
                  std::string("got same server id (") +
                      connection.localServerId() + ")" + endpointString +
                      " as the local applier server's id");
  }

  int major = 0;
  int minor = 0;
  std::string const versionString(version.copyString());
  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    return Result(TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE,
                  std::string("invalid master version info") + endpointString +
                      ": '" + versionString + "'");
  }

  if (major != 3) {
    // we can connect to 3.x only
    return Result(TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE,
                  std::string("got incompatible master version") +
                      endpointString + ": '" + versionString + "'");
  }

  master.majorVersion = major;
  master.minorVersion = minor;
  master.serverId = masterId;
  master.lastLogTick = lastLogTick;
  master.active = running;

  LOG_TOPIC(INFO, arangodb::Logger::REPLICATION)
      << "connected to master at " << master.endpoint << ", id "
      << master.serverId << ", version " << master.majorVersion << "."
      << master.minorVersion << ", last log tick " << master.lastLogTick;

  return Result();
}
}  // namespace

namespace arangodb {
namespace replutils {

std::string const ReplicationUrl = "/_api/replication";

Connection::Connection(Syncer* syncer,
                       ReplicationApplierConfiguration const& applierConfig)
    : _endpointString{applierConfig._endpoint},
      _localServerId{basics::StringUtils::itoa(ServerIdFeature::getId())} {
  std::unique_ptr<httpclient::GeneralClientConnection> connection;
  std::unique_ptr<Endpoint> endpoint{Endpoint::clientFactory(_endpointString)};
  if (endpoint != nullptr) {
    connection.reset(httpclient::GeneralClientConnection::factory(
        endpoint, applierConfig._requestTimeout, applierConfig._connectTimeout,
        (size_t)applierConfig._maxConnectRetries,
        (uint32_t)applierConfig._sslProtocol));
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

    httpclient::SimpleHttpClientParams params(applierConfig._requestTimeout,
                                              false);
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
    params.setLocationRewriter(syncer, &(syncer->rewriteLocation));
    client.reset(new httpclient::SimpleHttpClient(connection, params));
  }
}

bool Connection::valid() const { return (client != nullptr); }

std::string const& Connection::endpoint() const { return _endpointString; }

std::string const& Connection::localServerId() const { return _localServerId; }

void Connection::setAborted(bool value) {
  MUTEX_LOCKER(locker, _mutex);

  if (client) {
    client->setAborted(value);
  }
}

bool Connection::isAborted() const {
  MUTEX_LOCKER(locker, _mutex);
  if (client) {
    return client->isAborted();
  }
  return true;
}

ProgressInfo::ProgressInfo(Setter s) : _mutex{}, _setter{s} {}

void ProgressInfo::set(std::string const& msg) {
  MUTEX_LOCKER(locker, _mutex);
  _setter(msg);
}

/// @brief send a "create barrier" command
Result BarrierInfo::create(Connection& connection, TRI_voc_tick_t minTick) {
  // TODO make sure all callers verify not child syncer
  id = 0;
  std::string const url = ReplicationUrl + "/barrier";
  VPackBuilder builder;
  builder.openObject();
  builder.add("ttl", VPackValue(ttl));
  builder.add("tick", VPackValue(std::to_string(minTick)));
  builder.close();
  std::string body = builder.slice().toJson();

  // send request
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      connection.client->retryRequest(rest::RequestType::POST, url, body.data(),
                                      body.size()));
  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url, connection);
  }

  builder.clear();
  Result r = parseResponse(builder, response.get());
  if (r.fail()) {
    return r;
  }

  VPackSlice const slice = builder.slice();
  std::string const barrierId =
      basics::VelocyPackHelper::getStringValue(slice, "id", "");
  if (barrierId.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "id is missing in create barrier response");
  }

  id = basics::StringUtils::uint64(barrierId);
  updateTime = TRI_microtime();
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "created WAL logfile barrier " << id;

  return Result();
}

/// @brief send an "extend barrier" command
Result BarrierInfo::extend(Connection& connection, TRI_voc_tick_t tick) {
  // TODO make sure all callers verify not child syncer
  using basics::StringUtils::itoa;

  if (id == 0) {
    return Result();
  }

  double now = TRI_microtime();
  if (now <= updateTime + ttl * 0.25) {
    // no need to extend the barrier yet
    return Result();
  }

  std::string const url = ReplicationUrl + "/barrier/" + itoa(id);
  std::string const body =
      "{\"ttl\":" + itoa(ttl) + ",\"tick\"" + itoa(tick) + "\"}";

  // send request
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      connection.client->request(rest::RequestType::PUT, url, body.data(),
                                 body.size()));
  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE);
  }
  TRI_ASSERT(response != nullptr);
  if (response->wasHttpError()) {
    return Result(TRI_ERROR_REPLICATION_MASTER_ERROR);
  }

  updateTime = TRI_microtime();

  return Result();
}

Result BatchInfo::start(replutils::Connection& connection,
                        replutils::ProgressInfo& progress) {
  // TODO make sure all callers verify not child syncer
  if (!connection.valid()) {
    return {TRI_ERROR_INTERNAL};
  }

  double const now = TRI_microtime();
  id = 0;
  std::string const url =
      ReplicationUrl + "/batch" + "?serverId=" + connection.localServerId();
  std::string const body = "{\"ttl\":" + basics::StringUtils::itoa(ttl) + "}";

  // send request
  progress.set("sending batch start command to url " + url);

  std::unique_ptr<httpclient::SimpleHttpResult> response(
      connection.client->retryRequest(rest::RequestType::POST, url,
                                      body.c_str(), body.size()));

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url, connection);
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());

  if (r.fail()) {
    return r;
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "start batch response is not an object");
  }

  std::string const batchId =
      basics::VelocyPackHelper::getStringValue(slice, "id", "");
  if (batchId.empty()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "start batch id is missing in response");
  }

  id = basics::StringUtils::uint64(batchId);
  updateTime = now;

  return Result();
}

/// @brief send an "extend batch" command
Result BatchInfo::extend(replutils::Connection& connection,
                         replutils::ProgressInfo& progress) {
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

  std::string const url = ReplicationUrl + "/batch/" +
                          basics::StringUtils::itoa(id) +
                          "?serverId=" + connection.localServerId();
  std::string const body = "{\"ttl\":" + basics::StringUtils::itoa(ttl) + "}";

  // send request
  progress.set("sending batch extend command to url " + url);

  std::unique_ptr<httpclient::SimpleHttpResult> response(
      connection.client->request(rest::RequestType::PUT, url, body.c_str(),
                                 body.size()));

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url, connection);
  }

  updateTime = now;

  return Result();
}

/// @brief send a "finish batch" command
Result BatchInfo::finish(replutils::Connection& connection,
                         replutils::ProgressInfo& progress) {
  if (id == 0) {
    return Result();
  } else if (!connection.valid()) {
    return {TRI_ERROR_INTERNAL};
  }

  try {
    std::string const url = ReplicationUrl + "/batch/" +
                            basics::StringUtils::itoa(id) +
                            "?serverId=" + connection.localServerId();

    // send request
    progress.set("sending batch finish command to url " + url);
    std::unique_ptr<httpclient::SimpleHttpResult> response(
        connection.client->retryRequest(rest::RequestType::DELETE_REQ, url,
                                        nullptr, 0));

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

MasterInfo::MasterInfo(ReplicationApplierConfiguration const& applierConfig) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _force32mode = applierConfig._force32mode;
#endif
}

/// @brief get master state
Result MasterInfo::getState(replutils::Connection& connection,
                            bool isChildSyncer) {
  if (isChildSyncer) {
    TRI_ASSERT(endpoint.empty());
    TRI_ASSERT(serverId != 0);
    TRI_ASSERT(majorVersion != 0);
    return Result();
  }

  std::string const url =
      ReplicationUrl + "/logger-state?serverId=" + connection.localServerId();

  // store old settings
  size_t maxRetries = connection.client->params().getMaxRetries();
  uint64_t retryWaitTime = connection.client->params().getRetryWaitTime();

  // apply settings that prevent endless waiting here
  connection.client->params().setMaxRetries(1);
  connection.client->params().setRetryWaitTime(500 * 1000);  // 0.5s

  std::unique_ptr<httpclient::SimpleHttpResult> response(
      connection.client->retryRequest(rest::RequestType::GET, url, nullptr, 0));

  // restore old settings
  connection.client->params().setMaxRetries(maxRetries);
  connection.client->params().setRetryWaitTime(retryWaitTime);

  if (hasFailed(response.get())) {
    return buildHttpError(response.get(), url, connection);
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());
  if (r.fail()) {
    return r;
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION)
        << "syncer::getMasterState - state is not an object";
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      endpoint + ": invalid JSON");
  }

  return ::handleMasterStateResponse(connection, *this, slice);
}

bool MasterInfo::simulate32Client() const {
  TRI_ASSERT(!endpoint.empty() && serverId != 0 && majorVersion != 0);
  bool is33 = (majorVersion > 3 || (majorVersion == 3 && minorVersion >= 3));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // allows us to test the old replication API
  return !is33 || _force32mode;
#else
  return !is33;
#endif
}

std::unordered_map<std::string, std::string> createHeaders() {
  return {{StaticStrings::ClusterCommSource, ServerState::instance()->getId()}};
}

bool hasFailed(httpclient::SimpleHttpResult* response) {
  return (response == nullptr || !response->isComplete() ||
          response->wasHttpError());
}

Result buildHttpError(httpclient::SimpleHttpResult* response,
                      std::string const& url, Connection const& connection) {
  TRI_ASSERT(hasFailed(response));

  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE,
                  std::string("could not connect to master at ") +
                      connection.endpoint() + " for URL " + url + ": " +
                      connection.client->getErrorMessage());
  }

  TRI_ASSERT(response->wasHttpError());
  return Result(TRI_ERROR_REPLICATION_MASTER_ERROR,
                std::string("got invalid response from master at ") +
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
  } catch (...) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE);
  }
}

}  // namespace replutils
}  // namespace arangodb
