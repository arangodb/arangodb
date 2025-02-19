////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>
#include <absl/strings/str_cat.h>

#include "process-utils.h"

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

#include <sys/resource.h>
#include <fcntl.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "Basics/NumberUtils.h"
#include "Basics/PageSize.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/memory.h"
#include "Basics/operating-system.h"
#include "Basics/signals.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Containers/FlatHashMap.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <algorithm>
#include <string_view>

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

}  // namespace

/// @brief all external processes
std::vector<ExternalProcess*> ExternalProcesses;

/// @brief lock for protected access to vector ExternalProcesses
std::mutex ExternalProcessesLock;

ProcessInfo::ProcessInfo()
    : _minorPageFaults(0),
      _majorPageFaults(0),
      _userTime(0),
      _systemTime(0),
      _numberThreads(0),
      _residentSize(0),  // resident set size in number of bytes
      _virtualSize(0),
      _scClkTck(0) {}

ExternalId::ExternalId() : _pid(0), _readPipe(-1), _writePipe(-1) {}

ExternalProcess::~ExternalProcess() {
  TRI_ASSERT(_numberArguments == 0 || _arguments != nullptr);
  for (size_t i = 0; i < _numberArguments; i++) {
    if (_arguments[i] != nullptr) {
      TRI_Free(_arguments[i]);
    }
  }
  if (_arguments) {
    TRI_Free(_arguments);
  }

  if (_readPipe != -1) {
    close(_readPipe);
  }
  if (_writePipe != -1) {
    close(_writePipe);
  }
}

ExternalProcess* TRI_LookupSpawnedProcess(TRI_pid_t pid) {
  {
    std::lock_guard guard{ExternalProcessesLock};
    auto found = std::find_if(
        ExternalProcesses.begin(), ExternalProcesses.end(),
        [pid](const ExternalProcess* m) -> bool { return m->_pid == pid; });
    if (found != ExternalProcesses.end()) {
      return *found;
    }
  }
  return nullptr;
}

