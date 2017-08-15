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

#ifndef ARANGOD_HTTP_SERVER_ASYNC_JOB_MANAGER_H
#define ARANGOD_HTTP_SERVER_ASYNC_JOB_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/ReadWriteLock.h"

namespace arangodb {
class GeneralResponse;

namespace rest {
class AsyncCallbackContext;
class GeneralServerJob;
class RestHandler;

// -----------------------------------------------------------------------------
// --SECTION--                                                    AsyncJobResult
// -----------------------------------------------------------------------------

struct AsyncJobResult {
 public:
  typedef enum { JOB_UNDEFINED, JOB_PENDING, JOB_DONE } Status;
  typedef uint64_t IdType;

 public:
  AsyncJobResult();

  AsyncJobResult(IdType jobId, Status status, AsyncCallbackContext* ctx,
                 RestHandler* handler);

  ~AsyncJobResult();

 public:
  IdType _jobId;
  GeneralResponse* _response;
  double _stamp;
  Status _status;
  AsyncCallbackContext* _ctx;
  RestHandler* _handler;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   AsyncJobManager
// -----------------------------------------------------------------------------

class AsyncJobManager {
  AsyncJobManager(AsyncJobManager const&) = delete;
  AsyncJobManager& operator=(AsyncJobManager const&) = delete;

 public:
  typedef std::unordered_map<AsyncJobResult::IdType, AsyncJobResult> JobList;

 public:
  AsyncJobManager();
  ~AsyncJobManager();

 public:
  GeneralResponse* getJobResult(AsyncJobResult::IdType, AsyncJobResult::Status&,
                                bool removeFromList);
  bool deleteJobResult(AsyncJobResult::IdType);
  void deleteJobResults();
  void deleteExpiredJobResults(double stamp);
  Result cancelJob(AsyncJobResult::IdType);

  std::vector<AsyncJobResult::IdType> pending(size_t maxCount);
  std::vector<AsyncJobResult::IdType> done(size_t maxCount);
  std::vector<AsyncJobResult::IdType> byStatus(AsyncJobResult::Status,
                                               size_t maxCount);
  void initAsyncJob(RestHandler*, char const*);
  void finishAsyncJob(RestHandler*);

 private:
  basics::ReadWriteLock _lock;
  JobList _jobs;
};
}
}

#endif
