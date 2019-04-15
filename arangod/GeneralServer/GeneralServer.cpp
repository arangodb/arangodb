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

#include "Basics/exitcodes.h"
#include "Endpoint/EndpointList.h"
#include "GeneralServer/GeneralDefinitions.h"
#include "GeneralServer/GeneralListenTask.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
GeneralServer::GeneralServer(uint64_t numIoThreads)
    : _numIoThreads(numIoThreads), _contexts(numIoThreads) {}

void GeneralServer::setEndpointList(EndpointList const* list) {
  _endpointList = list;
}

void GeneralServer::startListening() {
  unsigned int i = 0;

  for (auto& it : _endpointList->allEndpoints()) {
    LOG_TOPIC("e62e0", TRACE, arangodb::Logger::FIXME)
        << "trying to bind to endpoint '" << it.first << "' for requests";

    // distribute endpoints across all io contexts
    IoContext& ioContext = _contexts[i++ % _numIoThreads];
    bool ok = openEndpoint(ioContext, it.second);

    if (ok) {
      LOG_TOPIC("dc45a", DEBUG, arangodb::Logger::FIXME) << "bound to endpoint '" << it.first << "'";
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
  for (auto& context : _contexts) {
    context.stop();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

bool GeneralServer::openEndpoint(IoContext& ioContext, Endpoint* endpoint) {
  ProtocolType protocolType;

  if (endpoint->encryption() == Endpoint::EncryptionType::SSL) {
    protocolType = ProtocolType::HTTPS;
  } else {
    protocolType = ProtocolType::HTTP;
  }

  auto task = std::make_shared<GeneralListenTask>(*this, ioContext, endpoint, protocolType);
  if (!task->start()) {
    return false;
  }

  return true;
}

GeneralServer::IoThread::~IoThread() { shutdown(); }

GeneralServer::IoThread::IoThread(IoContext& iocontext)
    : Thread("Io"), _iocontext(iocontext) {}

void GeneralServer::IoThread::run() {
  // run the asio io context
  _iocontext._asioIoContext.run();
}

GeneralServer::IoContext::IoContext()
    : _clients(0),
      _thread(*this),
      _asioIoContext(1),  // only a single thread per context
      _asioWork(_asioIoContext),
      _stopped(false) {
  _thread.start();
}

GeneralServer::IoContext::~IoContext() { stop(); }

void GeneralServer::IoContext::stop() { _asioIoContext.stop(); }

GeneralServer::IoContext& GeneralServer::selectIoContext() {
  uint64_t low = _contexts[0]._clients.load();
  size_t lowpos = 0;

  for (size_t i = 1; i < _contexts.size(); ++i) {
    uint64_t x = _contexts[i]._clients.load();
    if (x < low) {
      low = x;
      lowpos = i;
    }
  }

  return _contexts[lowpos];
}
