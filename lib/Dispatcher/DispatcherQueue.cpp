////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher queue
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "DispatcherQueue.h"

#include "Basics/ConditionLocker.h"
#include "Dispatcher/DispatcherThread.h"
#include "Logger/Logger.h"

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new dispatcher queue
////////////////////////////////////////////////////////////////////////////////

DispatcherQueue::DispatcherQueue (Dispatcher* dispatcher,
                                  std::string const& name,
                                  Dispatcher::newDispatcherThread_fptr creator,
                                  size_t nrThreads)
  : _name(name),
    _accessQueue(),
    _readyJobs(),
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
    _nrThreads(nrThreads),
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup _Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a job
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::addJob (Job* job) {
  CONDITION_LOCKER(guard, _accessQueue);

  // add the job to the list of ready jobs
  _readyJobs.push_back(job);

  // wake up the _dispatcher queue threads
  if (0 < _nrWaiting) {
    guard.broadcast();
  }
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

  LOGGER_DEBUG("beginning shutdown sequence of dispatcher queue '" << _name <<"'");

  // broadcast the we want to stop
  size_t const MAX_TRIES = 10;

  _stopping = 1;

  for (size_t count = 0;  count < MAX_TRIES;  ++count) {
    {
      CONDITION_LOCKER(guard, _accessQueue);

      LOGGER_TRACE("shutdown sequence dispatcher queue '" << _name << "', status: "
                   << _nrRunning << " running threads, "
                   << _nrWaiting << " waiting threads, "
                   << _nrSpecial << " special threads");

      if (0 == _nrRunning + _nrWaiting) {
        break;
      }

      guard.broadcast();
    }

    usleep(10000);
  }

  LOGGER_DEBUG("shutdown sequence dispatcher queue '" << _name << "', status: "
               << _nrRunning << " running threads, "
               << _nrWaiting << " waiting threads, "
               << _nrSpecial << " special threads");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the queue
////////////////////////////////////////////////////////////////////////////////

void DispatcherQueue::shutdown () {
  LOGGER_DEBUG("shutting down the dispatcher queue '" << _name << "'");

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
  DispatcherThread * thread = (*createDispatcherThread)(this);
  bool ok = thread->start();

  if (! ok) {
    LOGGER_FATAL_AND_EXIT("cannot start dispatcher thread");
  }
  else {
    _nrStarted++;
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
