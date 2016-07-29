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

#include "DispatcherQueue.h"

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Dispatcher/DispatcherThread.h"
#include "Dispatcher/Job.h"
#include "Logger/Logger.h"

using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new dispatcher queue
////////////////////////////////////////////////////////////////////////////////

DispatcherQueue::DispatcherQueue(Scheduler* scheduler, Dispatcher* dispatcher,
                                 size_t id,
                                 Dispatcher::newDispatcherThread_fptr creator,
                                 size_t nrThreads, size_t nrExtra,
                                 size_t maxSize)
    : _id(id),
      _nrThreads(nrThreads),
      _nrExtra(nrExtra),
      _maxSize(maxSize),
      _waitLock(),
      _readyJobs(maxSize),
      _numberJobs(0),
      _hazardLock(),
      _hazardPointer(nullptr),
      _stopping(false),
      _threadsLock(),
      _startedThreads(),
      _stoppedThreads(0),
      _nrRunning(0),
      _nrWaiting(0),
      _nrBlocked(0),
      _lastChanged(0.0),
      _gracePeriod(5.0),
      _scheduler(scheduler),
      _dispatcher(dispatcher),
      createDispatcherThread(creator),
      _affinityCores(),
      _affinityPos(0),
      _jobs(),
      _jobPositions(_maxSize) {
  // keep a list of all jobs
  _jobs = new std::atomic<Job*>[ maxSize ];

  // and a list of positions into this array
  for (size_t i = 0; i < maxSize; ++i) {
    _jobPositions.push(i);
    _jobs[i] = nullptr;
  }
}

