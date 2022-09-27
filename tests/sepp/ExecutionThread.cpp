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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionThread.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include "Execution.h"

namespace arangodb::sepp {

ExecutionThread::ExecutionThread(Execution& exec, Server& server)
    : _server(server),
      _execution(exec),
      _thread(&ExecutionThread::threadFunc, this) {}

void ExecutionThread::threadFunc() {
  _execution.signalStartingThread();
  waitUntilAllThreadsAreStarted();

  try {
    doRun();
  } catch (std::exception const& e) {
    std::cerr << "Thread " << std::this_thread::get_id()
              << " failed: " << e.what() << std::endl;
    // signal the executor to stop
    _execution.stop();
    // note our own failure
    _failed = true;
  }

  _state.store(ThreadState::kFinished);
  _execution.signalFinishedThread();
}

void ExecutionThread::doRun() {
  if (_execution.state(std::memory_order_relaxed) == ExecutionState::kStopped) {
    return;
  }

  _state.store(ThreadState::kRunning);

  waitUntilInitialization();

  initialize(_execution.numThreads());

  _state.store(ThreadState::kReady);

  waitUntilBenchmarkStarts();

  auto start = std::chrono::high_resolution_clock::now();

  while (_execution.state() == ExecutionState::kRunning && !shouldStop()) {
    run();
  }

  _runtime = std::chrono::high_resolution_clock::now() - start;
}

void ExecutionThread::waitUntilAllThreadsAreStarted() const {
  while (_execution.state(std::memory_order_acquire) ==
         ExecutionState::kStarting) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

void ExecutionThread::waitUntilInitialization() const {
  while (_execution.state() == ExecutionState::kPreparing) {
    std::this_thread::yield();
  }
}

void ExecutionThread::waitUntilBenchmarkStarts() const {
  while (_execution.state() == ExecutionState::kInitializing) {
    std::this_thread::yield();
  }
}

}  // namespace arangodb::sepp
