////////////////////////////////////////////////////////////////////////////////
/// @brief generic thread pool
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ThreadPool.h"
#include "Basics/WorkerThread.h"

using namespace triagens::basics;



////////////////////////////////////////////////////////////////////////////////
/// @brief create a pool with the specified size of worker threads
////////////////////////////////////////////////////////////////////////////////

ThreadPool::ThreadPool(size_t size, std::string const& name)
    : _condition(), _threads(), _tasks(), _name(name), _stopping(false) {
  _threads.reserve(size);

  for (size_t i = 0; i < size; ++i) {
    auto workerThread = new WorkerThread(this);

    try {
      _threads.emplace_back(workerThread);
    } catch (...) {
      delete workerThread;
      throw;
    }

    workerThread->start();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the pool
////////////////////////////////////////////////////////////////////////////////

ThreadPool::~ThreadPool() {
  _stopping = true;
  _condition.broadcast();

  for (auto it : _threads) {
    it->waitForDone();
    delete it;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief dequeue a task
////////////////////////////////////////////////////////////////////////////////

bool ThreadPool::dequeue(std::function<void()>& result) {
  while (!_stopping) {
    CONDITION_LOCKER(guard, _condition);

    if (_tasks.empty()) {
      guard.wait();
    }

    if (_stopping) {
      break;
    }

    if (!_tasks.empty()) {
      result = _tasks.front();
      _tasks.pop_front();
      return true;
    }
  }

  return false;
}


