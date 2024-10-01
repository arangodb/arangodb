////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <errno.h>
#include <signal.h>
#include <chrono>
#include <thread>

#include "Basics/operating-system.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "Thread.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifdef TRI_HAVE_PROCESS_H
#include <process.h>
#endif

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;

namespace {

// ever-increasing counter for thread numbers.
// not used on Windows
std::atomic<uint64_t> NEXT_THREAD_ID(1);

// helper struct to assign and retrieve a thread id
struct ThreadNumber {
  ThreadNumber() noexcept
      : value(NEXT_THREAD_ID.fetch_add(1, std::memory_order_seq_cst)) {}

  uint64_t get() const noexcept { return value; }

 private:
  uint64_t value;
};

}  // namespace

/// @brief local thread number
static thread_local ::ThreadNumber LOCAL_THREAD_NUMBER{};
static thread_local char const* LOCAL_THREAD_NAME = nullptr;

// retrieve the current thread's name. the string view will
// remain valid as long as the ThreadNameFetcher remains valid.
ThreadNameFetcher::ThreadNameFetcher() noexcept {
  memset(&_buffer[0], 0, sizeof(_buffer));

  char const* name = LOCAL_THREAD_NAME;
  if (name != nullptr) {
    size_t len = strlen(name);
    if (len > sizeof(_buffer) - 1) {
      len = sizeof(_buffer) - 1;
    }
    memcpy(&_buffer[0], name, len);
  } else {
#ifdef TRI_HAVE_SYS_PRCTL_H
    // will read at most 16 bytes and null-terminate the data
    prctl(PR_GET_NAME, &_buffer, 0, 0, 0);
    // be extra cautious about null-termination of the buffer
    _buffer[sizeof(_buffer) - 1] = '\0';
#endif
  }
  if (_buffer[0] == '\0') {
    // if there is no other name, simply return "main"
    memcpy(&_buffer[0], "main", 4);
  }
}

std::string_view ThreadNameFetcher::get() const noexcept {
  // guaranteed to be properly null-terminated
  return {&_buffer[0]};
}

/// @brief static started with access to the private variables
void Thread::startThread(void* arg) {
  TRI_ASSERT(arg != nullptr);
  Thread* ptr = static_cast<Thread*>(arg);
  TRI_ASSERT(ptr != nullptr);

  ptr->_threadNumber = LOCAL_THREAD_NUMBER.get();

  LOCAL_THREAD_NAME = ptr->name().c_str();

  // make sure we drop our reference when we are finished!
  auto guard = scopeGuard([ptr]() noexcept {
    LOCAL_THREAD_NAME = nullptr;
    ptr->_state.store(ThreadState::STOPPED);
    ptr->releaseRef();
  });

  ThreadState expected = ThreadState::STARTING;
  bool res =
      ptr->_state.compare_exchange_strong(expected, ThreadState::STARTED);
  if (!res) {
    TRI_ASSERT(expected == ThreadState::STOPPING);
    // we are already shutting down -> don't bother calling run!
    return;
  }

  try {
    ptr->runMe();
  } catch (std::exception const& ex) {
    LOG_TOPIC("6784f", WARN, Logger::THREADS)
        << "caught exception in thread '" << ptr->_name << "': " << ex.what();
    throw;
  }
}

/// @brief returns the process id
TRI_pid_t Thread::currentProcessId() { return getpid(); }

/// @brief returns the thread process id
uint64_t Thread::currentThreadNumber() noexcept {
  return LOCAL_THREAD_NUMBER.get();
}

