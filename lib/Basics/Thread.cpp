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
#include "Logger/Logger.h"

#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;

/// @brief local thread number
static thread_local uint64_t LOCAL_THREAD_NUMBER = 0;
static thread_local char const* LOCAL_THREAD_NAME = nullptr;

#if !defined(ARANGODB_HAVE_GETTID) && !defined(_WIN32)

namespace {
std::atomic<uint64_t> NEXT_THREAD_ID(1);
}

#endif

/// @brief static started with access to the private variables
void Thread::startThread(void* arg) {
#if defined(ARANGODB_HAVE_GETTID)
  LOCAL_THREAD_NUMBER = (uint64_t)gettid();
#elif defined(_WIN32)
  LOCAL_THREAD_NUMBER = (uint64_t)GetCurrentThreadId();
#else
  LOCAL_THREAD_NUMBER = NEXT_THREAD_ID.fetch_add(1, std::memory_order_seq_cst);
#endif

  TRI_ASSERT(arg != nullptr);
  Thread* ptr = static_cast<Thread*>(arg);
  TRI_ASSERT(ptr != nullptr);

  ptr->_threadNumber = LOCAL_THREAD_NUMBER;

  LOCAL_THREAD_NAME = ptr->name().c_str();

  // make sure we drop our reference when we are finished!
  auto guard = scopeGuard([ptr]() {
    LOCAL_THREAD_NAME = nullptr;
    ptr->_state.store(ThreadState::STOPPED);
    ptr->releaseRef();
  });

  ThreadState expected = ThreadState::STARTING;
  bool res = ptr->_state.compare_exchange_strong(expected, ThreadState::STARTED);
  if (!res) {
    TRI_ASSERT(expected == ThreadState::STOPPING);
    // we are already shutting down -> don't bother calling run!
    return;
  }

  try {
    ptr->runMe();
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::THREADS)
        << "caught exception in thread '" << ptr->_name << "': " << ex.what();
    ptr->crashNotification(ex);
    throw;
  }
}

/// @brief returns the process id
TRI_pid_t Thread::currentProcessId() {
#ifdef _WIN32
  return _getpid();
#else
  return getpid();
#endif
}

/// @brief returns the thread process id
uint64_t Thread::currentThreadNumber() { return LOCAL_THREAD_NUMBER; }

/// @brief returns the name of the current thread, if set
/// note that this function may return a nullptr
char const* Thread::currentThreadName() { return LOCAL_THREAD_NAME; }

/// @brief returns the thread id
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
    case ThreadState::STARTING:
      return "starting";
    case ThreadState::STARTED:
      return "started";
    case ThreadState::STOPPING:
      return "stopping";
    case ThreadState::STOPPED:
      return "stopped";
  }
  return "unknown";
}

/// @brief constructs a thread
Thread::Thread(std::string const& name, bool deleteOnExit, std::uint32_t terminationTimeout)
    : _deleteOnExit(deleteOnExit),
      _threadStructInitialized(false),
      _refs(0),
      _name(name),
      _thread(),
      _threadNumber(0),
      _terminationTimeout(terminationTimeout),
      _finishedCondition(nullptr),
      _state(ThreadState::CREATED) {
  TRI_InitThread(&_thread);
}

/// @brief deletes the thread
Thread::~Thread() {
  TRI_ASSERT(_refs.load() == 0);

  auto state = _state.load();
  LOG_TOPIC(TRACE, Logger::THREADS)
      << "delete(" << _name << "), state: " << stringify(state);

  if (state != ThreadState::STOPPED) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "thread '" << _name << "' is not stopped but " << stringify(state)
        << ". shutting down hard";
    FATAL_ERROR_ABORT();
  }
}

/// @brief flags the thread as stopping
void Thread::beginShutdown() {
  LOG_TOPIC(TRACE, Logger::THREADS)
      << "beginShutdown(" << _name << ") in state " << stringify(_state.load());

  ThreadState state = _state.load();

  while (state == ThreadState::CREATED) {
    _state.compare_exchange_weak(state, ThreadState::STOPPED);
  }

  while (state != ThreadState::STOPPING && state != ThreadState::STOPPED) {
    _state.compare_exchange_weak(state, ThreadState::STOPPING);
  }

  LOG_TOPIC(TRACE, Logger::THREADS) << "beginShutdown(" << _name << ") reached state "
                                    << stringify(_state.load());
}

