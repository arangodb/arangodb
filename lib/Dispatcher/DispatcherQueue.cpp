////////////////////////////////////////////////////////////////////////////////
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

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "DispatcherQueue.h"

#include "Basics/ConditionLocker.h"
#include "Basics/logging.h"
#include "Dispatcher/DispatcherThread.h"

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
                                  std::string const& name,
                                  Dispatcher::newDispatcherThread_fptr creator,
                                  void* threadData,
                                  size_t nrThreads,
                                  size_t maxSize)
  : _name(name),
    _threadData(threadData),
    _accessQueue(),
    _readyJobs(),
    _runningJobs(),
    _maxSize(maxSize),
    _stopping(0),
    _monopolizer(0),
    _startedThreads(),
    _stoppedThreads(),
    _nrStarted(0),
    _nrUp(0),
    _nrRunning(0),
    _nrWaiting(0),
    _nrStopped(0),
    _nrSpecial(0),
    _nrBlocked(0),
    _nrThreads(nrThreads),
    _scheduler(scheduler),
    _dispatcher(dispatcher),
    createDispatcherThread(creator) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

DispatcherQueue::~DispatcherQueue () {
  if (_stopping == 0) {
    beginShutdown();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a job
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::addJob (Job* job) {
  TRI_ASSERT(job != 0);

  CONDITION_LOCKER(guard, _accessQueue);

  // queue is full
  if (_readyJobs.size() >= _maxSize) {
    return false;
  }

  // if all threads are block, we start new threads
  if (0 == _nrWaiting && _nrRunning + _nrStarted <= _nrBlocked) {
    LOG_ERROR("GOING TO STARTED AN ADDITIONAL THREAD!");
    startQueueThread();
  }

  // add the job to the list of ready jobs
  _readyJobs.push_back(job);

  // wake up the _dispatcher queue threads
  if (0 < _nrWaiting) {
    guard.broadcast();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel a job
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::cancelJob (uint64_t jobId) {
  CONDITION_LOCKER(guard, _accessQueue);

  if (jobId == 0) {
    return false;
  }

  // job is already running, try to cancel it
  for (set<Job*>::iterator it = _runningJobs.begin();  it != _runningJobs.end();  ++it) {
    Job* job = *it;

    if (job->id() == jobId) {
      job->cancel(true);
      return true;
    }
  }

  // maybe there is a waiting job with this it, try to remove it
  for (list<Job*>::iterator it = _readyJobs.begin();  it != _readyJobs.end();  ++it) {
    Job* job = *it;

    if (job->id() == jobId) {
      bool canceled = job->cancel(false);

      if (canceled) {
        try {
          job->setDispatcherThread(0);
          job->cleanup();
        }
        catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
          if (_stopping != 0) {
            LOG_WARNING("caught cancellation exception during cleanup");
            throw;
          }
#endif

          LOG_WARNING("caught error while cleaning up!");
        }

        _readyJobs.erase(it);
      }

      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief downgrades the thread to special
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::specializeThread (DispatcherThread* thread) {
  CONDITION_LOCKER(guard, _accessQueue);

  if (thread->_jobType == Job::READ_JOB || thread->_jobType == Job::WRITE_JOB) {
    thread->_jobType = Job::SPECIAL_JOB;

    _nrRunning--;
    _nrSpecial++;

    startQueueThread();

    if (_monopolizer == thread) {
      _monopolizer = 0;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread is doing a blocking operation
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::blockThread (DispatcherThread* thread) {
  CONDITION_LOCKER(guard, _accessQueue);

  if (thread->_jobType == Job::READ_JOB || thread->_jobType == Job::WRITE_JOB) {
    LOG_ERROR("BLOCKED");
    _nrBlocked++;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::unblockThread (DispatcherThread* thread) {
  CONDITION_LOCKER(guard, _accessQueue);

  if (thread->_jobType == Job::READ_JOB || thread->_jobType == Job::WRITE_JOB) {
    if (_nrBlocked == 0) {
      LOG_ERROR("unblocking too many threads");
    }
    else {
      LOG_ERROR("UNBLOCKED");
      _nrBlocked--;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a queue
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::start () {
  CONDITION_LOCKER(guard, _accessQueue);

  for (size_t i = 0;  i < _nrThreads;  ++i) {
    bool ok = startQueueThread();

    if (! ok) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins the shutdown sequence the queue
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::beginShutdown () {
  if (_stopping != 0) {
    return;
  }

  LOG_DEBUG("beginning shutdown sequence of dispatcher queue '%s'", _name.c_str());

  // broadcast the we want to stop
  size_t const MAX_TRIES = 10;

  _stopping = 1;
  
  // kill all jobs in the queue that were not yet executed
  {
    CONDITION_LOCKER(guard, _accessQueue);
    for (auto it = _readyJobs.begin();  it != _readyJobs.end();  ++it) {
      Job* job = *it;

      bool canceled = job->cancel(false);

      if (canceled) {
        try {
          job->setDispatcherThread(0);
          job->cleanup();
        }
        catch (...) {
        }
      }
    }
    _readyJobs.clear();
  }


  for (size_t count = 0;  count < MAX_TRIES;  ++count) {
    {
      CONDITION_LOCKER(guard, _accessQueue);

      LOG_TRACE("shutdown sequence dispatcher queue '%s', status: %d running threads, %d waiting threads, %d special threads",
                _name.c_str(),
                (int) _nrRunning,
                (int) _nrWaiting,
                (int) _nrSpecial);

      if (0 == _nrRunning + _nrWaiting) {
        break;
      }

      guard.broadcast();
    }

    usleep(10000);
  }

  LOG_DEBUG("shutdown sequence dispatcher queue '%s', status: %d running threads, %d waiting threads, %d special threads",
            _name.c_str(),
            (int) _nrRunning,
            (int) _nrWaiting,
            (int) _nrSpecial);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the queue
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::shutdown () {
  LOG_DEBUG("shutting down the dispatcher queue '%s'", _name.c_str());

  // try to stop threads forcefully
  set<DispatcherThread*> threads;

  {
    CONDITION_LOCKER(guard, _accessQueue);

    threads.insert(_startedThreads.begin(), _startedThreads.end());
    threads.insert(_stoppedThreads.begin(), _stoppedThreads.end());
  }

  for (set<DispatcherThread*>::iterator i = threads.begin();  i != threads.end();  ++i) {
    (*i)->stop();
  }

  usleep(10000);

  for (set<DispatcherThread*>::iterator i = threads.begin();  i != threads.end();  ++i) {
    delete *i;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the dispatcher thread is up and running
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::isStarted () {
  CONDITION_LOCKER(guard, _accessQueue);

  return (_nrStarted + _nrRunning + _nrSpecial) <= _nrUp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief is the queue still active
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::isRunning () {
  CONDITION_LOCKER(guard, _accessQueue);

  return 0 < (_nrStarted + _nrRunning + _nrSpecial);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a new queue thread
////////////////////////////////////////////////////////////////////////////////

bool DispatcherQueue::startQueueThread () {
  DispatcherThread * thread = (*createDispatcherThread)(this, _threadData);
  bool ok = thread->start();

  if (! ok) {
    LOG_FATAL_AND_EXIT("cannot start dispatcher thread");
  }
  else {
    _nrStarted++;
  }

  return ok;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
