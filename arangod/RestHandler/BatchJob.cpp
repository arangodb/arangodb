////////////////////////////////////////////////////////////////////////////////
/// @brief batch job
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_HANDLER_BATCH_JOB_H
#define TRIAGENS_REST_HANDLER_BATCH_JOB_H 1

#include "GeneralServer/GeneralServerJob.h"

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new server job
////////////////////////////////////////////////////////////////////////////////

BatchJob::BatchJob (HttpServer* server, HttpHandler* handler)
  : GeneralServer<HttpServer, HttpHandler>(server, handler),
    _handlers(),
    _subjobs(),
    _doneLock(),
    _jobsDone(0),
    _done(false),
    _cleanup(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a server job
////////////////////////////////////////////////////////////////////////////////

BatchJob::~BatchJob () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sub-job is done
////////////////////////////////////////////////////////////////////////////////

BatchJob::jobDone (BatchSubjob* subjob) {
  MUTEX_LOCKER(_doneLock);

  ++_jobsDone;

  if (_jobsDone >= _handlers.size()) {
    if (_cleanup) {
      GeneralServer<HttpServer, HttpHandler>(server, handler)::cleanup();
    }
    else {
      _done = true;
      _subjobs.clear();
    }
  }
  else {
    _subjobs.erase(subjob);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

status_e BatchJob::work () {
  LOGGER_TRACE << "beginning job " << static_cast<Job*>(this);

  if (_shutdown != 0) {
    return Job::JOB_DONE;
  }

  bool direct = true;
  _handlers = _handler->subhandlers();

  {
    MUTEX_LOCKER(&_doneLock);

    for (vector<HttpHandler*>::iterator i = _handlers.begin();  i != _handlers.end();  ++i) {
      HttpHandler* handler = *i;

      if (handler->isDirect()) {
        executeDirectHandler(handler);
      }
      else {
        createSubjob(handler);
      }
    }
  }

  return Job::JOB_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void BatchJob::cleanup () {
  bool done = false;

  {
    MUTEX_LOCKER(&_doneLock);

    if (_done) {
      done = true;
    }
    else {
      _cleanup = true;
    }
  }

  if (done) {
    GeneralServer<HttpServer, HttpHandler>(server, handler)::cleanup();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool BatchJob::beginShutdown () {
  LOGGER_TRACE << "shutdown job " << static_cast<Job*>(this);

  _shutdown = 1;

  {
    MUTEX_LOCKER(&_abandonLock);

    for (vector<BatchSubjob*>::iterator i = _subjobs.begin();  i != _subjobs.end();  ++i) {
      (*i)->abandon();
    }
  }

  MUTEX_LOCKER(&_doneLock);

  if (_cleanup) {
    delete this;
  }
  else {
    _done = true;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create subjobs
////////////////////////////////////////////////////////////////////////////////

void BatchJob::createSubjob (HttpHandler* handler) {

  // execute handler via dispatcher
  Dispatcher* dispatcher = server->getDispatcher();

  BatchSubjob* job = new BatchSubjob(this, server, handler);

  _subjobs->insert(job);
  dispatcher->addJob(job);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief directly execute job
////////////////////////////////////////////////////////////////////////////////

void BatchJob::executeDirectHandler (HttpHandler* handler) {
  Handler::status_e status = Handler::HANDLER_FAILED;

  do {
    try {
      status = handler->execute();
    }
    catch (basics::TriagensError const& ex) {
      handler->handleError(ex);
    }
    catch (std::exception const& ex) {
      basics::InternalError err(ex);

      handler->handleError(err);
    }
    catch (...) {
      basics::InternalError err;
      handler->handleError(err);
    }
  }
  while (status == Handler::HANDLER_REQUEUE);

  ++_jobsDone;

  if (_jobsDone >= _handlers.size()) {
    _done = true;
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
