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

#pragma once

#include <atomic>
#include <chrono>
#include <random>
#include <thread>

#include "Options.h"
#include "Report.h"

namespace arangodb::sepp {

struct Workload;
struct Server;

enum class ExecutionState {
  starting,
  preparing,
  initializing,
  running,
  stopped
};

enum class ThreadState { starting, running, ready, finished };

struct Execution;

struct ExecutionThread {
  ExecutionThread(std::uint32_t id, Execution const& exec, Server& server);
  virtual ~ExecutionThread() = default;
  virtual void setup() {}
  virtual void run() = 0;
  virtual void initialize(std::uint32_t /*numThreads*/) {}
  [[nodiscard]] virtual ThreadReport report() const { return {{}, 0}; }
  [[nodiscard]] std::uint32_t id() const { return _id; }

 protected:
  Server& _server;

 private:
  Execution const& _execution;
  std::uint32_t const _id;
  std::atomic<ThreadState> _state{ThreadState::starting};
  std::mt19937_64 _randomizer{};
  std::thread _thread{};
  std::chrono::duration<double, std::milli> _runtime{};

  friend struct Execution;
  void threadFunc();
  void doRun();
  void waitUntilAllThreadsAreStarted();
  void waitUntilInitialization();
  void waitUntilBenchmarkStarts();
};

struct Execution {
  static constexpr std::uint32_t threadIdBits = 16;
  static constexpr std::uint32_t threadIdMask = (1 << threadIdBits) - 1;

  Execution(std::uint32_t round, Options const& options,
            std::shared_ptr<Workload> workload);
  ~Execution();
  void createThreads(Server& server);
  RoundReport run();
  [[nodiscard]] ExecutionState state(
      std::memory_order order = std::memory_order_relaxed) const;
  [[nodiscard]] std::uint32_t numThreads() const {
    return static_cast<std::uint32_t>(_threads.size());
  }

 private:
  void waitUntilAllThreadsAre(ThreadState state);

  void waitUntilRunning(ExecutionThread const& thread) const;
  void waitUntilFinished(ExecutionThread const& thread) const;
  static void waitUntilThreadStateIs(ExecutionThread const& thread,
                                     ThreadState expected);

  RoundReport buildReport(double runtime);

  std::atomic<ExecutionState> _state;
  std::uint32_t _round;
  Options const& _options;
  std::shared_ptr<Workload> _workload;
  std::vector<std::unique_ptr<ExecutionThread>> _threads;
};

}  // namespace arangodb::sepp