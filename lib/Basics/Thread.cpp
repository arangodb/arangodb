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

#include <errno.h>
#include <signal.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/WorkMonitor.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief local thread number
////////////////////////////////////////////////////////////////////////////////

static thread_local uint64_t LOCAL_THREAD_NUMBER = 0;

#if !defined(ARANGODB_HAVE_GETTID) && !defined(_WIN32)

namespace {
std::atomic<uint64_t> NEXT_THREAD_ID(1);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief current work description as thread local variable
////////////////////////////////////////////////////////////////////////////////

thread_local Thread* Thread::CURRENT_THREAD = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief static started with access to the private variables
////////////////////////////////////////////////////////////////////////////////

void Thread::startThread(void* arg) {
#if defined(ARANGODB_HAVE_GETTID)
  LOCAL_THREAD_NUMBER = (uint64_t)gettid();
#elif defined(_WIN32)
  LOCAL_THREAD_NUMBER = (uint64_t)GetCurrentThreadId();
#else
  LOCAL_THREAD_NUMBER = NEXT_THREAD_ID.fetch_add(1, std::memory_order_seq_cst);
#endif

  Thread* ptr = static_cast<Thread*>(arg);

  ptr->_threadNumber = LOCAL_THREAD_NUMBER;

  bool pushed = WorkMonitor::pushThread(ptr);

  try {
    ptr->runMe();
  } catch (...) {
    if (pushed) {
      WorkMonitor::popThread(ptr);
    }
    throw;
  }

  if (pushed) {
    WorkMonitor::popThread(ptr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the process id
////////////////////////////////////////////////////////////////////////////////

TRI_pid_t Thread::currentProcessId() {
#ifdef _WIN32
  return _getpid();
#else
  return getpid();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the thread process id
////////////////////////////////////////////////////////////////////////////////

uint64_t Thread::currentThreadNumber() { return LOCAL_THREAD_NUMBER; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the thread id
////////////////////////////////////////////////////////////////////////////////

TRI_tid_t Thread::currentThreadId() {
#ifdef TRI_HAVE_WIN32_THREADS
  return GetCurrentThreadId();
#else
#ifdef TRI_HAVE_POSIX_THREADS
  return pthread_self();
#else
#error "Thread::currentThreadId not implemented"
#endif
#endif
}

std::string Thread::stringify(ThreadState state) {
  switch (state) {
    case ThreadState::CREATED:
      return "created";
    case ThreadState::STARTED:
      return "started";
    case ThreadState::STOPPING:
      return "stopping";
    case ThreadState::STOPPED:
      return "stopped";
    case ThreadState::DETACHED:
      return "detached";
  }
  return "unknown";
}

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
  
  // allow failing memory allocations for all threads by default 
  TRI_AllowMemoryFailures();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the thread
////////////////////////////////////////////////////////////////////////////////

Thread::~Thread() {
  auto state = _state.load();
  LOG_TOPIC(TRACE, Logger::THREADS) << "delete(" << _name
                                    << "), state: " << stringify(state);

  if (state == ThreadState::STOPPED) {
    int res = TRI_JoinThread(&_thread);

    if (res != 0) {
      LOG_TOPIC(INFO, Logger::THREADS) << "cannot detach thread";
    }

    _state.store(ThreadState::DETACHED);
  }

  state = _state.load();

  if (state != ThreadState::DETACHED && state != ThreadState::CREATED) {
    LOG(FATAL) << "thread is not detached but " << stringify(state)
               << ". shutting down hard";
    FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flags the thread as stopping
////////////////////////////////////////////////////////////////////////////////

void Thread::beginShutdown() {
  LOG_TOPIC(TRACE, Logger::THREADS)
      << "beginShutdown(" << _name << ") in state " << stringify(_state.load());

  ThreadState state = _state.load();

  while (state == ThreadState::CREATED) {
    _state.compare_exchange_strong(state, ThreadState::STOPPED);
  }

  while (state != ThreadState::STOPPING && state != ThreadState::STOPPED &&
         state != ThreadState::DETACHED) {
    _state.compare_exchange_strong(state, ThreadState::STOPPING);
  }

  LOG_TOPIC(TRACE, Logger::THREADS) << "beginShutdown(" << _name
                                    << ") reached state "
                                    << stringify(_state.load());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called from the destructor
////////////////////////////////////////////////////////////////////////////////

void Thread::shutdown() {
  LOG_TOPIC(TRACE, Logger::THREADS) << "shutdown(" << _name << ")";

  ThreadState state = _state.load();

  while (state == ThreadState::CREATED) {
    bool res = _state.compare_exchange_strong(state, ThreadState::DETACHED);

    if (res) {
      return;
    }
  }

  if (_state.load() == ThreadState::STARTED) {
    beginShutdown();

    if (!isSilent()) {
      LOG_TOPIC(WARN, Logger::THREADS) << "forcefully shutting down thread '"
                                       << _name << "' in state "
                                       << stringify(_state.load());
    }
  }

  size_t n = 10 * 60 * 5;  // * 100ms = 1s * 60 * 5

  for (size_t i = 0; i < n; ++i) {
    if (_state.load() == ThreadState::STOPPED) {
      break;
    }

    usleep(100 * 1000);
  }

  if (_state.load() != ThreadState::STOPPED) {
    LOG(FATAL) << "cannot shutdown thread, giving up";
    FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the current thread was asked to stop
////////////////////////////////////////////////////////////////////////////////

bool Thread::isStopping() const {
  auto state = _state.load(std::memory_order_relaxed);

  return state == ThreadState::STOPPING || state == ThreadState::STOPPED ||
         state == ThreadState::DETACHED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the thread
////////////////////////////////////////////////////////////////////////////////

bool Thread::start(ConditionVariable* finishedCondition) {
  if (!isSystem() && !ApplicationServer::isPrepared()) {
    LOG(FATAL) << "trying to start a thread '" << _name
               << "' before prepare has finished, current state: "
               << (ApplicationServer::server == nullptr
                       ? -1
                       : (int)ApplicationServer::server->state());
    FATAL_ERROR_EXIT();
  }

  _finishedCondition = finishedCondition;
  ThreadState state = _state.load();

  if (state != ThreadState::CREATED) {
    LOG_TOPIC(FATAL, Logger::THREADS)
        << "called started on an already started thread, thread is in state "
        << stringify(state);
    FATAL_ERROR_EXIT();
  }

  ThreadState expected = ThreadState::CREATED;
  bool res = _state.compare_exchange_strong(expected, ThreadState::STARTED);

  if (!res) {
    LOG_TOPIC(WARN, Logger::THREADS)
        << "thread died before it could start, thread is in state "
        << stringify(expected);
    return false;
  }

  bool ok =
      TRI_StartThread(&_thread, &_threadId, _name.c_str(), &startThread, this);

  if (ok) {
    if (0 <= _affinity) {
      TRI_SetProcessorAffinity(&_thread, (size_t)_affinity);
    }
  } else {
    _state.store(ThreadState::STOPPED);
    LOG_TOPIC(ERR, Logger::THREADS) << "could not start thread '" << _name
                                    << "': " << strerror(errno);

    return false;
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
  return _workDescription.exchange(_workDescription.load()->_prev.load());
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

    case ThreadState::DETACHED:
      b->add("started", VPackValue("detached"));
      break;
  }
}

void Thread::runMe() {
  try {
    run();
    _state.store(ThreadState::STOPPED);
  } catch (Exception const& ex) {
    LOG_TOPIC(ERR, Logger::THREADS) << "exception caught in thread '" << _name
                                    << "': " << ex.what();
    Logger::flush();
    _state.store(ThreadState::STOPPED);
    throw;
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, Logger::THREADS) << "exception caught in thread '" << _name
                                    << "': " << ex.what();
    Logger::flush();
    _state.store(ThreadState::STOPPED);
    throw;
  } catch (...) {
    if (!isSilent()) {
      LOG_TOPIC(ERR, Logger::THREADS) << "exception caught in thread '" << _name
                                      << "'";
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
