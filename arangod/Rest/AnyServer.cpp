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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "AnyServer.h"

#ifdef TRI_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "ApplicationServer/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/Logger.h"
#include "Basics/process-utils.h"

#include <fstream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a pid file
////////////////////////////////////////////////////////////////////////////////

static void WritePidFile(std::string const& pidFile, int pid) {
  std::ofstream out(pidFile.c_str(), std::ios::trunc);

  if (!out) {
    LOG(FATAL) << "cannot write pid-file '" << pidFile.c_str() << "'"; FATAL_ERROR_EXIT();
  }

  out << pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a pid file
////////////////////////////////////////////////////////////////////////////////

static void CheckPidFile(std::string const& pidFile) {
  // check if the pid-file exists
  if (!pidFile.empty()) {
    if (FileUtils::isDirectory(pidFile)) {
      LOG(FATAL) << "pid-file '" << pidFile.c_str() << "' is a directory"; FATAL_ERROR_EXIT();
    } else if (FileUtils::exists(pidFile) && FileUtils::size(pidFile) > 0) {
      LOG(INFO) << "pid-file '" << pidFile.c_str() << "' already exists, verifying pid";

      std::ifstream f(pidFile.c_str());

      // file can be opened
      if (f) {
        TRI_pid_t oldPid;

        f >> oldPid;

        if (oldPid == 0) {
          LOG(FATAL) << "pid-file '" << pidFile.c_str() << "' is unreadable"; FATAL_ERROR_EXIT();
        }

        LOG(DEBUG) << "found old pid: " << oldPid;

#ifdef TRI_HAVE_FORK
        int r = kill(oldPid, 0);
#else
        int r = 0;  // TODO for windows use TerminateProcess
#endif

        if (r == 0) {
          LOG(FATAL) << "pid-file '" << pidFile.c_str() << "' exists and process with pid " << oldPid << " is still running"; FATAL_ERROR_EXIT();
        } else if (errno == EPERM) {
          LOG(FATAL) << "pid-file '" << pidFile.c_str() << "' exists and process with pid " << oldPid << " is still running"; FATAL_ERROR_EXIT();
        } else if (errno == ESRCH) {
          LOG(ERR) << "pid-file '" << pidFile.c_str() << " exists, but no process with pid " << oldPid << " exists";

          if (!FileUtils::remove(pidFile)) {
            LOG(FATAL) << "pid-file '" << pidFile.c_str() << "' exists, no process with pid " << oldPid << " exists, but pid-file cannot be removed"; FATAL_ERROR_EXIT();
          }

          LOG(INFO) << "removed stale pid-file '" << pidFile.c_str() << "'";
        } else {
          LOG(FATAL) << "pid-file '" << pidFile.c_str() << "' exists and kill " << oldPid << " failed"; FATAL_ERROR_EXIT();
        }
      }

      // failed to open file
      else {
        LOG(FATAL) << "pid-file '" << pidFile.c_str() << "' exists, but cannot be opened"; FATAL_ERROR_EXIT();
      }
    }

    LOG(DEBUG) << "using pid-file '" << pidFile.c_str() << "'";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forks a new process
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_FORK

static int ForkProcess(std::string const& workingDirectory,
                       std::string& current) {
  // fork off the parent process
  TRI_pid_t pid = fork();

  if (pid < 0) {
    LOG(FATAL) << "cannot fork"; FATAL_ERROR_EXIT();
  }

  // Upon successful completion, fork() shall return 0 to the child process and
  // shall return the process ID of the child process to the parent process.

  // if we got a good PID, then we can exit the parent process
  if (pid > 0) {
    LOG(DEBUG) << "started child process with pid " << pid;
    return pid;
  }

  // change the file mode mask
  umask(0);

  // create a new SID for the child process
  TRI_pid_t sid = setsid();

  if (sid < 0) {
    LOG(FATAL) << "cannot create sid"; FATAL_ERROR_EXIT();
  }

  // store current working directory
  int err = 0;
  current = FileUtils::currentDirectory(&err);

  if (err != 0) {
    LOG(FATAL) << "cannot get current directory"; FATAL_ERROR_EXIT();
  }

  // change the current working directory
  if (!workingDirectory.empty()) {
    if (!FileUtils::changeDirectory(workingDirectory)) {
      LOG(FATAL) << "cannot change into working directory '" << workingDirectory.c_str() << "'"; FATAL_ERROR_EXIT();
    } else {
      LOG(INFO) << "changed working directory for child process to '" << workingDirectory.c_str() << "'";
    }
  }

  // we're a daemon so there won't be a terminal attached
  // close the standard file descriptors and re-open them mapped to /dev/null
  int fd = open("/dev/null", O_RDWR | O_CREAT, 0644);

  if (fd < 0) {
    LOG(FATAL) << "cannot open /dev/null"; FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDIN_FILENO) < 0) {
    LOG(FATAL) << "cannot re-map stdin to /dev/null"; FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDOUT_FILENO) < 0) {
    LOG(FATAL) << "cannot re-map stdout to /dev/null"; FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDERR_FILENO) < 0) {
    LOG(FATAL) << "cannot re-map stderr to /dev/null"; FATAL_ERROR_EXIT();
  }

  close(fd);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for the supervisor process with pid to return its exit status
/// waits for at most 10 seconds. if the supervisor has not returned until then,
/// we assume a successful start
////////////////////////////////////////////////////////////////////////////////

int WaitForSupervisor(int pid) {
  if (!isatty(STDIN_FILENO)) {
    // during system boot, we don't have a tty, and we don't want to delay
    // the boot process
    return EXIT_SUCCESS;
  }

  // in case a tty is present, this is probably a manual invocation of the start
  // procedure
  double const end = TRI_microtime() + 10.0;

  while (TRI_microtime() < end) {
    int status;
    int res = waitpid(pid, &status, WNOHANG);

    if (res == -1) {
      // error in waitpid. don't know what to do
      break;
    }

    if (res != 0 && WIFEXITED(status)) {
      // give information about supervisor exit code
      if (WEXITSTATUS(status) == 0) {
        // exit code 0
        return EXIT_SUCCESS;
      } else if (WIFSIGNALED(status)) {
        switch (WTERMSIG(status)) {
          case 2:
          case 9:
          case 15:
            // terminated normally
            return EXIT_SUCCESS;
          default:
            break;
        }
      }

      // failure!
      LOG(ERR) << "unable to start arangod. please check the logfiles for errors";
      return EXIT_FAILURE;
    }

    // sleep a while and retry
    usleep(500 * 1000);
  }

  // enough time has elapsed... we now abort our loop
  return EXIT_SUCCESS;
}

#else

// .............................................................................
// TODO: use windows API CreateProcess & CreateThread to minic fork()
// .............................................................................

static int ForkProcess(std::string const& workingDirectory,
                       std::string& current) {
  // fork off the parent process
  TRI_pid_t pid = -1;  // fork();

  if (pid < 0) {
    LOG(FATAL) << "cannot fork"; FATAL_ERROR_EXIT();
  }

  return 0;
}

#endif

AnyServer::AnyServer()
    : _mode(ServerMode::MODE_STANDALONE),
      _daemonMode(false),
      _supervisorMode(false),
      _pidFile(""),
      _workingDirectory(""),
      _applicationServer(nullptr) {}

AnyServer::~AnyServer() { delete _applicationServer; }

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the server
////////////////////////////////////////////////////////////////////////////////

int AnyServer::start() {
  startupProgress();

  if (_applicationServer == nullptr) {
    buildApplicationServer();
  }

  startupProgress();

  if (_supervisorMode) {
    return startupSupervisor();
  } else if (_daemonMode) {
    return startupDaemon();
  } else {
    _applicationServer->setupLogging(true, false, false);

    startupProgress();

    if (!_pidFile.empty()) {
      CheckPidFile(_pidFile);
      WritePidFile(_pidFile, TRI_CurrentProcessId());
    }

    startupProgress();

    int res = startupServer();

    if (!_pidFile.empty()) {
      if (!FileUtils::remove(_pidFile)) {
        LOG(DEBUG) << "cannot remove pid file '" << _pidFile.c_str() << "'";
      }
    }
    startupProgress();

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void AnyServer::beginShutdown() {
  if (_applicationServer != nullptr) {
    _applicationServer->beginShutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a supervisor
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_FORK

int AnyServer::startupSupervisor() {
  static time_t const MIN_TIME_ALIVE_IN_SEC = 30;

  LOG(INFO) << "starting up in supervisor mode";

  CheckPidFile(_pidFile);

  _applicationServer->setupLogging(false, true, false);

  std::string current;
  int result = ForkProcess(_workingDirectory, current);

  // main process
  if (result != 0) {
    // wait for a few seconds for the supervisor to return
    // if it returns within a reasonable time, we can fetch its exit code
    // and report it
    return WaitForSupervisor(result);
  }

  // child process
  else {
    setMode(ServerMode::MODE_SERVICE);

    time_t startTime = time(0);
    time_t t;
    bool done = false;
    result = 0;

    while (!done) {
      // fork of the server
      TRI_pid_t pid = fork();

      if (pid < 0) {
        TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
      }

      // parent
      if (0 < pid) {
        _applicationServer->setupLogging(false, true, true);
        TRI_SetProcessTitle("arangodb [supervisor]");
        LOG(DEBUG) << "supervisor mode: within parent";

        int status;
        waitpid(pid, &status, 0);
        bool horrible = true;

        if (WIFEXITED(status)) {
          // give information about cause of death
          if (WEXITSTATUS(status) == 0) {
            LOG(INFO) << "child " << pid << " died of natural causes";
            done = true;
            horrible = false;
          } else {
            t = time(0) - startTime;

            LOG(ERR) << "child " << pid << " died a horrible death, exit status " << WEXITSTATUS(status);

            if (t < MIN_TIME_ALIVE_IN_SEC) {
              LOG(ERR) << "child only survived for " << t << " seconds, this will not work - please fix the error first";
              done = true;
            } else {
              done = false;
            }
          }
        } else if (WIFSIGNALED(status)) {
          switch (WTERMSIG(status)) {
            case 2:
            case 9:
            case 15:
              LOG(INFO) << "child " << pid << " died of natural causes, exit status " << WTERMSIG(status);
              done = true;
              horrible = false;
              break;

            default:
              t = time(0) - startTime;

              LOG(ERR) << "child " << pid << " died a horrible death, signal " << WTERMSIG(status);

              if (t < MIN_TIME_ALIVE_IN_SEC) {
                LOG(ERR) << "child only survived for " << t << " seconds, this will not work - please fix the error first";
                done = true;

#ifdef WCOREDUMP
                if (WCOREDUMP(status)) {
                  LOG(WARN) << "child process " << pid << " produced a core dump";
                }
#endif
              } else {
                done = false;
              }

              break;
          }
        } else {
          LOG(ERR) << "child " << pid << " died a horrible death, unknown cause";
          done = false;
        }

        // remove pid file
        if (horrible) {
          if (!FileUtils::remove(_pidFile)) {
            LOG(DEBUG) << "cannot remove pid file '" << _pidFile.c_str() << "'";
          }

          result = EXIT_FAILURE;
        }
      }

      // child
      else {
        _applicationServer->setupLogging(true, false, true);
        LOG(DEBUG) << "supervisor mode: within child";

        // write the pid file
        WritePidFile(_pidFile, TRI_CurrentProcessId());

// force child to stop if supervisor dies
#ifdef TRI_HAVE_PRCTL
        prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);
#endif

        // startup server
        result = startupServer();

        // remove pid file
        if (!FileUtils::remove(_pidFile)) {
          LOG(DEBUG) << "cannot remove pid file '" << _pidFile.c_str() << "'";
        }

        // and stop
        TRI_EXIT_FUNCTION(result, NULL);
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a daemon
////////////////////////////////////////////////////////////////////////////////

int AnyServer::startupDaemon() {
  LOG(INFO) << "starting up in daemon mode";

  CheckPidFile(_pidFile);

  _applicationServer->setupLogging(false, true, false);

  std::string current;
  int result = ForkProcess(_workingDirectory, current);

  // main process
  if (result != 0) {
    TRI_SetProcessTitle("arangodb [daemon]");
    WritePidFile(_pidFile, result);

    // issue #549: this is used as the exit code
    result = 0;
  }

  // child process
  else {
    setMode(ServerMode::MODE_SERVICE);
    _applicationServer->setupLogging(true, false, true);
    LOG(DEBUG) << "daemon mode: within child";

    // and startup server
    result = startupServer();

    // remove pid file
    if (!FileUtils::remove(_pidFile)) {
      LOG(DEBUG) << "cannot remove pid file '" << _pidFile.c_str() << "'";
    }
  }

  return result;
}

#else

int AnyServer::startupSupervisor() { return 0; }

int AnyServer::startupDaemon() { return 0; }

#endif
