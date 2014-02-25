////////////////////////////////////////////////////////////////////////////////
/// @brief job dispatcher
///
/// @file
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Dispatcher.h"

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "BasicsC/logging.h"

#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherThread.h"
#include "Dispatcher/Job.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the default dispatcher thread
////////////////////////////////////////////////////////////////////////////////

DispatcherThread* Dispatcher::defaultDispatcherThread (DispatcherQueue* queue) {
  return new DispatcherThread(queue);
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

Dispatcher::Dispatcher (Scheduler* scheduler)
  : _scheduler(scheduler),
    _accessDispatcher(),
    _stopping(0),
    _queues() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Dispatcher::~Dispatcher () {
  for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    delete i->second;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief is the dispatcher still running
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::isRunning () {
  MUTEX_LOCKER(_accessDispatcher);

  bool isRunning = false;

  for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    isRunning = isRunning || i->second->isRunning();
  }

  return isRunning;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new queue
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::addQueue (std::string const& name,
                           size_t nrThreads,
                           size_t maxSize) {
  _queues[name] = new DispatcherQueue(
    _scheduler,
    this,
    name,
    defaultDispatcherThread,
    nrThreads,
    maxSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a queue which given dispatcher thread type
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::addQueue (std::string const& name,
                           newDispatcherThread_fptr func,
                           size_t nrThreads,
                           size_t maxSize) {
  _queues[name] = new DispatcherQueue(
    _scheduler,
    this,
    name,
    func,
    nrThreads,
    maxSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new job
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::addJob (Job* job) {
  RequestStatisticsAgentSetQueueStart(job);

  // do not start new jobs if we are already shutting down
  if (_stopping != 0) {
    return false;
  }

  // try to find a suitable queue
  const string& name = job->queue();
  DispatcherQueue* queue = lookupQueue(name);

  if (queue == 0) {
    LOG_WARNING("unknown queue '%s'", name.c_str());
    return false;
  }

  // log success, but do this BEFORE the real add, because the addJob might execute
  // and delete the job before we have a chance to log something
  LOG_TRACE("added job %p to queue '%s'", (void*) job, name.c_str());

  // add the job to the list of ready jobs
  if (! queue->addJob(job)) {
    // queue full etc.
    return false;
  }

  // indicate success, BUT never access job after it has been added to the queue
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the dispatcher
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::start () {
  MUTEX_LOCKER(_accessDispatcher);

  for (map<std::string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    bool ok = i->second->start();

    if (! ok) {
      LOG_FATAL_AND_EXIT("cannot start dispatcher queue");
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the dispatcher queues are up and running
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::isStarted () {
  MUTEX_LOCKER(_accessDispatcher);

  for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    bool started = i->second->isStarted();

    if (! started) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the dispatcher for business
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::open () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown process
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::beginShutdown () {
  if (_stopping != 0) {
    return;
  }

  MUTEX_LOCKER(_accessDispatcher);

  if (_stopping == 0) {
    LOG_DEBUG("beginning shutdown sequence of dispatcher");

    _stopping = 1;

    for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
      i->second->beginShutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the queue
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::shutdown () {
  MUTEX_LOCKER(_accessDispatcher);

  LOG_DEBUG("shutting down the dispatcher");

  for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
    i->second->shutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reports status of dispatcher queues
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::reportStatus () {
  if (TRI_IsDebugLogging(__FILE__)) {
    MUTEX_LOCKER(_accessDispatcher);

    for (map<string, DispatcherQueue*>::iterator i = _queues.begin();  i != _queues.end();  ++i) {
      DispatcherQueue* q = i->second;
#ifdef TRI_ENABLE_LOGGER
      string const& name = i->first;

      LOG_DEBUG("dispatcher queue '%s': threads = %d, started: %d, running = %d, waiting = %d, stopped = %d, special = %d, monopolistic = %s",
                name.c_str(),
                (int) q->_nrThreads,
                (int) q->_nrStarted,
                (int) q->_nrRunning,
                (int) q->_nrWaiting,
                (int) q->_nrStopped,
                (int) q->_nrSpecial,
                (q->_monopolizer ? "yes" : "no"));
#endif
      CONDITION_LOCKER(guard, q->_accessQueue);

      for (set<DispatcherThread*>::iterator j = q->_startedThreads.begin();  j != q->_startedThreads.end(); ++j) {
        (*j)->reportStatus();
      }
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a queue
////////////////////////////////////////////////////////////////////////////////

DispatcherQueue* Dispatcher::lookupQueue (const std::string& name) {
  map<std::string, DispatcherQueue*>::const_iterator i = _queues.find(name);

  if (i == _queues.end()) {
    return 0;
  }
  else {
    return i->second;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
