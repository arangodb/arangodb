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
#include <thread>

#include "Inspection/VPack.h"

#include "Workload.h"

namespace arangodb::sepp {

Execution::Execution(Options const& options, std::shared_ptr<Workload> workload)
    : _state(ExecutionState::kStarting),
      _options(options),
      _workload(std::move(workload)) {}

Execution::~Execution() {
  _state.store(ExecutionState::kStopped);
  joinThreads();
}

void Execution::createThreads(Server& server) {
  _threads = _workload->createThreads(*this, server);
}

void Execution::joinThreads() {
  for (auto& thread : _threads) {
    if (thread->_thread.joinable()) {
      thread->_thread.join();
    }
  }
}

ExecutionState Execution::state(std::memory_order order) const noexcept {
  return _state.load(order);
}

void Execution::signalStartingThread() noexcept { _activeThreads.fetch_add(1); }
void Execution::signalFinishedThread() noexcept { _activeThreads.fetch_sub(1); }

void Execution::advanceStatusIfNotStopped(ExecutionState state) noexcept {
  ExecutionState value = _state.load();
  // never move the status backwards
  while (value != ExecutionState::kStopped &&
         !_state.compare_exchange_strong(value, state)) {
    std::this_thread::yield();
  }
}

void Execution::stop() noexcept { _state.store(ExecutionState::kStopped); }

bool Execution::stopped() const noexcept {
  return _state.load(std::memory_order_relaxed) == ExecutionState::kStopped;
}

Report Execution::run() {
  advanceStatusIfNotStopped(ExecutionState::kPreparing);

  waitUntilAllThreadsAre(ThreadState::kRunning);

  advanceStatusIfNotStopped(ExecutionState::kInitializing);

  waitUntilAllThreadsAre(ThreadState::kReady);

  advanceStatusIfNotStopped(ExecutionState::kRunning);

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

  // set our execution state to stopped
  stop();

  waitUntilAllThreadsAre(ThreadState::kFinished);

  std::chrono::duration<double, std::milli> runtime =
      std::chrono::high_resolution_clock::now() - start;

  joinThreads();

  for (auto const& thread : _threads) {
    if (thread->failed()) {
      throw std::runtime_error("aborted due to runtime failure");
    }
  }

  return buildReport(runtime.count());
}

Report Execution::buildReport(double runtime) {
  std::vector<ThreadReport> threadReports;
  threadReports.reserve(_threads.size());
  for (auto& thread : _threads) {
    threadReports.push_back(thread->report());
  }

  Report report{.timestamp = {},
                .config = {},
                .configBuilder = {},
                .threads = std::move(threadReports),
                .runtime = runtime,
                .databaseSize = {}};
  velocypack::serialize(report.configBuilder, _options);
  report.config = report.configBuilder.slice();
  return report;
}

void Execution::waitUntilAllThreadsAre(ThreadState state) const {
  for (auto const& thread : _threads) {
    waitUntilThreadStateIs(*thread, state);
  }
}

void Execution::waitUntilThreadStateIs(ExecutionThread const& thread,
                                       ThreadState expected) {
  auto state = thread._state.load(std::memory_order_relaxed);
  while (state != expected) {
    if (state == ThreadState::kFinished) {
      throw std::runtime_error("worker thread finished prematurely");
    }
    std::this_thread::yield();
    state = thread._state.load(std::memory_order_relaxed);
  }
}

}  // namespace arangodb::sepp
