////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <unordered_map>
#include <vector>

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"

namespace arangodb {
class GeneralResponse;

namespace rest {
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

  AsyncJobResult(IdType jobId, Status status,
                 std::shared_ptr<RestHandler>&& handler);

  ~AsyncJobResult();

 public:
  IdType _jobId;
  GeneralResponse* _response;
  double _stamp;
  Status _status;
  std::shared_ptr<RestHandler> _handler;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   AsyncJobManager
// -----------------------------------------------------------------------------

/// @brief Manages responses which will be fetched later by clients.
class AsyncJobManager {
  AsyncJobManager(AsyncJobManager const&) = delete;
  AsyncJobManager& operator=(AsyncJobManager const&) = delete;

 public:
  typedef std::unordered_map<AsyncJobResult::IdType,
                             std::pair<std::string, AsyncJobResult>>
      JobList;

 public:
  AsyncJobManager();
  ~AsyncJobManager();

 public:
  GeneralResponse* getJobResult(AsyncJobResult::IdType, AsyncJobResult::Status&,
                                bool removeFromList);
  bool deleteJobResult(AsyncJobResult::IdType);
  void deleteJobs();
  void deleteExpiredJobResults(double stamp);

  /// @brief cancel and delete a specific job
  Result cancelJob(AsyncJobResult::IdType);

  /// @brief cancel and delete all pending / done jobs
  Result clearAllJobs();

  std::vector<AsyncJobResult::IdType> pending(size_t maxCount);
  std::vector<AsyncJobResult::IdType> done(size_t maxCount);
  std::vector<AsyncJobResult::IdType> byStatus(AsyncJobResult::Status,
                                               size_t maxCount);
  void initAsyncJob(std::shared_ptr<RestHandler>);
  void finishAsyncJob(RestHandler*);
  std::pair<uint64_t, uint64_t> getNrPendingAndDone();

  void initiateSoftShutdown() {
    _softShutdownOngoing.store(true, std::memory_order_relaxed);
  }

 private:
  basics::ReadWriteLock _lock;
  JobList _jobs;

  ////////////////////////////////////////////////////////////////////////////
  /// @brief flag, if a soft shutdown is ongoing, this is used for the soft
  /// shutdown feature in coordinators, it is initially `false` and is set
  /// to true once a soft shutdown has begun.
  ////////////////////////////////////////////////////////////////////////////

  std::atomic<bool> _softShutdownOngoing;
};
}  // namespace rest
}  // namespace arangodb
