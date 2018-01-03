////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "ClientTaskQueue.hpp"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/Result.h"
#include "Basics/Thread.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/ClientManager.hpp"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

namespace arangodb {
template <typename JobData>
class ClientWorker : public arangodb::Thread {
 private:
  ClientWorker(ClientWorker const&) = delete;
  ClientWorker& operator=(ClientWorker const&) = delete;

 public:
  explicit ClientWorker(ClientTaskQueue<JobData>&,
                        std::unique_ptr<SimpleHttpClient>&&);
  virtual ~ClientWorker();

  bool isIdle() const noexcept;  // not currently processing a job

 protected:
  void run() override;

 private:
  ClientTaskQueue<JobData>& _queue;
  std::unique_ptr<SimpleHttpClient> _client;
  std::atomic<bool> _idle;
};
}  // namespace arangodb

template <typename JobData>
ClientTaskQueue<JobData>::ClientTaskQueue(JobProcessor processJob,
                                          JobResultHandler handleJobResult)
    : _processJob(processJob), _handleJobResult(handleJobResult) {}

template <typename JobData>
ClientTaskQueue<JobData>::~ClientTaskQueue() {
  for (auto& worker : _workers) {
    worker->beginShutdown();
  }
  _jobsCondition.broadcast();
}

template <typename JobData>
bool ClientTaskQueue<JobData>::spawnWorkers(ClientManager& manager,
                                            uint32_t const& numWorkers) noexcept {
  MUTEX_LOCKER(lock, _workersLock);
  uint32_t spawned = 0;
  for (; spawned < numWorkers; spawned++) {
    try {
      auto client = manager.getConnectedClient();
      auto worker =
          std::make_unique<ClientWorker<JobData>>(*this, std::move(client));
      _workers.emplace_back(std::move(worker));
      _workers.back()->start();
    } catch (...) {
      break;
    }
  }
  return (numWorkers == spawned);
}

template <typename JobData>
bool ClientTaskQueue<JobData>::isQueueEmpty() const noexcept {
  MUTEX_LOCKER(lock, _jobsLock);
  return _jobs.empty();
}

template <typename JobData>
bool ClientTaskQueue<JobData>::allWorkersBusy() const noexcept {
  MUTEX_LOCKER(lock, _workersLock);
  for (auto& worker : _workers) {
    if (worker->isIdle()) {
      return false;
    }
  }
  return true;
}

template <typename JobData>
bool ClientTaskQueue<JobData>::allWorkersIdle() const noexcept {
  MUTEX_LOCKER(lock, _workersLock);
  for (auto& worker : _workers) {
    if (!worker->isIdle()) {
      return false;
    }
  }
  return true;
}

template <typename JobData>
bool ClientTaskQueue<JobData>::queueJob(
    std::unique_ptr<JobData>&& job) noexcept {
  MUTEX_LOCKER(lock, _jobsLock);
  try {
    _jobs.emplace(std::move(job));
    _jobsCondition.signal();
    return true;
  } catch (...) {
    return false;
  }
}

template <typename JobData>
void ClientTaskQueue<JobData>::clearQueue() noexcept {
  MUTEX_LOCKER(lock, _jobsLock);
  while (!_jobs.empty()) {
    _jobs.pop();
  }
}

template <typename JobData>
void ClientTaskQueue<JobData>::waitForIdle() noexcept {
  while (true) {
    if (isQueueEmpty() && allWorkersIdle()) {
      break;
    }

    CONDITION_LOCKER(lock, _workersCondition);
    lock.wait(std::chrono::milliseconds(500));
  }
}

template <typename JobData>
std::unique_ptr<JobData> ClientTaskQueue<JobData>::fetchJob() noexcept {
  std::unique_ptr<JobData> job(nullptr);
  MUTEX_LOCKER(lock, _jobsLock);

  if (_jobs.empty()) {
    return job;
  }

  job = std::move(_jobs.front());
  _jobs.pop();
  return job;
}

template <typename JobData>
void ClientTaskQueue<JobData>::waitForWork() noexcept {
  if (!isQueueEmpty()) {
    return;
  }

  CONDITION_LOCKER(lock, _jobsCondition);
  lock.wait(std::chrono::milliseconds(500));
}

template <typename JobData>
void ClientTaskQueue<JobData>::notifyIdle() noexcept {
  _workersCondition.signal();
}

template <typename JobData>
ClientWorker<JobData>::ClientWorker(
    ClientTaskQueue<JobData>& queue,
    std::unique_ptr<httpclient::SimpleHttpClient>&& client)
    : Thread("ClientWorker"),
      _queue(queue),
      _client(std::move(client)),
      _idle(true) {}

template <typename JobData>
ClientWorker<JobData>::~ClientWorker() {
  Thread::shutdown();
}

template <typename JobData>
bool ClientWorker<JobData>::isIdle() const noexcept {
  return _idle.load();
}

template <typename JobData>
void ClientWorker<JobData>::run() {
  while (!isStopping()) {
    std::unique_ptr<JobData> job = _queue.fetchJob();

    if (job) {
      _idle.store(false);

      Result result = _queue._processJob(*_client, *job);
      _queue._handleJobResult(std::move(job), result);

      _idle.store(true);
      _queue.notifyIdle();
    }

    _queue.waitForWork();
  }
}
