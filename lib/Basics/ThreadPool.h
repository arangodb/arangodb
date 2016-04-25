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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_THREAD_POOL_H
#define ARANGODB_BASICS_THREAD_POOL_H 1

#include "Basics/Common.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"

#include <deque>

namespace arangodb {
namespace basics {

class WorkerThread;

class ThreadPool {
 public:
  ThreadPool(ThreadPool const&) = delete;
  ThreadPool& operator=(ThreadPool const&) = delete;

  ThreadPool(size_t, std::string const&);

  ~ThreadPool();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the number of threads in the pool
  //////////////////////////////////////////////////////////////////////////////

  size_t numThreads() const { return _threads.size(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the name of the pool
  //////////////////////////////////////////////////////////////////////////////

  char const* name() const { return _name.c_str(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dequeue a task
  //////////////////////////////////////////////////////////////////////////////

  bool dequeue(std::function<void()>&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief enqueue a task
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  void enqueue(T task) {
    {
      CONDITION_LOCKER(guard, _condition);
      _tasks.emplace_back(std::function<void()>(task));
    }

    _condition.signal();
  }

 private:
  arangodb::basics::ConditionVariable _condition;

  std::vector<arangodb::basics::WorkerThread*> _threads;

  std::deque<std::function<void()>> _tasks;

  std::string _name;

  std::atomic<bool> _stopping;
};

}  // namespace arangodb::basics
}  // namespace arangodb

#endif
