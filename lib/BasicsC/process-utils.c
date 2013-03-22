////////////////////////////////////////////////////////////////////////////////
/// @brief collection of process functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Esteban Lombeyda
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "process-utils.h"

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef TRI_HAVE_MACH
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
// #include <mach/shared_memory_server.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#endif

#include "BasicsC/strings.h"
#include "BasicsC/logging.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SystemProcess
/// @{
////////////////////////////////////////////////////////////////////////////////

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
    unsigned long cutime;
    /* ld */
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
    unsigned long rsslim;
    unsigned long startcode;
    unsigned long endcode;
    unsigned long startstack;
    unsigned long kstkesp;
    unsigned long signal;
    /* obsolete lu*/
    unsigned long blocked;
    unsigned long sigignore;
    unsigned int sigcatch;
    unsigned long wchan;
    /* no maintained lu */
    unsigned long nswap;
    unsigned long cnswap;
    /* d */
    int exit_signal;
    int processor;
    /* u */
    unsigned rt_priority;
    unsigned policy;
    /* llu */
    long long unsigned int delayacct_blkio_ticks;
    /* lu */
    unsigned long guest_time;
    /* ld */
    long cguest_time;
}
process_state_t;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SystemProcess
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief original process name
////////////////////////////////////////////////////////////////////////////////

static char* ProcessName = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief argc
////////////////////////////////////////////////////////////////////////////////

static int ARGC = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief argv
////////////////////////////////////////////////////////////////////////////////

static char** ARGV = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief true, if environment has been copied already
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_TAMPER_WITH_ENVIRON
static bool IsEnvironmentEnlarged = false;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief do we need to free the copy of the environ data on shutdown
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_TAMPER_WITH_ENVIRON
static bool MustFreeEnvironment = false;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal size of the process title
////////////////////////////////////////////////////////////////////////////////

static size_t MaximalProcessTitleSize = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SystemProcess
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts usec and sec into seconds
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_GETRUSAGE

