////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "utils/process_utils.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <process.h>  // _getpid
#include <tlhelp32.h>
#else
#include <unistd.h>  // getpid

#include <csignal>
#endif

#include <cstdlib>
#include <numeric>

namespace irs {

bool is_running(pid_t pid) {
#ifdef _WIN32
  HANDLE ps = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);

  PROCESSENTRY32 pe = {0};
  pe.dwSize = sizeof(pe);
  if (Process32First(ps, &pe)) {
    do {
      // pe.szExeFile can also be useful
      if (pe.th32ProcessID == DWORD(pid)) {
        CloseHandle(ps);
        return true;
      }
    } while (Process32Next(ps, &pe));
  }
  CloseHandle(ps);
  return false;
#else
  return 0 == kill(pid, 0);
#endif
}

pid_t get_pid() {
#ifdef _WIN32
  return _getpid();
#else
  return getpid();
#endif
}

bool is_valid_pid(const char* buf) {
  const auto pid = strtol(buf, nullptr, 10);
  return 0 != pid && pid <= std::numeric_limits<pid_t>::max() &&
         pid >= std::numeric_limits<pid_t>::min() &&
         is_running(static_cast<pid_t>(pid));
}

}  // namespace irs
