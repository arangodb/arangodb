////////////////////////////////////////////////////////////////////////////////
/// @brief job dispatcher
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

#include "DispatcherImpl.h"

#include <Basics/ConditionLocker.h>
#include <Basics/Logger.h>
#include <Basics/MutexLocker.h>
#include <Basics/StringUtils.h>
#include <Rest/Job.h>

#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherThread.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    DispatcherThread* DispatcherImpl::defaultDispatcherThread (DispatcherQueue* queue) {
      return new DispatcherThread(queue);
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    DispatcherImpl::DispatcherImpl ()
      : stopping(0) {
    }



    DispatcherImpl::~DispatcherImpl () {
      for (map<string, DispatcherQueue*>::iterator i = queues.begin();  i != queues.end();  ++i) {
        delete i->second;
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void DispatcherImpl::addQueue (string const& name, size_t nrThreads) {
      queues[name] = new DispatcherQueue(this, defaultDispatcherThread, nrThreads);
    }


    void DispatcherImpl::addQueue (string const& name, newDispatcherThread_fptr func, size_t nrThreads) {
      queues[name] = new DispatcherQueue(this, func, nrThreads);
    }


    bool DispatcherImpl::addJob (Job* job) {

      // do not start new jobs if we are already shutting down
      if (stopping != 0) {
        return false;
      }

      // try to find a suitable queue
      DispatcherQueue* queue = lookupQueue(job->queue());

      if (queue == 0) {
        LOGGER_WARNING << "unknown queue '" << job->queue() << "'";
        return false;
      }

      // log success, but do this BEFORE the real add, because the addJob might execute
      // and delete the job before we have a chance to log something
      LOGGER_TRACE << "added job " << job << " to queue " << job->queue();

      // add the job to the list of ready jobs
      queue->addJob(job);

      // indicate sucess, BUT never access job after it has been added to the queue
      return true;
    }



    bool DispatcherImpl::start () {
      MUTEX_LOCKER(accessDispatcher);

      for (map<string, DispatcherQueue*>::iterator i = queues.begin();  i != queues.end();  ++i) {
        bool ok = i->second->start();

        if (! ok) {
          LOGGER_FATAL << "cannot start dispatcher queue";
          return false;
        }
      }

      return true;
    }



    bool DispatcherImpl::isRunning () {
      MUTEX_LOCKER(accessDispatcher);

      bool isRunning = false;

      for (map<string, DispatcherQueue*>::iterator i = queues.begin();  i != queues.end();  ++i) {
        isRunning = isRunning || i->second->isRunning();
      }

      return isRunning;
    }



    void DispatcherImpl::beginShutdown () {
      if (stopping != 0) {
        return;
      }

      MUTEX_LOCKER(accessDispatcher);

      if (stopping == 0) {
        LOGGER_DEBUG << "beginning shutdown sequence of dispatcher";

        stopping = 1;

        for (map<string, DispatcherQueue*>::iterator i = queues.begin();  i != queues.end();  ++i) {
          i->second->beginShutdown();
        }
      }
    }



    void DispatcherImpl::reportStatus () {
      if (TRI_IsDebugLogging()) {
        MUTEX_LOCKER(accessDispatcher);

        for (map<string, DispatcherQueue*>::iterator i = queues.begin();  i != queues.end();  ++i) {
          string const& name = i->first;
          DispatcherQueue* q = i->second;

          LOGGER_DEBUG << "dispatcher queue '" << name << "': "
                       << "threads = " << q->nrThreads << " "
                       << "started = " << q->nrStarted << " "
                       << "running = " << q->nrRunning << " "
                       << "waiting = " << q->nrWaiting << " "
                       << "stopped = " << q->nrStopped << " "
                       << "special = " << q->nrSpecial << " "
                       << "monopolistic = " << (q->monopolizer ? "yes" : "no");

          LOGGER_HEARTBEAT
          << LoggerData::Task("dispatcher status")
          << LoggerData::Extra(name)
          << LoggerData::Extra("threads")
          << LoggerData::Extra(StringUtils::itoa(q->nrThreads))
          << LoggerData::Extra("started")
          << LoggerData::Extra(StringUtils::itoa(q->nrStarted))
          << LoggerData::Extra("running")
          << LoggerData::Extra(StringUtils::itoa(q->nrRunning))
          << LoggerData::Extra("waiting")
          << LoggerData::Extra(StringUtils::itoa(q->nrWaiting))
          << LoggerData::Extra("stopped")
          << LoggerData::Extra(StringUtils::itoa(q->nrStopped))
          << LoggerData::Extra("special")
          << LoggerData::Extra(StringUtils::itoa(q->nrSpecial))
          << LoggerData::Extra("monopilizer")
          << LoggerData::Extra((q->monopolizer ? "1" : "0"))
          << "dispatcher status";

          CONDITION_LOCKER(guard, q->accessQueue);

          for (set<DispatcherThread*>::iterator j = q->startedThreads.begin();  j != q->startedThreads.end(); ++j) {
            (*j)->reportStatus();
          }
        }
      }
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    DispatcherQueue* DispatcherImpl::lookupQueue (string const& name) {
      map<string, DispatcherQueue*>::const_iterator i = queues.find(name);

      if (i == queues.end()) {
        return 0;
      }
      else {
        return i->second;
      }
    }
  }
}