/// @brief returns the thread id
TRI_tid_t Thread::currentThreadId() {
#ifdef TRI_HAVE_POSIX_THREADS
  return pthread_self();
#else
#error "Thread::currentThreadId not implemented"
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
Thread::Thread(application_features::ApplicationServer&,
               std::string const& name, bool deleteOnExit,
               std::uint32_t terminationTimeout)
    : Thread(name, deleteOnExit, terminationTimeout) {}
Thread::Thread(std::string const& name, bool deleteOnExit,
               std::uint32_t terminationTimeout)
    : _threadStructInitialized(false),
      _refs(0),
      _name(name),
      _thread(),
      _threadNumber(0),
      _terminationTimeout(terminationTimeout),
      _deleteOnExit(deleteOnExit),
      _finishedCondition(nullptr),
      _state(ThreadState::CREATED) {
  TRI_InitThread(&_thread);
}

/// @brief deletes the thread
Thread::~Thread() {
  TRI_ASSERT(_refs.load() == 0);

  auto state = _state.load();
  LOG_TOPIC("944b1", TRACE, Logger::THREADS)
      << "delete(" << _name << "), state: " << stringify(state);

  if (state != ThreadState::STOPPED) {
    LOG_TOPIC("80e0e", FATAL, arangodb::Logger::FIXME)
        << "thread '" << _name << "' is not stopped but " << stringify(state)
        << ". shutting down hard";
    FATAL_ERROR_ABORT();
  }
}

/// @brief flags the thread as stopping
void Thread::beginShutdown() {
  LOG_TOPIC("1a183", TRACE, Logger::THREADS)
      << "beginShutdown(" << _name << ") in state " << stringify(_state.load());

  ThreadState state = _state.load();

  while (state == ThreadState::CREATED) {
    _state.compare_exchange_weak(state, ThreadState::STOPPED);
  }

  while (state != ThreadState::STOPPING && state != ThreadState::STOPPED) {
    _state.compare_exchange_weak(state, ThreadState::STOPPING);
  }

  LOG_TOPIC("1fa5b", TRACE, Logger::THREADS)
      << "beginShutdown(" << _name << ") reached state "
      << stringify(_state.load());
}

/// @brief MUST be called from the destructor of the MOST DERIVED class
void Thread::shutdown() {
  LOG_TOPIC("93614", TRACE, Logger::THREADS) << "shutdown(" << _name << ")";

  beginShutdown();
  if (_threadStructInitialized.exchange(false, std::memory_order_acquire)) {
    if (TRI_IsSelfThread(&_thread)) {
      // we must ignore any errors here, but TRI_DetachThread will log them
      TRI_DetachThread(&_thread);
    } else {
      auto ret = TRI_JoinThreadWithTimeout(&_thread, _terminationTimeout);

      if (ret != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC("825a5", FATAL, arangodb::Logger::FIXME)
            << "cannot shutdown thread '" << _name << "', giving up";
        FATAL_ERROR_ABORT();
      }
    }
  }
  TRI_ASSERT(_refs.load() == 0);
  TRI_ASSERT(_state.load() == ThreadState::STOPPED);
}

/// @brief checks if the current thread was asked to stop
bool Thread::isStopping() const noexcept {
  // need acquire to ensure we establish a happens before relation with the
  // update that updates _state, so threads that wait for isStopping to return
  // true are properly synchronized
  auto state = _state.load(std::memory_order_acquire);
  return state == ThreadState::STOPPING || state == ThreadState::STOPPED;
}

/// @brief starts the thread
bool Thread::start(ConditionVariable* finishedCondition) {
  _finishedCondition = finishedCondition;
  ThreadState state = _state.load();

  if (state != ThreadState::CREATED) {
    LOG_TOPIC("11a39", FATAL, Logger::THREADS)
        << "called started on an already started thread '" << _name
        << "', thread is in state " << stringify(state);
    FATAL_ERROR_ABORT();
  }

  ThreadState expected = ThreadState::CREATED;
  if (!_state.compare_exchange_strong(expected, ThreadState::STARTING)) {
    // This should never happen! If it does, it means we have multiple calls to
    // start().
    LOG_TOPIC("7e453", WARN, Logger::THREADS)
        << "failed to set thread '" << _name
        << "' to state 'starting'; thread is in unexpected state "
        << stringify(expected);
    FATAL_ERROR_ABORT();
  }

  // we count two references - one for the current thread and one for the thread
  // that we are trying to start.
  _refs.fetch_add(2);
  TRI_ASSERT(_refs.load() == 2);

  TRI_ASSERT(_threadStructInitialized == false);
  TRI_InitThread(&_thread);

  bool ok = TRI_StartThread(&_thread, _name.c_str(), &startThread, this);
  if (!ok) {
    // could not start the thread -> decrement ref for the foreign thread
    _refs.fetch_sub(1);
    _state.store(ThreadState::STOPPED);
    LOG_TOPIC("f5915", ERR, Logger::THREADS)
        << "could not start thread '" << _name << "': " << TRI_last_error();
  } else {
    _threadStructInitialized.store(true, std::memory_order_release);
  }

  releaseRef();

  return ok;
}

void Thread::markAsStopped() noexcept {
  _state.store(ThreadState::STOPPED);

  if (_finishedCondition != nullptr) {
    std::lock_guard locker{_finishedCondition->mutex};
    _finishedCondition->cv.notify_all();
  }
}

void Thread::runMe() {
  // make sure the thread is marked as stopped under all circumstances
  auto sg = arangodb::scopeGuard([&]() noexcept { markAsStopped(); });

  try {
    run();
  } catch (std::exception const& ex) {
    if (!isSilent()) {
      LOG_TOPIC("3a30c", ERR, Logger::THREADS)
          << "exception caught in thread '" << _name << "': " << ex.what();
    }
    throw;
  } catch (...) {
    if (!isSilent()) {
      LOG_TOPIC("83582", ERR, Logger::THREADS)
          << "unknown exception caught in thread '" << _name << "'";
    }
    throw;
  }
}

void Thread::releaseRef() noexcept {
  auto refs = _refs.fetch_sub(1) - 1;
  TRI_ASSERT(refs >= 0);
  if (refs == 0 && _deleteOnExit) {
    LOCAL_THREAD_NAME = nullptr;
    delete this;
  }
}