/// @brief MUST be called from the destructor of the MOST DERIVED class
void Thread::shutdown() {
  LOG_TOPIC(TRACE, Logger::THREADS) << "shutdown(" << _name << ")";

  beginShutdown();
  if (_threadStructInitialized) {
    if (TRI_IsSelfThread(&_thread)) {
      // we must ignore any errors here, but TRI_DetachThread will log them
      TRI_DetachThread(&_thread);
    } else {
#ifdef __APPLE__
      // MacOS does not provide an implemenation of pthread_timedjoin_np which is used in
      // TRI_JoinThreadWithTimeout, so instead we simply wait for _state to be set to STOPPED.

      std::uint32_t n = _terminationTimeout / 100;
      for (std::uint32_t i = 0; i < n || _terminationTimeout == INFINITE; ++i) {
        if (_state.load() == ThreadState::STOPPED) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      // we still have to wait here until the thread has terminated, but this should
      // happen immediately after _state has been set to STOPPED!
      int ret = _state.load() == ThreadState::STOPPED ? TRI_JoinThread(&_thread) : TRI_ERROR_FAILED;
#else
      auto ret = TRI_JoinThreadWithTimeout(&_thread, _terminationTimeout);
#endif

      if (ret != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "cannot shutdown thread '" << _name << "', giving up";
        FATAL_ERROR_ABORT();
      }
    }
  }
  TRI_ASSERT(_refs.load() == 0);
  TRI_ASSERT(_state.load() == ThreadState::STOPPED);
}

/// @brief checks if the current thread was asked to stop
bool Thread::isStopping() const {
  auto state = _state.load(std::memory_order_relaxed);

  return state == ThreadState::STOPPING || state == ThreadState::STOPPED;
}

/// @brief starts the thread
bool Thread::start(ConditionVariable* finishedCondition) {
  if (!isSystem() && !ApplicationServer::isPrepared()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "trying to start a thread '" << _name << "' before prepare has finished, current state: "
        << (ApplicationServer::server == nullptr
                ? -1
                : (int)ApplicationServer::server->state());
    FATAL_ERROR_ABORT();
  }

  _finishedCondition = finishedCondition;
  ThreadState state = _state.load();

  if (state != ThreadState::CREATED) {
    LOG_TOPIC(FATAL, Logger::THREADS)
        << "called started on an already started thread '" << _name << "', thread is in state "
        << stringify(state);
    FATAL_ERROR_ABORT();
  }

  ThreadState expected = ThreadState::CREATED;
  if (!_state.compare_exchange_strong(expected, ThreadState::STARTING)) {
    // This should never happen! If it does, it means we have multiple calls to start().
    LOG_TOPIC(WARN, Logger::THREADS)
        << "failed to set thread '" << _name << "' to state 'starting'; thread is in unexpected state "
        << stringify(expected);
    FATAL_ERROR_ABORT();
  }
  
  // we count two references - one for the current thread and one for the thread that
  // we are trying to start.
  _refs.fetch_add(2);
  TRI_ASSERT(_refs.load() == 2);

  TRI_ASSERT(_threadStructInitialized == false);
  TRI_InitThread(&_thread);

  bool ok = TRI_StartThread(&_thread, _name.c_str(), &startThread, this);
  if (!ok) {
    // could not start the thread -> decrement ref for the foreign thread 
    _refs.fetch_sub(1);
    _state.store(ThreadState::STOPPED);
    LOG_TOPIC(ERR, Logger::THREADS)
        << "could not start thread '" << _name << "': " << TRI_last_error();
  }

  _threadStructInitialized = true;

  releaseRef();

  return ok;
}

void Thread::markAsStopped() {
  // TODO - marked as stopped before accessing finishedCondition?
  _state.store(ThreadState::STOPPED);

  if (_finishedCondition != nullptr) {
    CONDITION_LOCKER(locker, *_finishedCondition);
    locker.broadcast();
  }
}

void Thread::runMe() {
  // make sure the thread is marked as stopped under all circumstances
  TRI_DEFER(markAsStopped());

  try {
    run();
  } catch (std::exception const& ex) {
    if (!isSilent()) {
      LOG_TOPIC(ERR, Logger::THREADS)
          << "exception caught in thread '" << _name << "': " << ex.what();
      Logger::flush();
    }
    throw;
  } catch (...) {
    if (!isSilent()) {
      LOG_TOPIC(ERR, Logger::THREADS)
          << "unknown exception caught in thread '" << _name << "'";
      Logger::flush();
    }
    throw;
  }
}

void Thread::releaseRef() {
  auto refs = _refs.fetch_sub(1) - 1;
  TRI_ASSERT(refs >= 0);
  if (refs == 0 && _deleteOnExit) {
    LOCAL_THREAD_NAME = nullptr;
    delete this;
  }
}
