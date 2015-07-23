///////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher queue
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "DispatcherQueue.h"

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/logging.h"
#include "Dispatcher/DispatcherThread.h"
#include "Dispatcher/Job.h"

using namespace std;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new dispatcher queue
////////////////////////////////////////////////////////////////////////////////

DispatcherQueue::DispatcherQueue (Scheduler* scheduler,
                                  Dispatcher* dispatcher,
                                  size_t id,
                                  Dispatcher::newDispatcherThread_fptr creator,
                                  size_t nrThreads,
                                  size_t maxSize)
  : _id(id),
    _nrThreads(nrThreads),
    _maxSize(maxSize),
    _waitLock(),
    _readyJobs(maxSize),
    _hazardLock(),
    _hazardPointer(nullptr),
    _stopping(false),
    _threadsLock(),
    _startedThreads(),
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
  _jobs = new atomic<Job*>[maxSize];

  // and a list of positions into this array
  for (size_t i = 0;  i < maxSize;  ++i) {
    _jobPositions.push(i);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

DispatcherQueue::~DispatcherQueue () {
  beginShutdown();
  delete _jobs;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a job
////////////////////////////////////////////////////////////////////////////////

int DispatcherQueue::addJob (Job* job) {
  TRI_ASSERT(job != nullptr);

  // get next free slot, return false is queue is full
  size_t pos;

  if (! _jobPositions.pop(pos)) {
    return TRI_ERROR_QUEUE_FULL;
  }
  
  _jobs[pos] = job;

  // set the position inside the job
  job->setQueuePosition(pos);

  // add the job to the list of ready jobs
  bool ok = _readyJobs.push(job);

  if (! ok) {
    LOG_WARNING("cannot insert job into ready queue, giving up");

    removeJob(job);
    delete job;

    return TRI_ERROR_QUEUE_FULL;
  }

  // wake up the _dispatcher queue threads - only if someone is waiting
  if (0 < _nrWaiting) {
    CONDITION_LOCKER(guard, _waitLock);
    guard.signal();
  }

  // if all threads are blocked, start a new one - we ignore race conditions
  else if (_nrRunning <= min(_nrThreads - 1, (size_t) _nrBlocked.load())) {
    startQueueThread();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a job
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::removeJob (Job* job) {
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
/// @brief tries to cancel a job
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::cancelJob (uint64_t jobId) {
  if (jobId == 0) {
    return false;
  }

  // and wait until we get set the hazard pointer
  MUTEX_LOCKER(_hazardLock);

  // first find the job
  Job* job = nullptr;

  for (size_t i = 0;  i < _maxSize;  ++i) {
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

    if (job != nullptr && job->id() == jobId) {

      // cancel the job
      try {
        job->cancel();
      }
      catch (...) {
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

void DispatcherQueue::blockThread () {
  ++_nrBlocked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::unblockThread () {
  if (--_nrBlocked < 0) {
    LOG_ERROR("internal error, unblocking too many threads");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins the shutdown sequence the queue
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::beginShutdown () {
  if (_stopping) {
    return;
  }

  LOG_DEBUG("beginning shutdown sequence of dispatcher queue '%lu'", (unsigned long) _id);

  // broadcast the we want to stop
  size_t const MAX_TRIES = 10;

  _stopping = true;
  
  // kill all jobs in the queue
  {
    Job* job = nullptr;
    
    while(_readyJobs.pop(job)) {
      if (job != nullptr) {
        try {
          job->cancel();
        }
        catch (...) {
        }

        removeJob(job);
        delete job;
      }
    }
  }

  // no try to get rid of the remaining jobs
  MUTEX_LOCKER(_hazardLock);

  for (size_t i = 0;  i < _maxSize;  ++i) {
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
      }
      catch (...) {
      }

      _hazardPointer = nullptr;

      removeJob(job);
      delete job;
    }
  }

  _hazardPointer = nullptr;

  // wait for threads to shutdown
  for (size_t count = 0;  count < MAX_TRIES;  ++count) {
    LOG_TRACE("shutdown sequence dispatcher queue '%lu', status: %d running threads, %d waiting threads",
              (unsigned long) _id,
              (int) _nrRunning,
              (int) _nrWaiting);

    if (0 == _nrRunning + _nrWaiting) {
      break;
    }

    {
      CONDITION_LOCKER(guard, _waitLock);
      guard.broadcast();
    }

    usleep(10 * 1000);
  }

  LOG_DEBUG("shutdown sequence dispatcher queue '%lu', status: %d running threads, %d waiting threads",
            (unsigned long) _id,
            (int) _nrRunning,
            (int) _nrWaiting);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the queue
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::shutdown () {
  LOG_DEBUG("shutting down the dispatcher queue '%lu'", (unsigned long) _id);

  // try to stop threads forcefully
  {
    MUTEX_LOCKER(_threadsLock);

    for (auto& it : _startedThreads) {
      it->stop();
    }
  }

  // wait a little while
  usleep(100 * 1000);

  // and butcher the threads
  {
    MUTEX_LOCKER(_threadsLock);

    for (auto& it : _startedThreads) {
      delete it;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a new queue thread
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::startQueueThread () {
  DispatcherThread * thread = (*createDispatcherThread)(this);

  if (! _affinityCores.empty()) {
    size_t c = _affinityCores[_affinityPos];

    LOG_DEBUG("using core %d for standard dispatcher thread", (int) c);

    thread->setProcessorAffinity(c);

    ++_affinityPos;

    if (_affinityPos >= _affinityCores.size()) {
      _affinityPos = 0;
    }
  }

  {
    MUTEX_LOCKER(_threadsLock);
    _startedThreads.insert(thread);
  }

  ++_nrRunning;
  bool ok = thread->start();

  if (! ok) {
    LOG_FATAL_AND_EXIT("cannot start dispatcher thread");
  }
  else {
    _lastChanged = TRI_microtime();
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called when a thread has stopped
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::removeStartedThread (DispatcherThread* thread) {
  {
    MUTEX_LOCKER(_threadsLock);
    _startedThreads.erase(thread);
  }

  --_nrRunning;

  delete thread;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we have enough threads
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::tooManyThreads () {
  if (_nrThreads + _nrBlocked < _nrRunning) {
    double now = TRI_microtime();

    if (_lastChanged + _gracePeriod < now) {
      _lastChanged = now;
      return true;
    }

    return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::setProcessorAffinity (const vector<size_t>& cores) {
  _affinityCores = cores;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
