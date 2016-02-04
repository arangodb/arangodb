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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "Thread.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <errno.h>
#include <signal.h>

#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/Logger.h"
#include "Basics/WorkMonitor.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief local thread number
////////////////////////////////////////////////////////////////////////////////

static thread_local uint64_t LOCAL_THREAD_NUMBER = 0;

#ifndef TRI_HAVE_GETTID

namespace {
std::atomic_uint_fast64_t NEXT_THREAD_ID(1);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief static started with access to the private variables
////////////////////////////////////////////////////////////////////////////////

void Thread::startThread(void* arg) {
#ifdef TRI_HAVE_GETTID
  LOCAL_THREAD_NUMBER = (uint64_t)_gettid();
#else
  LOCAL_THREAD_NUMBER = NEXT_THREAD_ID.fetch_add(1, std::memory_order_seq_cst);
#endif

  Thread* ptr = static_cast<Thread*>(arg);

  ptr->_threadNumber = LOCAL_THREAD_NUMBER;

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

uint64_t Thread::currentThreadNumber() { return LOCAL_THREAD_NUMBER; }

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
      LOG(WARN) << "forcefully shutting down thread '" << _name.c_str() << "'";
    }

    int res = TRI_StopThread(&_thread);

    if (res != TRI_ERROR_NO_ERROR) {
      errno = res;
      TRI_set_errno(TRI_ERROR_SYS_ERROR);

      LOG(WARN) << "unable to stop thread '" << _name.c_str() << "': " << TRI_last_error();
    }
  }

  if (_started && !_joined) {
    int res = TRI_DetachThread(&_thread);

    // ignore threads that already died
    if (res != 0 && res != ESRCH) {
      errno = res;
      TRI_set_errno(TRI_ERROR_SYS_ERROR);

      LOG(WARN) << "unable to detach thread '" << _name.c_str() << "': " << TRI_last_error();
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

std::string const& Thread::name() const { return _name; }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for running
////////////////////////////////////////////////////////////////////////////////

bool Thread::isRunning() { return _running; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the thread number
////////////////////////////////////////////////////////////////////////////////

uint64_t Thread::threadNumber() const { return _threadNumber; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the system thread identifier
////////////////////////////////////////////////////////////////////////////////

TRI_tid_t Thread::threadId() { return _threadId; }

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the thread
////////////////////////////////////////////////////////////////////////////////

bool Thread::start(ConditionVariable* finishedCondition) {
  _finishedCondition = finishedCondition;

  if (_started) {
    LOG(FATAL) << "called started on an already started thread"; FATAL_ERROR_EXIT();
  }

  _started = true;

  bool ok =
      TRI_StartThread(&_thread, &_threadId, _name.c_str(), &startThread, this);

  if (!ok) {
    LOG(ERR) << "could not start thread '" << _name.c_str() << "': " << strerror(errno);
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
    LOG(TRACE) << "trying to cancel (aka stop) the thread '" << _name.c_str() << "'";
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

      LOG(WARN) << "unable to stop thread '" << _name.c_str() << "': " << TRI_last_error();
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
        LOG(DEBUG) << "set asynchronous cancelation for thread '" << _name.c_str() << "'";
        TRI_AllowCancelation();
      } else {
        LOG(ERR) << "cannot change cancelation type of an already running thread from the outside";
      }
    } else {
      LOG(WARN) << "thread has already stopped, it is useless to change the cancelation type";
    }
  } else {
    _asynchronousCancelation = true;
  }
}

void Thread::runMe() {
  if (_asynchronousCancelation) {
    LOG(DEBUG) << "set asynchronous cancelation for thread '" << _name.c_str() << "'";
    TRI_AllowCancelation();
  }

  _running = true;

  try {
    run();
  } catch (Exception const& ex) {
    LOG(ERR) << "exception caught in thread '" << _name.c_str() << "': " << ex.what();
    Logger::flush();
    throw;
  } catch (std::exception const& ex) {
    LOG(ERR) << "exception caught in thread '" << _name.c_str() << "': " << ex.what();
    Logger::flush();
    throw;
  } catch (...) {
    _running = false;
    if (!isSilent()) {
      LOG(ERR) << "exception caught in thread '" << _name.c_str() << "'";
      Logger::flush();
    }
    throw;
  }

  _running = false;

  if (_finishedCondition != nullptr) {
    CONDITION_LOCKER(locker, *_finishedCondition);
    locker.broadcast();
  }
}
