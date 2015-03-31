////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpServerJob.h"

#include "Basics/logging.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServer.h"

using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                               class HttpServerJob
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new server job
////////////////////////////////////////////////////////////////////////////////

HttpServerJob::HttpServerJob (HttpServer* server,
                              HttpHandler* handler,
                              bool isDetached)
  : Job("HttpServerJob"),
    _server(server),
    _handler(handler),
    _shutdown(false),
    _abandon(false),
    _isDetached(isDetached) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a server job
////////////////////////////////////////////////////////////////////////////////

HttpServerJob::~HttpServerJob () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the underlying handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler* HttpServerJob::getHandler () const {
  return _handler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the job is detached
////////////////////////////////////////////////////////////////////////////////

bool HttpServerJob::isDetached () const {
  return _isDetached;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abandon job
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::abandon () {
  _abandon.store(true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::JobType HttpServerJob::type () const {
  return _handler->type();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

const string& HttpServerJob::queue () const {
  return _handler->queue();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::setDispatcherThread (DispatcherThread* thread) {
  _handler->setDispatcherThread(thread);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Job::status_t HttpServerJob::work () {
  LOG_TRACE("beginning job %p", (void*) this);

  this->RequestStatisticsAgent::transfer(_handler);

  if (_shutdown.load()) {
    return status_t(Job::JOB_DONE);
  }

  RequestStatisticsAgentSetRequestStart(_handler);
  _handler->prepareExecute();
  Handler::status_t status;

  try {
    status = _handler->execute();
  }
  catch (...) {
    _handler->finalizeExecute();
    throw;
  }

  _handler->finalizeExecute();
  RequestStatisticsAgentSetRequestEnd(_handler);

  LOG_TRACE("finished job %p with status %d", (void*) this, (int) status.status);

  return status.jobStatus();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpServerJob::cancel (bool running) {
  return _handler->cancel(running);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::cleanup () {
  bool abandon = _abandon.load();

  if (! abandon && _server != nullptr) {
    _server->jobDone(this);
  }

  delete this;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpServerJob::beginShutdown () {
  LOG_TRACE("shutdown job %p", (void*) this);

  _shutdown.store(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpServerJob::handleError (basics::Exception const& ex) {
  _handler->handleError(ex);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
