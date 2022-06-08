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

#include "Report.h"

namespace arangodb::sepp {

struct Server;

enum class ThreadState { kStarting, kRunning, kReady, kFinished };

struct Execution;

struct ExecutionThread {
  ExecutionThread(Execution& exec, Server& server);
  virtual ~ExecutionThread() = default;
  virtual void setup() {}
  virtual void run() = 0;
  virtual void initialize(std::uint32_t /*numThreads*/) {}
  virtual bool shouldStop() const noexcept = 0;
  [[nodiscard]] virtual ThreadReport report() const { return {{}, 0}; }

  // not supposed to be accessed while the test is running, but only
  // at the end
  virtual bool failed() const noexcept { return _failed; }
  Execution const& execution() const { return _execution; }

 protected:
  Server& _server;

 private:
  Execution& _execution;
  std::atomic<ThreadState> _state{ThreadState::kStarting};
  std::mt19937_64 _randomizer{};
  std::thread _thread{};
  std::chrono::duration<double, std::milli> _runtime{};
  bool _failed{false};

  friend struct Execution;
  void threadFunc();
  void doRun();
  void waitUntilAllThreadsAreStarted() const;
  void waitUntilInitialization() const;
  void waitUntilBenchmarkStarts() const;
};

}  // namespace arangodb::sepp
