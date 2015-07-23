////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread
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

#include "DispatcherThread.h"

#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/logging.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/Job.h"
#include "Dispatcher/RequeueTask.h"
#include "Scheduler/Scheduler.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                            class DispatcherThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            thread local variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a global, but thread-local place to hold the current dispatcher
/// thread. If we are not in a dispatcher thread this is set to nullptr.
////////////////////////////////////////////////////////////////////////////////

thread_local DispatcherThread* DispatcherThread::currentDispatcherThread = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

DispatcherThread::DispatcherThread (DispatcherQueue* queue)
  : Thread("dispat"+ 
           (queue->_id == Dispatcher::STANDARD_QUEUE 
            ? std::string("_std")
            : std::string("_aql"))),
    _queue(queue) {

  allowAsynchronousCancelation();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::run () {
  currentDispatcherThread = this;

  // iterate until we are shutting down
  while (! _queue->_stopping.load(memory_order_relaxed)) {

    // drain the job queue
    {
      Job* job = nullptr;
      bool worked = false;

      while (_queue->_readyJobs.pop(job)) {
        if (job != nullptr) {
          worked = true;
          handleJob(job);
        }
      }

      // we need to check again if more work has arrived after we have
      // aquired the lock. The lockfree queue and _nrWaiting are accessed
      // using "memory_order_seq_cst", this guaranties that we do not
      // miss a signal.

      if (! worked) {
        ++_queue->_nrWaiting;

        CONDITION_LOCKER(guard, _queue->_waitLock);

        if (! _queue->_readyJobs.empty()) {
          --_queue->_nrWaiting;
          continue;
        }

        // wait at most 100ms
        _queue->_waitLock.wait(100 * 1000);
        --_queue->_nrWaiting;
      }
    }
      

    // there is a chance, that we created more threads than necessary because
    // we ignore race conditions for the statistic variables
    if (_queue->tooManyThreads()) {
      break;
    }
  }

  LOG_TRACE("dispatcher thread has finished");

  // this will delete the thread
  _queue->removeStartedThread(this);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread is doing a blocking operation
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::block () {
  _queue->blockThread();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::unblock () {
  _queue->unblockThread();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief do the real work
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::handleJob (Job* job) {

  // set running job
  LOG_DEBUG("starting to run job: %s", job->getName().c_str());

  // do the work (this might change the job type)
  Job::status_t status(Job::JOB_FAILED);

  try {
    RequestStatisticsAgentSetQueueEnd(job);

    // set current thread
    job->setDispatcherThread(this);

    // and do all the dirty work
    status = job->work();
  }
  catch (Exception const& ex) {
    try {
      job->handleError(ex);
    }
    catch (Exception const& ex) {
      LOG_WARNING("caught error while handling error: %s", ex.what());
    }
    catch (std::exception const& ex) {
      LOG_WARNING("caught error while handling error: %s", ex.what());
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }

    status = Job::status_t(Job::JOB_FAILED);
  }
  catch (std::bad_alloc const& ex) {
    try {
      Exception ex2(TRI_ERROR_OUT_OF_MEMORY, string("job failed with bad_alloc: ") + ex.what(), __FILE__, __LINE__);

      job->handleError(ex2);
      LOG_WARNING("caught exception in work(): %s", ex2.what());
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }

    status = Job::status_t(Job::JOB_FAILED);
  }
  catch (std::exception const& ex) {
    try {
      Exception ex2(TRI_ERROR_INTERNAL, string("job failed with error: ") + ex.what(), __FILE__, __LINE__);

      job->handleError(ex2);
      LOG_WARNING("caught exception in work(): %s", ex2.what());
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }

    status = Job::status_t(Job::JOB_FAILED);
  }
  catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
    if (_queue->_stopping.load(memory_order_relaxed)) {
      LOG_WARNING("caught cancellation exception during work");
      throw;
    }
#endif

    try {
      Exception ex(TRI_ERROR_INTERNAL, "job failed with unknown error", __FILE__, __LINE__);

      job->handleError(ex);
      LOG_WARNING("caught unknown exception in work()");
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }

    status = Job::status_t(Job::JOB_FAILED);
  }

  // finish jobs
  try {
    job->setDispatcherThread(nullptr);

    if (status.status == Job::JOB_DONE || status.status == Job::JOB_FAILED) {
      job->cleanup(_queue);
    }
    else if (status.status == Job::JOB_REQUEUE) {
      if (0.0 < status.sleep) {
        _queue->_scheduler->registerTask(
          new RequeueTask(_queue->_scheduler,
                          _queue->_dispatcher,
                          status.sleep,
                          job));
      }
      else {
        _queue->_dispatcher->addJob(job);
      }
    }
  }
  catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
    if (_queue->_stopping.load(memory_order_relaxed)) {
      LOG_WARNING("caught cancellation exception during cleanup");
      throw;
    }
#endif

    LOG_WARNING("caught error while cleaning up!");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
