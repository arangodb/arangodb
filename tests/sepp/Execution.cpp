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
                         [](auto size, auto const& path) {
                           return std::filesystem::is_directory(path)
                                      ? size
                                      : size + std::filesystem::file_size(path);
                         });
}
}  // namespace

namespace arangodb::sepp {

Execution::Execution(Options const& options, std::shared_ptr<Workload> workload)
    : _state(ExecutionState::kStarting),
      _options(options),
      _workload(std::move(workload)) {}

Execution::~Execution() {
  _state.store(ExecutionState::kStopped);
  for (auto& thread : _threads) {
    if (thread->_thread.joinable()) {
      thread->_thread.join();
    }
  }
}

void Execution::createThreads(Server& server) {
  _threads = _workload->createThreads(*this, server);
}

ExecutionState Execution::state(std::memory_order order) const noexcept {
  return _state.load(order);
}

void Execution::signalFinishedThread() noexcept { _activeThreads.fetch_sub(1); }

Report Execution::run() {
  _activeThreads.store(_threads.size());
  _state.store(ExecutionState::kPreparing);

  waitUntilAllThreadsAre(ThreadState::kRunning);

  _state.store(ExecutionState::kInitializing);

  waitUntilAllThreadsAre(ThreadState::kReady);

  _state.store(ExecutionState::kRunning);

  auto start = std::chrono::high_resolution_clock::now();

  std::uint32_t sleeptimePerRound = 100;
  auto rounds = [&]() -> std::uint32_t {
    auto stoppingCriterion = _workload->stoppingCriterion();
    if (std::holds_alternative<StoppingCriterion::Runtime>(stoppingCriterion)) {
      return std::get<StoppingCriterion::Runtime>(stoppingCriterion).ms /
             sleeptimePerRound;
    } else {
      // for stopping criteria based on number of operations we use an artifical
      // time limit of one hour
      return 1000 * 60 * 60 / 100;
    }
  }();

  for (std::uint32_t r = 0;
       r < rounds && _activeThreads.load(std::memory_order_relaxed) > 0; ++r) {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleeptimePerRound));
    // TODO - record samples (memory usage, rocksdb stats, ...)
  }

  _state.store(ExecutionState::kStopped);

  waitUntilAllThreadsAre(ThreadState::kFinished);

  std::chrono::duration<double, std::milli> runtime =
      std::chrono::high_resolution_clock::now() - start;
  return buildReport(runtime.count());
}

Report Execution::buildReport(double runtime) {
  for (auto& thread : _threads) {
    if (thread->_thread.joinable()) {
      thread->_thread.join();
    }
  }

  std::vector<ThreadReport> threadReports;
  threadReports.reserve(_threads.size());
  for (auto& thread : _threads) {
    threadReports.push_back(thread->report());
  }
  return {.threads = std::move(threadReports),
          .runtime = runtime,
          .databaseSize = getFolderSize(_options.databaseDirectory)};
}

void Execution::waitUntilAllThreadsAre(ThreadState state) const {
  for (auto const& thread : _threads) {
    waitUntilThreadStateIs(*thread, state);
  }
}

void Execution::waitUntilThreadStateIs(ExecutionThread const& thread,
                                       ThreadState expected) const {
  auto state = thread._state.load(std::memory_order_relaxed);
  while (state != expected) {
    if (state == ThreadState::kFinished) {
      throw std::runtime_error("worker thread finished prematurely");
    }
    state = thread._state.load(std::memory_order_relaxed);
  }
}

}  // namespace arangodb::sepp