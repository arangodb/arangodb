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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <queue>
#include <vector>

#include <velocypack/Iterator.h>

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/Result.h"
#include "Basics/Thread.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/ClientManager.h"

namespace arangodb {
template<typename JobData>
class Worker;

/**
 * @brief Provides a simple, parallel task queue for `arangosh`-based clients.
 */
template<typename JobData>
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
  using JobProcessor = std::function<void(httpclient::SimpleHttpClient& client,
                                          JobData& jobData)>;

  /**
   * @brief Handles the result of an individual jobs
   *
   * Each job will be processed by a worker, and many jobs may run in parallel.
   * Thus, any method of this type must be thread-safe. A given instance of the
   * JobData class will only be handled by one worker at a time, so access to
   * the data need not be synchronized. Can be used to requeue a failed job,
   * notify another actor that the job is done, etc.
   *
   * @param server  A reference the the underlying application server
   * @param jobData Data describing the job which was just processed
   * @param result  The result status of the job
   */
  using JobResultHandler = std::function<void(
      std::unique_ptr<JobData>&& jobData, Result const& result)>;

 public:
  ClientTaskQueue(application_features::ApplicationServer& server,
                  JobProcessor processJob);
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
   * @brief Determines if the job queue is currently empty and all workers
   * are idle
   *
   * Thread-safe.
   *
   * @return `true` if queue is empty and all workers are idle
   */
  bool isQueueEmptyAndAllWorkersIdle() const noexcept;

  /**
   * @brief Determines the number of currently queued jobs, the number
   * of total workers and the number of busy workers
   *
   * Thread-safe.
   *
   * @return number of queued jobs, number of workers, number of busy workers
   */
  std::tuple<size_t, size_t, size_t> statistics() const noexcept;

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
  class Worker : public arangodb::Thread {
   private:
    Worker(Worker const&) = delete;
    Worker& operator=(Worker const&) = delete;

   public:
    explicit Worker(application_features::ApplicationServer&,
                    ClientTaskQueue<JobData>&,
                    std::unique_ptr<httpclient::SimpleHttpClient>&&);
    virtual ~Worker();

    bool isIdle() const noexcept;  // not currently processing a job

   protected:
    void run() override;

   private:
    ClientTaskQueue<JobData>& _queue;
    std::unique_ptr<httpclient::SimpleHttpClient> _client;
    std::atomic<bool> _idle;
  };

 private:
  application_features::ApplicationServer& _server;

  JobProcessor _processJob;

  // protects _jobs
  mutable basics::ConditionVariable _jobsCondition;
  std::queue<std::unique_ptr<JobData>> _jobs;

  // protects _workers
  mutable basics::ConditionVariable _workersCondition;
  std::vector<std::unique_ptr<Worker>> _workers;

  friend class Worker;
};

////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION BELOW
////////////////////////////////////////////////////////////////////////////////

template<typename JobData>
inline ClientTaskQueue<JobData>::ClientTaskQueue(
    application_features::ApplicationServer& server, JobProcessor processJob)
    : _server(server), _processJob(processJob) {}

template<typename JobData>
ClientTaskQueue<JobData>::~ClientTaskQueue() {
  for (auto& worker : _workers) {
    worker->beginShutdown();
  }

  clearQueue();
  _jobsCondition.broadcast();
}

template<typename JobData>
inline bool ClientTaskQueue<JobData>::spawnWorkers(
    ClientManager& manager, uint32_t const& numWorkers) noexcept {
  uint32_t spawned = 0;
  try {
    CONDITION_LOCKER(lock, _workersCondition);
    for (; spawned < numWorkers; spawned++) {
      auto client = manager.getConnectedClient(false, false, true, spawned);
      auto worker = std::make_unique<Worker>(_server, *this, std::move(client));
      _workers.emplace_back(std::move(worker));
      _workers.back()->start();
    }
  } catch (...) {
  }
  return (numWorkers == spawned);
}

template<typename JobData>
inline bool ClientTaskQueue<JobData>::isQueueEmpty() const noexcept {
  CONDITION_LOCKER(lock, _jobsCondition);
  return _jobs.empty();
}

template<typename JobData>
inline bool ClientTaskQueue<JobData>::isQueueEmptyAndAllWorkersIdle()
    const noexcept {
  // acquire both locks here, so we can avoid data races
  CONDITION_LOCKER(lock1, _jobsCondition);
  CONDITION_LOCKER(lock2, _workersCondition);

  if (!_jobs.empty()) {
    return false;
  }
  for (auto const& worker : _workers) {
    if (!worker->isIdle()) {
      return false;
    }
  }

  return true;
}

