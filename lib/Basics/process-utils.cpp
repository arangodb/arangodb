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

#include "process-utils.h"

#if defined(TRI_HAVE_MACOS_MEM_STATS)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
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
#endif

#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/tri-strings.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief physical memory
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_PhysicalMemory;

////////////////////////////////////////////////////////////////////////////////
/// @brief contains all data documented by "proc"
///
/// @see man 5 proc for the state of a process
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_LINUX_PROC

typedef struct process_state_s {
  pid_t pid;
  /* size was choosen arbitrary */
  char comm[128];
  char state;
  int ppid;
  int pgrp;
  int session;
  int tty_nr;
  int tpgid;
  unsigned flags;
  /* lu */
  unsigned long minflt;
  unsigned long cminflt;
  unsigned long majflt;
  unsigned long cmajflt;
  unsigned long utime;
  unsigned long stime;
  /* ld */
  long cutime;
  long cstime;
  long priority;
  long nice;
  long num_threads;
  long itrealvalue;
  /* llu */
  long long unsigned int starttime;
  /* lu */
  unsigned long vsize;
  /* ld */
  long rss;
  /* lu */
  // cppcheck-suppress *
  unsigned long rsslim;
  // cppcheck-suppress *
  unsigned long startcode;
  // cppcheck-suppress *
  unsigned long endcode;
  // cppcheck-suppress *
  unsigned long startstack;
  // cppcheck-suppress *
  unsigned long kstkesp;
  // cppcheck-suppress *
  unsigned long signal;
  /* obsolete lu*/
  // cppcheck-suppress *
  unsigned long blocked;
  // cppcheck-suppress *
  unsigned long sigignore;
  // cppcheck-suppress *
  unsigned int sigcatch;
  // cppcheck-suppress *
  unsigned long wchan;
  /* no maintained lu */
  // cppcheck-suppress *
  unsigned long nswap;
  // cppcheck-suppress *
  unsigned long cnswap;
  /* d */
  // cppcheck-suppress *
  int exit_signal;
  // cppcheck-suppress *
  int processor;
  /* u */
  // cppcheck-suppress *
  unsigned rt_priority;
  // cppcheck-suppress *
  unsigned policy;
  /* llu */
  // cppcheck-suppress *
  long long unsigned int delayacct_blkio_ticks;
  /* lu */
  // cppcheck-suppress *
  unsigned long guest_time;
  /* ld */
  // cppcheck-suppress *
  long cguest_time;
} process_state_t;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief all external processes
////////////////////////////////////////////////////////////////////////////////

static std::vector<TRI_external_t*> ExternalProcesses;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for protected access to vector ExternalProcesses
////////////////////////////////////////////////////////////////////////////////

static arangodb::Mutex ExternalProcessesLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates pipe pair
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
static bool CreatePipes(int* pipe_server_to_child, int* pipe_child_to_server) {
  if (pipe(pipe_server_to_child) == -1) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create pipe";
    return false;
  }

  if (pipe(pipe_child_to_server) == -1) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create pipe";

    close(pipe_server_to_child[0]);
    close(pipe_server_to_child[1]);

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts external process
////////////////////////////////////////////////////////////////////////////////

static void StartExternalProcess(TRI_external_t* external, bool usePipes) {
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
      close(0);
      fcntl(1, F_SETFD, 0);
      fcntl(2, F_SETFD, 0);
    }

    // ignore signals in worker process
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);

    // execute worker
    execvp(external->_executable, external->_arguments);

    _exit(1);
  }

  // parent
  if (processPid == -1) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "fork failed";

    if (usePipes) {
      close(pipe_server_to_child[0]);
      close(pipe_server_to_child[1]);
      close(pipe_child_to_server[0]);
      close(pipe_child_to_server[1]);
    }

    external->_status = TRI_EXT_FORK_FAILED;
    return;
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "fork succeeded, child pid: " << processPid;

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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << ""
             << "stdout pipe creation failed";
    return false;
  }

  // create a pipe for the child process's STDIN
  if (!CreatePipe(hChildStdinRd, hChildStdinWr, &saAttr, 0)) {
    CloseHandle(hChildStdoutRd);
    CloseHandle(hChildStdoutWr);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "stdin pipe creation failed";
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

static char* makeWindowsArgs(TRI_external_t* external) {
  TRI_string_buffer_t* buf;
  size_t i;
  int err = TRI_ERROR_NO_ERROR;
  char* res;

  buf = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  if (buf == NULL) {
    return NULL;
  }
  TRI_ReserveStringBuffer(buf, 1024);
  err = appendQuotedArg(buf, external->_executable);
  if (err != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buf);
    return NULL;
  }
  for (i = 1; i < external->_numberArguments; i++) {
    err = TRI_AppendCharStringBuffer(buf, ' ');
    if (err != TRI_ERROR_NO_ERROR) {
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buf);
      return NULL;
    }
    err = appendQuotedArg(buf, external->_arguments[i]);
  }
  res = TRI_StealStringBuffer(buf);
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buf);
  return res;
}

