////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include <chrono>
#include <thread>
#include <algorithm>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

GeneralServer::GeneralServer(GeneralServerFeature& feature,
                             uint64_t numIoThreads, bool allowEarlyConnections)
    : _feature(feature), _allowEarlyConnections(allowEarlyConnections) {
  auto& server = feature.server();

  _contexts.reserve(numIoThreads);
  for (size_t i = 0; i < numIoThreads; ++i) {
    _contexts.emplace_back(server);
  }
}

GeneralServer::~GeneralServer() = default;

bool GeneralServer::allowEarlyConnections() const noexcept {
  return _allowEarlyConnections;
}

void GeneralServer::registerTask(std::shared_ptr<CommTask> task) {
  if (_feature.server().isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  auto* t = task.get();
  LOG_TOPIC("29da9", TRACE, Logger::REQUESTS)
      << "registering CommTask with ptr " << t;
  {
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

void GeneralServer::startListening(EndpointList& list) {
  unsigned int i = 0;

  list.apply([&i, this](std::string const& specification, Endpoint& ep) {
    LOG_TOPIC("e62e0", TRACE, arangodb::Logger::FIXME)
        << "trying to bind to endpoint '" << specification << "' for requests";

    // distribute endpoints across all io contexts
    IoContext& ioContext = _contexts[i++ % _contexts.size()];
    bool ok = openEndpoint(ioContext, &ep);

    if (ok) {
      LOG_TOPIC("dc45a", DEBUG, arangodb::Logger::FIXME)
          << "bound to endpoint '" << specification << "'";
    } else {
      LOG_TOPIC("c81f6", FATAL, arangodb::Logger::FIXME)
          << "failed to bind to endpoint '" << specification
          << "'. Please check whether another instance is already "
             "running using this endpoint and review your endpoints "
             "configuration.";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_COULD_NOT_BIND_PORT);
    }
  });

  // print out messages to which endpoints the server is bound to
  list.dump();
}

/// stop accepting new connections
void GeneralServer::stopListening() {
  for (std::unique_ptr<Acceptor>& acceptor : _acceptors) {
    acceptor->close();
  }
}

/// stop connections
void GeneralServer::stopConnections() {
  // close connections of all socket tasks so the tasks will
  // eventually shut themselves down
  std::lock_guard<std::recursive_mutex> guard(_tasksLock);
  for (auto const& pair : _commTasks) {
    pair.second->stop();
  }
}

void GeneralServer::stopWorking() {
  auto const started{std::chrono::system_clock::now()};
  constexpr auto timeout{std::chrono::seconds(5)};
  do {
    {
      std::unique_lock<std::recursive_mutex> guard(_tasksLock);
      if (_commTasks.empty()) break;
    }
    std::this_thread::yield();
  } while ((std::chrono::system_clock::now() - started) < timeout);
  {
    std::lock_guard<std::recursive_mutex> guard(_tasksLock);
    _commTasks.clear();
  }
  // need to stop IoThreads before cleaning up the acceptors
  for (auto& ctx : _contexts) {
    ctx.stop();
  }
  _acceptors.clear();
  _contexts.clear();
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
  return *std::min_element(
      _contexts.begin(), _contexts.end(),
      [](auto const& a, auto const& b) { return a.clients() < b.clients(); });
}

#ifdef USE_ENTERPRISE
extern int clientHelloCallback(SSL* ssl, int* al, void* arg);
#endif

SslServerFeature::SslContextList GeneralServer::sslContexts() {
  std::lock_guard<std::mutex> guard(_sslContextMutex);
  if (!_sslContexts) {
    _sslContexts = server().getFeature<SslServerFeature>().createSslContexts();
#ifdef USE_ENTERPRISE
    if (_sslContexts->size() > 0) {
      // Set a client hello callback such that we have a chance to change the
      // SSL context:
      SSL_CTX_set_client_hello_cb((*_sslContexts)[0].native_handle(),
                                  &clientHelloCallback, (void*)this);
    }
#endif
  }
  return _sslContexts;
}

SSL_CTX* GeneralServer::getSSL_CTX(size_t index) {
  std::lock_guard<std::mutex> guard(_sslContextMutex);
  return (*_sslContexts)[index].native_handle();
}

Result GeneralServer::reloadTLS() {
  try {
    {
      std::lock_guard<std::mutex> guard(_sslContextMutex);
      _sslContexts =
          server().getFeature<SslServerFeature>().createSslContexts();
#ifdef USE_ENTERPRISE
      if (_sslContexts->size() > 0) {
        // Set a client hello callback such that we have a chance to change the
        // SSL context:
        SSL_CTX_set_client_hello_cb((*_sslContexts)[0].native_handle(),
                                    &clientHelloCallback, (void*)this);
      }
#endif
    }
    // Now cancel every acceptor once, such that a new AsioSocket is generated
    // which will use the new context. Otherwise, the first connection will
    // still use the old certs:
    for (auto& a : _acceptors) {
      a->cancel();
    }
    return TRI_ERROR_NO_ERROR;
  } catch (std::exception& e) {
    LOG_TOPIC("feffe", ERR, Logger::SSL)
        << "Could not reload TLS context from files, got exception with this "
           "error: "
        << e.what();
    return Result(TRI_ERROR_CANNOT_READ_FILE,
                  "Could not reload TLS context from files.");
  }
}

ArangodServer& GeneralServer::server() const { return _feature.server(); }
