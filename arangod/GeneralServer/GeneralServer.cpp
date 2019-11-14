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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "GeneralServer.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Endpoint/Endpoint.h"
#include "Endpoint/EndpointList.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/CommTask.h"
#include "GeneralServer/GeneralDefinitions.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/SslServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
GeneralServer::GeneralServer(GeneralServerFeature& feature, uint64_t numIoThreads)
    : _feature(feature), _endpointList(nullptr), _contexts() {
  auto& server = feature.server();
  for (size_t i = 0; i < numIoThreads; ++i) {
    _contexts.emplace_back(server);
  }
}

GeneralServer::~GeneralServer() = default;

void GeneralServer::registerTask(std::shared_ptr<CommTask> task) {
  auto& server = application_features::ApplicationServer::server();
  if (server.isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  auto* t = task.get();
  LOG_TOPIC("29da9", TRACE, Logger::REQUESTS)
      << "registering CommTask with ptr " << t;
  {
    auto* t = task.get();
    std::lock_guard<std::recursive_mutex> guard(_tasksLock);
    _commTasks.try_emplace(t, std::move(task));
  }
  t->start();
}

void GeneralServer::unregisterTask(CommTask* task) {
  LOG_TOPIC("090d8", TRACE, Logger::REQUESTS)
      << "unregistering CommTask with ptr " << task;
  std::shared_ptr<CommTask> old;
  {
    std::lock_guard<std::recursive_mutex> guard(_tasksLock);
    auto it = _commTasks.find(task);
    if (it != _commTasks.end()) {
      old = std::move(it->second);
      _commTasks.erase(it);
    }
  }
  old.reset();
}

void GeneralServer::setEndpointList(EndpointList const* list) {
  _endpointList = list;
}

void GeneralServer::startListening() {
  unsigned int i = 0;

  for (auto& it : _endpointList->allEndpoints()) {
    LOG_TOPIC("e62e0", TRACE, arangodb::Logger::FIXME)
        << "trying to bind to endpoint '" << it.first << "' for requests";

    // distribute endpoints across all io contexts
    IoContext& ioContext = _contexts[i++ % _contexts.size()];
    bool ok = openEndpoint(ioContext, it.second);

    if (ok) {
      LOG_TOPIC("dc45a", DEBUG, arangodb::Logger::FIXME)
          << "bound to endpoint '" << it.first << "'";
    } else {
      LOG_TOPIC("c81f6", FATAL, arangodb::Logger::FIXME)
          << "failed to bind to endpoint '" << it.first
          << "'. Please check whether another instance is already "
             "running using this endpoint and review your endpoints "
             "configuration.";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_COULD_NOT_BIND_PORT);
    }
  }
}

void GeneralServer::stopListening() {
  for (std::unique_ptr<Acceptor>& acceptor : _acceptors) {
    acceptor->close();
  }

  // close connections of all socket tasks so the tasks will
  // eventually shut themselves down
  std::lock_guard<std::recursive_mutex> guard(_tasksLock);
  _commTasks.clear();
}

void GeneralServer::stopWorking() {
  _acceptors.clear();
  _contexts.clear();  // stops threads
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

bool GeneralServer::openEndpoint(IoContext& ioContext, Endpoint* endpoint) {
  auto acceptor = rest::Acceptor::factory(*this, ioContext, endpoint);
  try {
    acceptor->open();
  } catch (...) {
    return false;
  }
  _acceptors.emplace_back(std::move(acceptor));
  return true;
}

IoContext& GeneralServer::selectIoContext() {
  uint64_t low = _contexts[0].clients();
  size_t lowpos = 0;

  for (size_t i = 1; i < _contexts.size(); ++i) {
    uint64_t x = _contexts[i].clients();
    if (x < low) {
      low = x;
      lowpos = i;
    }
  }

  return _contexts[lowpos];
}

asio_ns::ssl::context& GeneralServer::sslContext() {
  std::lock_guard<std::mutex> guard(_sslContextMutex);
  if (!_sslContext) {
    _sslContext.reset(new asio_ns::ssl::context(SslServerFeature::SSL->createSslContext()));
  }
  return *_sslContext;
}

application_features::ApplicationServer& GeneralServer::server() const {
  return _feature.server();
}