static bool startProcess(TRI_external_t* external, HANDLE rd, HANDLE wr) {
  char* args;
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFO siStartInfo;
  BOOL bFuncRetn = FALSE;
  TRI_ERRORBUF;
  
  args = makeWindowsArgs(external);
  if (args == NULL) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "execute of '" << external->_executable
             << "' failed making args";
    return false;
  }

  // set up members of the PROCESS_INFORMATION structure
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

  // set up members of the STARTUPINFO structure
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb = sizeof(STARTUPINFO);

  siStartInfo.dwFlags = STARTF_USESTDHANDLES;
  siStartInfo.hStdInput = rd ? rd : nullptr;
  siStartInfo.hStdOutput = wr ? wr : GetStdHandle(STD_OUTPUT_HANDLE);
  siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

  // create the child process
  bFuncRetn = CreateProcess(NULL,
                            args,  // command line
                            NULL,  // process security attributes
                            NULL,  // primary thread security attributes
                            TRUE,  // handles are inherited
                            CREATE_NEW_PROCESS_GROUP,  // creation flags
                            NULL,          // use parent's environment
                            NULL,          // use parent's current directory
                            &siStartInfo,  // STARTUPINFO pointer
                            &piProcInfo);  // receives PROCESS_INFORMATION

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, args);

  if (bFuncRetn == FALSE) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "execute of '" << external->_executable
             << "' failed, error: " << GetLastError() << " " << TRI_GET_ERRORBUF;
    return false;
  } else {
    external->_pid = piProcInfo.dwProcessId;
    external->_process = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
    return true;
  }
}

static void StartExternalProcess(TRI_external_t* external, bool usePipes) {
  HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;
  HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;
  bool fSuccess;
  if (usePipes) {
    fSuccess = createPipes(&hChildStdinRd, &hChildStdinWr, &hChildStdoutRd,
                           &hChildStdoutWr);

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
      
void TRI_LogProcessInfoSelf(char const* message) {
  TRI_process_info_t info = TRI_ProcessInfoSelf();

  if (message == nullptr) {
    message = "";
  }
  
  LOG_TOPIC(TRACE, Logger::MEMORY) << message << "virtualSize: " << info._virtualSize << ", residentSize: " << info._residentSize << ", numberThreads: " << info._numberThreads;
}

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

TRI_process_info_t TRI_ProcessInfoSelf() {
  return TRI_ProcessInfo(Thread::currentProcessId());
}

#elif ARANGODB_HAVE_GETRUSAGE

TRI_process_info_t TRI_ProcessInfoSelf() {
  TRI_process_info_t result;

  memset(&result, 0, sizeof(result));
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

      vm_deallocate(mach_task_self(), (vm_address_t)array,
                    sizeof(thread_t) * count);
    }
  }

  {
    kern_return_t rc;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    rc = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info,
                   &t_info_count);
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

TRI_process_info_t TRI_ProcessInfoSelf() {
  TRI_process_info_t result;
  PROCESS_MEMORY_COUNTERS_EX pmc;
  memset(&result, 0, sizeof(result));
  pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);
  // compiler warning wird in kauf genommen!c
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms684874(v=vs.85).aspx
  if (GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&pmc,
                           pmc.cb)) {
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
  if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime,
                      &kernelTime, &userTime)) {
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
  DWORD myPID = GetCurrentProcessId();
  HANDLE snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, myPID);

  if (snapShot != INVALID_HANDLE_VALUE) {
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    if (Thread32First(snapShot, &te32)) {
      result._numberThreads++;
      while (Thread32Next(snapShot, &te32)) {
        if (te32.th32OwnerProcessID == myPID) {
          result._numberThreads++;
        }
      }
    }
    CloseHandle(snapShot);
  }

  return result;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the process
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_LINUX_PROC

