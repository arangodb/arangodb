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

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
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
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

DispatcherThread::DispatcherThread (DispatcherQueue* queue)
  : Thread("dispatcher"),
    _queue(queue),
    _jobType(Job::READ_JOB) {
  allowAsynchronousCancelation();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::run () {
  _queue->_accessQueue.lock();

  _queue->_nrStarted--;
  _queue->_nrRunning++;
  _queue->_nrUp++;

  _queue->_startedThreads.insert(this);

  // iterate until we are shutting down.
  while (_jobType != Job::SPECIAL_JOB && _queue->_stopping == 0) {

    // delete old jobs
    for (list<DispatcherThread*>::iterator i = _queue->_stoppedThreads.begin();  i != _queue->_stoppedThreads.end();  ++i) {
      delete *i;
    }

    _queue->_stoppedThreads.clear();
    _queue->_nrStopped = 0;

    // a job is waiting to execute
    if (  ! _queue->_readyJobs.empty()
         && _queue->_monopolizer == 0
         && ! (_queue->_readyJobs.front()->type() == Job::WRITE_JOB && 1 < _queue->_nrRunning)) {

      // try next job
      Job* job = _queue->_readyJobs.front();
      _queue->_readyJobs.pop_front();

      // handle job type
      _jobType = job->type();

      // start a new thread for special jobs
      if (_jobType == Job::SPECIAL_JOB) {
        _queue->_nrRunning--;
        _queue->_nrSpecial++;
        _queue->startQueueThread();
      }

      // monopolize queue
      else if (_jobType == Job::WRITE_JOB) {
        _queue->_monopolizer = this;
      }

      // set running job
      _queue->_runningJobs.insert(job);

      // now release the queue lock (initialise is inside the lock, work outside)
      _queue->_accessQueue.unlock();

      // do the work (this might change the job type)
      Job::status_t status(Job::JOB_FAILED);

      try {
        RequestStatisticsAgentSetQueueEnd(job);

        // set current thread
        job->setDispatcherThread(this);

        // and do all the dirty work
        status = job->work();
      }
      catch (TriagensError const& ex) {
        try {
          job->handleError(ex);
        }
        catch (TriagensError const& ex) {
          LOG_WARNING("caught error while handling error: %s", DIAGNOSTIC_INFORMATION(ex));
        }
        catch (std::exception const& ex) {
          LOG_WARNING("caught error while handling error: %s", ex.what());
        }
        catch (...) {
          LOG_WARNING("caught error while handling error!");
        }

        status = Job::status_t(Job::JOB_FAILED);
      }
      catch (std::exception const& ex) {
        try {
          InternalError err(string("job failed with unknown in work: ") + ex.what(), __FILE__, __LINE__);

          job->handleError(err);
        }
        catch (TriagensError const& ex) {
          LOG_WARNING("caught error while handling error: %s", DIAGNOSTIC_INFORMATION(ex));
        }
        catch (std::exception const& ex) {
          LOG_WARNING("caught error while handling error: %s", ex.what());
        }
        catch (...) {
          LOG_WARNING("caught error while handling error!");
        }

        status = Job::status_t(Job::JOB_FAILED);
      }
      catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
        if (_queue->_stopping != 0) {
          LOG_WARNING("caught cancellation exception during work");
          throw;
        }
#endif

        try {
          InternalError err("job failed with unknown error in work", __FILE__, __LINE__);

          job->handleError(err);
        }
        catch (TriagensError const& ex) {
          LOG_WARNING("caught error while handling error: %s", DIAGNOSTIC_INFORMATION(ex));
        }
        catch (std::exception const& ex) {
          LOG_WARNING("caught error while handling error: %s", ex.what());
        }
        catch (...) {
          LOG_WARNING("caught error while handling error!");
        }

        status = Job::status_t(Job::JOB_FAILED);
      }

      // clear running job
      _queue->_accessQueue.lock();
      _queue->_runningJobs.erase(job);
      _queue->_accessQueue.unlock();

      // trigger GC
      tick(false);

      // detached jobs (status == JOB::DETACH) might be killed asynchronously by other means
      // it is not safe to use detached jobs after job->work()

      if (status.status == Job::JOB_DETACH) {
        // we must do absolutely nothing with dispatched jobs here because they might be
        // killed asynchronously and this is not under our control
      }

      // normal jobs
      else {

        // finish jobs
        try {
          job->setDispatcherThread(0);

          if (status.status == Job::JOB_DONE) {
            job->cleanup();
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
          else if (status.status == Job::JOB_FAILED) {
            job->cleanup();
          }
        }
        catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
          if (_queue->_stopping != 0) {
            LOG_WARNING("caught cancellation exception during cleanup");
            throw;
          }
#endif

          LOG_WARNING("caught error while cleaning up!");
        }
      }

      // require the lock
      _queue->_accessQueue.lock();

      // cleanup
      _queue->_monopolizer = 0;

      if (0 < _queue->_nrWaiting && ! _queue->_readyJobs.empty()) {
        _queue->_accessQueue.broadcast();
      }
    }
    else {

      // cleanup without holding a lock
      _queue->_accessQueue.unlock();
      tick(true);
      _queue->_accessQueue.lock();

      // wait, if there are no jobs
      if (_queue->_readyJobs.empty()) {
        _queue->_nrRunning--;
        _queue->_nrWaiting++;

        _queue->_accessQueue.wait();

        _queue->_nrWaiting--;
        _queue->_nrRunning++;
      }
    }
  }

  _queue->_stoppedThreads.push_back(this);
  _queue->_startedThreads.erase(this);

  _queue->_nrRunning--;
  _queue->_nrStopped++;

  if (_jobType == Job::SPECIAL_JOB) {
    _queue->_nrSpecial--;
  }

  _queue->_nrUp--;

  _queue->_accessQueue.unlock();

  LOG_TRACE("dispatcher thread has finished");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread is doing a blocking operation
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::blockThread () {
  _queue->blockThread(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::unblockThread () {
  _queue->unblockThread(this);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief called to report the status of the thread
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::reportStatus () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called in regular intervals
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::tick (bool) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
