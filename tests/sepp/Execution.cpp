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

#include "Execution.h"

#include <iostream>
#include <filesystem>
#include <memory>

#include "Workload.h"

namespace {
std::size_t getFolderSize(std::string_view path) {
  return std::accumulate(std::filesystem::recursive_directory_iterator(path),
                         std::filesystem::recursive_directory_iterator(), 0ull,
                         [](auto size, auto& path) {
                           return std::filesystem::is_directory(path)
                                      ? size
                                      : size + std::filesystem::file_size(path);
                         });
}
}  // namespace

namespace arangodb::sepp {

Execution::Execution(std::uint32_t round, Options const& options,
                     std::shared_ptr<Workload> workload)
    : _state(ExecutionState::starting),
      _round(round),
      _options(options),
      _workload(std::move(workload)) {}

Execution::~Execution() {
  _state.store(ExecutionState::stopped);
  for (auto& thread : _threads) {
    if (thread->_thread.joinable()) {
      thread->_thread.join();
    }
  }
}

void Execution::createThreads(Server& server) {
  // TODO - allow configuration of different thread types
  std::uint32_t cnt = _options.threads;

  _threads.reserve(cnt);
  for (std::uint32_t i = 0; i < cnt; ++i) {
    auto id = (_round << threadIdBits) | cnt;
    auto thread = _workload->createThread(id, *this, server);
    _threads.push_back(std::move(thread));
    _threads.back()->setup();
  }
}

ExecutionState Execution::state(std::memory_order order) const {
  return _state.load(order);
}

RoundReport Execution::run() {
  _state.store(ExecutionState::preparing);

  waitUntilAllThreadsAre(ThreadState::running);

  _state.store(ExecutionState::initializing);

  waitUntilAllThreadsAre(ThreadState::ready);

  _state.store(ExecutionState::running);

  auto start = std::chrono::high_resolution_clock::now();

  std::this_thread::sleep_for(std::chrono::milliseconds(_options.runtime));

  _state.store(ExecutionState::stopped);

  waitUntilAllThreadsAre(ThreadState::finished);

  std::chrono::duration<double, std::milli> runtime =
      std::chrono::high_resolution_clock::now() - start;
  return buildReport(runtime.count());
}

RoundReport Execution::buildReport(double runtime) {
  for (auto& thread : _threads) {
    thread->_thread.join();
  }

  std::vector<ThreadReport> threadReports;
  threadReports.reserve(_threads.size());
  for (auto& thread : _threads) {
    threadReports.push_back(thread->report());
  }
  return {.threads = threadReports,
          .runtime = runtime,
          .databaseSize = getFolderSize(_options.databaseDirectory)};
}

void Execution::waitUntilAllThreadsAre(ThreadState state) {
  for (auto& thread : _threads) {
    waitUntilThreadStateIs(*thread, state);
  }
}

void Execution::waitUntilThreadStateIs(ExecutionThread const& thread,
                                       ThreadState expected) {
  auto state = thread._state.load(std::memory_order_relaxed);
  while (state != expected) {
    if (state == ThreadState::finished) {
      throw std::runtime_error("worker thread finished prematurely");
    }
    state = thread._state.load(std::memory_order_relaxed);
  }
}

ExecutionThread::ExecutionThread(std::uint32_t id, Execution const& exec,
                                 Server& server)
    : _server(server),
      _execution(exec),
      _id(id),
      _thread(&ExecutionThread::threadFunc, this) {}

void ExecutionThread::threadFunc() {
  waitUntilAllThreadsAreStarted();
  try {
    doRun();
  } catch (std::exception& e) {
    std::cerr << "Thread " << std::this_thread::get_id()
              << " failed: " << e.what() << std::endl;
  }
  _state.store(ThreadState::finished);
}

void ExecutionThread::doRun() {
  if (_execution.state(std::memory_order_relaxed) == ExecutionState::stopped) {
    return;
  }

  _state.store(ThreadState::running);

  waitUntilInitialization();

  initialize(_execution.numThreads());

  _state.store(ThreadState::ready);

  waitUntilBenchmarkStarts();

  auto start = std::chrono::high_resolution_clock::now();

  while (_execution.state() == ExecutionState::running) {
    run();
  }

  _runtime = std::chrono::high_resolution_clock::now() - start;
}

void ExecutionThread::waitUntilAllThreadsAreStarted() {
  while (_execution.state(std::memory_order_acquire) ==
         ExecutionState::starting) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

void ExecutionThread::waitUntilInitialization() {
  while (_execution.state() == ExecutionState::preparing) {
    ;
  }
}

void ExecutionThread::waitUntilBenchmarkStarts() {
  while (_execution.state() == ExecutionState::initializing) {
    ;
  }
}

}  // namespace arangodb::sepp