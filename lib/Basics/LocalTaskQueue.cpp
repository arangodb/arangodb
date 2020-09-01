////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "LocalTaskQueue.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/debugging.h"
#include "Logger/Logger.h"

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task tied to the specified queue
////////////////////////////////////////////////////////////////////////////////

LocalTask::LocalTask(std::shared_ptr<LocalTaskQueue> const& queue)
    : _queue(queue) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatch this task to the scheduler
////////////////////////////////////////////////////////////////////////////////

void LocalTask::dispatch() {
  _queue->post([self = shared_from_this(), this]() {
    _queue->startTask();
    try {
      run();
      _queue->stopTask();
    } catch (...) {
      _queue->stopTask();
      throw;
    }
    return true;
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a callback task tied to the specified queue
////////////////////////////////////////////////////////////////////////////////

LocalCallbackTask::LocalCallbackTask(std::shared_ptr<LocalTaskQueue> const& queue,
                                     std::function<void()> const& cb)
    : _queue(queue), _cb(cb) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the callback and join
////////////////////////////////////////////////////////////////////////////////

void LocalCallbackTask::run() {
  try {
    _cb();
  } catch (...) {
  }
  _queue->join();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatch the callback task to the scheduler
////////////////////////////////////////////////////////////////////////////////

void LocalCallbackTask::dispatch() {
  _queue->post([self = shared_from_this(), this]() { 
    run(); 
    return true;
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a queue
////////////////////////////////////////////////////////////////////////////////

LocalTaskQueue::LocalTaskQueue(application_features::ApplicationServer& server, PostFn poster)
    : _server(server),
      _poster(poster),
      _queue(),
      _callbackQueue(),
      _condition(),
      _mutex(),
      _missing(0),
      _started(0),
      _status(TRI_ERROR_NO_ERROR) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the queue.
////////////////////////////////////////////////////////////////////////////////

LocalTaskQueue::~LocalTaskQueue() = default;

void LocalTaskQueue::startTask() {
  CONDITION_LOCKER(guard, _condition);
  ++_started;
}

void LocalTaskQueue::stopTask() {
  CONDITION_LOCKER(guard, _condition);
  --_started;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief enqueue a task to be run
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::enqueue(std::shared_ptr<LocalTask> task) {
  MUTEX_LOCKER(locker, _mutex);
  _queue.push(task);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief enqueue a callback task to be run after all normal tasks finish;
/// useful for cleanup tasks
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::enqueueCallback(std::shared_ptr<LocalCallbackTask> task) {
  MUTEX_LOCKER(locker, _mutex);
  _callbackQueue.push(task);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief post a function to the scheduler. Should only be used internally
/// by task dispatch.
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::post(std::function<bool()> fn) { 
  bool result = _poster(fn); 
  if (!result) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUEUE_FULL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief join a single task. reduces the number of waiting tasks and wakes
/// up the queue's dispatchAndWait() routine
////////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::join() {
  CONDITION_LOCKER(guard, _condition);
  TRI_ASSERT(_missing > 0);
  --_missing;
  _condition.signal();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief dispatch all tasks, including those that are queued while running,
/// and wait for all tasks to join; then dispatch all callback tasks and wait
/// for them to join
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::dispatchAndWait() {
  // regular task loop
  if (!_queue.empty()) {
    while (true) {
      CONDITION_LOCKER(guard, _condition);

      {
        MUTEX_LOCKER(locker, _mutex);
        // dispatch all newly queued tasks
        if (_status == TRI_ERROR_NO_ERROR) {
          while (!_queue.empty()) {
            auto task = _queue.front();
            task->dispatch();
            _queue.pop();
            ++_missing;
          }
        }
      }

      if (_missing == 0) {
        break;
      }

      if (_missing > 0 && _started == 0 && _server.isStopping()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      guard.wait(100000);
    }
  }

  // callback task loop
  if (!_callbackQueue.empty()) {
    while (true) {
      CONDITION_LOCKER(guard, _condition);

      {
        MUTEX_LOCKER(locker, _mutex);
        // dispatch all newly queued callbacks
        while (!_callbackQueue.empty()) {
          auto task = _callbackQueue.front();
          task->dispatch();
          _callbackQueue.pop();
          ++_missing;
        }
      }

      if (_missing == 0) {
        break;
      }

      if (_missing > 0 && _started == 0 && _server.isStopping()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      guard.wait(100000);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief set status of queue
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::setStatus(int status) {
  MUTEX_LOCKER(locker, _mutex);
  _status = status;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return overall status of queue tasks
//////////////////////////////////////////////////////////////////////////////

int LocalTaskQueue::status() {
  MUTEX_LOCKER(locker, _mutex);
  return _status;
}

}  // namespace basics
}  // namespace arangodb