TRI_process_info_t TRI_ProcessInfo(TRI_pid_t pid) {
  TRI_process_info_t result;
  memset(&result, 0, sizeof(result));

  char fn[1024];
  snprintf(fn, sizeof(fn), "/proc/%d/stat", pid);

  int fd = open(fn, O_RDONLY);

  if (fd >= 0) {
    char str[1024];
    process_state_t st;
    size_t n;

    memset(&str, 0, sizeof(str));

    n = read(fd, str, sizeof(str));
    close(fd);

    if (n == 0) {
      return result;
    }

    // fix process name in buffer. sadly, the process name might contain
    // whitespace
    // and the sscanf format '%s' will not honor that
    char* p = &str[0];
    char* e = p + n;
    // first skip over the process id at the start of the string
    while (*p != '\0' && p < e && *p != ' ') {
      ++p;
    }
    // skip space
    if (p < e && *p == ' ') {
      ++p;
    }
    // check if filename is contained in parentheses
    if (p < e && *p == '(') {
      // yes
      ++p;
      // now replace all whitespace within the process name with underscores
      // otherwise the sscanf below will happily parse the string incorrectly
      while (p < e && *p != '0' && *p != ')') {
        if (*p == ' ') {
          *p = '_';
        }
        ++p;
      }
    }

    // cppcheck-suppress *
    sscanf(str,
           "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld "
           "%ld %ld %llu %lu %ld",
           &st.pid, (char*)&st.comm, &st.state, &st.ppid, &st.pgrp, &st.session,
           &st.tty_nr, &st.tpgid, &st.flags, &st.minflt, &st.cminflt,
           &st.majflt, &st.cmajflt, &st.utime, &st.stime, &st.cutime,
           &st.cstime, &st.priority, &st.nice, &st.num_threads, &st.itrealvalue,
           &st.starttime, &st.vsize, &st.rss);

    result._minorPageFaults = st.minflt;
    result._majorPageFaults = st.majflt;
    result._userTime = st.utime;
    result._systemTime = st.stime;
    result._numberThreads = st.num_threads;
    // st.rss is measured in number of pages, we need to multiply by page size
    // to get the actual amount
    result._residentSize = st.rss * getpagesize();
    result._virtualSize = st.vsize;
    result._scClkTck = sysconf(_SC_CLK_TCK);
  }

  return result;
}

#else

TRI_process_info_t TRI_ProcessInfo(TRI_pid_t pid) {
  TRI_process_info_t result;

  memset(&result, 0, sizeof(result));
  result._scClkTck = 1;

  return result;
}

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
/// @brief frees an external process structure
////////////////////////////////////////////////////////////////////////////////

