////////////////////////////////////////////////////////////////////////////////
/// @brief application ZeroMQ
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationZeroMQ.h"

#include <zmq.h>

#include "Basics/Thread.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Logger/Logger.h"
#include "ZeroMQ/ZeroMQQueueThread.h"
#include "ZeroMQ/ZeroMQWorkerThread.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the internal worker bridge
////////////////////////////////////////////////////////////////////////////////

string const ApplicationZeroMQ::ZEROMQ_INTERNAL_BRIDGE = "inproc://arango-zeromq";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationZeroMQ::ApplicationZeroMQ (ApplicationServer* applicationServer)
  : ApplicationFeature("ZeroMQ"),
    _applicationServer(applicationServer),
    _dispatcher(0),
    _handlerFactory(0),
    _zeroMQThreads(0),
    _nrZeroMQThreads(1),
    _zeroMQConcurrency(1),
    _context(0),
    _connection() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationZeroMQ::~ApplicationZeroMQ () {
  if (_context != 0) {
    zctx_destroy(&_context);
  }

  if (_zeroMQThreads != 0) {
    shutdown();

    for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
      delete _zeroMQThreads[i];
    }

    delete _zeroMQThreads;
  }

  if (_handlerFactory != 0) {
    delete _handlerFactory;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true, if feature is active
////////////////////////////////////////////////////////////////////////////////

bool ApplicationZeroMQ::isActive () {
  return ! _connection.empty();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines the handler factory
////////////////////////////////////////////////////////////////////////////////

void ApplicationZeroMQ::setHttpHandlerFactory (HttpHandlerFactory* handlerFactory) {
  if (_handlerFactory != 0) {
    delete handlerFactory;
  }

  _handlerFactory = handlerFactory;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationZeroMQ::setupOptions (map<string, ProgramOptionsDescription>& options) {

  // .............................................................................
  // application server options
  // .............................................................................

  options[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("zeromq.port", &_connection, "ZeroMQ responder address")
  ;

  options["THREAD Options:help-admin"]
    ("zeromq.threads", &_nrZeroMQThreads, "number of threads for ZeroMQ scheduler")
    ("zeromq.concurrency", &_zeroMQConcurrency, "concurrency of the ZeroMQ context")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationZeroMQ::parsePhase1 (basics::ProgramOptions& options) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationZeroMQ::parsePhase2 (basics::ProgramOptions& options) {
  if (! isActive()) {
    return true;
  }

  if (_nrZeroMQThreads < 0) {
    LOGGER_FATAL << "ZeroMQ connection '" << _connection << "' with negative number of threads";
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationZeroMQ::start () {
  if (! isActive()) {
    return true;
  }

  _context = zctx_new();

  if (_context == 0) {
    LOGGER_FATAL << "cannot create the ZeroMQ context: " << strerror(errno);
    return false;
  }
  
  if (1 < _zeroMQConcurrency) {
    zctx_set_iothreads(_context, _zeroMQConcurrency);
  }
  
  // need one more thread for queue
  ++_nrZeroMQThreads;

  // setup a thread pool for workers
  _zeroMQThreads = new ZeroMQThread*[_nrZeroMQThreads + 1];
  
  for (size_t i = 0;  i < _nrZeroMQThreads - 1;  ++i) {
    _zeroMQThreads[1 + i] = new ZeroMQWorkerThread(_dispatcher,
                                                   _handlerFactory,
                                                   _context,
                                                   ZEROMQ_INTERNAL_BRIDGE);
  }
  
  // and queue
  _zeroMQThreads[0] = new ZeroMQQueueThread(_context, _connection, ZEROMQ_INTERNAL_BRIDGE);
  
  // and start all threads
  for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
    _zeroMQThreads[i]->start(0);
    usleep(1000);
  }
  
  bool starting = true;
  
  // wait until all threads are up and running
  while (starting) {
    starting = false;
    
    for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
      if (! _zeroMQThreads[i]->isRunning()) {
        starting = true;
      }
      
      if (starting) {
        usleep(1000);
      }
    }
  }
  
  LOGGER_INFO << "started ZeroMQ on '" << _connection << "' with " << (_nrZeroMQThreads-1) << " threads and concurrency " << _zeroMQConcurrency;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationZeroMQ::isRunning () {
  if (_zeroMQThreads != 0) {
    for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
      if (_zeroMQThreads[i]->isRunning()) {
        return true;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationZeroMQ::beginShutdown () {
  if (_zeroMQThreads != 0) {
    for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
      _zeroMQThreads[i]->beginShutdown();
    }
  }

  if (_context != 0) {
    zctx_destroy(&_context);
    _context = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationZeroMQ::shutdown () {
  if (_zeroMQThreads != 0) {
    int count = 0;

    while (++count < 6 && isRunning()) {
      LOGGER_TRACE << "waiting for scheduler to stop";
      sleep(1);
    }

    for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
      _zeroMQThreads[i]->stop();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
