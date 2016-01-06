////////////////////////////////////////////////////////////////////////////////
/// @brief Thread
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
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Thread.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <errno.h>
#include <signal.h>

#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/logging.h"
#include "Basics/WorkMonitor.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace triagens::basics;


////////////////////////////////////////////////////////////////////////////////
/// @brief static started with access to the private variables
////////////////////////////////////////////////////////////////////////////////

void Thread::startThread(void* arg) {
  Thread* ptr = static_cast<Thread*>(arg);

  WorkMonitor::pushThread(ptr);

  try {
    ptr->runMe();
    ptr->cleanup();
  } catch (...) {
    WorkMonitor::popThread(ptr);
    throw;
  }

  WorkMonitor::popThread(ptr);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the process id
////////////////////////////////////////////////////////////////////////////////

TRI_pid_t Thread::currentProcessId() { return TRI_CurrentProcessId(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the thread process id
////////////////////////////////////////////////////////////////////////////////

TRI_pid_t Thread::currentThreadProcessId() {
  return TRI_CurrentThreadProcessId();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the thread id
////////////////////////////////////////////////////////////////////////////////

TRI_tid_t Thread::currentThreadId() { return TRI_CurrentThreadId(); }


////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a thread
////////////////////////////////////////////////////////////////////////////////

Thread::Thread(std::string const& name)
    : _name(name),
      _asynchronousCancelation(false),
      _thread(),
      _threadId(),
      _finishedCondition(nullptr),
      _started(false),
      _running(false),
      _joined(false),
      _affinity(-1),
      _workDescription(nullptr) {
  TRI_InitThread(&_thread);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the thread
////////////////////////////////////////////////////////////////////////////////

Thread::~Thread() {
  if (_running) {
    if (!isSilent()) {
      LOG_WARNING("forcefully shutting down thread '%s'", _name.c_str());
    }

    int res = TRI_StopThread(&_thread);

    if (res != TRI_ERROR_NO_ERROR) {
      errno = res;
      TRI_set_errno(TRI_ERROR_SYS_ERROR);

      LOG_WARNING("unable to stop thread '%s': %s", _name.c_str(),
                  TRI_last_error());
    }
  }

  if (_started && !_joined) {
    int res = TRI_DetachThread(&_thread);

    // ignore threads that already died
    if (res != 0 && res != ESRCH) {
      errno = res;
      TRI_set_errno(TRI_ERROR_SYS_ERROR);

      LOG_WARNING("unable to detach thread '%s': %s", _name.c_str(),
                  TRI_last_error());
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the thread is chatty on shutdown
////////////////////////////////////////////////////////////////////////////////

bool Thread::isSilent() { return false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief name of a thread
////////////////////////////////////////////////////////////////////////////////

const std::string& Thread::name() const { return _name; }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for running
////////////////////////////////////////////////////////////////////////////////

bool Thread::isRunning() { return _running; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a thread identifier
////////////////////////////////////////////////////////////////////////////////

TRI_tid_t Thread::threadId() { return _threadId; }

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the thread
////////////////////////////////////////////////////////////////////////////////

bool Thread::start(ConditionVariable* finishedCondition) {
  _finishedCondition = finishedCondition;

  if (_started) {
    LOG_FATAL_AND_EXIT("called started on an already started thread");
  }

  _started = true;

  std::string text = "[" + _name + "]";
  bool ok =
      TRI_StartThread(&_thread, &_threadId, text.c_str(), &startThread, this);

  if (!ok) {
    LOG_ERROR("could not start thread '%s': %s", _name.c_str(),
              strerror(errno));
  }

  if (0 <= _affinity) {
    TRI_SetProcessorAffinity(&_thread, (size_t)_affinity);
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the thread
////////////////////////////////////////////////////////////////////////////////

int Thread::stop() {
  if (_running) {
    LOG_TRACE("trying to cancel (aka stop) the thread '%s'", _name.c_str());
    return TRI_StopThread(&_thread);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief joins the thread
////////////////////////////////////////////////////////////////////////////////

int Thread::join() {
  int res = TRI_ERROR_NO_ERROR;

  if (!_joined) {
    res = TRI_JoinThread(&_thread);

    if (res == TRI_ERROR_NO_ERROR) {
      _joined = true;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops and joins the thread
////////////////////////////////////////////////////////////////////////////////

int Thread::shutdown() {
  size_t const MAX_TRIES = 10;
  size_t const WAIT = 10000;

  for (size_t i = 0; i < MAX_TRIES; ++i) {
    if (!_running) {
      break;
    }

    usleep(WAIT);
  }

  if (_running) {
    int res = TRI_StopThread(&_thread);

    if (res != TRI_ERROR_NO_ERROR) {
      errno = res;
      TRI_set_errno(TRI_ERROR_SYS_ERROR);

      LOG_WARNING("unable to stop thread '%s': %s", _name.c_str(),
                  TRI_last_error());
    }
  }

  return this->join();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void Thread::setProcessorAffinity(size_t c) { _affinity = (int)c; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current work description
////////////////////////////////////////////////////////////////////////////////

WorkDescription* Thread::workDescription() { return _workDescription.load(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the current work description
////////////////////////////////////////////////////////////////////////////////

void Thread::setWorkDescription(WorkDescription* desc) {
  _workDescription.store(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the previous work description
////////////////////////////////////////////////////////////////////////////////

WorkDescription* Thread::setPrevWorkDescription() {
  return _workDescription.exchange(_workDescription.load()->_prev);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets status
////////////////////////////////////////////////////////////////////////////////

void Thread::addStatus(VPackBuilder* b) {
  b->add("asynchronousCancelation", VPackValue(_asynchronousCancelation));
  b->add("started", VPackValue(_started.load()));
  b->add("running", VPackValue(_running.load()));
  b->add("affinity", VPackValue(_affinity));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief allows asynchronous cancelation
////////////////////////////////////////////////////////////////////////////////

void Thread::allowAsynchronousCancelation() {
  if (_started) {
    if (_running) {
      if (TRI_IsSelfThread(&_thread)) {
        LOG_DEBUG("set asynchronous cancelation for thread '%s'",
                  _name.c_str());
        TRI_AllowCancelation();
      } else {
        LOG_ERROR(
            "cannot change cancelation type of an already running thread from "
            "the outside");
      }
    } else {
      LOG_WARNING(
          "thread has already stopped, it is useless to change the cancelation "
          "type");
    }
  } else {
    _asynchronousCancelation = true;
  }
}


void Thread::runMe() {
  if (_asynchronousCancelation) {
    LOG_DEBUG("set asynchronous cancelation for thread '%s'", _name.c_str());
    TRI_AllowCancelation();
  }

  _running = true;

  try {
    run();
  } catch (Exception const& ex) {
    LOG_ERROR("exception caught in thread '%s': %s", _name.c_str(), ex.what());
    TRI_FlushLogging();
    throw;
  } catch (std::exception const& ex) {
    LOG_ERROR("exception caught in thread '%s': %s", _name.c_str(), ex.what());
    TRI_FlushLogging();
    throw;
  } catch (...) {
    _running = false;
    if (!isSilent()) {
      LOG_ERROR("exception caught in thread '%s'", _name.c_str());
      TRI_FlushLogging();
    }
    throw;
  }

  _running = false;

  if (_finishedCondition != nullptr) {
    CONDITION_LOCKER(locker, *_finishedCondition);
    locker.broadcast();
  }
}


