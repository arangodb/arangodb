////////////////////////////////////////////////////////////////////////////////
/// @brief job manager
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
/// @author Jan Steemann
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2004-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "AsyncJobManager.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/logging.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServerJob.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                        class AsyncCallbackContext
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief AsyncCallbackContext
////////////////////////////////////////////////////////////////////////////////

class triagens::rest::AsyncCallbackContext {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

    AsyncCallbackContext (string const& coordHeader)
      : _coordHeader(coordHeader),
        _response(nullptr) {
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    ~AsyncCallbackContext () {
      if (_response != nullptr) {
        delete _response;
      }
    }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the coordinator header
////////////////////////////////////////////////////////////////////////////////

    string& getCoordinatorHeader () {
      return _coordHeader;
    }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief coordinator header
////////////////////////////////////////////////////////////////////////////////

    string _coordHeader;

////////////////////////////////////////////////////////////////////////////////
/// @brief http response
////////////////////////////////////////////////////////////////////////////////

    HttpResponse* _response;
};

// -----------------------------------------------------------------------------
// --SECTION--                                              class AsyncJobResult
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for an unspecified job result
////////////////////////////////////////////////////////////////////////////////

AsyncJobResult::AsyncJobResult ()
  : _jobId(0),
    _response(nullptr),
    _stamp(0.0),
    _status(JOB_UNDEFINED),
    _ctx(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a specific job result
////////////////////////////////////////////////////////////////////////////////

AsyncJobResult::AsyncJobResult (IdType jobId,
                                HttpResponse* response,
                                double stamp,
                                Status status,
                                AsyncCallbackContext* ctx)
  : _jobId(jobId),
    _response(response),
    _stamp(stamp),
    _status(status),
    _ctx(ctx) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

AsyncJobResult::~AsyncJobResult () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             class AsyncJobManager
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

AsyncJobManager::AsyncJobManager (generate_fptr idFunc, callback_fptr callback)
  : _lock(),
    _jobs(),
    generate(idFunc),
    callback(callback) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

AsyncJobManager::~AsyncJobManager () {
  // remove all results that haven't been fetched
  deleteJobResults();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the result of an async job
////////////////////////////////////////////////////////////////////////////////

HttpResponse* AsyncJobManager::getJobResult (AsyncJobResult::IdType jobId,
                                             AsyncJobResult::Status& status,
                                             bool removeFromList) {
  WRITE_LOCKER(_lock);

  auto it = _jobs.find(jobId);

  if (it == _jobs.end()) {
    status = AsyncJobResult::JOB_UNDEFINED;
    return nullptr;
  }

  HttpResponse* response = (*it).second._response;
  status = (*it).second._status;

  if (status == AsyncJobResult::JOB_PENDING) {
    return nullptr;
  }

  if (! removeFromList) {
    return nullptr;
  }

  // remove the job from the list
  _jobs.erase(it);
  return response;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the result of an async job
////////////////////////////////////////////////////////////////////////////////

bool AsyncJobManager::deleteJobResult (AsyncJobResult::IdType jobId) {
  WRITE_LOCKER(_lock);

  auto it = _jobs.find(jobId);

  if (it == _jobs.end()) {
    return false;
  }

  HttpResponse* response = (*it).second._response;

  if (response != nullptr) {
    delete response;
  }

  // remove the job from the list
  _jobs.erase(it);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes all results
////////////////////////////////////////////////////////////////////////////////

void AsyncJobManager::deleteJobResults () {
  WRITE_LOCKER(_lock);

  auto it = _jobs.begin();

  while (it != _jobs.end()) {
    HttpResponse* response = (*it).second._response;

    if (response != nullptr) {
      delete response;
    }

    ++it;
  }

  _jobs.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes expired results
////////////////////////////////////////////////////////////////////////////////

void AsyncJobManager::deleteExpiredJobResults (double stamp) {
  WRITE_LOCKER(_lock);

  auto it = _jobs.begin();

  while (it != _jobs.end()) {
    AsyncJobResult ajr = (*it).second;

    if (ajr._stamp < stamp) {
      HttpResponse* response = ajr._response;

      if (response != nullptr) {
        delete response;
      }

      _jobs.erase(it++);
    }
    else {
      ++it;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of pending jobs
////////////////////////////////////////////////////////////////////////////////

const vector<AsyncJobResult::IdType> AsyncJobManager::pending (size_t maxCount) {
  return byStatus(AsyncJobResult::JOB_PENDING, maxCount);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of done jobs
////////////////////////////////////////////////////////////////////////////////

const vector<AsyncJobResult::IdType> AsyncJobManager::done (size_t maxCount) {
  return byStatus(AsyncJobResult::JOB_DONE, maxCount);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of jobs by status
////////////////////////////////////////////////////////////////////////////////

const vector<AsyncJobResult::IdType> AsyncJobManager::byStatus (AsyncJobResult::Status status,
                                                                size_t maxCount) {
  vector<AsyncJobResult::IdType> jobs;

  {
    size_t n = 0;
    READ_LOCKER(_lock);
    auto it = _jobs.begin();

    // iterate the list. the list is sorted by id
    while (it != _jobs.end()) {
      AsyncJobResult::IdType jobId = (*it).first;

      if ((*it).second._status == status) {
        jobs.emplace_back(jobId);

        if (++n >= maxCount) {
          break;
        }
      }

      ++it;
    }
  }

  return jobs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an async job
////////////////////////////////////////////////////////////////////////////////

void AsyncJobManager::initAsyncJob (HttpServerJob* job, uint64_t* jobId) {
  if (jobId == nullptr) {
    return;
  }

  TRI_ASSERT(job != nullptr);

  *jobId = generate();
  job->assignId(*jobId);

  AsyncCallbackContext* ctx = nullptr;

  bool found;
  char const* hdr = job->handler()->getRequest()->header("x-arango-coordinator", found);

  if (found) {
    LOG_DEBUG("Found header X-Arango-Coordinator in async request");
    ctx = new AsyncCallbackContext(string(hdr));
  }

  AsyncJobResult ajr(*jobId,
                     nullptr,
                     TRI_microtime(),
                     AsyncJobResult::JOB_PENDING,
                     ctx);

  WRITE_LOCKER(_lock);

  _jobs.emplace(*jobId, ajr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finishes the execution of an async job
////////////////////////////////////////////////////////////////////////////////

void AsyncJobManager::finishAsyncJob (HttpServerJob* job) {
  TRI_ASSERT(job != nullptr);

  HttpHandler* handler = job->handler();
  TRI_ASSERT(handler != nullptr);

  AsyncJobResult::IdType jobId = job->id();

  if (jobId == 0) {
    return;
  }

  double const now = TRI_microtime();
  AsyncCallbackContext* ctx = nullptr;
  HttpResponse* response    = nullptr;

  {
    WRITE_LOCKER(_lock);
    auto it = _jobs.find(jobId);

    if (it == _jobs.end()) {
      // job is already deleted.
      // do nothing here. the dispatcher will throw away the handler,
      // which will also dispose the response
      return;
    }
    else {
      response = handler->stealResponse();

      (*it).second._response = response;
      (*it).second._status = AsyncJobResult::JOB_DONE;
      (*it).second._stamp = now;

      ctx = (*it).second._ctx;

      if (ctx != nullptr) {
        // we have found a context object, so we can immediately remove the job
        // from the list of "done" jobs
        _jobs.erase(it);
      }
    }
  }

  // If we got here, then we have stolen the pointer to the response

  // If there is a callback context, the job is no longer in the
  // list of "done" jobs, so we have to free the response and the
  // callback context:

  if (nullptr != ctx && nullptr != callback) {
    callback(ctx->getCoordinatorHeader(), response);
    delete ctx;

    if (response != nullptr) {
      delete response;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
