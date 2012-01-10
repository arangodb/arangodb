////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "DispatcherThread.h"

#include <Basics/Exceptions.h>
#include <Logger/Logger.h>
#include <Basics/StringUtils.h>

#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/Job.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructs and destructors
    // -----------------------------------------------------------------------------

    DispatcherThread::DispatcherThread (DispatcherQueue* queue)
      : Thread("DispatcherThread"),
        queue(queue),
        jobType(Job::READ_JOB) {
      allowAsynchronousCancelation();
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    void DispatcherThread::reportStatus () {
    }



    void DispatcherThread::tick () {
    }

    // -----------------------------------------------------------------------------
    // Thread methods
    // -----------------------------------------------------------------------------

    void DispatcherThread::run () {
      queue->accessQueue.lock();

      queue->nrStarted--;
      queue->nrRunning++;

      queue->startedThreads.insert(this);

      // iterate until we are shutting down.
      while (jobType != Job::SPECIAL_JOB && queue->stopping == 0) {

        // delete old jobs
        for (list<DispatcherThread*>::iterator i = queue->stoppedThreads.begin();  i != queue->stoppedThreads.end();  ++i) {
          delete *i;
        }

        queue->stoppedThreads.clear();
        queue->nrStopped = 0;

        // a job is waiting to execute
        if (  ! queue->readyJobs.empty()
           && queue->monopolizer == 0
           && ! (queue->readyJobs.front()->type() == Job::WRITE_JOB && 1 < queue->nrRunning)) {

          // try next job
          Job* job = queue->readyJobs.front();
          queue->readyJobs.pop_front();

          // handle job type
          jobType = job->type();

          // start a new thread for special jobs
          if (jobType == Job::SPECIAL_JOB) {
            queue->nrRunning--;
            queue->nrSpecial++;
            queue->startQueueThread();
          }

          // monopolize queue
          else if (jobType == Job::WRITE_JOB) {
            queue->monopolizer = this;
          }

          // now release the queue lock (initialise is inside the lock, work outside)
          queue->accessQueue.unlock();

          // do the work (this might change the job type)
          Job::status_e status = Job::JOB_FAILED;

          try {

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
              LOGGER_WARNING << "caught error while handling error: " << DIAGNOSTIC_INFORMATION(ex);
            }
            catch (std::exception const& ex) {
              LOGGER_WARNING << "caught error while handling error: " << ex.what();
            }
            catch (...) {
              LOGGER_WARNING << "caught error while handling error!";
            }

            status = Job::JOB_FAILED;
          }
          catch (std::exception const& ex) {
            try {
              InternalError err(string("job failed with unknown in work: ") + ex.what());

              job->handleError(err);
            }
            catch (TriagensError const& ex) {
              LOGGER_WARNING << "caught error while handling error: " << DIAGNOSTIC_INFORMATION(ex);
            }
            catch (std::exception const& ex) {
              LOGGER_WARNING << "caught error while handling error: " << ex.what();
            }
            catch (...) {
              LOGGER_WARNING << "caught error while handling error!";
            }

            status = Job::JOB_FAILED;
          }
          catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
            if (queue->stopping != 0) {
              LOGGER_WARNING << "caught cancellation exception during work";
              throw;
            }
#endif

            try {
              InternalError err("job failed with unknown error in work");

              job->handleError(err);
            }
            catch (TriagensError const& ex) {
              LOGGER_WARNING << "caught error while handling error: " << DIAGNOSTIC_INFORMATION(ex);
            }
            catch (std::exception const& ex) {
              LOGGER_WARNING << "caught error while handling error: " << ex.what();
            }
            catch (...) {
              LOGGER_WARNING << "caught error while handling error!";
            }

            status = Job::JOB_FAILED;
          }

          queue->accessQueue.lock();

          queue->monopolizer = 0;

          // finish jobs
          try {
            job->setDispatcherThread(0);

            if (status == Job::JOB_DONE) {
              job->cleanup();
            }
            else if (status == Job::JOB_REQUEUE) {
              queue->dispatcher->addJob(job);
            }
            else if (status == Job::JOB_FAILED) {
              job->cleanup();
            }
          }
          catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
            if (queue->stopping != 0) {
              LOGGER_WARNING << "caught cancellation exception during cleanup";
              throw;
            }
#endif

            LOGGER_WARNING << "caught error while cleaning up!";
          }

          if (0 < queue->nrWaiting && ! queue->readyJobs.empty()) {
            queue->accessQueue.broadcast();
          }

          tick();
        }
        else {
          queue->nrRunning--;
          queue->nrWaiting++;

          tick();

          queue->accessQueue.wait();

          tick();

          queue->nrWaiting--;
          queue->nrRunning++;
        }
      }

      queue->stoppedThreads.push_back(this);
      queue->startedThreads.erase(this);

      queue->nrRunning--;
      queue->nrStopped++;

      if (jobType == Job::SPECIAL_JOB) {
        queue->nrSpecial--;
      }

      queue->accessQueue.unlock();

      LOGGER_TRACE << "dispatcher thread has finished";
    }
  }
}
