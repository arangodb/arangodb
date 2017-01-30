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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "LocalTaskQueue.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/asio-helper.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task tied to the specified queue
////////////////////////////////////////////////////////////////////////////////

LocalTask::LocalTask(LocalTaskQueue* queue) : _queue(queue) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatch this task to the underlying io_service
////////////////////////////////////////////////////////////////////////////////

void LocalTask::dispatch() {
  auto self = shared_from_this();
  _queue->ioService()->post([self, this]() { run(); });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a callback task tied to the specified queue
////////////////////////////////////////////////////////////////////////////////

LocalCallbackTask::LocalCallbackTask(LocalTaskQueue* queue,
                                     std::function<void()> cb)
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
/// @brief dispatch this task to the underlying io_service
////////////////////////////////////////////////////////////////////////////////

void LocalCallbackTask::dispatch() {
  auto self = shared_from_this();
  _queue->ioService()->post([self, this]() { run(); });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a queue using the specified io_service
////////////////////////////////////////////////////////////////////////////////

LocalTaskQueue::LocalTaskQueue(boost::asio::io_service* ioService)
    : _ioService(ioService),
      _queue(),
      _callbackQueue(),
      _condition(),
      _mutex(),
      _missing(0),
      _status(TRI_ERROR_NO_ERROR) {
  TRI_ASSERT(_ioService != nullptr);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief exposes underlying io_service
//////////////////////////////////////////////////////////////////////////////

boost::asio::io_service* LocalTaskQueue::ioService() { return _ioService; }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the queue.
////////////////////////////////////////////////////////////////////////////////

LocalTaskQueue::~LocalTaskQueue() {}

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
