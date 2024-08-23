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

#pragma once

#include <atomic>
#include <string>
#include <string_view>

#include "Basics/threads.h"
#include "Basics/DownCast.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace basics {
struct ConditionVariable;
}

class ThreadNameFetcher {
 public:
  ThreadNameFetcher() noexcept;
  ThreadNameFetcher(ThreadNameFetcher const&) = delete;
  ThreadNameFetcher& operator=(ThreadNameFetcher const&) = delete;

  // retrieve the current thread's name. the string view will
  // remain valid as long as the ThreadNameFetcher remains valid.
  std::string_view get() const noexcept;

 private:
  // stored retrieved thread name, used by the ctor
  char _buffer[32];
};

/// @brief thread
///
/// Each subclass must implement a method run. A thread can be started by
/// start and is stopped either when the method run ends or when stop is
/// called.
class Thread {
  Thread(Thread const&) = delete;
  Thread& operator=(Thread const&) = delete;

 public:
#if defined(TRI_HAVE_POSIX_THREADS)
  typedef pthread_t thread_t;
#else
#error OS not supported
#endif

  enum class ThreadState { CREATED, STARTING, STARTED, STOPPING, STOPPED };

  std::string stringifyState() { return stringify(state()); }

  static std::string stringify(ThreadState);

  /// @brief returns the process id
  static TRI_pid_t currentProcessId();

  /// @brief returns the thread number
  ///
  /// Returns a number that uniquely identifies the current thread. If threads
  /// are implemented using processes, this will return a process identifier.
  /// Otherwise it might just return a unique number without any additional
  /// meaning.
  ///
  /// Note that there is a companion function "threadNumber", which returns
  /// the thread number of a running thread.
  static uint64_t currentThreadNumber() noexcept;

  /// @brief returns the thread id
  static TRI_tid_t currentThreadId();

 public:
  [[deprecated("server argument is no longer needed")]] Thread(
      application_features::ApplicationServer&, std::string const& name,
      bool deleteOnExit = false, std::uint32_t terminationTimeout = INFINITE);
  Thread(std::string const& name, bool deleteOnExit = false,
         std::uint32_t terminationTimeout = INFINITE);
  virtual ~Thread();

  // whether or not the thread is allowed to start during prepare
  virtual bool isSystem() const { return false; }

  /// @brief whether or not the thread is chatty on shutdown
  virtual bool isSilent() const { return false; }

  /// @brief flags the thread as stopping
  /// Classes that override this function must ensure that they
  /// always call Thread::beginShutdown()!
  virtual void beginShutdown();

  bool runningInThisThread() const {
    return currentThreadNumber() == this->threadNumber();
  }

  /// @brief name of a thread
  std::string const& name() const noexcept { return _name; }

  /// @brief returns the thread number
  ///
  /// See currentThreadNumber().
  uint64_t threadNumber() const noexcept { return _threadNumber; }

  /// @brief false, if the thread is just created
  bool hasStarted() const noexcept {
    return _state.load() != ThreadState::CREATED;
  }

  /// @brief true, if the thread is still running
  bool isRunning() const noexcept {
    // need acquire to ensure we establish a happens before relation with the
    // update that sets the state to STOPPED, so threads that wait for isRunning
    // to return false are properly synchronized
    return _state.load(std::memory_order_acquire) != ThreadState::STOPPED;
  }

  /// @brief checks if the current thread was asked to stop
  bool isStopping() const noexcept;

  /// @brief starts the thread
  bool start(basics::ConditionVariable* _finishedCondition = nullptr);

  /// @brief return the threads current state
  ThreadState state() const noexcept {
    return _state.load(std::memory_order_relaxed);
  }

  /// @brief MUST be called from the destructor of the MOST DERIVED class
  ///
  /// shutdown sets the _state to signal the thread that it should stop
  /// and waits for the thread to finish. This is necessary to avoid any
  /// races in the destructor.
  /// That is also the reason why it has to be called by the MOST DERIVED
  /// class (potential race on the objects vtable). Usually the call to
  /// shutdown should be the very first thing in the destructor. Any access
  /// to members of the thread that happen before the call to shutdown must
  /// be thread-safe!
  void shutdown();

 protected:
  /// @brief the thread program. note that any implementation of run() is
  /// responsible for handling its own exceptions inside run(). failure to do
  /// so will lead to the thread being aborted, and the exception escaping
  /// from  it!!
  virtual void run() = 0;

 private:
  /// @brief static started with access to the private variables
  static void startThread(void* arg);
  void markAsStopped() noexcept;
  void runMe();
  void releaseRef() noexcept;

 private:
  std::atomic<bool> _threadStructInitialized;
  std::atomic<int> _refs;

  // name of the thread
  std::string const _name;

  // internal thread information
  thread_t _thread;
  uint64_t _threadNumber;

  // The max timeout (in ms) to wait for the thread to terminate.
  // Failure to terminate within the specified time results in process abortion!
  // The default value is INFINITE, i.e., we want to wait forever instead of
  // aborting the process.
  std::uint32_t _terminationTimeout;

  bool const _deleteOnExit;

  basics::ConditionVariable* _finishedCondition;

  std::atomic<ThreadState> _state;
};

template<typename ServerT>
class ServerThread : public Thread {
 public:
  using Server = ServerT;

  ServerThread(Server& server, std::string const& name,
               bool deleteOnExit = false,
               std::uint32_t terminationTimeout = INFINITE)
      : Thread{server, name, deleteOnExit, terminationTimeout},
        _server(server) {}

  Server& server() noexcept { return _server; }

 protected:
  Server& _server;
};

}  // namespace arangodb
