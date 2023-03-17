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
#include "V8/processMonitor.h"
#include "V8/v8-deadline.h"


using namespace arangodb;

namespace {

/// @brief lock for protected access to vector ExternalProcesses
static arangodb::Mutex ExitedExternalProcessesLock;

}
namespace arangodb {

/// @brief if monitored & exited we keep them here:
std::map<TRI_pid_t, ExternalProcessStatus> ExitedExternalProcessStatus;

std::vector<ExternalId> monitoredProcesses;

void addMonitorPID(ExternalId& pid) {
  MUTEX_LOCKER(mutexLocker, ExitedExternalProcessesLock);
  monitoredProcesses.push_back(pid);
}

void removeMonitorPID(ExternalId const& pid) {
  MUTEX_LOCKER(mutexLocker, ExitedExternalProcessesLock);
  for (auto it = monitoredProcesses.begin(); it != monitoredProcesses.end();
       ++it) {
    if (it->_pid == pid._pid) {
      monitoredProcesses.erase(it);
      break;
    }
  }
}


ExternalProcessStatus const *getHistoricStatus(TRI_pid_t pid) {
  auto it = ExitedExternalProcessStatus.find(pid);
  if (it == ExitedExternalProcessStatus.end()) {
    return nullptr;
  }
  return &(it->second);
}

class ProcessMonitorThread : public arangodb::Thread {
 public:
  ProcessMonitorThread(application_features::ApplicationServer& server)
    : Thread(server, "ProcessMonitorThread") {}
  ~ProcessMonitorThread() { shutdown(); }
 protected:
  void run() override {
    while (!isStopping()) {
      // TODO lock
      std::vector<ExternalId> mp;
      {
        MUTEX_LOCKER(mutexLocker, ExitedExternalProcessesLock);
        mp = monitoredProcesses;
      }
      for (auto const &pid: mp) {
        auto status = TRI_CheckExternalProcess(pid, false, 0);
        if ((status._status == TRI_EXT_TERMINATED) ||
            (status._status == TRI_EXT_ABORTED) ||
            (status._status == TRI_EXT_NOT_FOUND)) {
          // Its dead and gone - good
          removeMonitorPID(pid);
          ExitedExternalProcessStatus[pid._pid] = status;
          triggerV8DeadlineNow();
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
};

ProcessMonitorThread *mt = nullptr;
void launchMonitorThread(application_features::ApplicationServer& server) {
  if (mt == nullptr) {
    mt = new ProcessMonitorThread(server);
    mt->start();
  }
}
void terminateMonitorThread(application_features::ApplicationServer& server) {
  mt->shutdown();
}
}
