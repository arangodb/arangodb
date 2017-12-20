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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ClientHelper.hpp"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/Result.h"
#include "Basics/Thread.h"

#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

namespace arangodb {
template <typename JobData>
class ClientWorker : public arangodb::Thread {
 private:
  ClientWorker(ClientWorker const&) = delete;
  ClientWorker& operator=(ClientWorker const&) = delete;

 public:
  explicit ClientWorker(ClientHelper<JobData>&,
                        std::unique_ptr<SimpleHttpClient>&&);
  virtual ~ClientWorker();

  bool isIdle() const noexcept;

 protected:
  void run() override;

 private:
  ClientHelper<JobData>& _helper;
  std::unique_ptr<SimpleHttpClient> _client;
  std::atomic<bool> _idle;
};
}  // namespace arangodb

template <typename JobData>
ClientHelper<JobData>::ClientHelper() {}

template <typename JobData>
ClientHelper<JobData>::~ClientHelper() {}

// helper to rewrite HTTP location
template <typename JobData>
std::string ClientHelper<JobData>::rewriteLocation(
    void* data, std::string const& location) noexcept {
  if (location.compare(0, 5, "/_db/") == 0) {
    return location;
  }

  ClientFeature* client = static_cast<ClientFeature*>(data);
  std::string const& dbname = client->databaseName();

  if (location[0] == '/') {
    return "/_db/" + dbname + location;
  }
  return "/_db/" + dbname + "/" + location;
}

// extract an error message from a response
template <typename JobData>
std::string ClientHelper<JobData>::getHttpErrorMessage(SimpleHttpResult* result,
                                                       int* err) noexcept {
  if (err != nullptr) {
    *err = TRI_ERROR_NO_ERROR;
  }
  std::string details;

  try {
    std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
    VPackSlice const body = parsedBody->slice();

    std::string const& errorMessage =
        basics::VelocyPackHelper::getStringValue(body, "errorMessage", "");
    int errorNum =
        basics::VelocyPackHelper::getNumericValue<int>(body, "errorNum", 0);

    if (errorMessage != "" && errorNum > 0) {
      if (err != nullptr) {
        *err = errorNum;
      }
      details =
          ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
    }
  } catch (...) {
    // No action, fallthrough for error
  }
  return "got error from server: HTTP " +
         StringUtils::itoa(result->getHttpReturnCode()) + " (" +
         result->getHttpReturnMessage() + ")" + details;
}

template <typename JobData>
std::unique_ptr<httpclient::SimpleHttpClient>
ClientHelper<JobData>::getConnectedClient(bool force, bool verbose) noexcept {
  std::unique_ptr<httpclient::SimpleHttpClient> httpClient(nullptr);

  ClientFeature* client =
      application_features::ApplicationServer::getFeature<ClientFeature>(
          "Client");
  TRI_ASSERT(client);

  try {
    httpClient = client->createHttpClient();
  } catch (...) {
    LOG_TOPIC(FATAL, Logger::FIXME)
        << "cannot create server connection, giving up!";
    FATAL_ERROR_EXIT();
  }

  std::string dbName = client->databaseName();
  httpClient->params().setLocationRewriter(static_cast<void*>(client),
                                           &rewriteLocation);
  httpClient->params().setUserNamePassword("/", client->username(),
                                           client->password());
  std::string const versionString = httpClient->getServerVersion();

  if (!httpClient->isConnected()) {
    LOG_TOPIC(ERR, Logger::FIXME)
        << "Could not connect to endpoint '" << client->endpoint()
        << "', database: '" << dbName << "', username: '" << client->username()
        << "'";
    LOG_TOPIC(FATAL, Logger::FIXME)
        << "Error message: '" << httpClient->getErrorMessage() << "'";

    FATAL_ERROR_EXIT();
  }

  if (verbose) {
    // successfully connected
    LOG_TOPIC(INFO, Logger::FIXME) << "Server version: " << versionString;
  }

  // validate server version
  std::pair<int, int> version =
      rest::Version::parseVersionString(versionString);

  if (version.first < 3) {
    // we can connect to 3.x
    LOG_TOPIC(ERR, Logger::FIXME)
        << "Error: got incompatible server version '" << versionString << "'";

    if (!force) {
      FATAL_ERROR_EXIT();
    }
  }

  return httpClient;
}

