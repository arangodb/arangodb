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

#ifndef ARANGODB_BASICS_THREAD_H
#define ARANGODB_BASICS_THREAD_H 1

#include "Basics/Common.h"

#include "Basics/WorkDescription.h"
#include "Basics/threads.h"

namespace arangodb {
namespace velocypack {
class Builder;
}
}

namespace arangodb {
namespace basics {
class ConditionVariable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thread
///
/// Each subclass must implement a method run. A thread can be started by
/// start and is stopped either when the method run ends or when stop is
/// called.
////////////////////////////////////////////////////////////////////////////////

class Thread {
  Thread(Thread const&) = delete;
  Thread& operator=(Thread const&) = delete;

 public:
#if defined(TRI_HAVE_POSIX_THREADS)
  typedef pthread_t thread_t;
#elif defined(TRI_HAVE_WIN32_THREADS)
  typedef HANDLE thread_t;
#else
#error OS not supported
#endif

  enum class ThreadState { CREATED, STARTED, STOPPING, STOPPED, DETACHED };

  static std::string stringify(ThreadState);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief current work description as thread local variable
  //////////////////////////////////////////////////////////////////////////////

  static thread_local Thread* CURRENT_THREAD;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the process id
  //////////////////////////////////////////////////////////////////////////////

  static TRI_pid_t currentProcessId();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the thread number
  ///
  /// Returns a number that uniquely identifies the current thread. If threads
  /// are implemented using processes, this will return a process identifier.
  /// Otherwise it might just return a unique number without any additional
  /// meaning.
  ///
  /// Note that there is a companion function "threadNumber", which returns
  /// the thread number of a running thread.
  //////////////////////////////////////////////////////////////////////////////

  static uint64_t currentThreadNumber();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the thread id
  //////////////////////////////////////////////////////////////////////////////

  static TRI_tid_t currentThreadId();

  // returns the current thread
  static Thread* current() { return CURRENT_THREAD; }

  // returns the current work description or nullptr
  static WorkDescription* currentWorkDescription() {
    return CURRENT_THREAD == nullptr ? nullptr
                                     : CURRENT_THREAD->workDescription();
  }

  // returns the current work context or nullptr
  static WorkContext* currentWorkContext() {
    if (CURRENT_THREAD == nullptr) {
      return nullptr;
    }

    auto description = CURRENT_THREAD->workDescription();

    if (description == nullptr) {
      return nullptr;
    }

    return description->_context.get();
  }

  // checks if work has been canceled
  static bool isCanceled() {
    Thread* thread = CURRENT_THREAD;

    if (thread == nullptr) {
      return false;
    }

    WorkDescription* desc = thread->workDescription();

    return desc == nullptr ? false : desc->_data._thread._canceled.load();
  }

 public:
  Thread(std::string const& name, bool deleteOnExit = false);
  virtual ~Thread();

 public:
  // whether or not the thread is allowed to start during prepare
  virtual bool isSystem() { return false; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the thread is chatty on shutdown
  //////////////////////////////////////////////////////////////////////////////

  virtual bool isSilent() { return false; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief flags the thread as stopping
  //////////////////////////////////////////////////////////////////////////////

  virtual void beginShutdown();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief called from the destructor
  //////////////////////////////////////////////////////////////////////////////

  void shutdown();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief name of a thread
  //////////////////////////////////////////////////////////////////////////////

  std::string const& name() const { return _name; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the thread number
  ///
  /// See currentThreadNumber().
  //////////////////////////////////////////////////////////////////////////////

  uint64_t threadNumber() const { return _threadNumber; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the system thread identifier
  //////////////////////////////////////////////////////////////////////////////

  TRI_tid_t threadId() const { return _threadId; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief false, if the thread is just created
  //////////////////////////////////////////////////////////////////////////////

  bool hasStarted() const { return _state.load() != ThreadState::CREATED; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief true, if the thread is still running
  //////////////////////////////////////////////////////////////////////////////

  bool isRunning() const {
    return _state.load(std::memory_order_relaxed) != ThreadState::STOPPED;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if the current thread was asked to stop
  //////////////////////////////////////////////////////////////////////////////

  bool isStopping() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief starts the thread
  //////////////////////////////////////////////////////////////////////////////

  bool start(basics::ConditionVariable* _finishedCondition = nullptr);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the process affinity
  //////////////////////////////////////////////////////////////////////////////

  void setProcessorAffinity(size_t c);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the current work description
  //////////////////////////////////////////////////////////////////////////////

  WorkDescription* workDescription() const { return _workDescription.load(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the current work description
  //////////////////////////////////////////////////////////////////////////////

  void setWorkDescription(WorkDescription*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the previous work description
  //////////////////////////////////////////////////////////////////////////////

  WorkDescription* setPrevWorkDescription();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generates a description of the thread
  //////////////////////////////////////////////////////////////////////////////

  virtual void addStatus(arangodb::velocypack::Builder* b);

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the thread program
  //////////////////////////////////////////////////////////////////////////////

  virtual void run() = 0;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief static started with access to the private variables
  //////////////////////////////////////////////////////////////////////////////

  static void startThread(void* arg);

 private:
  void markAsStopped();
  void runMe();
  void cleanupMe();

 private:
  bool const _deleteOnExit;

  // name of the thread
  std::string const _name;

  // internal thread information
  thread_t _thread;
  uint64_t _threadNumber;
  TRI_tid_t _threadId;

  basics::ConditionVariable* _finishedCondition;

  std::atomic<ThreadState> _state;

  // processor affinity
  int _affinity;

  std::atomic<WorkDescription*> _workDescription;
};
}

#endif
