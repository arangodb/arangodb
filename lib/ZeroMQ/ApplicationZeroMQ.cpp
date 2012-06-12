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

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                class ZeroMQThread
// -----------------------------------------------------------------------------

namespace {
  class ZeroMQThread : public Thread {
    public:
      ZeroMQThread (void* context, string connection)
        : Thread("zeromq"),
          _stopping(0),
          _context(context),
          _connection(connection) {
      }

    public:
      void beginShutdown () {
        _stopping = 1;
      }

    protected:
      void run () {
        void* responder = zmq_socket(_context, ZMQ_REP);
        int res = zmq_bind(responder, _connection.c_str());

        if (res != 0) {
          LOGGER_FATAL << "cannot initialize ZeroMQ responder: " << strerror(errno);
          exit(EXIT_FAILURE);
        }

        while (_stopping == 0) {
          zmq_msg_t request;
          zmq_msg_init(&request);

          // receive next message
          zmq_recv(responder, &request, 0);
          zmq_msg_close(&request);
    
          // do some work
    
          // send reply back to client
          zmq_msg_t reply;
          zmq_msg_init_size(&reply, 5);
          memcpy(zmq_msg_data(&reply), "World", 5);

          zmq_send(responder, &reply, 0);
          zmq_msg_close(&reply);
        }

        zmq_close(responder);
      }

    private:
      sig_atomic_t _stopping;
      void* _context;
      string _connection;
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                           class ApplicationZeroMQ
// -----------------------------------------------------------------------------

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
  : _applicationServer(applicationServer),
    _context(0),
    _connection() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationZeroMQ::~ApplicationZeroMQ () {
  if (_context != 0) {
    zmq_term(context);
  }

  delete _zeroMQThread;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
  }
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
  if (! _connection.empty()) {
    if (_nrZeroMQThreads < 0) {
      LOGGER_FATAL << "ZeroMQ connection '" << _connection << "' request without negativ threads";
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationZeroMQ::start () {
  if (! _connection.empty()) {
    _context = zmq_init(_nrZeroMQThreads);

    if (_context == 0) {
      LOGGER_FATAL << "cannot create the ZeroMQ context: " << strerror(errno);
      return false;
    }

    _zeroMQThread = new ZeroMQThread(_context, _connection);

    _zeroMQThread->start(0);

    while (! _zeroMQThread->isRunning()) {
      usleep(1000);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationZeroMQ::isRunning () {
  if (_zeroMQThread != 0) {
    return _zeroMQThread->isRunning();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationZeroMQ::beginShutdown () {
  if (_zeroMQThread != 0) {
    _zeroMQThread->beginShutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationZeroMQ::shutdown () {
  if (_zeroMQThread != 0) {
    int count = 0;

    while (++count < 6 && _zeroMQThread->isRunning()) {
      LOGGER_TRACE << "waiting for scheduler to stop";
      sleep(1);
    }

    if (_zeroMQThread->isRunning()) {
      _zeroMQThread->stop();
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
