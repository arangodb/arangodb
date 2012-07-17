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

#include "BatchJob.h"
#include "RestHandler/BatchSubjob.h"

using namespace triagens::basics;
using namespace triagens::rest;
    
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
  : GeneralServerJob<HttpServer, HttpHandler>(server, handler),
    _doneAccomplisher(NOONE),
    _handlers(),
    _subjobs(),
    _doneLock(),
    _iteratorLock(),
    _jobsDone(0),
    _cleanup(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a server job
////////////////////////////////////////////////////////////////////////////////

BatchJob::~BatchJob () {
  MUTEX_LOCKER(_iteratorLock);
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

void BatchJob::jobDone (BatchSubjob* subjob) {
  _handler->addResponse(subjob->getHandler());
  _doneLock.lock(); 

  ++_jobsDone;

  if (_jobsDone >= _handlers.size()) {
    // all sub-jobs are done
    if (_cleanup) {
      _doneLock.unlock();

      // cleanup might delete ourselves!
      GeneralServerJob<HttpServer, HttpHandler>::cleanup();
    }
    else {
      _doneAccomplisher = ASYNC;
      _subjobs.clear();
      _doneLock.unlock();
      
      cleanup();
    }
  }
  else {
    // still something to do
    _subjobs.erase(subjob);
    _doneLock.unlock();
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

Job::status_e BatchJob::work () {
  LOGGER_TRACE << "beginning job " << static_cast<Job*>(this);

  if (_shutdown != 0) {
    return Job::JOB_DONE;
  }
 
  // we must grab this lock so no one else can kill us while we're iterating 
  // over the sub handlers 
  MUTEX_LOCKER(_iteratorLock);

  // handler::execute() is called to prepare the batch handler
  // if it returns anything else but HANDLER_DONE, this is an
  // indication of an error
  if (_handler->execute() != Handler::HANDLER_DONE) {
    // handler failed
    _doneAccomplisher = DIRECT;

    return Job::JOB_FAILED;
  }
  
  // setup did not fail

  bool hasAsync = false;
  _handlers = _handler->subhandlers();

  for (vector<HttpHandler*>::const_iterator i = _handlers.begin();  i != _handlers.end();  ++i) {
    HttpHandler* handler = *i;

    if (handler->isDirect()) {
      executeDirectHandler(handler);
    }
    else {
      if (!hasAsync) {
        // we must do this ourselves. it is not safe to have the dispatcherThread
        // call this method because the job might be deleted before that
        _handler->setDispatcherThread(0);
        hasAsync = true;
      }
      createSubjob(handler);
    }
  }

  if (!hasAsync) {
    // only jobs executed directly, we're done and let the dispatcher kill us
    return Job::JOB_DONE;
  }

  MUTEX_LOCKER(_doneLock);
  if (_doneAccomplisher == DIRECT) {
    // all jobs already done. last job was finished by direct execution
    return Job::JOB_DONE;
  }

  // someone else must kill this job
  return Job::JOB_DETACH;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void BatchJob::cleanup () {
  bool done = false;

  {
    MUTEX_LOCKER(_doneLock);

    if (_doneAccomplisher != NOONE) {
      done = true;
    }
    else {
      _cleanup = true;
    }
  }

  if (done) {
    GeneralServerJob<HttpServer, HttpHandler>::cleanup();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool BatchJob::beginShutdown () {
  LOGGER_TRACE << "shutdown job " << static_cast<Job*>(this);

  MUTEX_LOCKER(_doneLock);
  _shutdown = 1;

  {
    MUTEX_LOCKER(_abandonLock);

    for (set<BatchSubjob*>::iterator i = _subjobs.begin();  i != _subjobs.end();  ++i) {
      (*i)->abandon();
    }
  }

  if (_cleanup) {
    //GeneralServerJob<HttpServer, HttpHandler>::cleanup();

  }
  else {
    _doneAccomplisher = TASK;
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
  BatchSubjob* job = new BatchSubjob(this, _server, handler);
  {
    MUTEX_LOCKER(_doneLock);
    _subjobs.insert(job);
  }
  _server->getDispatcher()->addJob(job);
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

  if (status == Handler::HANDLER_DONE) {
    _handler->addResponse(handler);
  }
  
  MUTEX_LOCKER(_doneLock);
  ++_jobsDone;

  if (_jobsDone >= _handlers.size()) {
    _doneAccomplisher = DIRECT;
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
