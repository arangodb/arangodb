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

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/application-exit.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include "V8/v8-deadline.h"
#include "V8/V8SecurityFeature.h"

#include "ProcessMonitoringFeature.h"

using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb;

namespace arangodb {

void ProcessMonitoringFeature::addMonitorPID(ExternalId const& pid) {
  MUTEX_LOCKER(mutexLocker, _MonitoredExternalProcessesLock);
  _monitoredProcesses.push_back(pid);
}

void ProcessMonitoringFeature::removeMonitorPID(ExternalId const& pid) {
  {
    MUTEX_LOCKER(mutexLocker, _MonitoredExternalProcessesLock);
    for (auto it = _monitoredProcesses.begin(); it != _monitoredProcesses.end();
         ++it) {
      if (it->_pid == pid._pid) {
        _monitoredProcesses.erase(it);
        break;
      }
    }
  }
  // make sure its really not monitored anymore once we exit:
  std::this_thread::sleep_for(std::chrono::milliseconds(110));
}

std::optional<ExternalProcessStatus>
ProcessMonitoringFeature::getHistoricStatus(TRI_pid_t pid) {
  MUTEX_LOCKER(mutexLocker, _MonitoredExternalProcessesLock);
  auto it = _ExitedExternalProcessStatus.find(pid);
  if (it == _ExitedExternalProcessStatus.end()) {
    return std::nullopt;
  }
  return std::optional<ExternalProcessStatus>{it->second};
}

ProcessMonitoringFeature::ProcessMonitoringFeature(Server& server)
    : ArangoshFeature{server, *this} {
  startsAfter<V8SecurityFeature>();
  _monitoredProcesses.reserve(10);
}

ProcessMonitoringFeature::~ProcessMonitoringFeature() = default;

void ProcessMonitoringFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {
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
  std::vector<ExternalId> mp;
  mp.reserve(10);
  while (!isStopping()) {
    try {
      {
        MUTEX_LOCKER(mutexLocker,
                     _processMonitorFeature._MonitoredExternalProcessesLock);
        mp = _processMonitorFeature._monitoredProcesses;
      }
      for (auto const& pid : mp) {
        auto status = TRI_CheckExternalProcess(pid, false, 0);
        if ((status._status == TRI_EXT_TERMINATED) ||
            (status._status == TRI_EXT_ABORTED) ||
            (status._status == TRI_EXT_NOT_FOUND)) {
          // Its dead and gone - good
          _processMonitorFeature.removeMonitorPID(pid);
          {
            MUTEX_LOCKER(
                mutexLocker,
                _processMonitorFeature._MonitoredExternalProcessesLock);
            _processMonitorFeature._ExitedExternalProcessStatus[pid._pid] =
                status;
          }
          triggerV8DeadlineNow(false);
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } catch (std::exception const& ex) {
      LOG_TOPIC("e78b9", ERR, Logger::SYSCALL)
          << "process monitoring thread caught exception: " << ex.what();
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