std::optional<ExternalProcessStatus> TRI_LookupSpawnedProcessStatus(
    TRI_pid_t pid) {
  std::lock_guard guard{ExternalProcessesLock};
  auto found = std::find_if(
      ExternalProcesses.begin(), ExternalProcesses.end(),
      [pid](ExternalProcess const* m) -> bool { return m->_pid == pid; });
  if (found != ExternalProcesses.end()) {
    ExternalProcessStatus ret;
    ret._status = (*found)->_status;
    ret._exitStatus = (*found)->_exitStatus;
    return std::optional<ExternalProcessStatus>{ret};
  }
  return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates pipe pair
////////////////////////////////////////////////////////////////////////////////

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

static void StartExternalProcessPosixSpawn(
    ExternalProcess* external, bool usePipes,
    std::vector<std::string> const& additionalEnv,
    std::string const& fileForStdErr) {
  int pipe_server_to_child[2];
  int pipe_child_to_server[2];

  if (usePipes) {
    bool ok = CreatePipes(pipe_server_to_child, pipe_child_to_server);

    if (!ok) {
      external->_status = TRI_EXT_PIPE_FAILED;
      return;
    }
  }

  int err = 0;  // accumulate any errors we might get
  posix_spawn_file_actions_t file_actions;
  err |= posix_spawn_file_actions_init(&file_actions);
  // file actions are performed in order they were added.

  if (usePipes) {
    err |= posix_spawn_file_actions_adddup2(&file_actions,
                                            pipe_server_to_child[0], 0);
    err |= posix_spawn_file_actions_adddup2(&file_actions,
                                            pipe_child_to_server[1], 1);

    err |= posix_spawn_file_actions_addclose(&file_actions,
                                             pipe_server_to_child[0]);
    err |= posix_spawn_file_actions_addclose(&file_actions,
                                             pipe_server_to_child[1]);
    err |= posix_spawn_file_actions_addclose(&file_actions,
                                             pipe_child_to_server[0]);
    err |= posix_spawn_file_actions_addclose(&file_actions,
                                             pipe_child_to_server[1]);
  } else {
    err |= posix_spawn_file_actions_addopen(&file_actions, 0, "/dev/null",
                                            O_RDONLY, 0);
  }

  if (!fileForStdErr.empty()) {
    err |= posix_spawn_file_actions_addopen(&file_actions, 2,
                                            fileForStdErr.c_str(),
                                            O_CREAT | O_WRONLY | O_TRUNC, 0644);
  }

  posix_spawnattr_t spawn_attrs;
  err |= posix_spawnattr_init(&spawn_attrs);
  err |= posix_spawnattr_setflags(
      &spawn_attrs, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
  sigset_t all;
  sigfillset(&all);
  err |= posix_spawnattr_setsigdefault(&spawn_attrs, &all);
  sigset_t none;
  sigemptyset(&none);
  err |= posix_spawnattr_setsigmask(&spawn_attrs, &none);

  ScopeGuard cleanup([&]() noexcept {
    posix_spawnattr_destroy(&spawn_attrs);
    posix_spawn_file_actions_destroy(&file_actions);
  });

  if (err != 0) {
    external->_status = TRI_EXT_PIPE_FAILED;
    if (usePipes) {
      close(pipe_server_to_child[0]);
      close(pipe_server_to_child[1]);
      close(pipe_child_to_server[0]);
      close(pipe_child_to_server[1]);
    }
    return;
  }

  auto extractName = [](char const* value) noexcept {
    std::string_view v;
    if (value != nullptr) {
      v = value;
      if (auto pos = v.find('='); pos != std::string_view::npos) {
        v = v.substr(0, pos);
      }
    }
    return v;
  };

  // note the position in the envs vector for every unique environment variable.
  // we do this to make the passed environment variables unique, e.g. in case
  // the original env contains a setting with is later overriden by
  // additionalEnv. in this case we want additionalEnv to win.
  containers::FlatHashMap<std::string_view, size_t> positions;

  std::vector<char*> envs;
  for (char** e = environ; *e != nullptr; ++e) {
    positions.insert_or_assign(extractName(*e), envs.size());
    envs.push_back(*e);
  }

  envs.reserve(envs.size() + additionalEnv.size() + 1);
  for (auto const& e : additionalEnv) {
    auto name = extractName(e.data());
    if (auto it = positions.find(name); it != positions.end()) {
      // environment variable already set. now update it
      envs[it->second] = const_cast<char*>(e.data());
    } else {
      // new environment variable
      positions.emplace(name, envs.size());
      envs.push_back(const_cast<char*>(e.data()));
    }
  }

  TRI_ASSERT(std::unique(envs.begin(), envs.end(),
                         [&](char const* lhs, char const* rhs) {
                           return extractName(lhs) == extractName(rhs);
                         }) == envs.end());

  // terminate the array with a null pointer entry. this is required for
  // posix_spawnp
  envs.emplace_back(nullptr);

  int result = posix_spawnp(&external->_pid, external->_executable.c_str(),
                            &file_actions, &spawn_attrs, external->_arguments,
                            envs.data());

  if (result != 0) {
    int errnoCopy = errno;
    if (errnoCopy == ENOENT) {
      // We fake the old legacy behaviour here from the fork/exec times:
      external->_status = TRI_EXT_TERMINATED;
      external->_exitStatus = 1;
      LOG_TOPIC("e3a2c", ERR, arangodb::Logger::FIXME)
          << "spawn failed: executable '" << external->_executable
          << "' not found";
    } else {
      external->_status = TRI_EXT_FORK_FAILED;

      LOG_TOPIC("e3a2b", ERR, arangodb::Logger::FIXME)
          << "spawning of executable '" << external->_executable
          << "' failed: " << strerror(errnoCopy);
    }
    if (usePipes) {
      close(pipe_server_to_child[0]);
      close(pipe_server_to_child[1]);
      close(pipe_child_to_server[0]);
      close(pipe_child_to_server[1]);
    }

    return;
  }

  LOG_TOPIC("ac58b", DEBUG, arangodb::Logger::FIXME)
      << "spawning executable '" << external->_executable
      << "' succeeded, child pid: " << external->_pid;

  if (usePipes) {
    close(pipe_server_to_child[0]);
    close(pipe_child_to_server[1]);

    external->_writePipe = pipe_server_to_child[1];
    external->_readPipe = pipe_child_to_server[0];
  } else {
    external->_writePipe = -1;
    external->_readPipe = -1;
  }

  external->_status = TRI_EXT_RUNNING;
}

[[maybe_unused]] static void StartExternalProcess(
    ExternalProcess* external, bool usePipes,
    std::vector<std::string> const& additionalEnv,
    std::string const& fileForStdErr) {
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
      {  // "close" stdin, but avoid fd 0 being reused!
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) {
          dup2(fd, 0);
          close(fd);
        }
      }
      fcntl(1, F_SETFD, 0);
      fcntl(2, F_SETFD, 0);
    }
    if (!fileForStdErr.empty()) {
      // Redirect stderr:
      int fd = open(fileForStdErr.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
      if (fd >= 0) {
        dup2(fd, 2);  // note that 2 was open before, so fd is not equal to 2,
                      // and the previous 2 is now silently being closed!
                      // Furthermore, if this fails, we stay on the other one.
        close(fd);    // need to get rid of the second file descriptor.
      }
    }

    // add environment variables
    for (auto const& it : additionalEnv) {
      putenv(TRI_DuplicateString(it.c_str(), it.size()));
    }

    arangodb::signals::unmaskAllSignals();

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

[[maybe_unused]] static time_t _FileTime_to_POSIX(FILETIME* ft) {
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
  if (GetProcessMemoryInfo(processHandle, (PPROCESS_MEMORY_COUNTERS)&pmc,
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
  if (GetProcessTimes(processHandle, &creationTime, &exitTime, &kernelTime,
                      &userTime)) {
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
    } else {
      LOG_TOPIC("66667", ERR, arangodb::Logger::FIXME)
          << "failed to acquire thread from snapshot - " << GetLastError();
    }
    CloseHandle(snapShot);
  } else {
    LOG_TOPIC("66668", ERR, arangodb::Logger::FIXME)
        << "failed to acquire process threads count - " << GetLastError();
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

    auto n = TRI_READ(fd, str, static_cast<TRI_read_t>(sizeof(str)));
    close(fd);

    if (n == 0) {
      return result;
    }

    /// buffer now contains all data documented by "proc"
    /// see man 5 proc for the state of a process

    char const* p = &str[0];
    char const* e = p + n;

    skipEntry(p, e);                                      // process id
    skipEntry(p, e);                                      // process name
    skipEntry(p, e);                                      // process state
    skipEntry(p, e);                                      // ppid
    skipEntry(p, e);                                      // pgrp
    skipEntry(p, e);                                      // session
    skipEntry(p, e);                                      // tty nr
    skipEntry(p, e);                                      // tpgid
    skipEntry(p, e);                                      // flags
    result._minorPageFaults = readEntry<uint64_t>(p, e);  // min flt
    skipEntry(p, e);                                      // cmin flt
    result._majorPageFaults = readEntry<uint64_t>(p, e);  // maj flt
    skipEntry(p, e);                                      // cmaj flt
    result._userTime = readEntry<uint64_t>(p, e);         // utime
    result._systemTime = readEntry<uint64_t>(p, e);       // stime
    skipEntry(p, e);                                      // cutime
    skipEntry(p, e);                                      // cstime
    skipEntry(p, e);                                      // priority
    skipEntry(p, e);                                      // nice
    result._numberThreads = readEntry<int64_t>(p, e);     // num threads
    skipEntry(p, e);                                      // itrealvalue
    skipEntry(p, e);                                      // starttime
    result._virtualSize = readEntry<uint64_t>(p, e);      // vsize
    result._residentSize =
        readEntry<int64_t>(p, e) * PageSize::getValue();  // rss
    result._scClkTck = sysconf(_SC_CLK_TCK);
  }

  return result;
}

#else
ProcessInfo TRI_ProcessInfo(TRI_pid_t pid) {
  ProcessInfo result;

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
/// @brief starts an external process
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateExternalProcess(char const* executable,
                               std::vector<std::string> const& arguments,
                               std::vector<std::string> const& additionalEnv,
                               bool usePipes, ExternalId* pid,
                               std::string const& fileForStdErr) {
  size_t const n = arguments.size();
  // create the external structure
  auto external = std::make_unique<ExternalProcess>();
  external->_executable = executable;
  external->_numberArguments = n + 1;

  external->_arguments =
      static_cast<char**>(TRI_Allocate((n + 2) * sizeof(char*)));

  if (external->_arguments == nullptr) {
    // gracefully handle out of memory
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }

  memset(external->_arguments, 0, (n + 2) * sizeof(char*));

  external->_arguments[0] = TRI_DuplicateString(executable, strlen(executable));
  if (external->_arguments[0] == nullptr) {
    // OOM
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }

  for (size_t i = 0; i < n; ++i) {
    external->_arguments[i + 1] =
        TRI_DuplicateString(arguments[i].c_str(), arguments[i].size());
    if (external->_arguments[i + 1] == nullptr) {
      // OOM
      pid->_pid = TRI_INVALID_PROCESS_ID;
      return;
    }
  }

  external->_arguments[n + 1] = nullptr;
  external->_status = TRI_EXT_NOT_STARTED;

  StartExternalProcessPosixSpawn(external.get(), usePipes, additionalEnv,
                                 fileForStdErr);

  if (external->_status != TRI_EXT_RUNNING &&
      external->_status != TRI_EXT_TERMINATED) {
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

  std::lock_guard guard{ExternalProcessesLock};

  try {
    ExternalProcesses.push_back(external.get());
    external.release();
  } catch (...) {
    pid->_pid = TRI_INVALID_PROCESS_ID;
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reads from the pipe of processes
////////////////////////////////////////////////////////////////////////////////

void TRI_ClosePipe(ExternalProcess* process, bool read) {
  if (process == nullptr || (read && TRI_IS_INVALID_PIPE(process->_readPipe)) ||
      (!read && TRI_IS_INVALID_PIPE(process->_writePipe))) {
    return;
  }

  auto pipe = (read) ? &process->_readPipe : &process->_writePipe;

  if (*pipe != -1) {
    FILE* stream = fdopen(*pipe, "w");
    if (stream != nullptr) {
      fflush(stream);
    }
    close(*pipe);
    *pipe = -1;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Reads from the pipe of processes
////////////////////////////////////////////////////////////////////////////////

TRI_read_return_t TRI_ReadPipe(ExternalProcess const* process, char* buffer,
                               size_t bufferSize) {
  if (process == nullptr || TRI_IS_INVALID_PIPE(process->_readPipe)) {
    return 0;
  }

  memset(buffer, 0, bufferSize);

  return TRI_ReadPointer(process->_readPipe, buffer, bufferSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes from the pipe of processes
////////////////////////////////////////////////////////////////////////////////

bool TRI_WritePipe(ExternalProcess const* process, char const* buffer,
                   size_t bufferSize) {
  if (process == nullptr || TRI_IS_INVALID_PIPE(process->_writePipe)) {
    return false;
  }

  return TRI_WritePointer(process->_writePipe, buffer, bufferSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of an external process
////////////////////////////////////////////////////////////////////////////////

ExternalProcessStatus TRI_CheckExternalProcess(
    ExternalId pid, bool wait, uint32_t timeout,
    std::function<bool()> const& deadlineReached) {
  auto status = TRI_LookupSpawnedProcessStatus(pid._pid);

  if (!status.has_value()) {
    LOG_TOPIC("f5f99", WARN, arangodb::Logger::FIXME)
        << "checkExternal: pid not found: " << pid._pid;
    return ExternalProcessStatus{
        TRI_EXT_NOT_FOUND, -1,
        absl::StrCat("the pid you're looking for is not in our list: ",
                     pid._pid)};
  }

  if (status->_status == TRI_EXT_RUNNING ||
      status->_status == TRI_EXT_STOPPED) {
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
        res = waitpid(pid._pid, &loc, opts);
        if (res != 0) {
          break;
        }
        double now = TRI_microtime();
        if (endTime == 0.0) {
          endTime = now + timeout / 1000.0;
        } else if (now >= endTime) {
          res = pid._pid;
          timeoutHappened = true;
          break;
        }
        if (deadlineReached) {
          timeoutHappened = deadlineReached();
          if (timeoutHappened) {
            break;
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    } else {
      res = waitpid(pid._pid, &loc, opts);
    }

    if (res == 0) {
      if (wait) {
        status->_errorMessage = absl::StrCat(
            "waitpid returned 0 for pid while it shouldn't ", pid._pid);
        if (timeoutHappened) {
          status->_status = TRI_EXT_TIMEOUT;
          status->_exitStatus = -1;
        } else if (WIFEXITED(loc)) {
          status->_status = TRI_EXT_TERMINATED;
          status->_exitStatus = WEXITSTATUS(loc);
        } else if (WIFSIGNALED(loc)) {
          status->_status = TRI_EXT_ABORTED;
          status->_exitStatus = WTERMSIG(loc);
        } else if (WIFSTOPPED(loc)) {
          status->_status = TRI_EXT_STOPPED;
          status->_exitStatus = 0;
        } else {
          status->_status = TRI_EXT_ABORTED;
          status->_exitStatus = 0;
        }
      } else {
        status->_exitStatus = 0;
      }
    } else if (res == -1) {
      if (errno == ECHILD) {
        status->_status = TRI_EXT_NOT_FOUND;
      }
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("308ea", WARN, arangodb::Logger::FIXME)
          << "waitpid returned error for pid " << pid._pid << " (" << wait
          << "): " << TRI_last_error();
      status->_errorMessage = absl::StrCat("waitpid returned error for pid ",
                                           pid._pid, ": ", TRI_last_error());
    } else if (static_cast<TRI_pid_t>(pid._pid) ==
               static_cast<TRI_pid_t>(res)) {
      if (timeoutHappened) {
        status->_status = TRI_EXT_TIMEOUT;
        status->_exitStatus = -1;
      } else if (WIFEXITED(loc)) {
        status->_status = TRI_EXT_TERMINATED;
        status->_exitStatus = WEXITSTATUS(loc);
      } else if (WIFSIGNALED(loc)) {
        status->_status = TRI_EXT_ABORTED;
        status->_exitStatus = WTERMSIG(loc);
      } else if (WIFSTOPPED(loc)) {
        status->_status = TRI_EXT_STOPPED;
        status->_exitStatus = 0;
      } else {
        status->_status = TRI_EXT_ABORTED;
        status->_exitStatus = 0;
      }
    } else {
      LOG_TOPIC("0ab33", WARN, arangodb::Logger::FIXME)
          << "unexpected waitpid result for pid " << pid._pid << ": " << res;
      status->_errorMessage = absl::StrCat("unexpected waitpid result for pid ",
                                           pid._pid, ": ", res);
    }
  } else {
    LOG_TOPIC("1cff4", WARN, arangodb::Logger::FIXME)
        << "unexpected process status " << status->_status << ": "
        << status->_exitStatus;
    status->_errorMessage =
        absl::StrCat("unexpected process status ", status->_status, ": ",
                     status->_exitStatus);
  }

  // Persist our fresh status or unlink the process
  ExternalProcess* deleteMe = nullptr;
  {
    std::lock_guard guard{ExternalProcessesLock};

    auto found =
        std::find_if(ExternalProcesses.begin(), ExternalProcesses.end(),
                     [pid](ExternalProcess const* m) -> bool {
                       return (m->_pid == pid._pid);
                     });
    if (found != ExternalProcesses.end()) {
      if ((status->_status != TRI_EXT_RUNNING) &&
          (status->_status != TRI_EXT_STOPPED) &&
          (status->_status != TRI_EXT_TIMEOUT)) {
        deleteMe = *found;
        std::swap(*found, ExternalProcesses.back());
        ExternalProcesses.pop_back();
      } else {
        (*found)->_status = status->_status;
        (*found)->_exitStatus = status->_exitStatus;
      }
    }
  }
  if (deleteMe) {
    delete deleteMe;
  }

  return *status;
}

////////////////////////////////////////////////////////////////////////////////
// @brief check for a process we didn't spawn, and check for access rights to
//        send it signals.
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

/// @brief check for a process we didn't spawn, and check for access rights to
/// send it signals.
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

////////////////////////////////////////////////////////////////////////////////
/// @brief kills an external process
////////////////////////////////////////////////////////////////////////////////

ExternalProcessStatus TRI_KillExternalProcess(ExternalId pid, int signal,
                                              bool isTerminal) {
  LOG_TOPIC("77bc5", DEBUG, arangodb::Logger::FIXME)
      << "Sending process: " << pid._pid << " the signal: " << signal;

  ExternalProcess* external = nullptr;
  {
    std::lock_guard guard{ExternalProcessesLock};

    for (auto it = ExternalProcesses.begin(); it != ExternalProcesses.end();
         ++it) {
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
        << "kill: process not found: " << pid._pid
        << " in our starting table - adding";

    // ok, we didn't spawn it, but now we claim the
    // ownership.
    std::lock_guard guard{ExternalProcessesLock};

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
      ExternalProcessStatus status =
          TRI_CheckExternalProcess(pid, false, 0, noDeadLine);
      if (!isTerminal) {
        // we just sent a signal, don't care whether
        // the process is gone by now.
        return status;
      }
      if ((status._status == TRI_EXT_TERMINATED) ||
          (status._status == TRI_EXT_ABORTED) ||
          (status._status == TRI_EXT_NOT_FOUND)) {
        // Its dead and gone - good.
        std::lock_guard guard{ExternalProcessesLock};
        for (auto it = ExternalProcesses.begin(); it != ExternalProcesses.end();
             ++it) {
          if (*it == external) {
            std::swap(*it, ExternalProcesses.back());
            ExternalProcesses.pop_back();
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
  return TRI_CheckExternalProcess(pid, false, 0, noDeadLine);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops an external process
////////////////////////////////////////////////////////////////////////////////

bool TRI_SuspendExternalProcess(ExternalId pid) {
  LOG_TOPIC("13e36", DEBUG, arangodb::Logger::FIXME)
      << "suspending process: " << pid._pid;

  return 0 == kill(pid._pid, SIGSTOP);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief continues an external process
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContinueExternalProcess(ExternalId pid) {
  LOG_TOPIC("45884", DEBUG, arangodb::Logger::FIXME)
      << "continueing process: " << pid._pid;

  return 0 == kill(pid._pid, SIGCONT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the process components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownProcess() {
  std::lock_guard guard{ExternalProcessesLock};
  for (auto* external : ExternalProcesses) {
    delete external;
  }
  ExternalProcesses.clear();
}

std::string TRI_SetPriority(ExternalId pid, int prio) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  errno = 0;
  int ret = setpriority(PRIO_PROCESS, pid._pid, prio);
  if (ret == -1) {
    int err = errno;
    if (err != 0) {
      std::stringstream ss;
      ss << "setting process priority for : '" << pid._pid
         << "' failed with error: " << strerror(err);
      return ss.str();
    }
  }
  return "";
#else
  return "only available in maintainer mode";
#endif
}
