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

#include <chrono>
#include <thread>

#include "Basics/application-exit.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include "V8/v8-deadline.h"
#include "ApplicationFeatures/V8SecurityFeature.h"

#include "ProcessMonitoringFeature.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

// Some random time
static constexpr auto kTimeoutMs = std::chrono::milliseconds(100);

void ProcessMonitoringFeature::addMonitorPID(ExternalId const& pid) {
  std::lock_guard guard{_monitoredExternalProcessesLock};
  _monitoredProcesses.push_back(pid);
}

void ProcessMonitoringFeature::removeMonitorPIDNoLock(ExternalId const& pid) {
  auto it = std::find_if(_monitoredProcesses.begin(), _monitoredProcesses.end(),
                         [&](auto const& e) { return e._pid == pid._pid; });
  if (it != _monitoredProcesses.end()) {
    std::swap(*it, _monitoredProcesses.back());
    _monitoredProcesses.pop_back();
  }
}

void ProcessMonitoringFeature::moveMonitoringPIDToAttic(
    ExternalId const& pid, ExternalProcessStatus const& exitStatus) {
  std::lock_guard lock{_monitoredExternalProcessesLock};
  removeMonitorPIDNoLock(pid);
  _exitedExternalProcessStatus[pid._pid] = exitStatus;
}

void ProcessMonitoringFeature::removeMonitorPID(ExternalId const& pid) {
  std::unique_lock lock{_monitoredExternalProcessesLock};
  removeMonitorPIDNoLock(pid);
  auto const counter = _counter.load();
  lock.unlock();
  while (counter + 1 > _counter.load()) {
    std::this_thread::sleep_for(kTimeoutMs);
  }
}

std::optional<ExternalProcessStatus>
ProcessMonitoringFeature::getHistoricStatus(TRI_pid_t pid) {
  std::lock_guard guard{_monitoredExternalProcessesLock};
  auto it = _exitedExternalProcessStatus.find(pid);
  if (it == _exitedExternalProcessStatus.end()) {
    return std::nullopt;
  }
  return std::optional<ExternalProcessStatus>{it->second};
}

ProcessMonitoringFeature::ProcessMonitoringFeature(Server& server)
  : ArangoshFeature(server, *this) {
  startsAfter<V8SecurityFeature>();
  _monitoredProcesses.reserve(10);
}

ProcessMonitoringFeature::~ProcessMonitoringFeature() = default;

void ProcessMonitoringFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {
  _enabled =
      server().getFeature<V8SecurityFeature>().isAllowedToControlProcesses();
}

void ProcessMonitoringFeature::start() {
  if (_enabled) {
    _monitorThread = std::make_unique<ProcessMonitorThread>(server(), *this);
    if (!_monitorThread->start()) {
      LOG_TOPIC("33333", FATAL, Logger::SYSCALL)
          << "failed to launch monitoring background thread";
      FATAL_ERROR_EXIT();
    }
  }
}
void ProcessMonitoringFeature::beginShutdown() {
  if (_enabled) {
    _monitorThread->shutdown();
  }
}

void ProcessMonitoringFeature::stop() {
  if (_enabled) {
    _monitorThread->shutdown();
  }
}

void ProcessMonitorThread::run() {  // override
  while (!isStopping()) {
    try {
      _processMonitorFeature.visitMonitoring([&](auto const& pid) {
        auto status = TRI_CheckExternalProcess(pid, false, 0);
        if ((status._status == TRI_EXT_TERMINATED) ||
            (status._status == TRI_EXT_ABORTED) ||
            (status._status == TRI_EXT_NOT_FOUND)) {
          // Its dead and gone - good
          _processMonitorFeature.moveMonitoringPIDToAttic(pid, status);
          triggerV8DeadlineNow(false);
        }
      });
      std::this_thread::sleep_for(kTimeoutMs);
    } catch (std::exception const& e) {
      LOG_TOPIC("e78b9", ERR, Logger::SYSCALL)
          << "process monitoring thread caught exception: " << e.what();
    } catch (...) {
      LOG_TOPIC("7269b", ERR, Logger::SYSCALL)
          << "process monitoring thread caught unknown exception";
    }
  }
}

std::optional<ExternalProcessStatus> getHistoricStatus(
    TRI_pid_t pid, application_features::ApplicationServer& server) {
  return static_cast<ArangoshServer&>(server)
    .getFeature<ProcessMonitoringFeature>()
    .getHistoricStatus(pid);
}

}  // namespace arangodb
