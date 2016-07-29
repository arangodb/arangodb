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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
////////////////////////////////////////////////////////////////////////////////

#include "Dispatcher.h"

#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherThread.h"
#include "Dispatcher/Job.h"
#include "Logger/Logger.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the default dispatcher thread
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* CreateDispatcherThread(DispatcherQueue* queue) {
  return new DispatcherThread(queue);
}

Dispatcher::Dispatcher(Scheduler* scheduler)
    : _scheduler(scheduler), _stopping(false) {
  _queues.resize(SYSTEM_QUEUE_SIZE, nullptr);
}

Dispatcher::~Dispatcher() {
  for (size_t i = 0; i < SYSTEM_QUEUE_SIZE; ++i) {
    delete _queues[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the standard queue
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::addStandardQueue(size_t nrThreads, size_t nrExtraThreads,
                                  size_t maxSize) {
  TRI_ASSERT(_queues[STANDARD_QUEUE] == nullptr);

  _queues[STANDARD_QUEUE] = new DispatcherQueue(
      _scheduler, this, STANDARD_QUEUE, CreateDispatcherThread, nrThreads,
      nrExtraThreads, maxSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the AQL queue (used for the cluster)
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::addAQLQueue(size_t nrThreads, size_t nrExtraThreads,
                             size_t maxSize) {
  TRI_ASSERT(_queues[AQL_QUEUE] == nullptr);

  _queues[AQL_QUEUE] =
      new DispatcherQueue(_scheduler, this, AQL_QUEUE, CreateDispatcherThread,
                          nrThreads, nrExtraThreads, maxSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new job
////////////////////////////////////////////////////////////////////////////////

int Dispatcher::addJob(std::unique_ptr<Job>& job, bool startThread) {
  job->requestStatisticsAgentSetQueueStart();

  // do not start new jobs if we are already shutting down
  if (_stopping.load(std::memory_order_relaxed)) {
    return TRI_ERROR_DISPATCHER_IS_STOPPING;
  }

  // try to find a suitable queue
  size_t qnr = job->queue();
  DispatcherQueue* queue;

  if (qnr >= _queues.size() || (queue = _queues[qnr]) == nullptr) {
    LOG(WARN) << "unknown queue '" << qnr << "'";
    return TRI_ERROR_QUEUE_UNKNOWN;
  }

  // log success, but do this BEFORE the real add, because the addJob might
  // execute
  // and delete the job before we have a chance to log something
  LOG(TRACE) << "added job " << (void*)(job.get()) << " to queue '" << qnr
             << "'";

  // add the job to the list of ready jobs
  return queue->addJob(job, startThread);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel a job
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::cancelJob(uint64_t jobId) {
  bool done = false;

  for (size_t i = 0; !done && i < _queues.size(); ++i) {
    DispatcherQueue* queue = _queues[i];

    if (queue != nullptr) {
      done = queue->cancelJob(jobId);
    }
  }

  return done;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown process
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::beginShutdown() {
  if (_stopping) {
    return;
  }

  LOG(DEBUG) << "beginning shutdown sequence of dispatcher";

  _stopping = true;

  for (auto& queue : _queues) {
    if (queue != nullptr) {
      queue->beginShutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the queue
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::shutdown() {
  LOG(DEBUG) << "shutting down the dispatcher";

  for (auto queue : _queues) {
    if (queue != nullptr) {
      queue->shutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reports status of dispatcher queues
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::reportStatus() {
  for (size_t i = 0; i < _queues.size(); ++i) {
    DispatcherQueue* queue = _queues[i];

    if (queue != nullptr) {
      LOG(INFO) << "dispatcher queue '" << i
                << "': initial = " << queue->_nrThreads
                << ", running = " << queue->_nrRunning.load()
                << ", waiting = " << queue->_nrWaiting.load()
                << ", blocked = " << queue->_nrBlocked.load();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the processor affinity
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::setProcessorAffinity(size_t id,
                                      std::vector<size_t> const& cores) {
  LOG_TOPIC(DEBUG, Logger::THREADS) << "dispatcher cores: " << cores;

  DispatcherQueue* queue;

  if (id >= _queues.size() || (queue = _queues[id]) == nullptr) {
    return;
  }

  queue->setProcessorAffinity(cores);
}
