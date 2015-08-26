////////////////////////////////////////////////////////////////////////////////
/// @brief job dispatcher
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

#include "Dispatcher.h"

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/logging.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/DispatcherThread.h"
#include "Dispatcher/Job.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the default dispatcher thread
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* CreateDispatcherThread (DispatcherQueue* queue) {
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
    _stopping(false) {
  _queues.resize(SYSTEM_QUEUE_SIZE, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Dispatcher::~Dispatcher () {
  for (size_t i = 0;  i < SYSTEM_QUEUE_SIZE;  ++i) {
    delete _queues[i];
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the standard queue
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::addStandardQueue (size_t nrThreads, size_t maxSize) {
  TRI_ASSERT(_queues[STANDARD_QUEUE] == nullptr);

  _queues[STANDARD_QUEUE] = new DispatcherQueue(
    _scheduler,
    this,
    STANDARD_QUEUE,
    CreateDispatcherThread,
    nrThreads,
    maxSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the AQL queue (used for the cluster)
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::addAQLQueue (size_t nrThreads,
                             size_t maxSize) {
  TRI_ASSERT(_queues[AQL_QUEUE] == nullptr);

  _queues[AQL_QUEUE] = new DispatcherQueue(
    _scheduler,
    this,
    AQL_QUEUE,
    CreateDispatcherThread,
    nrThreads,
    maxSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a new named queue
///
/// This is not thread safe. Only used during initialisation.
////////////////////////////////////////////////////////////////////////////////

int Dispatcher::addExtraQueue (size_t identifier,
			       size_t nrThreads,
			       size_t maxSize) {
  if (identifier == 0) {
    return TRI_ERROR_QUEUE_ALREADY_EXISTS;
  }

  size_t n = identifier + (SYSTEM_QUEUE_SIZE - 1);

  if (_queues.size() <= n) {
    _queues.resize(n + 1, nullptr);
  }

  if (_queues[n] != nullptr) {
    return TRI_ERROR_QUEUE_ALREADY_EXISTS;
  }

  if (_stopping != 0) {
    return TRI_ERROR_DISPATCHER_IS_STOPPING;
  }

  _queues[n] = new DispatcherQueue(
    _scheduler,
    this,
    n,
    CreateDispatcherThread,
    nrThreads,
    maxSize);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new job
////////////////////////////////////////////////////////////////////////////////

int Dispatcher::addJob (Job* job) {
  RequestStatisticsAgentSetQueueStart(job);

  // do not start new jobs if we are already shutting down
  if (_stopping.load(memory_order_relaxed)) {
    return TRI_ERROR_DISPATCHER_IS_STOPPING;
  }

  // try to find a suitable queue
  size_t qnr = job->queue();
  DispatcherQueue* queue;

  if (qnr >= _queues.size() || (queue = _queues[qnr]) == nullptr) {
    LOG_WARNING("unknown queue '%lu'", (unsigned long) qnr);
    return TRI_ERROR_QUEUE_UNKNOWN;
  }

  // log success, but do this BEFORE the real add, because the addJob might execute
  // and delete the job before we have a chance to log something
  LOG_TRACE("added job %p to queue '%lu'", (void*) job, (unsigned long) qnr);

  // add the job to the list of ready jobs
  return queue->addJob(job);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel a job
////////////////////////////////////////////////////////////////////////////////

bool Dispatcher::cancelJob (uint64_t jobId) {
  bool done = false;

  for (size_t i = 0;  ! done && i < _queues.size();  ++i) {
    DispatcherQueue* queue = _queues[i];

    if (queue != nullptr) {
      done = queue->cancelJob(jobId);
    }
  }

  return done;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown process
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::beginShutdown () {
  if (_stopping) {
    return;
  }

  LOG_DEBUG("beginning shutdown sequence of dispatcher");

  _stopping = true;

  for (size_t i = 0;  i < _queues.size();  ++i) {
    DispatcherQueue* queue = _queues[i];

    if (queue != nullptr) {
      queue->beginShutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the queue
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::shutdown () {
  LOG_DEBUG("shutting down the dispatcher");

  for (size_t i = 0;  i < _queues.size();  ++i) {
    DispatcherQueue* queue = _queues[i];

    if (queue != nullptr) {
      queue->shutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reports status of dispatcher queues
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::reportStatus () {
#ifdef TRI_ENABLE_LOGGER

  for (size_t i = 0;  i < _queues.size();  ++i) {
    DispatcherQueue* queue = _queues[i];

    if (queue != nullptr) {
      LOG_INFO("dispatcher queue '%lu': initial = %d, running = %d, waiting = %d, blocked = %d",
               (unsigned long) i,
               (int) queue->_nrThreads,
               (int) queue->_nrRunning,
               (int) queue->_nrWaiting,
               (int) queue->_nrBlocked);
    }
  }

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void Dispatcher::setProcessorAffinity (size_t id, std::vector<size_t> const& cores) {
  DispatcherQueue* queue;

  if (id >= _queues.size() || (queue = _queues[id]) == nullptr) {
    return;
  }

  queue->setProcessorAffinity(cores);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
