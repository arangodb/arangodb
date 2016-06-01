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

#include "DispatcherThread.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/Job.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

DispatcherThread::DispatcherThread(DispatcherQueue* queue)
    : Thread("Dispatcher" + (queue->_id == Dispatcher::STANDARD_QUEUE
                                 ? std::string("Std")
                                 : (queue->_id == Dispatcher::AQL_QUEUE
                                        ? std::string("Aql")
                                        : ("_" + std::to_string(queue->_id))))),
      _queue(queue) {
}

void DispatcherThread::run() {
  int idleTries = 0;

  // iterate until we are shutting down
  while (!_queue->_stopping.load(std::memory_order_relaxed)) {
    // drain the job queue
    {
      ++idleTries;
      Job* job = nullptr;

      while (_queue->_readyJobs.pop(job)) {
        if (job != nullptr) {
          --_queue->_numberJobs;

          handleJob(job);
          idleTries = 0;
        }
      }

      // we need to check again if more work has arrived after we have
      // aquired the lock. The lockfree queue and _nrWaiting are accessed
      // using "memory_order_seq_cst", this guarantees that we do not
      // miss a signal.

      if (idleTries >= 2) {
        ++_queue->_nrWaiting;

        // wait at most 1000ms
        uintptr_t n = (uintptr_t) this;
        uint64_t waitTime = (1 + ((n >> 3) % 9)) * 100 * 1000;

        CONDITION_LOCKER(guard, _queue->_waitLock);
        
        // perform the waiting
        bool gotSignal = _queue->_waitLock.wait(waitTime);

        --_queue->_nrWaiting;

        // there is a chance that we created more threads than necessary
        // because we ignore race conditions for the statistic variables
        if (!gotSignal && _queue->tooManyThreads()) {
          break;
        }
      }
    }
  }

  LOG(TRACE) << "dispatcher thread has finished";

  // this will delete the thread
  _queue->removeStartedThread(this);
}

void DispatcherThread::addStatus(VPackBuilder* b) {
  Thread::addStatus(b);
  b->add("queue", VPackValue(_queue->_id));
  b->add("stopping", VPackValue(_queue->_stopping.load()));
  b->add("waitingJobs", VPackValue(_queue->_numberJobs.load()));
  b->add("numberRunning", VPackValue((int)_queue->_nrRunning.load()));
  b->add("numberWaiting", VPackValue((int)_queue->_nrWaiting.load()));
  b->add("numberBlocked", VPackValue((int)_queue->_nrBlocked.load()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread is doing a blocking operation
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::block() { _queue->blockThread(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::unblock() { _queue->unblockThread(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief do the real work
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::handleJob(Job* job) {
  LOG(DEBUG) << "starting to run job: " << job->getName();

  // start all the dirty work
  try {
    job->requestStatisticsAgentSetQueueEnd();
    job->work();
  } catch (Exception const& ex) {
    try {
      job->handleError(ex);
    } catch (Exception const& ex) {
      LOG(WARN) << "caught error while handling error: " << ex.what();
    } catch (std::exception const& ex) {
      LOG(WARN) << "caught error while handling error: " << ex.what();
    } catch (...) {
      LOG(WARN) << "caught unknown error while handling error!";
    }
  } catch (std::bad_alloc const& ex) {
    try {
      Exception ex2(TRI_ERROR_OUT_OF_MEMORY,
                    std::string("job failed with bad_alloc: ") + ex.what(),
                    __FILE__, __LINE__);

      job->handleError(ex2);
      LOG(WARN) << "caught exception in work(): " << ex2.what();
    } catch (...) {
      LOG(WARN) << "caught unknown error while handling error!";
    }
  } catch (std::exception const& ex) {
    try {
      Exception ex2(TRI_ERROR_INTERNAL,
                    std::string("job failed with error: ") + ex.what(),
                    __FILE__, __LINE__);

      job->handleError(ex2);
      LOG(WARN) << "caught exception in work(): " << ex2.what();
    } catch (...) {
      LOG(WARN) << "caught unknown error while handling error!";
    }
  } catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
    if (_queue->_stopping.load(std::memory_order_relaxed)) {
      LOG(WARN) << "caught cancelation exception during work";
      throw;
    }
#endif

    try {
      Exception ex(TRI_ERROR_INTERNAL, "job failed with unknown error",
                   __FILE__, __LINE__);

      job->handleError(ex);
      LOG(WARN) << "caught unknown exception in work()";
    } catch (...) {
      LOG(WARN) << "caught unknown error while handling error!";
    }
  }

  // finish jobs
  try {
    job->cleanup(_queue);
  } catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
    if (_queue->_stopping.load(std::memory_order_relaxed)) {
      LOG(WARN) << "caught cancelation exception during cleanup";
      throw;
    }
#endif

    LOG(WARN) << "caught error while cleaning up!";
  }
}
