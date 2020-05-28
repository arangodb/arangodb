////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Esteban Lombeyda
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

#include "process-utils.h"
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
#include <unicode/unistr.h>
#include "Basics/socket-utils.h"
#include "Basics/win-utils.h"
#endif
#include <fcntl.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/NumberUtils.h"
#include "Basics/PageSize.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/memory.h"
#include "Basics/operating-system.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;

namespace {

#ifdef TRI_HAVE_LINUX_PROC
/// @brief consumes all whitespace
void skipWhitespace(char const*& p, char const* e) {
  while (p < e && *p == ' ') {
    ++p;
  }
}
/// @brief consumes all non-whitespace
void skipNonWhitespace(char const*& p, char const* e) {
  if (p < e && *p == '(') {
    // special case: if the value starts with a '(', we will skip over all
    // data until we find the closing parenthesis. this is used for the process
    // name at least
    ++p;
    while (p < e && *p != ')') {
      ++p;
    }
    if (p < e && *p == ')') {
      ++p;
    }
  } else {
    // no parenthesis at start, so just skip over whitespace
    while (p < e && *p != ' ') {
      ++p;
    }
  }
}
/// @brief reads over the whitespace at the beginning plus the following data
void skipEntry(char const*& p, char const* e) {
  skipWhitespace(p, e);
  skipNonWhitespace(p, e);
}
/// @brief reads a numeric entry from the buffer
template<typename T>
T readEntry(char const*& p, char const* e) {
  skipWhitespace(p, e);
  char const* s = p;
  skipNonWhitespace(p, e);
  return arangodb::NumberUtils::atoi_unchecked<uint64_t>(s, p);
}
#endif

} // namespace


/// @brief all external processes
std::vector<ExternalProcess*> ExternalProcesses;

/// @brief lock for protected access to vector ExternalProcesses
static arangodb::Mutex ExternalProcessesLock;

ProcessInfo::ProcessInfo()
    : _minorPageFaults(0),
      _majorPageFaults(0),
      _userTime(0),
      _systemTime(0),
      _numberThreads(0),
      _residentSize(0),  // resident set size in number of bytes
      _virtualSize(0),
      _scClkTck(0) {}

ExternalId::ExternalId()
    :
#ifndef _WIN32
      _pid(0),
      _readPipe(-1),
      _writePipe(-1) {
}
#else
      _pid(0),
      _readPipe(INVALID_HANDLE_VALUE),
      _writePipe(INVALID_HANDLE_VALUE) {
}
#endif

ExternalProcess::ExternalProcess()
    : _numberArguments(0),
      _arguments(nullptr),
#ifdef _WIN32
      _process(nullptr),
#endif
      _status(TRI_EXT_NOT_STARTED),
      _exitStatus(0) {
}

ExternalProcess::~ExternalProcess() {
  for (size_t i = 0; i < _numberArguments; i++) {
    if (_arguments[i] != nullptr) {
      TRI_Free(_arguments[i]);
    }
  }
  if (_arguments) {
    TRI_Free(_arguments);
  }

#ifndef _WIN32
  if (_readPipe != -1) {
    close(_readPipe);
  }
  if (_writePipe != -1) {
    close(_writePipe);
  }
#else
  CloseHandle(_process);
  if (_readPipe != INVALID_HANDLE_VALUE) {
    CloseHandle(_readPipe);
  }
  if (_writePipe != INVALID_HANDLE_VALUE) {
    CloseHandle(_writePipe);
  }
#endif
}

ExternalProcessStatus::ExternalProcessStatus()
    : _status(TRI_EXT_NOT_STARTED), _exitStatus(0), _errorMessage() {}

