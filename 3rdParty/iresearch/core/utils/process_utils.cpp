//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "shared.hpp"
#include "process_utils.hpp"

#ifdef _WIN32
  #include <Windows.h>
  #include <process.h> // _getpid
  #include <tlhelp32.h>
#else
  #include <sys/types.h>
  #include <unistd.h>
  #include <signal.h>
#endif // _WIN32

NS_ROOT

bool is_running(pid_t pid) {
#ifdef _WIN32
  HANDLE ps = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);   

  PROCESSENTRY32 pe = { 0 };  
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
#endif // _WIN32
}

pid_t get_pid() {
#ifdef _WIN32
  return _getpid();
#else
  return getpid();
#endif // _WIN32
}

bool is_valid_pid(const char* buf) {
  const auto pid = strtol(buf, nullptr, 10);
  return 0 != pid
      && pid <= integer_traits<pid_t>::const_max
      && pid >= integer_traits<pid_t>::const_min
      && is_running(static_cast<pid_t>(pid));
}

NS_END
