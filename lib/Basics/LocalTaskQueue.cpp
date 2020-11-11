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
    : _queue(queue), _dispatched(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief notify the queue that the task is completely finished
////////////////////////////////////////////////////////////////////////////////
LocalTask::~LocalTask() {
  if (_dispatched) {
    _queue->join();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatch this task to the scheduler
////////////////////////////////////////////////////////////////////////////////

void LocalTask::dispatch() {
  // only called once by _queue, while _queue->_mutex is held
  _dispatched = true;
  bool queued = _queue->post([self = shared_from_this(), this]() {
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
  if (!queued && _queue->_status.ok()) {
    _queue->_status.reset(TRI_ERROR_QUEUE_FULL, "could not post task");
  }
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
  ++_started;
}

void LocalTaskQueue::stopTask() {
  --_started;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief enqueue a task to be run
//////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::enqueue(std::shared_ptr<LocalTask> task) {
  std::unique_lock<std::mutex> guard(_mutex);
  _queue.push(task);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief post a function to the scheduler. Should only be used internally
/// by task dispatch.
//////////////////////////////////////////////////////////////////////////////

bool LocalTaskQueue::post(std::function<bool()> fn) { return _poster(fn); }

////////////////////////////////////////////////////////////////////////////////
/// @brief join a single task. reduces the number of waiting tasks and wakes
/// up the queue's dispatchAndWait() routine
////////////////////////////////////////////////////////////////////////////////

void LocalTaskQueue::join() {
  {
    std::unique_lock<std::mutex> guard(_mutex);
    TRI_ASSERT(_dispatched > 0);
    --_dispatched;
  }
  _condition.notify_one();
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
      std::unique_lock<std::mutex> guard(_mutex);

      // dispatch all newly queued tasks
      if (_status.ok()) {
        while (_dispatched < _concurrency && !_queue.empty()) {
          auto task = _queue.front();
          task->dispatch();
          _queue.pop();
          ++_dispatched;
        }
      }

      if (_dispatched == 0) {
        break;
      }

      if (_dispatched > 0 && _server.isStopping() && _started.load() == 0) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      _condition.wait_for(guard, std::chrono::milliseconds(10));
    }
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