static ExternalProcess* TRI_LookupSpawnedProcess(TRI_pid_t pid) {
  {
    MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);
    auto found = std::find_if(ExternalProcesses.begin(), ExternalProcesses.end(),
                              [pid](const ExternalProcess * m) -> bool { return m->_pid == pid; });
    if (found != ExternalProcesses.end()) {
      return *found;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates pipe pair
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
static bool CreatePipes(int* pipe_server_to_child, int* pipe_child_to_server) {
  if (pipe(pipe_server_to_child) == -1) {
    LOG_TOPIC("ef919", ERR, arangodb::Logger::FIXME) << "cannot create pipe";
    return false;
  }

  if (pipe(pipe_child_to_server) == -1) {
    LOG_TOPIC("256ef", ERR, arangodb::Logger::FIXME) << "cannot create pipe";

    close(pipe_server_to_child[0]);
    close(pipe_server_to_child[1]);

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts external process
////////////////////////////////////////////////////////////////////////////////

static void StartExternalProcess(ExternalProcess* external, bool usePipes,
                                 std::vector<std::string> additionalEnv) {
  int pipe_server_to_child[2];
  int pipe_child_to_server[2];

  if (usePipes) {
    bool ok = CreatePipes(pipe_server_to_child, pipe_child_to_server);

    if (!ok) {
      external->_status = TRI_EXT_PIPE_FAILED;
      return;
    }
  }

  int processPid = fork();

  // child process
  if (processPid == 0) {
    // set stdin and stdout of child process
    if (usePipes) {
      dup2(pipe_server_to_child[0], 0);
      dup2(pipe_child_to_server[1], 1);

      fcntl(0, F_SETFD, 0);
      fcntl(1, F_SETFD, 0);
      fcntl(2, F_SETFD, 0);

      // close pipes
      close(pipe_server_to_child[0]);
      close(pipe_server_to_child[1]);
      close(pipe_child_to_server[0]);
      close(pipe_child_to_server[1]);
    } else {
      { // "close" stdin, but avoid fd 0 being reused!
        int fd = open("/dev/null", O_RDONLY);
        dup2(fd, 0);
        close(fd);
      }
      fcntl(1, F_SETFD, 0);
      fcntl(2, F_SETFD, 0);
    }

    // add environment variables
    for (auto it : additionalEnv) {
      putenv(TRI_DuplicateString(it.c_str()));
    }

    // execute worker
    execvp(external->_executable.c_str(), external->_arguments);

    _exit(1);
  }

  // parent
  if (processPid == -1) {
    LOG_TOPIC("e3a2a", ERR, arangodb::Logger::FIXME) << "fork failed";

    if (usePipes) {
      close(pipe_server_to_child[0]);
      close(pipe_server_to_child[1]);
      close(pipe_child_to_server[0]);
      close(pipe_child_to_server[1]);
    }

    external->_status = TRI_EXT_FORK_FAILED;
    return;
  }

  LOG_TOPIC("ac58a", DEBUG, arangodb::Logger::FIXME)
      << "fork succeeded, child pid: " << processPid;

  if (usePipes) {
    close(pipe_server_to_child[0]);
    close(pipe_child_to_server[1]);

    external->_writePipe = pipe_server_to_child[1];
    external->_readPipe = pipe_child_to_server[0];
  } else {
    external->_writePipe = -1;
    external->_readPipe = -1;
  }

  external->_pid = processPid;
  external->_status = TRI_EXT_RUNNING;
}
#else
static bool createPipes(HANDLE* hChildStdinRd, HANDLE* hChildStdinWr,
                        HANDLE* hChildStdoutRd, HANDLE* hChildStdoutWr) {
  // set the bInheritHandle flag so pipe handles are inherited
  SECURITY_ATTRIBUTES saAttr;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  // create a pipe for the child process's STDOUT
  if (!CreatePipe(hChildStdoutRd, hChildStdoutWr, &saAttr, 0)) {
    LOG_TOPIC("504dc", ERR, arangodb::Logger::FIXME)
        << ""
        << "stdout pipe creation failed";
    return false;
  }

  // create a pipe for the child process's STDIN
  if (!CreatePipe(hChildStdinRd, hChildStdinWr, &saAttr, 0)) {
    CloseHandle(hChildStdoutRd);
    CloseHandle(hChildStdoutWr);
    LOG_TOPIC("b7915", ERR, arangodb::Logger::FIXME)
        << "stdin pipe creation failed";
    return false;
  }

  return true;
}

#define appendChar(buf, x)                        \
  do {                                            \
    err = TRI_AppendCharStringBuffer((buf), (x)); \
    if (err != TRI_ERROR_NO_ERROR) {              \
      return err;                                 \
    }                                             \
  } while (false);

static int appendQuotedArg(TRI_string_buffer_t* buf, char const* p) {
  int err;

  appendChar(buf, '"');

  while (*p != 0) {
    unsigned int i;
    unsigned int NumberBackslashes = 0;
    char const* q = p;
    while (*q == '\\') {
      ++q;
      ++NumberBackslashes;
    }
    if (*q == 0) {
      // Escape all backslashes, but let the terminating
      // double quotation mark we add below be interpreted
      // as a metacharacter.
      for (i = 0; i < NumberBackslashes; i++) {
        appendChar(buf, '\\');
        appendChar(buf, '\\');
      }
      break;
    } else if (*q == '"') {
      // Escape all backslashes and the following
      // double quotation mark.
      for (i = 0; i < NumberBackslashes; i++) {
        appendChar(buf, '\\');
        appendChar(buf, '\\');
      }
      appendChar(buf, '\\');
      appendChar(buf, *q);
    } else {
      // Backslashes aren't special here.
      for (i = 0; i < NumberBackslashes; i++) {
        appendChar(buf, '\\');
      }
      appendChar(buf, *q);
    }
    p = ++q;
  }
  appendChar(buf, '"');
  return TRI_ERROR_NO_ERROR;
}

static int wAppendQuotedArg(std::wstring& buf, wchar_t const* p) {
  buf += L'"';

  while (*p != 0) {
    unsigned int i;
    unsigned int NumberBackslashes = 0;
    wchar_t const* q = p;
    while (*q == L'\\') {
      ++q;
      ++NumberBackslashes;
    }
    if (*q == 0) {
      // Escape all backslashes, but let the terminating
      // double quotation mark we add below be interpreted
      // as a metacharacter.
      for (i = 0; i < NumberBackslashes; i++) {
        buf += L'\\';
        buf += L'\\';
      }
      break;
    } else if (*q == L'"') {
      // Escape all backslashes and the following
      // double quotation mark.
      for (i = 0; i < NumberBackslashes; i++) {
        buf += L'\\';
        buf += L'\\';
      }
      buf += L'\\';
      buf += *q;
    } else {
      // Backslashes aren't special here.
      for (i = 0; i < NumberBackslashes; i++) {
        buf += L'\\';
      }
      buf += *q;
    }
    p = ++q;
  }
  buf += L'"';
  return TRI_ERROR_NO_ERROR;
}
static std::wstring makeWindowsArgs(ExternalProcess* external) {
  size_t i;
  int err = TRI_ERROR_NO_ERROR;
  std::wstring res;

  if ((external->_executable.find('/') == std::string::npos) &&
      (external->_executable.find('\\') == std::string::npos)) {
    // oK, this is a binary without path, start the lookup.
    // This will most probably break with non-ascii paths.
    char buf[MAX_PATH];
    char* pBuf;
    DWORD n;
    n = SearchPath(nullptr, external->_executable.c_str(), nullptr, MAX_PATH, buf, &pBuf);
    if (n > 0) {
      external->_executable = std::string(buf, n);
    }
  }

  icu::UnicodeString uwargs(external->_executable.c_str());

  err = wAppendQuotedArg(res, reinterpret_cast<wchar_t const*>(uwargs.getTerminatedBuffer()));
  if (err != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }
  for (i = 1; i < external->_numberArguments; i++) {
    res += L' ';
    uwargs = external->_arguments[i];
    err = wAppendQuotedArg(res, reinterpret_cast<wchar_t const*>(uwargs.getTerminatedBuffer()));
    if (err != TRI_ERROR_NO_ERROR) {
      return nullptr;
    }
  }
  return res;
}

static bool startProcess(ExternalProcess* external, HANDLE rd, HANDLE wr) {
  std::wstring args;
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFOW siStartInfo;
  BOOL bFuncRetn = FALSE;
  TRI_ERRORBUF;

  args = makeWindowsArgs(external);
  if (args.length() == 0) {
    LOG_TOPIC("1004e", ERR, arangodb::Logger::FIXME)
        << "execute of '" << external->_executable << "' failed making args";
    return false;
  }

  // set up members of the PROCESS_INFORMATION structure
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

  // set up members of the STARTUPINFO structure
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
  siStartInfo.cb = sizeof(STARTUPINFOW);

  siStartInfo.dwFlags = STARTF_USESTDHANDLES;
  siStartInfo.hStdInput = rd ? rd : nullptr;
  siStartInfo.hStdOutput = wr ? wr : GetStdHandle(STD_OUTPUT_HANDLE);
  siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

  // create the child process
  bFuncRetn = CreateProcessW(NULL,
                             (LPWSTR)args.c_str(),  // command line
                             NULL,  // process security attributes
                             NULL,  // primary thread security attributes
                             TRUE,  // handles are inherited
                             CREATE_NEW_PROCESS_GROUP,  // creation flags
                             NULL,          // use parent's environment
                             NULL,          // use parent's current directory
                             &siStartInfo,  // STARTUPINFO pointer
                             &piProcInfo);  // receives PROCESS_INFORMATION

  if (bFuncRetn == FALSE) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC("32092", ERR, arangodb::Logger::FIXME)
        << "execute of '" << external->_executable
        << "' failed, error: " << GetLastError() << " " << TRI_GET_ERRORBUF;
    return false;
  } else {
    external->_pid = piProcInfo.dwProcessId;
    external->_process = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
    return true;
  }
}

static void StartExternalProcess(ExternalProcess* external, bool usePipes,
                                 std::vector<std::string> additionalEnv) {
  HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;
  HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;
  bool fSuccess;
  if (usePipes) {
    fSuccess = createPipes(&hChildStdinRd, &hChildStdinWr, &hChildStdoutRd, &hChildStdoutWr);

    if (!fSuccess) {
      external->_status = TRI_EXT_PIPE_FAILED;
      return;
    }
  }

  // now create the child process.
  fSuccess = startProcess(external, hChildStdinRd, hChildStdoutWr);
  if (!fSuccess) {
    external->_status = TRI_EXT_FORK_FAILED;

    if (hChildStdoutRd != NULL) {
      CloseHandle(hChildStdoutRd);
    }
    if (hChildStdoutWr != NULL) {
      CloseHandle(hChildStdoutWr);
    }
    if (hChildStdinRd != NULL) {
      CloseHandle(hChildStdinRd);
    }
    if (hChildStdinWr != NULL) {
      CloseHandle(hChildStdinWr);
    }

    return;
  }

  CloseHandle(hChildStdinRd);
  CloseHandle(hChildStdoutWr);

  external->_readPipe = hChildStdoutRd;
  external->_writePipe = hChildStdinWr;
  external->_status = TRI_EXT_RUNNING;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief converts usec and sec into seconds
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_HAVE_GETRUSAGE

uint64_t TRI_MicrosecondsTv(struct timeval* tv) {
  time_t sec = tv->tv_sec;
  suseconds_t usec = tv->tv_usec;

  while (usec < 0) {
    usec += 1000000;
    sec -= 1;
  }

  return (sec * 1000000LL) + usec;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the current process
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_LINUX_PROC

ProcessInfo TRI_ProcessInfoSelf() {
  return TRI_ProcessInfo(Thread::currentProcessId());
}

#elif ARANGODB_HAVE_GETRUSAGE

ProcessInfo TRI_ProcessInfoSelf() {
  ProcessInfo result;

  result._scClkTck = 1000000;

  struct rusage used;
  int res = getrusage(RUSAGE_SELF, &used);

  if (res == 0) {
    result._minorPageFaults = used.ru_minflt;
    result._majorPageFaults = used.ru_majflt;

    result._systemTime = TRI_MicrosecondsTv(&used.ru_stime);
    result._userTime = TRI_MicrosecondsTv(&used.ru_utime);

    // ru_maxrss is the resident set size in kilobytes. need to multiply with
    // 1024 to get the number of bytes
    result._residentSize = used.ru_maxrss * ARANGODB_GETRUSAGE_MAXRSS_UNIT;
  }

#ifdef TRI_HAVE_MACH
  {
    kern_return_t rc;
    thread_array_t array;
    mach_msg_type_number_t count;

    rc = task_threads(mach_task_self(), &array, &count);

    if (rc == KERN_SUCCESS) {
      unsigned int i;

      result._numberThreads = count;

      for (i = 0; i < count; ++i) {
        mach_port_deallocate(mach_task_self(), array[i]);
      }

      vm_deallocate(mach_task_self(), (vm_address_t)array, sizeof(thread_t) * count);
    }
  }

  {
    kern_return_t rc;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    rc = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
    if (rc == KERN_SUCCESS) {
      result._virtualSize = t_info.virtual_size;
      result._residentSize = t_info.resident_size;
    } else {
      result._virtualSize = 0;
      result._residentSize = 0;
    }
  }
#endif

  return result;
}

#else
/// --------------------------------------------
/// transform a file time to timestamp
/// Particularities:
/// 1. FileTime can save a date at Jan, 1 1601
///    timestamp saves dates at 1970
/// --------------------------------------------

static uint64_t _TimeAmount(FILETIME* ft) {
  uint64_t ts, help;
  ts = ft->dwLowDateTime;
  help = ft->dwHighDateTime;
  help = help << 32;
  ts |= help;
  /// at moment without transformation
  return ts;
}

static time_t _FileTime_to_POSIX(FILETIME* ft) {
  LONGLONG ts, help;
  ts = ft->dwLowDateTime;
  help = ft->dwHighDateTime;
  help = help << 32;
  ts |= help;

  return (ts - 116444736000000000) / 10000000;
}

ProcessInfo TRI_ProcessInfoH(HANDLE processHandle, TRI_pid_t pid) {
  ProcessInfo result;

  PROCESS_MEMORY_COUNTERS_EX pmc;
  pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);
  // compiler warning wird in kauf genommen!c
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms684874(v=vs.85).aspx
  if (GetProcessMemoryInfo(processHandle, (PPROCESS_MEMORY_COUNTERS)&pmc, pmc.cb)) {
    result._majorPageFaults = pmc.PageFaultCount;
    // there is not any corresponce to minflt in linux
    result._minorPageFaults = 0;

    // from MSDN:
    // "The working set is the amount of memory physically mapped to the process
    // context at a given time.
    // Memory in the paged pool is system memory that can be transferred to the
    // paging file on disk(paged) when
    // it is not being used. Memory in the nonpaged pool is system memory that
    // cannot be paged to disk as long as
    // the corresponding objects are allocated. The pagefile usage represents
    // how much memory is set aside for the
    // process in the system paging file. When memory usage is too high, the
    // virtual memory manager pages selected
    // memory to disk. When a thread needs a page that is not in memory, the
    // memory manager reloads it from the
    // paging file."

    result._residentSize = pmc.WorkingSetSize;
    result._virtualSize = pmc.PrivateUsage;
  }
  /// computing times
  FILETIME creationTime, exitTime, kernelTime, userTime;
  if (GetProcessTimes(processHandle, &creationTime, &exitTime, &kernelTime, &userTime)) {
    // see remarks in
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683223(v=vs.85).aspx
    // value in seconds
    result._scClkTck = 10000000;  // 1e7
    result._systemTime = _TimeAmount(&kernelTime);
    result._userTime = _TimeAmount(&userTime);
    // for computing  the timestamps of creation and exit time
    // the function '_FileTime_to_POSIX' should be called
  }
  /// computing number of threads
  HANDLE snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);

  if (snapShot != INVALID_HANDLE_VALUE) {
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    if (Thread32First(snapShot, &te32)) {
      result._numberThreads++;
      while (Thread32Next(snapShot, &te32)) {
        if (te32.th32OwnerProcessID == pid) {
          result._numberThreads++;
        }
      }
    }
    else {
      LOG_TOPIC("66667", ERR, arangodb::Logger::FIXME) << "failed to acquire thread from snapshot - " << GetLastError();
    }
    CloseHandle(snapShot);
  }
  else {
    LOG_TOPIC("66668", ERR, arangodb::Logger::FIXME) << "failed to acquire process threads count - " << GetLastError();
  }

  return result;
}

ProcessInfo TRI_ProcessInfoSelf() {
  return TRI_ProcessInfoH(GetCurrentProcess(), GetCurrentProcessId());
}

ProcessInfo TRI_ProcessInfo(TRI_pid_t pid) {
  auto external = TRI_LookupSpawnedProcess(pid);
  if (external != nullptr) {
    return TRI_ProcessInfoH(external->_process, pid);
  }
  return {};
}

#endif

/// @brief returns information about the process
#ifdef TRI_HAVE_LINUX_PROC

ProcessInfo TRI_ProcessInfo(TRI_pid_t pid) {
  ProcessInfo result;

  char str[1024];
  memset(&str, 0, sizeof(str));

  // build filename /proc/<pid>/stat
  {
    char* p = &str[0];

    // a malloc-free sprintf...
    static constexpr char const* proc = "/proc/";
    static constexpr char const* stat = "/stat";
    
    // append /proc/
    memcpy(p, proc, strlen(proc));
    p += strlen(proc);
    // append pid
    p += arangodb::basics::StringUtils::itoa(uint64_t(pid), p);
    memcpy(p, stat, strlen(stat));
    p += strlen(stat);
    *p = '\0';
  }

  // open file...
  int fd = TRI_OPEN(&str[0], O_RDONLY);

  if (fd >= 0) {
    memset(&str, 0, sizeof(str));

    ssize_t n = TRI_READ(fd, str, static_cast<TRI_read_t>(sizeof(str)));
    close(fd);

    if (n == 0) {
      return result;
    }
  
    /// buffer now contains all data documented by "proc"
    /// see man 5 proc for the state of a process

    char const* p = &str[0];
    char const* e = p + n;
    
    skipEntry(p, e); // process id
    skipEntry(p, e); // process name
    skipEntry(p, e); // process state
    skipEntry(p, e); // ppid
    skipEntry(p, e); // pgrp
    skipEntry(p, e); // session
    skipEntry(p, e); // tty nr
    skipEntry(p, e); // tpgid
    skipEntry(p, e); // flags
    result._minorPageFaults = readEntry<uint64_t>(p, e); // min flt
    skipEntry(p, e); // cmin flt
    result._majorPageFaults = readEntry<uint64_t>(p, e); // maj flt
    skipEntry(p, e); // cmaj flt
    result._userTime = readEntry<uint64_t>(p, e); // utime
    result._systemTime = readEntry<uint64_t>(p, e); // stime
    skipEntry(p, e); // cutime
    skipEntry(p, e); // cstime
    skipEntry(p, e); // priority
    skipEntry(p, e); // nice
    result._numberThreads = readEntry<int64_t>(p, e); // num threads
    skipEntry(p, e); // itrealvalue
    skipEntry(p, e); // starttime
    result._virtualSize = readEntry<uint64_t>(p, e); // vsize
    result._residentSize = readEntry<int64_t>(p, e) * PageSize::getValue(); // rss
    result._scClkTck = sysconf(_SC_CLK_TCK);
  }

  return result;
}

#else
#ifndef _WIN32
ProcessInfo TRI_ProcessInfo(TRI_pid_t pid) {
  ProcessInfo result;

  result._scClkTck = 1;

  return result;
}
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process name
////////////////////////////////////////////////////////////////////////////////

void TRI_SetProcessTitle(char const* title) {
#ifdef TRI_HAVE_SYS_PRCTL_H
  prctl(PR_SET_NAME, title, 0, 0, 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts an external process
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateExternalProcess(char const* executable,
                               std::vector<std::string> const& arguments,
                               std::vector<std::string> additionalEnv,
                               bool usePipes, ExternalId* pid) {
  size_t const n = arguments.size();
  // create the external structure
  auto external = std::make_unique<ExternalProcess>();
  external->_executable = executable;
  external->_numberArguments = n + 1;

  external->_arguments = static_cast<char**>(TRI_Allocate((n + 2) * sizeof(char*)));

  if (external->_arguments == nullptr) {
    // gracefully handle out of memory
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }

  memset(external->_arguments, 0, (n + 2) * sizeof(char*));

  external->_arguments[0] = TRI_DuplicateString(executable);
  if (external->_arguments[0] == nullptr) {
    // OOM
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }

  for (size_t i = 0; i < n; ++i) {
    external->_arguments[i + 1] = TRI_DuplicateString(arguments[i].c_str());
    if (external->_arguments[i + 1] == nullptr) {
      // OOM
      pid->_pid = TRI_INVALID_PROCESS_ID;
      return;
    }
  }

  external->_arguments[n + 1] = nullptr;
  external->_status = TRI_EXT_NOT_STARTED;

  StartExternalProcess(external.get(), usePipes, additionalEnv);

  if (external->_status != TRI_EXT_RUNNING) {
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }

  LOG_TOPIC("58158", DEBUG, arangodb::Logger::FIXME)
      << "adding process " << external->_pid << " to list";

  // Note that the following deals with different types under windows,
  // however, this code here can be written in a platform-independent
  // way:
  pid->_pid = external->_pid;
  pid->_readPipe = external->_readPipe;
  pid->_writePipe = external->_writePipe;

  MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

  try {
    ExternalProcesses.push_back(external.get());
    external.release();
  } catch (...) {
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of an external process
////////////////////////////////////////////////////////////////////////////////

ExternalProcessStatus TRI_CheckExternalProcess(ExternalId pid, bool wait, uint32_t timeout) {
  ExternalProcessStatus status;
  status._status = TRI_EXT_NOT_FOUND;
  status._exitStatus = 0;

  auto external = TRI_LookupSpawnedProcess(pid._pid);

  if (external == nullptr) {
    status._errorMessage =
        std::string("the pid you're looking for is not in our list: ") +
        arangodb::basics::StringUtils::itoa(static_cast<int64_t>(pid._pid));
    status._status = TRI_EXT_NOT_FOUND;
    LOG_TOPIC("f5f99", WARN, arangodb::Logger::FIXME)
        << "checkExternal: pid not found: " << pid._pid;

    return status;
  }

  if (external->_status == TRI_EXT_RUNNING || external->_status == TRI_EXT_STOPPED) {
#ifndef _WIN32
    if (timeout > 0) {
      // if we use a timeout, it means we cannot use blocking
      wait = false;
    }

    int opts;
    if (wait) {
      opts = WUNTRACED;
    } else {
      opts = WNOHANG | WUNTRACED;
    }

    int loc = 0;
    TRI_pid_t res = 0;
    bool timeoutHappened = false;
    if (timeout) {
      TRI_ASSERT((opts & WNOHANG) != 0);
      double endTime = 0.0; 
      while (true) {
        res = waitpid(external->_pid, &loc, opts);
        if (res != 0) {
          break;
        }
        double now = TRI_microtime();
        if (endTime == 0.0) {
          endTime = now + timeout / 1000.0;
        } else if (now >= endTime) {
          res = external->_pid;
          timeoutHappened = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    } else {
      res = waitpid(external->_pid, &loc, opts);
    }

    if (res == 0) {
      if (wait) {
        status._errorMessage =
            std::string("waitpid returned 0 for pid while it shouldn't ") +
            arangodb::basics::StringUtils::itoa(external->_pid);
        if (timeoutHappened) {
          external->_status = TRI_EXT_TIMEOUT;
          external->_exitStatus = -1;
        } else if (WIFEXITED(loc)) {
          external->_status = TRI_EXT_TERMINATED;
          external->_exitStatus = WEXITSTATUS(loc);
        } else if (WIFSIGNALED(loc)) {
          external->_status = TRI_EXT_ABORTED;
          external->_exitStatus = WTERMSIG(loc);
        } else if (WIFSTOPPED(loc)) {
          external->_status = TRI_EXT_STOPPED;
          external->_exitStatus = 0;
        } else {
          external->_status = TRI_EXT_ABORTED;
          external->_exitStatus = 0;
        }
      } else {
        external->_exitStatus = 0;
      }
    } else if (res == -1) {
      if (errno == ECHILD) {
        external->_status = TRI_EXT_NOT_FOUND;
      }
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("308ea", WARN, arangodb::Logger::FIXME)
          << "waitpid returned error for pid " << external->_pid << " (" << wait
          << "): " << TRI_last_error();
      status._errorMessage = std::string("waitpid returned error for pid ") +
                             arangodb::basics::StringUtils::itoa(external->_pid) +
                             std::string(": ") + std::string(TRI_last_error());
    } else if (static_cast<TRI_pid_t>(external->_pid) == static_cast<TRI_pid_t>(res)) {
      if (timeoutHappened) {
        external->_status = TRI_EXT_TIMEOUT;
        external->_exitStatus = -1;
      } else if (WIFEXITED(loc)) {
        external->_status = TRI_EXT_TERMINATED;
        external->_exitStatus = WEXITSTATUS(loc);
      } else if (WIFSIGNALED(loc)) {
        external->_status = TRI_EXT_ABORTED;
        external->_exitStatus = WTERMSIG(loc);
      } else if (WIFSTOPPED(loc)) {
        external->_status = TRI_EXT_STOPPED;
        external->_exitStatus = 0;
      } else {
        external->_status = TRI_EXT_ABORTED;
        external->_exitStatus = 0;
      }
    } else {
      LOG_TOPIC("0ab33", WARN, arangodb::Logger::FIXME)
          << "unexpected waitpid result for pid " << external->_pid << ": " << res;
      status._errorMessage = std::string("unexpected waitpid result for pid ") +
                             arangodb::basics::StringUtils::itoa(external->_pid) +
                             std::string(": ") +
                             arangodb::basics::StringUtils::itoa(res);
    }
#else
    {
      char windowsErrorBuf[256];
      bool wantGetExitCode = wait;
      if (wait) {
        DWORD result;
        DWORD waitFor = INFINITE;
        if (timeout != 0) {
          waitFor = timeout;
        }
        result = WaitForSingleObject(external->_process, waitFor);
        if (result == WAIT_FAILED) {
          FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
                        windowsErrorBuf, sizeof(windowsErrorBuf), NULL);
          LOG_TOPIC("64246", WARN, arangodb::Logger::FIXME)
              << "could not wait for subprocess with pid " << external->_pid
              << ": " << windowsErrorBuf;
          status._errorMessage =
              std::string("could not wait for subprocess with pid ") +
              arangodb::basics::StringUtils::itoa(static_cast<int64_t>(external->_pid)) +
              windowsErrorBuf;
          status._exitStatus = GetLastError();
        } else if ((result == WAIT_TIMEOUT) && (timeout != 0)) {
          wantGetExitCode = false;
          external->_status = TRI_EXT_TIMEOUT;
          external->_exitStatus = -1;
        }
      } else {
        DWORD result;
        result = WaitForSingleObject(external->_process, 0);
        switch (result) {
          case WAIT_ABANDONED:
            wantGetExitCode = true;
            LOG_TOPIC("92708", WARN, arangodb::Logger::FIXME)
                << "WAIT_ABANDONED while waiting for subprocess with pid "
                << external->_pid;
            break;
          case WAIT_OBJECT_0:
            /// this seems to be the exit case - want getExitCodeProcess here.
            wantGetExitCode = true;
            break;
          case WAIT_TIMEOUT:
            // success - process is up and running.
            external->_exitStatus = 0;
            break;
          case WAIT_FAILED:
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
                          windowsErrorBuf, sizeof(windowsErrorBuf), NULL);
            LOG_TOPIC("f79de", WARN, arangodb::Logger::FIXME)
                << "could not wait for subprocess with pid " << external->_pid
                << ": " << windowsErrorBuf;
            status._errorMessage =
                std::string("could not wait for subprocess with PID '") +
                arangodb::basics::StringUtils::itoa(
                    static_cast<int64_t>(external->_pid)) +
                std::string("'") + windowsErrorBuf;
            status._exitStatus = GetLastError();
          default:
            wantGetExitCode = true;
            LOG_TOPIC("5c1fb", WARN, arangodb::Logger::FIXME)
                << "unexpected status while waiting for subprocess with pid "
                << external->_pid;
        }
      }
      if (wantGetExitCode) {
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(external->_process, &exitCode)) {
          LOG_TOPIC("798af", WARN, arangodb::Logger::FIXME)
              << "exit status could not be determined for pid " << external->_pid;
          status._errorMessage =
              std::string("exit status could not be determined for pid ") +
              arangodb::basics::StringUtils::itoa(static_cast<int64_t>(external->_pid));
          external->_exitStatus = -1;
          external->_status = TRI_EXT_NOT_STARTED;
        } else {
          if (exitCode == STILL_ACTIVE) {
            external->_exitStatus = 0;
          } else if (exitCode > 255) {
            // this should be one of our signals which we mapped...
            external->_status = TRI_EXT_ABORTED;
            external->_exitStatus = exitCode - 255;
          } else {
            external->_status = TRI_EXT_TERMINATED;
            external->_exitStatus = exitCode;
          }
        }
      } else if (timeout == 0) {
        external->_status = TRI_EXT_RUNNING;
      }
    }
#endif
  } else {
    LOG_TOPIC("1cff4", WARN, arangodb::Logger::FIXME)
        << "unexpected process status " << external->_status << ": "
        << external->_exitStatus;
    status._errorMessage = std::string("unexpected process status ") +
                           arangodb::basics::StringUtils::itoa(external->_status) +
                           std::string(": ") +
                           arangodb::basics::StringUtils::itoa(external->_exitStatus);
  }

  status._status = external->_status;
  status._exitStatus = external->_exitStatus;

  // Do we have to free our data?
  if (external->_status != TRI_EXT_RUNNING && external->_status != TRI_EXT_STOPPED &&
      external->_status != TRI_EXT_TIMEOUT) {
    MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

    for (auto it = ExternalProcesses.begin(); it != ExternalProcesses.end(); ++it) {
      if ((*it)->_pid == pid._pid) {
        ExternalProcesses.erase(it);
        break;
      }
    }

    delete external;
  }

  return status;
}

////////////////////////////////////////////////////////////////////////////////
// @brief check for a process we didn't spawn, and check for access rights to
//        send it signals.
#ifndef _WIN32
static ExternalProcess* getExternalProcess(TRI_pid_t pid) {
  if (kill(pid, 0) == 0) {
    ExternalProcess* external = new ExternalProcess();

    external->_pid = pid;
    external->_status = TRI_EXT_RUNNING;

    return external;
  }

  LOG_TOPIC("b0d9c", WARN, arangodb::Logger::FIXME)
      << "checking for external process: '" << pid
      << "' failed with error: " << strerror(errno);
  return nullptr;
}
#else
static ExternalProcess* getExternalProcess(TRI_pid_t pid) {
  HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

  if (hProcess != nullptr) {
    ExternalProcess* external = new ExternalProcess();

    external->_pid = pid;
    external->_status = TRI_EXT_RUNNING;
    external->_process = hProcess;

    return external;
  }
  return nullptr;
}
#endif

/// @brief check for a process we didn't spawn, and check for access rights to
/// send it signals.
#ifndef _WIN32
static bool killProcess(ExternalProcess* pid, int signal) {
  TRI_ASSERT(pid != nullptr);
  if (pid == nullptr) {
    return false;
  }
  if (signal == SIGKILL) {
    LOG_TOPIC("021b9", WARN, arangodb::Logger::FIXME)
        << "sending SIGKILL signal to process: " << pid->_pid;
  }
  if (kill(pid->_pid, signal) == 0) {
    return true;
  }
  return false;
}

#else
static bool killProcess(ExternalProcess* pid, int signal) {
  TRI_ASSERT(pid != nullptr);
  UINT uExitCode = 0;

  if (pid == nullptr) {
    return false;
  }

  // kill worker process
  if (0 != TerminateProcess(pid->_process, uExitCode)) {
    return true;
  } else {
    return false;
  }
}
#define SIGKILL 1
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief kills an external process
////////////////////////////////////////////////////////////////////////////////

ExternalProcessStatus TRI_KillExternalProcess(ExternalId pid, int signal, bool isTerminal) {
  LOG_TOPIC("77bc5", DEBUG, arangodb::Logger::FIXME)
      << "Sending process: " << pid._pid << " the signal: " << signal;

  ExternalProcess* external = nullptr;
  {
    MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

    for (auto it = ExternalProcesses.begin(); it != ExternalProcesses.end(); ++it) {
      if ((*it)->_pid == pid._pid) {
        external = (*it);
        break;
      }
    }
  }

  bool isChild = (external != nullptr);
  if (!isChild) {
    external = getExternalProcess(pid._pid);
    if (external == nullptr) {
      LOG_TOPIC("73b93", DEBUG, arangodb::Logger::FIXME)
          << "kill: process not found: " << pid._pid
          << " in our starting table and it doesn't exist.";
      ExternalProcessStatus status;
      status._status = TRI_EXT_NOT_FOUND;
      status._exitStatus = -1;
      return status;
    }
    LOG_TOPIC("349fa", DEBUG, arangodb::Logger::FIXME)
        << "kill: process not found: " << pid._pid << " in our starting table - adding";

    // ok, we didn't spawn it, but now we claim the
    // ownership.
    MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

    try {
      ExternalProcesses.push_back(external);
    } catch (...) {
      delete external;

      ExternalProcessStatus status;
      status._status = TRI_EXT_NOT_FOUND;
      status._exitStatus = -1;
      return status;
    }
  }

  TRI_ASSERT(external != nullptr);
  if (killProcess(external, signal)) {
    external->_status = TRI_EXT_STOPPED;
    // if the process wasn't spawned by us, no waiting required.
    int count = 0;
    while (true) {
      ExternalProcessStatus status = TRI_CheckExternalProcess(pid, false, 0);
      if (!isTerminal) {
        // we just sent a signal, don't care whether
        // the process is gone by now.
        return status;
      }
      if ((status._status == TRI_EXT_TERMINATED) || (status._status == TRI_EXT_ABORTED) ||
          (status._status == TRI_EXT_NOT_FOUND)) {
        // Its dead and gone - good.
        MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);
        for (auto it = ExternalProcesses.begin(); it != ExternalProcesses.end(); ++it) {
          if (*it == external) {
            ExternalProcesses.erase(it);
            break;
          }
        }
        if (!isChild && (status._status == TRI_EXT_NOT_FOUND)) {
          status._status = TRI_EXT_TERMINATED;
          status._errorMessage.clear();
        }
        return status;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (count >= 13) {
        TRI_ASSERT(external != nullptr);
        LOG_TOPIC("2af4e", WARN, arangodb::Logger::FIXME)
            << "about to send SIGKILL signal to process: " << external->_pid
            << ", status: " << (int)status._status;
        killProcess(external, SIGKILL);
      }
      if (count > 25) {
        return status;
      }
      count++;
    }
  }
  return TRI_CheckExternalProcess(pid, false, 0);
}

#ifdef _WIN32
typedef LONG(NTAPI* NtSuspendProcess)(IN HANDLE ProcessHandle);
typedef LONG(NTAPI* NtResumeProcess)(IN HANDLE ProcessHandle);

NtSuspendProcess pfnNtSuspendProcess =
    (NtSuspendProcess)GetProcAddress(GetModuleHandle("ntdll"),
                                     "NtSuspendProcess");
NtResumeProcess pfnNtResumeProcess =
    (NtResumeProcess)GetProcAddress(GetModuleHandle("ntdll"),
                                    "NtResumeProcess");
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief stops an external process, only on Unix
////////////////////////////////////////////////////////////////////////////////

bool TRI_SuspendExternalProcess(ExternalId pid) {
  LOG_TOPIC("13e36", DEBUG, arangodb::Logger::FIXME) << "suspending process: " << pid._pid;

#ifndef _WIN32
  return 0 == kill(pid._pid, SIGSTOP);
#else
  TRI_ERRORBUF;

  HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid._pid);
  bool rc = pfnNtSuspendProcess(processHandle) == 0;
  if (!rc) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC("4da8a", ERR, arangodb::Logger::FIXME)
        << "suspending of '" << pid._pid
        << "' failed, error: " << GetLastError() << " " << TRI_GET_ERRORBUF;
  }
  CloseHandle(processHandle);
  return rc;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief continues an external process, only on Unix
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContinueExternalProcess(ExternalId pid) {
  LOG_TOPIC("45884", DEBUG, arangodb::Logger::FIXME)
      << "continueing process: " << pid._pid;

#ifndef _WIN32
  return 0 == kill(pid._pid, SIGCONT);
#else
  TRI_ERRORBUF;

  HANDLE processHandle = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid._pid);
  bool rc = processHandle != NULL && pfnNtResumeProcess(processHandle) == 0;
  if (!rc) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC("57e23", ERR, arangodb::Logger::FIXME)
        << "resuming of '" << pid._pid << "' failed, error: " << GetLastError()
        << " " << TRI_GET_ERRORBUF;
  }
  CloseHandle(processHandle);
  return rc;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the process components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownProcess() {
  MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);
  for (auto* external : ExternalProcesses) {
    delete external;
  }
  ExternalProcesses.clear();
}