static void FreeExternal(TRI_external_t* ext) {
  TRI_Free(TRI_CORE_MEM_ZONE, ext->_executable);

  for (size_t i = 0; i < ext->_numberArguments; i++) {
    TRI_Free(TRI_CORE_MEM_ZONE, ext->_arguments[i]);
  }
  TRI_Free(TRI_CORE_MEM_ZONE, ext->_arguments);

#ifndef _WIN32
  if (ext->_readPipe != -1) {
    close(ext->_readPipe);
  }
  if (ext->_writePipe != -1) {
    close(ext->_writePipe);
  }
#else
  CloseHandle(ext->_process);
  if (ext->_readPipe != NULL) {
    CloseHandle(ext->_readPipe);
  }
  if (ext->_writePipe != NULL) {
    CloseHandle(ext->_writePipe);
  }
#endif
  TRI_Free(TRI_CORE_MEM_ZONE, ext);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts an external process
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateExternalProcess(char const* executable, char const** arguments,
                               size_t n, bool usePipes,
                               TRI_external_id_t* pid) {
  // create the external structure
  TRI_external_t* external = static_cast<TRI_external_t*>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_external_t)));

  if (external == nullptr) {
    // gracefully handle out of memory
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }

  memset(external, 0, sizeof(TRI_external_t));

  external->_executable = TRI_DuplicateString(executable);
  external->_numberArguments = n + 1;

  external->_arguments = static_cast<char**>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, (n + 2) * sizeof(char*)));

  if (external->_arguments == nullptr) {
    // gracefully handle out of memory
    pid->_pid = TRI_INVALID_PROCESS_ID;
    FreeExternal(external);
    return;
  }

  memset(external->_arguments, 0, (n + 2) * sizeof(char*));

  external->_arguments[0] = TRI_DuplicateString(executable);

  for (size_t i = 0; i < n; ++i) {
    external->_arguments[i + 1] = TRI_DuplicateString(arguments[i]);
  }

  external->_arguments[n + 1] = nullptr;
  external->_status = TRI_EXT_NOT_STARTED;

  StartExternalProcess(external, usePipes);

  if (external->_status != TRI_EXT_RUNNING) {
    pid->_pid = TRI_INVALID_PROCESS_ID;
    FreeExternal(external);
    return;
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "adding process " << external->_pid << " to list";

  // Note that the following deals with different types under windows,
  // however, this code here can be written in a platform-independent
  // way:
  pid->_pid = external->_pid;
  pid->_readPipe = external->_readPipe;
  pid->_writePipe = external->_writePipe;

  MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

  try {
    ExternalProcesses.push_back(external);
  } catch (...) {
    pid->_pid = TRI_INVALID_PROCESS_ID;
    FreeExternal(external);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of an external process
////////////////////////////////////////////////////////////////////////////////

TRI_external_status_t TRI_CheckExternalProcess(TRI_external_id_t pid,
                                               bool wait) {
  TRI_external_status_t status;
  status._status = TRI_EXT_NOT_FOUND;
  status._exitStatus = 0;

  TRI_external_t* external = nullptr;
  {
    MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

    for (auto& it : ExternalProcesses) {
      if (it->_pid == pid._pid) {
        external = it;
        break;
      }
    }
  }

  if (external == nullptr) {
    status._errorMessage =
        std::string("the pid you're looking for is not in our list: ") +
        arangodb::basics::StringUtils::itoa(static_cast<int64_t>(pid._pid));
    status._status = TRI_EXT_NOT_FOUND;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "checkExternal: pid not found: " << pid._pid;

    return status;
  }

  if (external->_status == TRI_EXT_RUNNING ||
      external->_status == TRI_EXT_STOPPED) {
#ifndef _WIN32
    int opts;
    int loc = 0;

    if (wait) {
      opts = WUNTRACED;
    } else {
      opts = WNOHANG | WUNTRACED;
    }

    TRI_pid_t res = waitpid(external->_pid, &loc, opts);

    if (res == 0) {
      if (wait) {
        status._errorMessage =
            std::string("waitpid returned 0 for pid while it shouldn't ") +
            arangodb::basics::StringUtils::itoa(external->_pid);

        if (WIFEXITED(loc)) {
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
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "waitpid returned error for pid " << external->_pid << " ("
                << wait << "): " << TRI_last_error();
      status._errorMessage =
          std::string("waitpid returned error for pid ") +
          arangodb::basics::StringUtils::itoa(external->_pid) +
          std::string(": ") + std::string(TRI_last_error());
    } else if (static_cast<TRI_pid_t>(external->_pid) ==
               static_cast<TRI_pid_t>(res)) {
      if (WIFEXITED(loc)) {
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
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "unexpected waitpid result for pid " << external->_pid
                << ": " << res;
      status._errorMessage =
          std::string("unexpected waitpid result for pid ") +
          arangodb::basics::StringUtils::itoa(external->_pid) +
          std::string(": ") + arangodb::basics::StringUtils::itoa(res);
    }
#else
    {
      char windowsErrorBuf[256];
      bool wantGetExitCode = wait;
      if (wait) {
        DWORD result;
        result = WaitForSingleObject(external->_process, INFINITE);
        if (result == WAIT_FAILED) {
          FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
                        windowsErrorBuf, sizeof(windowsErrorBuf), NULL);
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not wait for subprocess with pid "
                    << external->_pid << ": " << windowsErrorBuf;
          status._errorMessage =
              std::string("could not wait for subprocess with pid ") +
              arangodb::basics::StringUtils::itoa(
                  static_cast<int64_t>(external->_pid)) +
              windowsErrorBuf;
          status._exitStatus = GetLastError();
        }
      } else {
        DWORD result;
        result = WaitForSingleObject(external->_process, 0);
        switch (result) {
          case WAIT_ABANDONED:
            wantGetExitCode = true;
            LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "WAIT_ABANDONED while waiting for subprocess with pid "
                      << external->_pid;
            break;
          case WAIT_OBJECT_0:
            /// this seems to be the exit case - want getExitCodeProcess here.
            wantGetExitCode = true;
            break;
          case WAIT_TIMEOUT:
            // success - everything went well.
            external->_exitStatus = 0;
            break;
          case WAIT_FAILED:
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
                          windowsErrorBuf, sizeof(windowsErrorBuf), NULL);
            LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not wait for subprocess with pid "
                      << external->_pid << ": " << windowsErrorBuf;
            status._errorMessage =
                std::string("could not wait for subprocess with PID '") +
                arangodb::basics::StringUtils::itoa(
                    static_cast<int64_t>(external->_pid)) +
                std::string("'") + windowsErrorBuf;
            status._exitStatus = GetLastError();
          default:
            wantGetExitCode = true;
            LOG_TOPIC(WARN, arangodb::Logger::FIXME)
                << "unexpected status while waiting for subprocess with pid "
                << external->_pid;
        }
      }
      if (wantGetExitCode) {
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(external->_process, &exitCode)) {
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "exit status could not be determined for pid "
                    << external->_pid;
          status._errorMessage =
              std::string("exit status could not be determined for pid ") +
              arangodb::basics::StringUtils::itoa(
                  static_cast<int64_t>(external->_pid));
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
      } else {
        external->_status = TRI_EXT_RUNNING;
      }
    }
#endif
  } else {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "unexpected process status " << external->_status << ": "
              << external->_exitStatus;
    status._errorMessage =
        std::string("unexpected process status ") +
        arangodb::basics::StringUtils::itoa(external->_status) +
        std::string(": ") +
        arangodb::basics::StringUtils::itoa(external->_exitStatus);
  }

  status._status = external->_status;
  status._exitStatus = external->_exitStatus;

  // Do we have to free our data?
  if (external->_status != TRI_EXT_RUNNING &&
      external->_status != TRI_EXT_STOPPED) {
    MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

    for (auto it = ExternalProcesses.begin(); it != ExternalProcesses.end();
         ++it) {
      if ((*it)->_pid == pid._pid) {
        ExternalProcesses.erase(it);
        break;
      }
    }

    FreeExternal(external);
  }

  return status;
}

#ifndef _WIN32
static bool OurKillProcess(TRI_external_t* pid, int signal) {
  if (0 == kill(pid->_pid, signal)) {
    int count;

    // Otherwise we just let it be.
    for (count = 0; count < 10; count++) {
      pid_t p;
      int loc;

      // And wait for it to avoid a zombie:
      sleep(1);
      p = waitpid(pid->_pid, &loc, WUNTRACED | WNOHANG);
      if (p == pid->_pid) {
        return true;
      }
      if (count == 8) {
        kill(pid->_pid, SIGKILL);
      }
    }
  }
  return false;
}
#else
static bool OurKillProcess(TRI_external_t* pid, int signal) {
  bool ok = true;
  UINT uExitCode = 0;
  DWORD exitCode;

  // kill worker process
  if (0 != TerminateProcess(pid->_process, uExitCode)) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "kill of worker process succeeded";
  } else {
    DWORD e1 = GetLastError();
    BOOL ok = GetExitCodeProcess(pid->_process, &exitCode);

    if (ok) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "worker process already dead: " << exitCode;
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "kill of worker process failed: " << exitCode;
      ok = false;
    }
  }
  return ok;
}

static bool OurKillProcessPID(DWORD pid) {
  HANDLE hProcess;
  UINT uExitCode = 0;

  hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if (hProcess != nullptr) {
    TerminateProcess(hProcess, uExitCode);
    CloseHandle(hProcess);
    return true;
  }
  return false;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief kills an external process
////////////////////////////////////////////////////////////////////////////////

bool TRI_KillExternalProcess(TRI_external_id_t pid, int signal) {
  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "killing process: " << pid._pid;

  TRI_external_t* external = nullptr;  // just to please the compiler
  {
    MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);

    for (auto it = ExternalProcesses.begin(); it != ExternalProcesses.end();
         ++it) {
      if ((*it)->_pid == pid._pid) {
        external = (*it);
        ExternalProcesses.erase(it);
        break;
      }
    }
  }

  if (external == nullptr) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "kill: process not found: " << pid._pid;
#ifndef _WIN32
    // Kill just in case:
    if (0 == kill(pid._pid, signal)) {
      // Otherwise we just let it be.
      for (int count = 0; count < 10; count++) {
        int loc;
        pid_t p;

        // And wait for it to avoid a zombie:
        sleep(1);
        p = waitpid(pid._pid, &loc, WUNTRACED | WNOHANG);
        if (p == pid._pid) {
          return true;
        }
        if (count == 8) {
          kill(pid._pid, SIGKILL);
        }
      }
    }
    return false;
#else
    return OurKillProcessPID(pid._pid);
#endif
  }

  bool ok = true;
  if (external->_status == TRI_EXT_RUNNING ||
      external->_status == TRI_EXT_STOPPED) {
    ok = OurKillProcess(external, signal);
  }

  FreeExternal(external);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops an external process, only on Unix
////////////////////////////////////////////////////////////////////////////////

bool TRI_SuspendExternalProcess(TRI_external_id_t pid) {
  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "suspending process: " << pid._pid;

#ifndef _WIN32
  return 0 == kill(pid._pid, SIGSTOP);
#else
  return true;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief continues an external process, only on Unix
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContinueExternalProcess(TRI_external_id_t pid) {
  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "continueing process: " << pid._pid;

#ifndef _WIN32
  return 0 == kill(pid._pid, SIGCONT);
#else
  return true;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the physical memory
////////////////////////////////////////////////////////////////////////////////

#if defined(TRI_HAVE_MACOS_MEM_STATS)

static uint64_t GetPhysicalMemory() {
  int mib[2];
  int64_t physicalMemory;
  size_t length;

  // Get the Physical memory size
  mib[0] = CTL_HW;
#ifdef TRI_HAVE_MACOS_MEM_STATS
  mib[1] = HW_MEMSIZE;
#else
  mib[1] = HW_PHYSMEM;  // The bytes of physical memory. (kenel + user space)
#endif
  length = sizeof(int64_t);
  sysctl(mib, 2, &physicalMemory, &length, nullptr, 0);

  return (uint64_t)physicalMemory;
}

#else
#ifdef TRI_HAVE_SC_PHYS_PAGES

static uint64_t GetPhysicalMemory() {
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);

  return (uint64_t)(pages * page_size);
}

#else
#ifdef TRI_HAVE_WIN32_GLOBAL_MEMORY_STATUS

static uint64_t GetPhysicalMemory() {
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);

  return (uint64_t)status.ullTotalPhys;
}

#else

static uint64_t GetPhysicalMemory() {
  PROCESS_MEMORY_COUNTERS pmc;
  memset(&result, 0, sizeof(result));
  pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms684874(v=vs.85).aspx
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, pmc.cb)) {
    return pmc.PeakWorkingSetSize;
  }
  return 0;
}
#endif
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the process components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeProcess() {
  TRI_PhysicalMemory = GetPhysicalMemory();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the process components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownProcess() {
  MUTEX_LOCKER(mutexLocker, ExternalProcessesLock);
  for (auto* e : ExternalProcesses) {
    FreeExternal(e);
  }
  ExternalProcesses.clear();
}
