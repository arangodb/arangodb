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

#include "Basics/Thread.h"
#include "Basics/process-utils.h"
#include "Shell/arangosh.h"

namespace arangodb {
class ProcessMonitoringFeature;

class ProcessMonitorThread : public arangodb::Thread {
  friend ProcessMonitoringFeature;

 public:
  ProcessMonitorThread(application_features::ApplicationServer& server,
                       ProcessMonitoringFeature& processMonitorFeature)
      : Thread(server, "ProcessMonitor"),
        _processMonitorFeature(processMonitorFeature) {}
  ~ProcessMonitorThread() { shutdown(); }

 protected:
  ProcessMonitoringFeature& _processMonitorFeature;
  void run() override;
};

class ProcessMonitoringFeature final : public ArangoshFeature {
  friend ProcessMonitorThread;

 public:
  explicit ProcessMonitoringFeature(Server& server);
  ~ProcessMonitoringFeature();
  static constexpr std::string_view name() noexcept { return "ProcessMonitor"; }
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief enlist external process to become monitored
  ////////////////////////////////////////////////////////////////////////////////

  void addMonitorPID(ExternalId const& pid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief enlist external process to become monitored
  ////////////////////////////////////////////////////////////////////////////////

  void removeMonitorPID(ExternalId const& pid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get info about maybe exited processes
  ////////////////////////////////////////////////////////////////////////////////

  std::optional<ExternalProcessStatus> getHistoricStatus(TRI_pid_t pid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief external process stati of processes exited while being monitored
  ////////////////////////////////////////////////////////////////////////////////

 protected:
  arangodb::Mutex _MonitoredExternalProcessesLock;
  std::map<TRI_pid_t, ExternalProcessStatus> _ExitedExternalProcessStatus;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief external process being monitored
  ////////////////////////////////////////////////////////////////////////////////

  std::vector<ExternalId> _monitoredProcesses;
  std::unique_ptr<ProcessMonitorThread> _monitorThread;

  bool _enabled;
};

}  // namespace arangodb
