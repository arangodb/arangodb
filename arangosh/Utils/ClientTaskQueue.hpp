////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOSH_UTILS_CLIENT_TASK_QUEUE_H
#define ARANGOSH_UTILS_CLIENT_TASK_QUEUE_H 1

#include <memory>
#include <queue>
#include <vector>

#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"

namespace arangodb {
class Result;
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

class ClientManager;
template <typename JobData> class ClientWorker;

/**
 * @brief Provides a simple, parallel task queue for `arangosh`-based clients.
 */
template <typename JobData>
class ClientTaskQueue {
 public:
  /**
   * @brief Processes an individual job
   *
   * Each job will be processed by a worker, and many jobs may run in parallel.
   * Thus any method of this type must be thread-safe.
   *
   * @param  client  Client to use for any requests
   * @param  jobData Data describing the job
   * @return         The result status of the job
   */
  typedef std::function<Result(httpclient::SimpleHttpClient& client,
                               JobData& jobData)>
      JobProcessor;

  /**
   * @brief Handles the result of an individual jobs
   *
   * Each job will be processed by a worker, and many jobs may run in parallel.
   * Thus, any method of this type must be thread-safe. Can be used to requeue a
   * failed job, notify another actor that the job is done, etc.
   *
   * @param jobData Data describing the job which was just processed
   * @param result  The result status of the job
   */
  typedef std::function<void(std::unique_ptr<JobData>&& jobData,
                             Result const& result)>
      JobResultHandler;

 public:
  ClientTaskQueue(JobProcessor processJob, JobResultHandler handleJobResult);
  virtual ~ClientTaskQueue();

 protected:
  /**
   * @brief Spawn a number of workers to handle queued tasks
   *
   * The workers will be be live for the duration of the queue's lifecycle, so
   * it should only be necessary to spawn workers once.
   *
   * @param  manager    Manager to create a new client for each spawned worker
   * @param  numWorkers The number of workers to spawn
   * @return            `true` if successful
   */
  bool spawnWorkers(ClientManager& manager, size_t const& numWorkers) noexcept;

  /**
   * @brief Determines if the job queue is currently empty
   *
   * Thread-safe.
   *
   * @return `true` if there are no pending jobs
   */
  bool isQueueEmpty() const noexcept;

  /**
   * @brief Determines if all workers are currently busy processing a job
   *
   * Thread-safe.
   *
   * @return `true` if all workers are busy
   */
  bool allWorkersBusy() const noexcept;

  /**
   * @brief Queues a job to be processed
   *
   * Thread-safe.
   *
   * @param  jobData Data describing the job
   * @return         `true` if successfully queued
   */
  bool queueJob(std::unique_ptr<JobData>&& jobData) noexcept;

 private:
  std::unique_ptr<JobData> fetchJob() noexcept;
  void waitForWork() noexcept;

 private:
  JobProcessor _processJob;
  JobResultHandler _handleJobResult;
  mutable Mutex _jobsLock;
  basics::ConditionVariable _jobsCondition;
  std::queue<std::unique_ptr<JobData>> _jobs;
  mutable Mutex _workersLock;
  std::vector<std::unique_ptr<ClientWorker<JobData>>> _workers;

  friend class ClientWorker<JobData>;
};
}  // namespace arangodb

#endif
