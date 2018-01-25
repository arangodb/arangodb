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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/Result.h"
#include "Basics/Thread.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/ClientManager.hpp"

namespace arangodb {
template <typename JobData>
class ClientWorker;

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
   * Thus any method of this type must be thread-safe. A given instance of the
   * JobData class will only be handled by one worker at a time, so access to
   * the data need not be synchronized.
   *
   * @param  client  Client to use for any requests
   * @param  jobData Data describing the job
   * @return         The result status of the job
   */
  using JobProcessor = std::function<Result(
      httpclient::SimpleHttpClient& client, JobData& jobData)>;

  /**
   * @brief Handles the result of an individual jobs
   *
   * Each job will be processed by a worker, and many jobs may run in parallel.
   * Thus, any method of this type must be thread-safe. A given instance of the
   * JobData class will only be handled by one worker at a time, so access to
   * the data need not be synchronized. Can be used to requeue a failed job,
   * notify another actor that the job is done, etc.
   *
   * @param jobData Data describing the job which was just processed
   * @param result  The result status of the job
   */
  using JobResultHandler = std::function<void(
      std::unique_ptr<JobData>&& jobData, Result const& result)>;

 public:
  ClientTaskQueue(JobProcessor processJob, JobResultHandler handleJobResult);
  virtual ~ClientTaskQueue();

 public:
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
  bool spawnWorkers(ClientManager& manager,
                    uint32_t const& numWorkers) noexcept;

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
   * @brief Determines if all workers are currently waiting for work
   *
   * Thread-safe.
   *
   * @return `true` if all workers are idle
   */
  bool allWorkersIdle() const noexcept;

  /**
   * @brief Queues a job to be processed
   *
   * Thread-safe.
   *
   * @param  jobData Data describing the job
   * @return         `true` if successfully queued
   */
  bool queueJob(std::unique_ptr<JobData>&& jobData) noexcept;

  /**
   * @brief Empties the queue by deleting all jobs not yet started
   *
   * Thread-safe.
   */
  void clearQueue() noexcept;

  /**
   * @brief Waits for the queue to be empty and all workers to be idle
   *
   * Thread-safe.
   */
  void waitForIdle() noexcept;

 private:
  std::unique_ptr<JobData> fetchJob() noexcept;
  void notifyIdle() noexcept;
  void waitForWork() noexcept;

 private:
  JobProcessor _processJob;
  JobResultHandler _handleJobResult;
  mutable Mutex _jobsLock;
  basics::ConditionVariable _jobsCondition;
  std::queue<std::unique_ptr<JobData>> _jobs;
  mutable Mutex _workersLock;
  basics::ConditionVariable _workersCondition;
  std::vector<std::unique_ptr<ClientWorker<JobData>>> _workers;

  friend class ClientWorker<JobData>;
};
#include "ClientTaskQueue.ipp"
}  // namespace arangodb

#endif
