////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>
#include <mutex>

#include "Basics/Thread.h"
#include "Basics/process-utils.h"
#include "Shell/arangosh.h"
#include <absl/cleanup/cleanup.h>

namespace arangodb {
class ProcessMonitoringFeature;

class ProcessMonitorThread final : public arangodb::Thread {
 public:
  ProcessMonitorThread(application_features::ApplicationServer& server,
                       ProcessMonitoringFeature& processMonitorFeature)
      : Thread(server, "ProcessMonitor"),
        _processMonitorFeature(processMonitorFeature) {}
  ~ProcessMonitorThread() final { shutdown(); }

 private:
  void run() final;

  ProcessMonitoringFeature& _processMonitorFeature;
};

class ProcessMonitoringFeature final : public ArangoshFeature {
 public:
  explicit ProcessMonitoringFeature(Server& server);
  ~ProcessMonitoringFeature() final;
  static constexpr std::string_view name() noexcept { return "ProcessMonitor"; }
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> /*options*/) final;
  void start() final;
  void beginShutdown() final;
  void stop() final;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get info about maybe exited processes
  ////////////////////////////////////////////////////////////////////////////////

  std::optional<ExternalProcessStatus> getHistoricStatus(TRI_pid_t pid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief external process stati of processes exited while being monitored
  ////////////////////////////////////////////////////////////////////////////////

  void addMonitorPID(ExternalId const& pid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief enlist external process to become monitored
  ////////////////////////////////////////////////////////////////////////////////

  void removeMonitorPID(ExternalId const& pid);

  void moveMonitoringPIDToAttic(ExternalId const& pid,
                                ExternalProcessStatus const& exitStatus);

  template<typename Func>
  void visitMonitoring(Func const& func) {
    std::unique_lock lock{_monitoredExternalProcessesLock};
    // if someone erase from _monitoredProcesses before visitMonitoring,
    // it can stop waiting
    _counter.fetch_add(1);
    auto pids = _monitoredProcesses;
    lock.unlock();
    // if someone erase from _monitoredProcesses after _monitoredProcesses was
    // copied to pids, it want to wait when we stop using pids
    absl::Cleanup increment = [&] { _counter.fetch_add(1); };
    for (auto const& pid : pids) {
      func(pid);
    }
  }

 private:
  void removeMonitorPIDNoLock(ExternalId const& pid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief enlist external process to become monitored
  ////////////////////////////////////////////////////////////////////////////////

  std::atomic_uint64_t _counter{0};
  std::mutex _monitoredExternalProcessesLock;
  std::map<TRI_pid_t, ExternalProcessStatus> _exitedExternalProcessStatus;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief external process being monitored
  ////////////////////////////////////////////////////////////////////////////////

  std::vector<ExternalId> _monitoredProcesses;
  std::unique_ptr<ProcessMonitorThread> _monitorThread;

  bool _enabled;
};

}  // namespace arangodb
