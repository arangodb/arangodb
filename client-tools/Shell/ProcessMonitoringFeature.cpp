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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>

#include "Basics/process-utils.h"
#include "Basics/signals.h"
#include "Basics/system-functions.h"

#if defined(TRI_HAVE_MACOS_MEM_STATS)
#include <sys/sysctl.h>
#endif

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef TRI_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef TRI_HAVE_MACH
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#endif

#ifdef _WIN32
#include <Psapi.h>
#include <TlHelp32.h>
#include "Basics/socket-utils.h"
#include "Basics/win-utils.h"
#endif
#include <fcntl.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/DeadlockDetector.h"
#include "Basics/Thread.h"
#include "V8/v8-deadline.h"
#include "ProcessMonitoringFeature.h"

using namespace arangodb;

namespace arangodb {

void ProcessMonitoringFeature::addMonitorPID(ExternalId& pid) {
  MUTEX_LOCKER(mutexLocker, _MonitoredExternalProcessesLock);
  _monitoredProcesses.push_back(pid);
}

void ProcessMonitoringFeature::removeMonitorPID(ExternalId const& pid) {
  MUTEX_LOCKER(mutexLocker, _MonitoredExternalProcessesLock);
  for (auto it = _monitoredProcesses.begin(); it != _monitoredProcesses.end();
       ++it) {
    if (it->_pid == pid._pid) {
      _monitoredProcesses.erase(it);
      break;
    }
  }
}

std::optional<ExternalProcessStatus> ProcessMonitoringFeature::getHistoricStatus(TRI_pid_t pid) {
  MUTEX_LOCKER(mutexLocker, _MonitoredExternalProcessesLock);
  auto it = _ExitedExternalProcessStatus.find(pid);
  if (it == _ExitedExternalProcessStatus.end()) {
    return std::nullopt;
  }
  return std::optional<ExternalProcessStatus>{it->second};
}

ProcessMonitoringFeature::ProcessMonitoringFeature(Server& server):
  ArangoshFeature(server, *this)
{
  startsAfter<V8SecurityFeature>();
}

void ProcessMonitoringFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  _enabled = server().getFeature<V8SecurityFeature>().isAllowedToControlProcesses();
}

void ProcessMonitoringFeature::start(){
  if (_enabled) {
    _monitorThread = std::make_unique<ProcessMonitorThread>(server, this);
    _monitorThread->start();
  }
}
void ProcessMonitoringFeature::beginShutdown() {
  if (_enabled) {
    _monitorThread->shutdown();
  }
}

void ProcessMonitorThread::run() { // override
  while (!isStopping()) {
    std::vector<ExternalId> mp;
    {
      MUTEX_LOCKER(mutexLocker, _processMonitorFeature->_MonitoredExternalProcessesLock);
      mp = _processMonitorFeature->_monitoredProcesses;
    }
    for (auto const& pid : mp) {
      auto status = TRI_CheckExternalProcess(pid, false, 0);
      if ((status._status == TRI_EXT_TERMINATED) ||
          (status._status == TRI_EXT_ABORTED) ||
          (status._status == TRI_EXT_NOT_FOUND)) {
        // Its dead and gone - good
        _processMonitorFeature->removeMonitorPID(pid);
        {
          MUTEX_LOCKER(mutexLocker, _processMonitorFeature->_MonitoredExternalProcessesLock);
          _processMonitorFeature->_ExitedExternalProcessStatus[pid._pid] = status;
        }
        triggerV8DeadlineNow(false);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

std::optional<ExternalProcessStatus> getHistoricStatus(TRI_pid_t pid, arangodb::application_features::ApplicationServer& server) {
  return server().getFeature<ProcessMonitoringFeature>()->getHistoricStatus(pid);
}

}  // namespace arangodb
