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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <optional>

#include "Basics/Common.h"
#include "Basics/threads.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid process id
////////////////////////////////////////////////////////////////////////////////

#define TRI_INVALID_PROCESS_ID (-1)

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the process
////////////////////////////////////////////////////////////////////////////////

struct ProcessInfo {
  uint64_t _minorPageFaults;
  uint64_t _majorPageFaults;
  uint64_t _userTime;
  uint64_t _systemTime;
  int64_t _numberThreads;
  int64_t _residentSize;  // resident set size in number of bytes
  uint64_t _virtualSize;
  uint64_t _scClkTck;

  ProcessInfo();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief status of an external process
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_EXT_NOT_STARTED = 0,  // not yet started
  TRI_EXT_PIPE_FAILED = 1,  // pipe before start failed
  TRI_EXT_FORK_FAILED = 2,  // fork failed
  TRI_EXT_RUNNING = 3,      // running
  TRI_EXT_NOT_FOUND = 4,    // unknown pid
  TRI_EXT_TERMINATED = 5,   // process has terminated normally
  TRI_EXT_ABORTED = 6,      // process has terminated abnormally
  TRI_EXT_STOPPED = 7,      // process has been stopped
  TRI_EXT_TIMEOUT = 9       // waiting for the process timed out
} TRI_external_status_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief identifier of an external process
////////////////////////////////////////////////////////////////////////////////

struct ExternalId {
  TRI_pid_t _pid;
  int _readPipe;
  int _writePipe;
  ExternalId();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief external process description
////////////////////////////////////////////////////////////////////////////////

struct ExternalProcess : public ExternalId {
  std::string _executable;
  size_t _numberArguments = 0;
  char** _arguments = nullptr;

  TRI_external_status_e _status = TRI_EXT_NOT_STARTED;
  int64_t _exitStatus = 0;

  ExternalProcess(ExternalProcess const& other) = delete;
  ExternalProcess& operator=(ExternalProcess const& other) = delete;

  ExternalProcess() = default;
  ~ExternalProcess();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief external process status
////////////////////////////////////////////////////////////////////////////////

extern std::vector<ExternalProcess*> ExternalProcesses;
extern std::mutex ExternalProcessesLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief external process status
////////////////////////////////////////////////////////////////////////////////

struct ExternalProcessStatus {
  TRI_external_status_e _status = TRI_EXT_NOT_STARTED;
  int64_t _exitStatus = 0;
  std::string _errorMessage;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts usec and sec into seconds
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_HAVE_GETRUSAGE
uint64_t TRI_MicrosecondsTv(struct timeval* tv);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the current process
////////////////////////////////////////////////////////////////////////////////

ProcessInfo TRI_ProcessInfoSelf();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the process
////////////////////////////////////////////////////////////////////////////////

ProcessInfo TRI_ProcessInfo(TRI_pid_t pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up process in the process list
////////////////////////////////////////////////////////////////////////////////

ExternalProcess* TRI_LookupSpawnedProcess(TRI_pid_t pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up process in the process list
////////////////////////////////////////////////////////////////////////////////

std::optional<ExternalProcessStatus> TRI_LookupSpawnedProcessStatus(
    TRI_pid_t pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process name
////////////////////////////////////////////////////////////////////////////////

void TRI_SetProcessTitle(char const* title);

////////////////////////////////////////////////////////////////////////////////
/// @brief starts an external process
/// `fileForStdErr` is a file name, to which we will redirect stderr of the
/// external process, if the string is non-empty.
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateExternalProcess(
    char const* executable, std::vector<std::string> const& arguments,
    std::vector<std::string> const& additionalEnv, bool usePipes,
    ExternalId* pid, std::string const& fileForStdErr = std::string());

////////////////////////////////////////////////////////////////////////////////
/// @brief Reads from the pipe of processes
////////////////////////////////////////////////////////////////////////////////

void TRI_ClosePipe(ExternalProcess* process, bool read);
////////////////////////////////////////////////////////////////////////////////
/// @brief Reads from the pipe of processes
////////////////////////////////////////////////////////////////////////////////

TRI_read_return_t TRI_ReadPipe(ExternalProcess const* process, char* buffer,
                               size_t bufferSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief Reads from the pipe of processes
////////////////////////////////////////////////////////////////////////////////

bool TRI_WritePipe(ExternalProcess const* process, char const* buffer,
                   size_t bufferSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of an external process
////////////////////////////////////////////////////////////////////////////////

ExternalProcessStatus TRI_CheckExternalProcess(
    ExternalId pid, bool wait, uint32_t timeout,
    std::function<bool()> const& deadlineReached = {});

////////////////////////////////////////////////////////////////////////////////
/// @brief kills an external process
////////////////////////////////////////////////////////////////////////////////

ExternalProcessStatus TRI_KillExternalProcess(ExternalId pid, int signal,
                                              bool isTerminal);

////////////////////////////////////////////////////////////////////////////////
/// @brief suspends an external process, only on Unix
////////////////////////////////////////////////////////////////////////////////

bool TRI_SuspendExternalProcess(ExternalId pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief continue an external process, only on Unix
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContinueExternalProcess(ExternalId pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the process components
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownProcess();

////////////////////////////////////////////////////////////////////////////////
/// @brief change the process priority using setpriority / SetPriorityClass
////////////////////////////////////////////////////////////////////////////////

std::string TRI_SetPriority(ExternalId pid, int prio);

inline bool noDeadLine() { return false; }
