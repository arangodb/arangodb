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
/// @brief current work description as thread local variable
////////////////////////////////////////////////////////////////////////////////

thread_local WorkDescription* Thread::CURRENT_WORK_DESCRIPTION = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief current work description as thread local variable
////////////////////////////////////////////////////////////////////////////////

thread_local Thread* Thread::CURRENT_THREAD = nullptr;

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
      _thread(),
      _threadNumber(0),
      _threadId(),
      _finishedCondition(nullptr),
      _state(ThreadState::CREATED),
      _affinity(-1),
      _workDescription(nullptr) {
  TRI_InitThread(&_thread);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the thread
////////////////////////////////////////////////////////////////////////////////

Thread::~Thread() {
  shutdown(false);

  if (_state.load() == ThreadState::STOPPED) {
    int res = TRI_DetachThread(&_thread);

    if (res != 0) {
      LOG(TRACE) << "cannot detach thread";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flags the tread as stopping
////////////////////////////////////////////////////////////////////////////////

void Thread::beginShutdown() {
  ThreadState state = _state.load();

  while (state != ThreadState::STOPPING && state != ThreadState::STOPPED) {
    _state.compare_exchange_strong(state, ThreadState::STOPPING);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called from the destructor
////////////////////////////////////////////////////////////////////////////////

void Thread::shutdown(bool waitForStopped) {
  if (_state.load() == ThreadState::STARTED) {
    beginShutdown();

    if (!isSilent()) {
      LOG(WARN) << "forcefully shutting down thread '" << _name.c_str() << "'";
    }
  }

  size_t n = waitForStopped ? (10 * 60 * 20) : 20;

  for (size_t i = 0; i < n; ++i) {
    if (_state.load() == ThreadState::STOPPED) {
      break;
    }

    usleep(100 * 1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the current thread was asked to stop
////////////////////////////////////////////////////////////////////////////////

bool Thread::isStopping() const {
  auto state = _state.load(std::memory_order_relaxed);

  return state == ThreadState::STOPPING || state == ThreadState::STOPPED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the thread
////////////////////////////////////////////////////////////////////////////////

bool Thread::start(ConditionVariable* finishedCondition) {
  _finishedCondition = finishedCondition;

  if (_state.load() != ThreadState::CREATED) {
    LOG(FATAL) << "called started on an already started thread";
    FATAL_ERROR_EXIT();
  }

  bool ok =
      TRI_StartThread(&_thread, &_threadId, _name.c_str(), &startThread, this);

  if (!ok) {
    LOG(ERR) << "could not start thread '" << _name.c_str()
             << "': " << strerror(errno);
  }

  if (0 <= _affinity) {
    TRI_SetProcessorAffinity(&_thread, (size_t)_affinity);
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void Thread::setProcessorAffinity(size_t c) { _affinity = (int)c; }

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
  b->add("affinity", VPackValue(_affinity));

  switch (_state.load()) {
    case ThreadState::CREATED:
      b->add("started", VPackValue("created"));
      break;

    case ThreadState::STARTED:
      b->add("started", VPackValue("started"));
      break;

    case ThreadState::STOPPING:
      b->add("started", VPackValue("stopping"));
      break;

    case ThreadState::STOPPED:
      b->add("started", VPackValue("stopped"));
      break;
  }
}

void Thread::runMe() {
  ThreadState expected = ThreadState::CREATED;
  bool res = _state.compare_exchange_strong(expected, ThreadState::STARTED);

  if (!res) {
    LOG(WARN) << "thread died before it could start";
    return;
  }

  try {
    run();
    _state.store(ThreadState::STOPPED);
  } catch (Exception const& ex) {
    LOG(ERR) << "exception caught in thread '" << _name.c_str()
             << "': " << ex.what();
    Logger::flush();
    _state.store(ThreadState::STOPPED);
    throw;
  } catch (std::exception const& ex) {
    LOG(ERR) << "exception caught in thread '" << _name.c_str()
             << "': " << ex.what();
    Logger::flush();
    _state.store(ThreadState::STOPPED);
    throw;
  } catch (...) {
    if (!isSilent()) {
      LOG(ERR) << "exception caught in thread '" << _name.c_str() << "'";
      Logger::flush();
    }
    _state.store(ThreadState::STOPPED);
    throw;
  }

  if (_finishedCondition != nullptr) {
    CONDITION_LOCKER(locker, *_finishedCondition);
    locker.broadcast();
  }
}
