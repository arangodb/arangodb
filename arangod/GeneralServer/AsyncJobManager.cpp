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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "AsyncJobManager.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/voc-errors.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

class arangodb::rest::AsyncCallbackContext {
 public:
  explicit AsyncCallbackContext(std::string const& coordHeader)
      : _coordHeader(coordHeader), _response(nullptr) {}

  ~AsyncCallbackContext() {
    if (_response != nullptr) {
      delete _response;
    }
  }

 public:
  std::string& getCoordinatorHeader() { return _coordHeader; }

 private:
  std::string _coordHeader;
  GeneralResponse* _response;
};

AsyncJobResult::AsyncJobResult()
    : _jobId(0),
      _response(nullptr),
      _stamp(0.0),
      _status(JOB_UNDEFINED),
      _ctx(nullptr),
      _handler(nullptr) {}

AsyncJobResult::AsyncJobResult(IdType jobId, Status status,
                               AsyncCallbackContext* ctx,
                               RestHandler* handler)
    : _jobId(jobId),
      _response(nullptr),
      _stamp(TRI_microtime()),
      _status(status),
      _ctx(ctx),
      _handler(handler) {}

AsyncJobResult::~AsyncJobResult() {}

AsyncJobManager::AsyncJobManager()
    : _lock(), _jobs() {}

AsyncJobManager::~AsyncJobManager() {
  // remove all results that haven't been fetched
  deleteJobResults();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the result of an async job
////////////////////////////////////////////////////////////////////////////////

GeneralResponse* AsyncJobManager::getJobResult(AsyncJobResult::IdType jobId,
                                               AsyncJobResult::Status& status,
                                               bool removeFromList) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.find(jobId);

  if (it == _jobs.end()) {
    status = AsyncJobResult::JOB_UNDEFINED;
    return nullptr;
  }

  GeneralResponse* response = (*it).second._response;
  status = (*it).second._status;

  if (status == AsyncJobResult::JOB_PENDING) {
    return nullptr;
  }

  if (!removeFromList) {
    return nullptr;
  }

  // remove the job from the list
  _jobs.erase(it);
  return response;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the result of an async job
////////////////////////////////////////////////////////////////////////////////

bool AsyncJobManager::deleteJobResult(AsyncJobResult::IdType jobId) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.find(jobId);

  if (it == _jobs.end()) {
    return false;
  }

  GeneralResponse* response = (*it).second._response;

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

void AsyncJobManager::deleteJobResults() {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.begin();

  while (it != _jobs.end()) {
    GeneralResponse* response = (*it).second._response;

    if (response != nullptr) {
      delete response;
    }

    ++it;
  }

  _jobs.clear();
}

void AsyncJobManager::deleteExpiredJobResults(double stamp) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.begin();

  while (it != _jobs.end()) {
    AsyncJobResult ajr = (*it).second;

    if (ajr._stamp < stamp) {
      GeneralResponse* response = ajr._response;

      if (response != nullptr) {
        delete response;
      }

      _jobs.erase(it++);
    } else {
      ++it;
    }
  }
}

Result AsyncJobManager::cancelJob(AsyncJobResult::IdType jobId) {
  Result rv;
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.find(jobId);

  if (it == _jobs.end()) {
    rv.reset(TRI_ERROR_HTTP_NOT_FOUND
            , "could not find job (" + std::to_string(jobId) +
              ") in AsyncJobManager during cancel operation"
            );
    return rv;
  }

  bool ok = true;
  auto handler = it->second._handler;

  if (handler != nullptr) {
    ok = handler->cancel();
  }

  if(!ok){
    // if you end up here you might need to implement the cancel method on your handler
    rv.reset(TRI_ERROR_INTERNAL
            ,"could not cancel job (" + std::to_string(jobId) +
             ") in handler"
            );
  }
  return rv;
}

std::vector<AsyncJobResult::IdType> AsyncJobManager::pending(size_t maxCount) {
  return byStatus(AsyncJobResult::JOB_PENDING, maxCount);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of done jobs
////////////////////////////////////////////////////////////////////////////////

std::vector<AsyncJobResult::IdType> AsyncJobManager::done(size_t maxCount) {
  return byStatus(AsyncJobResult::JOB_DONE, maxCount);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of jobs by status
////////////////////////////////////////////////////////////////////////////////

std::vector<AsyncJobResult::IdType> AsyncJobManager::byStatus(
    AsyncJobResult::Status status, size_t maxCount) {
  std::vector<AsyncJobResult::IdType> jobs;

  {
    size_t n = 0;
    READ_LOCKER(readLocker, _lock);
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
/// @brief initializes an async job
////////////////////////////////////////////////////////////////////////////////

void AsyncJobManager::initAsyncJob(RestHandler* handler, char const* hdr) {
  AsyncJobResult::IdType jobId = handler->handlerId();
  AsyncCallbackContext* ctx = nullptr;

  if (hdr != nullptr) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "Found header X-Arango-Coordinator in async request";
    ctx = new AsyncCallbackContext(std::string(hdr));
  }

  AsyncJobResult ajr(jobId, AsyncJobResult::JOB_PENDING, ctx, handler);

  WRITE_LOCKER(writeLocker, _lock);

  _jobs.emplace(jobId, ajr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finishes the execution of an async job
////////////////////////////////////////////////////////////////////////////////

void AsyncJobManager::finishAsyncJob(RestHandler* handler) {
  AsyncJobResult::IdType jobId = handler->handlerId();
  std::unique_ptr<GeneralResponse> response = handler->stealResponse();
  AsyncCallbackContext* ctx = nullptr;

  {
    WRITE_LOCKER(writeLocker, _lock);
    auto it = _jobs.find(jobId);

    if (it == _jobs.end()) {
      return;
    }

    ctx = (*it).second._ctx;

    if (nullptr != ctx) {
      _jobs.erase(it);
    } else {
      (*it).second._response = response.release();
      (*it).second._status = AsyncJobResult::JOB_DONE;
      (*it).second._stamp = TRI_microtime();
    }
  }

  delete ctx;
}
