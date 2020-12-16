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

#include <chrono>

#include "LocalTaskQueue.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/debugging.h"

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task tied to the specified queue
////////////////////////////////////////////////////////////////////////////////

LocalTask::LocalTask(std::shared_ptr<LocalTaskQueue> const& queue)
    : _queue(queue) {}

LocalTask::~LocalTask() = default;

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatch this task to the scheduler
////////////////////////////////////////////////////////////////////////////////

bool LocalTask::dispatch() {
  // only called once by _queue, while _queue->_mutex is held
  return _queue->post([self = shared_from_this(), this]() {
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
  
LambdaTask::LambdaTask(std::shared_ptr<LocalTaskQueue> const& queue,
                       std::function<Result()>&& fn)
    : LocalTask(queue), _fn(std::move(fn)) {}

void LambdaTask::run() {
  Result res = _fn();
  if (res.fail()) {
    _queue->setStatus(res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a queue
////////////////////////////////////////////////////////////////////////////////

LocalTaskQueue::LocalTaskQueue(application_features::ApplicationServer& server, PostFn poster)
    : _server(server),
      _poster(poster),
      _queue(),
      _condition(),
      _mutex(),
      _dispatched(0),
      _concurrency(std::numeric_limits<std::size_t>::max()),
      _started(0),
      _status() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the queue.
////////////////////////////////////////////////////////////////////////////////

LocalTaskQueue::~LocalTaskQueue() = default;

void LocalTaskQueue::startTask() {
  _started.fetch_add(1, std::memory_order_relaxed);
}

void LocalTaskQueue::stopTask() {
  std::size_t old = _started.fetch_sub(1, std::memory_order_relaxed);
  TRI_ASSERT(old > 0);
  old = _dispatched.fetch_sub(1, std::memory_order_release);
  TRI_ASSERT(old > 0);

  // Notify the dispatching thread that new tasks can be scheduled.
  // Note: we are deliberately not using a mutex here to avoid contention, but that means that
  // the notification can potentially be missed. However, this should only happen very rarely
  // and the dispatching thread is only waiting for a limited time.
  _condition.notify_one();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief enqueue a task to be run
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::enqueue(std::shared_ptr<LocalTask> task) {
  std::unique_lock<std::mutex> guard(_mutex);
  _queue.push(std::move(task));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief post a function to the scheduler. Should only be used internally
/// by task dispatch.
//////////////////////////////////////////////////////////////////////////////

bool LocalTaskQueue::post(std::function<bool()>&& fn) { return _poster(std::move(fn)); }

//////////////////////////////////////////////////////////////////////////////
/// @brief dispatch all tasks, including those that are queued while running,
/// and wait for all tasks to join; then dispatch all callback tasks and wait
/// for them to join
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::dispatchAndWait() {
  // regular task loop
  {
    std::unique_lock<std::mutex> guard(_mutex);
    if (_queue.empty()) {
      return;
    }
  }

  while (true) {
    std::unique_lock<std::mutex> guard(_mutex);

    // dispatch all newly queued tasks
    if (_status.ok()) {
      while (_dispatched.load(std::memory_order_acquire) < _concurrency && !_queue.empty()) {
        // all your task are belong to us
        auto task = std::move(_queue.front());
        _queue.pop();

        // increase _dispatched by one, now. if dispatching fails, we will count it
        // down again
        _dispatched.fetch_add(1, std::memory_order_release);
        bool dispatched = false;
        try {
          dispatched = task->dispatch();
        } catch (basics::Exception const& ex) {
          TRI_ASSERT(!dispatched);
          _status.reset({ex.code(), ex.what()});
        } catch (std::exception const& ex) {
          TRI_ASSERT(!dispatched);
          _status.reset({TRI_ERROR_INTERNAL, ex.what()});
        } catch (...) {
          TRI_ASSERT(!dispatched);
          _status.reset({TRI_ERROR_QUEUE_FULL, "could not post task"});
        }

        if (!dispatched) {
          // dispatching the task has failed.
          // count down _dispatched again
          std::size_t old = _dispatched.fetch_sub(1, std::memory_order_release);;
          TRI_ASSERT(old > 0);
          
          if (_status.ok()) {
            // now register an error in the queue
           _status.reset({TRI_ERROR_QUEUE_FULL, "could not post task"});
          }
        }
      }
    }

    if (_server.isStopping() && _started.load() == 0) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    if (_dispatched.load(std::memory_order_acquire) == 0) {
      break;
    }

    // We must only wait for a limited time here, since the notify operation in stopTask
    // does not use a mutex, so there is a (rare) chance that we might miss a notification.
    _condition.wait_for(guard, std::chrono::milliseconds(50));
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief set status of queue
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::setStatus(Result status) {
  std::unique_lock<std::mutex> guard(_mutex);
  _status = status;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return overall status of queue tasks
//////////////////////////////////////////////////////////////////////////////

Result LocalTaskQueue::status() {
  std::unique_lock<std::mutex> guard(_mutex);
  return _status;
}

void LocalTaskQueue::setConcurrency(std::size_t input) {
  std::unique_lock<std::mutex> guard(_mutex);
  if (input > 0) {
    _concurrency = input;
  }
}

}  // namespace basics
}  // namespace arangodb