// check if server is a coordinator of a cluster
template <typename JobData>
bool ClientHelper<JobData>::getArangoIsCluster(int* err) noexcept {
  auto httpClient = getConnectedClient();
  std::unique_ptr<SimpleHttpResult> response(httpClient->request(
      rest::RequestType::GET, "/_admin/server/role", "", 0));

  if (response == nullptr || !response->isComplete()) {
    return false;
  }

  std::string role = "UNDEFINED";

  if (response->getHttpReturnCode() == (int)rest::ResponseCode::OK) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      role =
          basics::VelocyPackHelper::getStringValue(body, "role", "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      httpClient->setErrorMessage(getHttpErrorMessage(response.get(), err),
                                  false);
    }

    httpClient->disconnect();
  }

  return role == "COORDINATOR";
}

// check if server is using the specified storage engine
template <typename JobData>
bool ClientHelper<JobData>::getArangoIsUsingEngine(
    int* err, std::string const& name) noexcept {
  auto httpClient = getConnectedClient();
  std::unique_ptr<SimpleHttpResult> response(
      httpClient->request(rest::RequestType::GET, "/_api/engine", "", 0));

  if (response == nullptr || !response->isComplete()) {
    return false;
  }

  std::string engine = "UNDEFINED";

  if (response->getHttpReturnCode() == (int)rest::ResponseCode::OK) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      engine =
          basics::VelocyPackHelper::getStringValue(body, "name", "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      httpClient->setErrorMessage(getHttpErrorMessage(response.get(), err),
                                  false);
    }

    httpClient->disconnect();
  }

  return (engine == name);
}

template <typename JobData>
bool ClientHelper<JobData>::spawnWorkers(size_t const& toSpawn) noexcept {
  size_t spawned = 0;
  for (; spawned < toSpawn; spawned++) {
    try {
      auto client = getConnectedClient();
      auto worker =
          std::make_unique<ClientWorker<JobData>>(*this, std::move(client));
      _workers.emplace_back(std::move(worker));
    } catch (...) {
      break;
    }
  }
  return (toSpawn == spawned);
}

template <typename JobData>
bool ClientHelper<JobData>::isQueueEmpty() const noexcept {
  MUTEX_LOCKER(lock, _jobsLock);
  return _jobs.empty();
}

template <typename JobData>
bool ClientHelper<JobData>::allWorkersBusy() const noexcept {
  for (auto& worker : _workers) {
    if (worker->isIdle()) {
      return false;
    }
  }
  return true;
}

template <typename JobData>
bool ClientHelper<JobData>::queueJob(std::unique_ptr<JobData>&& job) noexcept {
  MUTEX_LOCKER(lock, _jobsLock);
  try {
    _jobs.emplace(std::move(job));
    _jobsCondition.signal();
    return true;
  } catch (...) {
    return false;
  }
}

template <typename JobData>
std::unique_ptr<JobData> ClientHelper<JobData>::fetchJob() noexcept {
  std::unique_ptr<JobData> job(nullptr);
  MUTEX_LOCKER(lock, _jobsLock);

  if (_jobs.empty()) {
    return job;
  }

  job = std::move(_jobs.front());
  _jobs.pop();
  return job;
}

template <typename JobData>
void ClientHelper<JobData>::waitForWork() noexcept {
  if (!isQueueEmpty()) {
    return;
  }

  CONDITION_LOCKER(lock, _jobsCondition);
  lock.wait(std::chrono::milliseconds(500));
}

template <typename JobData>
ClientWorker<JobData>::ClientWorker(
    ClientHelper<JobData>& helper,
    std::unique_ptr<httpclient::SimpleHttpClient>&& client)
    : Thread("ClientWorker"),
      _helper(helper),
      _client(std::move(client)),
      _idle(true) {}

template <typename JobData>
ClientWorker<JobData>::~ClientWorker() {}

template <typename JobData>
bool ClientWorker<JobData>::isIdle() const noexcept {
  return _idle.load();
}

template <typename JobData>
void ClientWorker<JobData>::run() {
  while (!isStopping()) {
    std::unique_ptr<JobData> job = _helper.fetchJob();

    if (job) {
      _idle.store(false);

      Result result = _helper.processJob(*_client, *job);
      _helper.handleJobResult(std::move(job), result);

      _idle.store(true);
    }

    _helper.waitForWork();
  }
}
