////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace arangodb::tests {

/**
 * @brief The constructor of WorkerThread starts a thread, which immediately
 * starts waiting on a condition variable. The execute() method takes a
 * callback, which is passed to the waiting thread and is then executed by it,
 * while execute() returns.
 */
struct WorkerThread : std::enable_shared_from_this<WorkerThread> {
  WorkerThread() = default;

  void run();

  void execute(std::function<void()> callback);

  void stop();

 private:
  std::thread _thread;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::function<void()> _callback;
  bool _stopped = false;
};

void operator<<(WorkerThread& workerThread, std::function<void()> callback);

}  // namespace arangodb::tests
