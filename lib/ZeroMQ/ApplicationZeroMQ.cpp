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
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "ProtocolBuffers/arangodb.pb.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

static HttpRequest* HttpRequestProtobuf (void* data, size_t size) {
  PB_ArangoMessage message;

  int ok = message.ParseFromArray(data, size);

  if (! ok) {
    LOGGER_DEBUG << "received corrupted message via ZeroMQ";
    return 0;
  }

  return 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class ZeroMQThread
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class ZeroMQThread : virtual public Thread {
      public:
        ZeroMQThread (void* context)
          : Thread("zeromq-res"),
            _stopping(0),
            _context(context) {
        }
        
      public:
        void beginShutdown () {
          _stopping = 1;
        }
        
      protected:
        sig_atomic_t _stopping;
        void* _context;
    };
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                          class ZeroMQWorkerThread
// -----------------------------------------------------------------------------

namespace {
  class ZeroMQWorkerThread : public ZeroMQThread {
    public:
      ZeroMQWorkerThread (void* context, string const& connection, bool bind)
        : Thread("zeromq-worker"),
          ZeroMQThread(context),
          _connection(connection),
          _bind(bind) {
      }

    protected:
      void run () {

        // create a socket for the server
        void* responder = zmq_socket(_context, ZMQ_REP);

        if (responder == 0) {
          LOGGER_FATAL << "cannot initialize ZeroMQ worker socket: " << strerror(errno);
          zmq_term(_context);
          exit(EXIT_FAILURE);
        }

        // and bind it to the connection
        int res;

        if (_bind) {
          res = zmq_bind(responder, _connection.c_str());
        }
        else {
          res = zmq_connect(responder, _connection.c_str());
        }

        if (res != 0) {
          LOGGER_FATAL << "cannot bind ZeroMQ worker socket: " << strerror(errno);
          zmq_close(responder);
          zmq_term(_context);
          exit(EXIT_FAILURE);
        }

        while (_stopping == 0) {
          zmq_msg_t request;
          zmq_msg_init(&request);

          // receive next message
          res = zmq_recv(responder, &request, 0);

          if (res < 0) {
            zmq_msg_close(&request);

            if (errno == ETERM) {
              break;
            }

            continue;
          }
    
          // convert body to HttpRequest
          HttpRequest* httpRequest = HttpRequestProtobuf(zmq_msg_data(&request), zmq_msg_size(&request));

          // close the request
          zmq_msg_close(&request);

          // received illegal message, force client to shutdown
          if (httpRequest == 0) {
            zmq_msg_t reply;
            zmq_msg_init_size(&reply, 0);

            zmq_send(responder, &reply, 0);
            zmq_msg_close(&reply);

            continue;
          }

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
      string const _connection;
      bool _bind;
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                           class ZeroMQQueueThread
// -----------------------------------------------------------------------------

namespace {
  class ZeroMQQueueThread : public ZeroMQThread {
    public:
      ZeroMQQueueThread (void* context, string const& connection, string const& inproc)
        : Thread("zeromq-queue"),
          ZeroMQThread(context),
          _connection(connection),
          _inproc(inproc) {
      }

    protected:
      void run () {

        // create a socket for the workers
        void* workers = zmq_socket(_context, ZMQ_XREQ);

        if (workers == 0) {
          LOGGER_FATAL << "cannot initialize ZeroMQ workers socket: " << strerror(errno);
          zmq_term(_context);
          exit(EXIT_FAILURE);
        }

        // and bind it to the connection
        int res = zmq_bind(workers, _inproc.c_str());

        if (res != 0) {
          LOGGER_FATAL << "cannot bind ZeroMQ workers socket: " << strerror(errno);
          zmq_close(workers);
          zmq_term(_context);
          exit(EXIT_FAILURE);
        }

        // create a socket for the server
        void* clients = zmq_socket(_context, ZMQ_XREP);

        if (clients == 0) {
          LOGGER_FATAL << "cannot initialize ZeroMQ clients socket: " << strerror(errno);
          zmq_close(workers);
          zmq_term(_context);
          exit(EXIT_FAILURE);
        }

        // and bind it to the connection
        res = zmq_bind(clients, _connection.c_str());

        if (res != 0) {
          LOGGER_FATAL << "cannot bind ZeroMQ clients socket: " << strerror(errno);
          zmq_close(workers);
          zmq_close(clients);
          zmq_term(_context);
          exit(EXIT_FAILURE);
        }

        zmq_device(ZMQ_QUEUE, clients, workers);

        if (_stopping == 0) {
          LOGGER_FATAL << "cannot setup queue: " << strerror(errno);
          exit(EXIT_FAILURE);
        }

        zmq_close(clients);
        zmq_close(workers);
      }

    private:
      string const _connection;
      string const _inproc;
  };
}

// -----------------------------------------------------------------------------
// --SECTION-- class ApplicationZeroMQ
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
    zmq_term(_context);
  }

  if (_zeroMQThreads != 0) {
    shutdown();

    for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
      delete _zeroMQThreads[i];
    }

    delete _zeroMQThreads;
  }
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
  if (! _connection.empty()) {
    if (_nrZeroMQThreads < 0) {
      LOGGER_FATAL << "ZeroMQ connection '" << _connection << "' with negative number of threads";
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
    _context = zmq_init(_zeroMQConcurrency);

    if (_context == 0) {
      LOGGER_FATAL << "cannot create the ZeroMQ context: " << strerror(errno);
      return false;
    }

    // .............................................................................
    // case single threaded: only use one worker thread
    // .............................................................................

    if (_nrZeroMQThreads == 1) {

      // setup a thread pool
      _zeroMQThreads = new ZeroMQThread*[_nrZeroMQThreads];

      for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
        _zeroMQThreads[i] = new ZeroMQWorkerThread(_context, _connection, true);
      }
    }

    // .............................................................................
    // case single threaded: only use one worker thread
    // .............................................................................

    else {
      static string inproc = "inproc://arango-zeromq";

      // setup a thread pool
      ++_nrZeroMQThreads;
      _zeroMQThreads = new ZeroMQThread*[_nrZeroMQThreads + 1];

      for (size_t i = 0;  i < _nrZeroMQThreads - 1;  ++i) {
        _zeroMQThreads[1 + i] = new ZeroMQWorkerThread(_context, inproc, false);
      }

      // must be started first
      _zeroMQThreads[0] = new ZeroMQQueueThread(_context, _connection, inproc);
    }

    // .............................................................................
    // and start all threads
    // .............................................................................

    for (size_t i = 0;  i < _nrZeroMQThreads;  ++i) {
      _zeroMQThreads[i]->start(0);
    }

    bool starting = true;

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

    LOGGER_INFO << "started ZeroMQ on '" << _connection << "' with " << _nrZeroMQThreads << " threads and concurrency " << _zeroMQConcurrency;
  }

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
    zmq_term(_context);
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
