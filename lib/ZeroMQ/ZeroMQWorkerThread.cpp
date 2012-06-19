////////////////////////////////////////////////////////////////////////////////
/// @brief ZeroMQ worker thread
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

#include "ZeroMQWorkerThread.h"

#include <czmq.h>

#include "Dispatcher/Dispatcher.h"
#include "Logger/Logger.h"
#include "ZeroMQ/ZeroMQBatchJob.h"

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

ZeroMQWorkerThread::ZeroMQWorkerThread (Dispatcher* dispatcher,
                                        HttpHandlerFactory* handlerFactory,
                                        zctx_t* context,
                                        string const& connection)
  : Thread("zeromq-worker"),
    ZeroMQThread(context),
    _connection(connection),
    _dispatcher(dispatcher),
    _handlerFactory(handlerFactory) {
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

void ZeroMQWorkerThread::run () {

  // create a socket for the server
  void* responder = zsocket_new(_context, ZMQ_DEALER);

  if (responder == 0) {
    LOGGER_FATAL << "cannot initialize ZeroMQ worker socket: " << zmq_strerror(errno);
    zctx_destroy(&_context);
    exit(EXIT_FAILURE);
  }

  // and bind it to the connection
  zsocket_connect(responder, _connection.c_str());

  // handle messages
  while (_stopping == 0) {

    // receive next message
    zmsg_t* request = zmsg_recv(responder);
    zmsg_dump(request);

    if (request == 0) {
      if (errno == ETERM) {
        break;
      }

      continue;
    }

    // extract the address and the content
    zframe_t* address = zmsg_pop(request);
    zmsg_pop(request);
    zframe_t* content = zmsg_pop(request);
    zmsg_destroy(&request);

    if (address == 0 || content == 0) {
      if (address != 0) {
        zframe_destroy(&address);
      }

      if (content != 0) {
        zframe_destroy(&content);
      }

      continue;
    }

    // create a Job for next batch
    ZeroMQBatchJob* job = new ZeroMQBatchJob(address,
                                             _handlerFactory,
                                             (char const*) zframe_data(content), 
                                             zframe_size(content));

    zframe_destroy(&content);

    // check if message contains any requests
    if (job->isDone()) {
      job->finish(responder);
    }

    // handle any direct requests
    else {
      job->extractNextRequest();

      // handle request directly
      if (job->isDirect()) {
        Job::status_e status = job->work();

        if (status == Job::JOB_DONE_ZEROMQ) {
          job->finish(responder);
        }
        else if (status == Job::JOB_REQUEUE) {
          _dispatcher->addJob(job);
        }
        else {
          continue;
        }
      }

      // let the dispatcher deal with the jobs
      else {
        _dispatcher->addJob(job);
      }
    }
  }

  zsocket_destroy(_context, &responder);
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
