////////////////////////////////////////////////////////////////////////////////
/// @brief ZeroMQ queue thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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

#include "ZeroMQQueueThread.h"

#include <czmq.h>

#include "Logger/Logger.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ZeroMQ
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ZeroMQQueueThread::ZeroMQQueueThread (void* context,
                                      string const& connection,
                                      string const& inproc)
  : Thread("zeromq-queue"),
    ZeroMQThread(context),
    _connection(connection),
    _inproc(inproc) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ZeroMQ
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ZeroMQQueueThread::run () {

  // create a socket for the workers
  void* workers = zmq_socket(_context, ZMQ_DEALER);

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
  void* clients = zmq_socket(_context, ZMQ_ROUTER);

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
