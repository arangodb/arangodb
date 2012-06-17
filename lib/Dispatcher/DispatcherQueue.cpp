////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher queue
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

#include "DispatcherQueue.h"

#include <Basics/ConditionLocker.h>
#include <Logger/Logger.h>

#include "Dispatcher/DispatcherThread.h"

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    DispatcherQueue::DispatcherQueue (Dispatcher* dispatcher,
                                      string const& name,
                                      Dispatcher::newDispatcherThread_fptr creator,
                                      size_t nrThreads)
      : _name(name),
        stopping(0),
        monopolizer(0),
        nrStarted(0),
        nrRunning(0),
        nrWaiting(0),
        nrStopped(0),
        nrSpecial(0),
        nrThreads(nrThreads),
        dispatcher(dispatcher),
        createDispatcherThread(creator) {
    }



    DispatcherQueue::~DispatcherQueue () {
      if (stopping == 0) {
        beginShutdown();
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void DispatcherQueue::addJob (Job* job) {
      CONDITION_LOCKER(guard, accessQueue);

      // add the job to the list of ready jobs
      readyJobs.push_back(job);

      // wake up the dispatcher queue threads
      if (0 < nrWaiting) {
        guard.broadcast();
      }
    }



    void DispatcherQueue::beginShutdown () {
      if (stopping != 0) {
        return;
      }

      LOGGER_DEBUG << "beginning shutdown sequence of dispatcher queue '" << _name <<"'"; 

      // broadcast the we want to stop
      size_t const MAX_TRIES = 10;

      stopping = 1;

      for (size_t count = 0;  count < MAX_TRIES;  ++count) {
        {
          CONDITION_LOCKER(guard, accessQueue);

          LOGGER_TRACE << "shutting down dispatcher queue '" << _name << "', "
                       << nrRunning << " running threads, "
                       << nrWaiting << " waiting threads, "
                       << nrSpecial << " special threads";

          if (0 == nrRunning + nrWaiting) {
            break;
          }

          guard.broadcast();
        }

        usleep(10000);
      }

      LOGGER_DEBUG << "shutting down dispatcher queue '" << _name << "', "
                   << nrRunning << " running threads, "
                   << nrWaiting << " waiting threads, "
                   << nrSpecial << " special threads";

      // try to stop threads forcefully
      set<DispatcherThread*> threads;

      {
        CONDITION_LOCKER(guard, accessQueue);

        threads.insert(startedThreads.begin(), startedThreads.end());
        threads.insert(stoppedThreads.begin(), stoppedThreads.end());
      }

      for (set<DispatcherThread*>::iterator i = threads.begin();  i != threads.end();  ++i) {
        (*i)->stop();
      }

      usleep(10000);

      for (set<DispatcherThread*>::iterator i = threads.begin();  i != threads.end();  ++i) {
        delete *i;
      }
    }



    bool DispatcherQueue::startQueueThread () {
      DispatcherThread * thread = (*createDispatcherThread)(this);
      bool ok = thread->start();

      if (! ok) {
        LOGGER_FATAL << "cannot start dispatcher thread";
      }
      else {
        nrStarted++;
      }

      return ok;
    }



    void DispatcherQueue::specializeThread (DispatcherThread* thread) {
      CONDITION_LOCKER(guard, accessQueue);

      if (thread->jobType == Job::READ_JOB || thread->jobType == Job::WRITE_JOB) {
        thread->jobType = Job::SPECIAL_JOB;

        nrRunning--;
        nrSpecial++;

        startQueueThread();

        if (monopolizer == thread) {
          monopolizer = 0;
        }
      }
    }



    bool DispatcherQueue::start () {
      CONDITION_LOCKER(guard, accessQueue);

      for (size_t i = 0;  i < nrThreads;  ++i) {
        bool ok = startQueueThread();

        if (! ok) {
          return false;
        }
      }

      return true;
    }



    bool DispatcherQueue::isRunning () {
      CONDITION_LOCKER(guard, accessQueue);

      return 0 < (nrStarted + nrRunning + nrSpecial);
    }
  }
}

