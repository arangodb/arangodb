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

#include "ExecutionThread.h"
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

struct Execution {
  Execution(Options const& options, std::shared_ptr<Workload> workload);
  ~Execution();
  void createThreads(Server& server);
  Report run();
  [[nodiscard]] ExecutionState state(
      std::memory_order order = std::memory_order_relaxed) const;
  [[nodiscard]] std::uint32_t numThreads() const {
    return static_cast<std::uint32_t>(_threads.size());
  }

  void signalFinishedThread() noexcept;

 private:
  void waitUntilAllThreadsAre(ThreadState state);

  void waitUntilRunning(ExecutionThread const& thread) const;
  void waitUntilFinished(ExecutionThread const& thread) const;
  static void waitUntilThreadStateIs(ExecutionThread const& thread,
                                     ThreadState expected);

  Report buildReport(double runtime);

  std::atomic<ExecutionState> _state;
  std::atomic<std::uint32_t> _activeThreads{0};
  Options const& _options;
  std::shared_ptr<Workload> _workload;
  std::vector<std::unique_ptr<ExecutionThread>> _threads;
};

}  // namespace arangodb::sepp