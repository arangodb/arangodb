////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"
#include "Utils/ExecContext.h"

namespace {
bool authorized(std::pair<std::string, arangodb::rest::AsyncJobResult> const& job) {
  arangodb::ExecContext const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    return true;
  }

  return (job.first == exec.user());
}
}  // namespace

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

AsyncJobResult::AsyncJobResult()
    : _jobId(0), _response(nullptr), _stamp(0.0), _status(JOB_UNDEFINED) {}

AsyncJobResult::AsyncJobResult(IdType jobId, Status status,
                               std::shared_ptr<RestHandler>&& handler)
    : _jobId(jobId),
      _response(nullptr),
      _stamp(TRI_microtime()),
      _status(status),
      _handler(std::move(handler)) {}

AsyncJobResult::~AsyncJobResult() = default;

AsyncJobManager::AsyncJobManager() : _lock(), _jobs() {}

AsyncJobManager::~AsyncJobManager() {
  // remove all results that haven't been fetched
  deleteJobs();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the result of an async job
////////////////////////////////////////////////////////////////////////////////

GeneralResponse* AsyncJobManager::getJobResult(AsyncJobResult::IdType jobId,
                                               AsyncJobResult::Status& status,
                                               bool removeFromList) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.find(jobId);

  if (it == _jobs.end() || !::authorized(it->second)) {
    status = AsyncJobResult::JOB_UNDEFINED;
    return nullptr;
  }

  GeneralResponse* response = (*it).second.second._response;
  status = (*it).second.second._status;

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

  if (it == _jobs.end() || !::authorized(it->second)) {
    return false;
  }

  GeneralResponse* response = (*it).second.second._response;

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

void AsyncJobManager::deleteJobs() {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.begin();

  while (it != _jobs.end()) {
    if (::authorized(it->second)) {
      GeneralResponse* response = (*it).second.second._response;
      if (response != nullptr) {
        delete response;
      }
      _jobs.erase(it++);
    } else {
      ++it;
    }
  }
}

void AsyncJobManager::deleteExpiredJobResults(double stamp) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.begin();

  while (it != _jobs.end()) {
    if (::authorized(it->second)) {
      AsyncJobResult ajr = (*it).second.second;

      if (ajr._stamp < stamp) {
        GeneralResponse* response = ajr._response;

        if (response != nullptr) {
          delete response;
        }

        _jobs.erase(it++);
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }
}

Result AsyncJobManager::cancelJob(AsyncJobResult::IdType jobId) {
  Result rv;
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _jobs.find(jobId);

  if (it == _jobs.end() || !::authorized(it->second)) {
    rv.reset(TRI_ERROR_HTTP_NOT_FOUND,
             "could not find job (" + std::to_string(jobId) +
                 ") in AsyncJobManager during cancel operation");
    return rv;
  }

  std::shared_ptr<RestHandler>& handler = it->second.second._handler;

  if (handler != nullptr) {
    handler->cancel();
  }
  
  // simon: handlers running async tasks use shared_ptr to keep alive
  it->second.second._handler = nullptr;

  return rv;
}

/// @brief cancel and delete all pending / done jobs
Result AsyncJobManager::clearAllJobs() {
  Result rv;
  WRITE_LOCKER(writeLocker, _lock);

  for (auto& it : _jobs) {
    std::shared_ptr<RestHandler>& handler = it.second.second._handler;

    if (handler != nullptr) {
      handler->cancel();
    }
  }
  _jobs.clear();

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

std::vector<AsyncJobResult::IdType> AsyncJobManager::byStatus(AsyncJobResult::Status status,
                                                              size_t maxCount) {
  std::vector<AsyncJobResult::IdType> jobs;

  {
    size_t n = 0;
    READ_LOCKER(readLocker, _lock);
    auto it = _jobs.begin();

    // iterate the list. the list is sorted by id
    while (it != _jobs.end()) {
      AsyncJobResult::IdType jobId = (*it).first;

      if ((*it).second.second._status == status && ::authorized(it->second)) {
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

void AsyncJobManager::initAsyncJob(std::shared_ptr<RestHandler> handler) {
  handler->assignHandlerId();
  AsyncJobResult::IdType jobId = handler->handlerId();

  std::string user = handler->request()->user();
  AsyncJobResult ajr(jobId, AsyncJobResult::JOB_PENDING, std::move(handler));

  WRITE_LOCKER(writeLocker, _lock);

  _jobs.try_emplace(jobId, std::move(user), std::move(ajr));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finishes the execution of an async job
////////////////////////////////////////////////////////////////////////////////

void AsyncJobManager::finishAsyncJob(RestHandler* handler) {
  AsyncJobResult::IdType jobId = handler->handlerId();
  std::unique_ptr<GeneralResponse> response = handler->stealResponse();

  {
    WRITE_LOCKER(writeLocker, _lock);
    auto it = _jobs.find(jobId);

    if (it == _jobs.end()) {
      return;  // job is already canceled
    }

    it->second.second._response = response.release();
    it->second.second._status = AsyncJobResult::JOB_DONE;
    it->second.second._stamp = TRI_microtime();
  }
}