template<typename JobData>
inline std::tuple<size_t, size_t, size_t> ClientTaskQueue<JobData>::statistics()
    const noexcept {
  size_t busy = 0;
  size_t workers = 0;

  CONDITION_LOCKER(lock, _workersCondition);
  for (auto const& worker : _workers) {
    ++workers;
    if (worker->isIdle()) {
      ++busy;
    }
  }
  return std::make_tuple(_jobs.size(), workers, busy);
}

template<typename JobData>
inline bool ClientTaskQueue<JobData>::allWorkersBusy() const noexcept {
  CONDITION_LOCKER(lock, _workersCondition);

  for (auto const& worker : _workers) {
    if (worker->isIdle()) {
      return false;
    }
  }
  return true;
}

template<typename JobData>
inline bool ClientTaskQueue<JobData>::allWorkersIdle() const noexcept {
  CONDITION_LOCKER(lock, _workersCondition);

  for (auto const& worker : _workers) {
    if (!worker->isIdle()) {
      return false;
    }
  }
  return true;
}

template<typename JobData>
inline bool ClientTaskQueue<JobData>::queueJob(
    std::unique_ptr<JobData>&& job) noexcept {
  try {
    {
      CONDITION_LOCKER(lock, _jobsCondition);
      _jobs.emplace(std::move(job));
    }
    _jobsCondition.signal();
    return true;
  } catch (...) {
    return false;
  }
}

template<typename JobData>
inline void ClientTaskQueue<JobData>::clearQueue() noexcept {
  CONDITION_LOCKER(lock, _jobsCondition);
  while (!_jobs.empty()) {
    _jobs.pop();
  }
}

template<typename JobData>
inline void ClientTaskQueue<JobData>::waitForIdle() noexcept {
  while (true) {
    if (isQueueEmptyAndAllWorkersIdle()) {
      break;
    }

    CONDITION_LOCKER(lock, _workersCondition);
    lock.wait(std::chrono::milliseconds(100));
  }
}

template<typename JobData>
inline std::unique_ptr<JobData> ClientTaskQueue<JobData>::fetchJob() noexcept {
  std::unique_ptr<JobData> job;

  CONDITION_LOCKER(lock, _jobsCondition);

  if (!_jobs.empty()) {
    job = std::move(_jobs.front());
    _jobs.pop();
  }

  return job;
}

template<typename JobData>
inline void ClientTaskQueue<JobData>::waitForWork() noexcept {
  CONDITION_LOCKER(lock, _jobsCondition);

  if (!_jobs.empty()) {
    return;
  }

  lock.wait(std::chrono::milliseconds(500));
}

template<typename JobData>
inline void ClientTaskQueue<JobData>::notifyIdle() noexcept {
  _workersCondition.signal();
}

template<typename JobData>
inline ClientTaskQueue<JobData>::Worker::Worker(
    application_features::ApplicationServer& server,
    ClientTaskQueue<JobData>& queue,
    std::unique_ptr<httpclient::SimpleHttpClient>&& client)
    : Thread(server, "Worker"),
      _queue(queue),
      _client(std::move(client)),
      _idle(true) {}

template<typename JobData>
inline ClientTaskQueue<JobData>::Worker::~Worker() {
  Thread::shutdown();
}

template<typename JobData>
inline bool ClientTaskQueue<JobData>::Worker::isIdle() const noexcept {
  return _idle.load();
}

template<typename JobData>
inline void ClientTaskQueue<JobData>::Worker::run() {
  while (!isStopping()) {
    // we must unconditionally report as busy here, regardless of if there is
    // actually a job on the queue or not. this is because otherwise there can
    // be a race between
    // - this thread popping a job from the queue and only after that reporting
    // busy.
    // - someone checking if there are no jobs left and all workers being idle.
    //
    _idle.store(false);

    std::unique_ptr<JobData> job = _queue.fetchJob();

    if (job) {
      try {
        _queue._processJob(*_client, *job);
      } catch (...) {
      }

      _idle.store(true);
      _queue.notifyIdle();
    } else {
      _idle.store(true);
    }

    _queue.waitForWork();
  }
}

}  // namespace arangodb