uint64_t TRI_MicrosecondsTv (struct timeval* tv) {
  time_t sec = tv->tv_sec;
  suseconds_t usec = tv->tv_usec;

  while (usec < 0) {
    usec += 1000000;
    sec  -= 1;
  }

  return (sec * 1000000LL) + usec;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the current process
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_LINUX_PROC

TRI_process_info_t TRI_ProcessInfoSelf () {
  return TRI_ProcessInfo(TRI_CurrentProcessId());
}

#elif TRI_HAVE_GETRUSAGE

TRI_process_info_t TRI_ProcessInfoSelf () {
  TRI_process_info_t result;
  struct rusage used;
  int res;

  memset(&result, 0, sizeof(result));
  result._scClkTck = 1000000;

  res = getrusage(RUSAGE_SELF, &used);

  if (res == 0) {
    result._minorPageFaults = used.ru_minflt;
    result._majorPageFaults = used.ru_majflt;

    result._systemTime = TRI_MicrosecondsTv(&used.ru_stime);
    result._userTime = TRI_MicrosecondsTv(&used.ru_utime);

    result._residentSize = used.ru_maxrss;
  }

#ifdef TRI_HAVE_MACH
  {
    int i;
    kern_return_t rc;
    thread_array_t array;
    mach_msg_type_number_t count;

    rc = task_threads(mach_task_self(), &array, &count);

    if (rc == KERN_SUCCESS) {
      result._numberThreads = count;

      for (i = 0;  i < count;  ++i) {
        mach_port_deallocate(mach_task_self(), array[i]);
      }

      vm_deallocate(mach_task_self(), (vm_address_t)array, sizeof(thread_t) * count);
    }
  }

  {
    kern_return_t rc;
    struct task_basic_info t_info;
/*
    struct host_basic_info h_info;
    struct vm_region_basic_info_64 vm_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    mach_msg_type_number_t h_info_count = HOST_BASIC_INFO_COUNT;
    mach_msg_type_number_t vm_info_count = VM_REGION_BASIC_INFO_COUNT_64;
    vm_address_t address = GLOBAL_SHARED_TEXT_SEGMENT;
    vm_size_t size;
    mach_port_t object_name;

    rc = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

    if (rc == KERN_SUCCESS) {
      rc = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)&h_info, &h_info_count);
      if (rc == KERN_SUCCESS) {
        rc = vm_region_64(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&vm_info, &vm_info_count, &object_name);

        if (rc == KERN_SUCCESS) {

          // check for firmware split libraries, this is copied from the ps source code
          if (vm_info.reserved
           && size == SHARED_TEXT_REGION_SIZE
           && t_info.virtual_size > (SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE)) {
		t_info.virtual_size -= (SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE);
          }

          result._virtualSize = t_info.virtual_size;
          result._residentSize = t_info.resident_size;
        }
      }
    }
*/
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    rc = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
    if (rc == KERN_SUCCESS) {
      result._virtualSize = t_info.virtual_size;
      result._residentSize = t_info.resident_size;
    }
    else {
      result._virtualSize = 0;
      result._residentSize = 0;
    }


  }
#endif

  return result;
}

#else

TRI_process_info_t TRI_ProcessInfoSelf () {
  TRI_process_info_t result;

  memset(&result, 0, sizeof(result));
  return result;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the process
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_LINUX_PROC

TRI_process_info_t TRI_ProcessInfo (TRI_pid_t pid) {
  char fn[1024];
  int fd;
  TRI_process_info_t result;

  memset(&result, 0, sizeof(result));

  snprintf(fn, sizeof(fn), "/proc/%d/stat", pid);
  fd = open(fn, O_RDONLY);

  if (fd > 0) {
    char str[1024];
    process_state_t st;
    size_t n;

    memset(&str, 0, sizeof(str));

    n = read(fd, str, sizeof(str));
    close(fd);

    if (n == 0) {
      return result;
    }

    sscanf(str, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %llu %lu %ld",
           &st.pid, (char*) &st.comm, &st.state, &st.ppid, &st.pgrp, &st.session, &st.tty_nr, &st.tpgid,
           &st.flags, &st.minflt, &st.cminflt, &st.majflt, &st.cmajflt, &st.utime, &st.stime,
           &st.cutime, &st.cstime, &st.priority, &st.nice, &st.num_threads, &st.itrealvalue,
           &st.starttime, &st.vsize, &st.rss);

    result._minorPageFaults = st.minflt;
    result._majorPageFaults = st.majflt;
    result._userTime = st.utime;
    result._systemTime = st.stime;
    result._numberThreads = st.num_threads;
    result._residentSize = st.rss;
    result._virtualSize = st.vsize;
    result._scClkTck = sysconf(_SC_CLK_TCK);
  }

  return result;
}

#else

TRI_process_info_t TRI_ProcessInfo (TRI_pid_t pid) {
  TRI_process_info_t result;

  memset(&result, 0, sizeof(result));
  result._scClkTck = 1;

  return result;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the size of the current process
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_ProcessSizeSelf () {
  return TRI_ProcessSize(TRI_CurrentProcessId());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the size of an process
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_ProcessSize (TRI_pid_t pid) {
  return TRI_ProcessInfo(pid)._virtualSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process name
////////////////////////////////////////////////////////////////////////////////

extern char** environ;

void TRI_SetProcessTitle (char const* title) {
#if TRI_TAMPER_WITH_ENVIRON

  if (! IsEnvironmentEnlarged) {
    size_t size;

    int envLen = -1;

    if (environ) {
      while (environ[++envLen]) {
        ;
      }
    }

    if (envLen > 0) {
      size = environ[envLen - 1] + strlen(environ[envLen - 1]) - ARGV[0];
    }
    else {
      size = ARGV[ARGC - 1] + strlen(ARGV[ARGC - 1]) - ARGV[0];
    }

    if (environ) {
      char** newEnviron = TRI_Allocate(TRI_CORE_MEM_ZONE, (envLen + 1) * sizeof(char*), false);
      size_t i = 0;

      while (environ[i]) {
        newEnviron[i] = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, environ[i]);
        ++i;
      }
      // pad with a null pointer so we know the end of the array
      newEnviron[i] = NULL;

      environ = newEnviron;
      MustFreeEnvironment = true;
    }

    IsEnvironmentEnlarged = true;
    MaximalProcessTitleSize = size;
  }

#else

  MaximalProcessTitleSize = ARGV[ARGC - 1] + strlen(ARGV[ARGC - 1]) - ARGV[0];

#endif

  if (0 < MaximalProcessTitleSize) {
    char* args = ARGV[0];

    memset(args, '\0', MaximalProcessTitleSize);
    snprintf(args, MaximalProcessTitleSize - 1, "%s", title);
  }

#ifdef TRI_HAVE_SYS_PRCTL_H
  prctl(PR_SET_NAME, title, 0, 0, 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SystemProcess
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the process components
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseProcess (int argc, char* argv[]) {
  if (ProcessName != 0) {
    return;
  }

  ProcessName = TRI_DuplicateString(argv[0]);
  ARGC = argc;
  ARGV = argv;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the process components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownProcess () {
  TRI_FreeString(TRI_CORE_MEM_ZONE, ProcessName);

#ifdef TRI_TAMPER_WITH_ENVIRON
  if (MustFreeEnvironment) {
    size_t i = 0;

    assert(environ);
    // free all arguments copied for environ

    while (environ[i]) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, environ[i]);
      ++i;
    }
    TRI_Free(TRI_CORE_MEM_ZONE, environ);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