DispatcherQueue::~DispatcherQueue() {
  beginShutdown();
  delete[] _jobs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a job
////////////////////////////////////////////////////////////////////////////////

int DispatcherQueue::addJob(std::unique_ptr<Job>& job, bool startThread) {
  TRI_ASSERT(job.get() != nullptr);

  // get next free slot, return false is queue is full
  size_t pos;

  if (!_jobPositions.pop(pos)) {
    LOG(TRACE) << "cannot add job " << (void*)job.get() << " to queue "
               << (void*)this << ". queue is full";
    return TRI_ERROR_QUEUE_FULL;
  }

  Job* raw = job.release();
  _jobs[pos] = raw;

  // set the position inside the job
  raw->setQueuePosition(pos);

  // add the job to the list of ready jobs
  bool ok;
  try {
    ok = _readyJobs.push(raw);
  } catch (...) {
    ok = false;
  }

  if (ok) {
    ++_numberJobs;
  } else {
    LOG(WARN) << "cannot insert job into ready queue, giving up";

    removeJob(raw);
    delete raw;

    return TRI_ERROR_QUEUE_FULL;
  }

  // wake up the _dispatcher queue threads - only if someone is waiting
  if (0 < _nrWaiting) {
    _waitLock.signal();
  }

  // if all threads are blocked, start a new one - we ignore race conditions
  else if (notEnoughThreads()) {
    startQueueThread(startThread);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a job
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::removeJob(Job* job) {
  size_t pos = job->queuePosition();

  // clear the entry
  _jobs[pos] = nullptr;

  // wait until we are done with the job in another thread
  while (_hazardPointer == job) {
    usleep(1000);
  }

  // reuse the position
  _jobPositions.push(pos);

  // the caller will now delete the job
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancels a job
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::cancelJob(uint64_t jobId) {
  if (jobId == 0) {
    return false;
  }

  // and wait until we get set the hazard pointer
  MUTEX_LOCKER(mutexLocker, _hazardLock);

  // first find the job
  Job* job = nullptr;

  for (size_t i = 0; i < _maxSize; ++i) {
    while (true) {
      job = _jobs[i];

      if (job == nullptr) {
        break;
      }

      _hazardPointer = job;
      Job* job2 = _jobs[i];

      if (job == job2) {
        break;
      }
    }

    if (job != nullptr && job->jobId() == jobId) {
      // cancel the job
      try {
        job->cancel();
      } catch (...) {
        job = nullptr;
      }

      _hazardPointer = nullptr;
      return true;
    }
  }

  _hazardPointer = nullptr;
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread is doing a blocking operation
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::blockThread() { ++_nrBlocked; }

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::unblockThread() {
  if (--_nrBlocked < 0) {
    LOG(ERR) << "internal error, unblocking too many threads";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins the shutdown sequence the queue
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::beginShutdown() {
  if (_stopping) {
    return;
  }

  LOG(DEBUG) << "beginning shutdown sequence of dispatcher queue '" << _id
             << "'";

  // broadcast the we want to stop
  size_t const MAX_TRIES = 10;

  _stopping = true;

  // kill all jobs in the queue
  {
    Job* job = nullptr;

    while (_readyJobs.pop(job)) {
      if (job != nullptr) {
        --_numberJobs;

        removeJob(job);
        delete job;
      }
    }
  }

  // now try to get rid of the remaining (running) jobs
  MUTEX_LOCKER(mutexLocker, _hazardLock);

  for (size_t i = 0; i < _maxSize; ++i) {
    Job* job = nullptr;

    while (true) {
      job = _jobs[i];

      if (job == nullptr) {
        break;
      }

      _hazardPointer = job;
      Job* job2 = _jobs[i];

      if (job == job2) {
        break;
      }
    }

    if (job != nullptr) {
      try {
        job->cancel();
      } catch (...) {
      }

      _hazardPointer = nullptr;

      removeJob(job);
    }
  }

  _hazardPointer = nullptr;

  // wait for threads to shutdown
  for (size_t count = 0; count < MAX_TRIES; ++count) {
    LOG(TRACE) << "shutdown sequence dispatcher queue '" << _id
               << "', status: " << _nrRunning.load() << " running threads, "
               << _nrWaiting.load() << " waiting threads";

    if (0 == _nrRunning + _nrWaiting) {
      break;
    }

    {
      CONDITION_LOCKER(guard, _waitLock);
      guard.broadcast();
    }

    usleep(10 * 1000);
  }

  LOG(DEBUG) << "shutdown sequence dispatcher queue '" << _id
             << "', status: " << _nrRunning.load() << " running threads, "
             << _nrWaiting.load() << " waiting threads";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the queue
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::shutdown() {
  LOG(DEBUG) << "shutting down the dispatcher queue '" << _id << "'";

  // try to stop threads forcefully
  {
    MUTEX_LOCKER(mutexLocker, _threadsLock);

    for (auto& it : _startedThreads) {
      it->beginShutdown();
    }
  }

  // wait a little while
  usleep(100 * 1000);

  // delete all old threads
  deleteOldThreads();

  // and butcher the remaining threads
  std::set<DispatcherThread*> allStartedThreads;
  {
    MUTEX_LOCKER(mutexLocker, _threadsLock);
    allStartedThreads = _startedThreads;
  }

  // delete threads without holding the mutex
  for (auto& it : allStartedThreads) {
    delete it;
  }

  // and delete old jobs
  for (size_t i = 0; i < _maxSize; ++i) {
    Job* job = _jobs[i];

    if (job != nullptr) {
      delete job;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a new queue thread
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::startQueueThread(bool force) {
  DispatcherThread* thread = (*createDispatcherThread)(this);

  if (!_affinityCores.empty()) {
    size_t c = _affinityCores[_affinityPos];

    LOG_TOPIC(DEBUG, Logger::THREADS) << "using core " << c
                                      << " for standard dispatcher thread";
    thread->setProcessorAffinity(c);

    ++_affinityPos;

    if (_affinityPos >= _affinityCores.size()) {
      _affinityPos = 0;
    }
  }

  {
    MUTEX_LOCKER(mutexLocker, _threadsLock);

    if (!force && !notEnoughThreads()) {
      delete thread;
      return;
    }

    try {
      _startedThreads.insert(thread);
    } catch (...) {
      delete thread;
      return;
    }

    ++_nrRunning;
  }

  bool ok = thread->start();

  if (!ok) {
    LOG(FATAL) << "cannot start dispatcher thread";
    FATAL_ERROR_EXIT();
  } else {
    _lastChanged = TRI_microtime();
  }

  deleteOldThreads();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called when a thread has stopped
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::removeStartedThread(DispatcherThread* thread) {
  {
    MUTEX_LOCKER(mutexLocker, _threadsLock);
    _startedThreads.erase(thread);
  }

  --_nrRunning;

  _stoppedThreads.push(thread);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we have too many threads
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::tooManyThreads() {
  size_t nrRunning = _nrRunning.load(std::memory_order_relaxed);
  size_t nrBlocked = (size_t)_nrBlocked.load(std::memory_order_relaxed);

  if ((_nrThreads + nrBlocked) < nrRunning) {
    double now = TRI_microtime();
    double lastChanged = _lastChanged.load(std::memory_order_relaxed);

    if (lastChanged + _gracePeriod < now) {
      _lastChanged.store(now, std::memory_order_relaxed);
      return true;
    }
    // fall-through
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we have enough threads
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::notEnoughThreads() {
  size_t nrRunning = _nrRunning.load(std::memory_order_relaxed);
  size_t nrBlocked = (size_t)_nrBlocked.load(std::memory_order_relaxed);

  if (nrRunning + nrBlocked >= _nrThreads + _nrExtra) {
    // we have reached the absolute maximum capacity
    return false;
  }

  return nrRunning <= (_nrThreads + _nrExtra - 1) || nrRunning <= nrBlocked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::setProcessorAffinity(std::vector<size_t> const& cores) {
  _affinityCores = cores;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes old threads
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::deleteOldThreads() {
  DispatcherThread* thread;

  while (_stoppedThreads.pop(thread)) {
    if (thread != nullptr) {
      delete thread;
    }
  }
}
