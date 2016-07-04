////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "DaemonFeature.h"

#include <iostream>
#include <fstream>

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

DaemonFeature::DaemonFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Daemon") {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
  startsAfter("WorkMonitor");

#ifndef _WIN32
  _workingDirectory = "/var/tmp";
#endif
}

void DaemonFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addHiddenOption("--daemon",
                           "background the server, running it as daemon",
                           new BooleanParameter(&_daemon));

  options->addHiddenOption("--pid-file", "pid-file in daemon mode",
                           new StringParameter(&_pidFile));

  options->addHiddenOption("--working-directory",
                           "working directory in daemon mode",
                           new StringParameter(&_workingDirectory));
}

void DaemonFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_daemon) {
    return;
  }
  
  if (_pidFile.empty()) {
    LOG(FATAL) << "need --pid-file in --daemon mode";
    FATAL_ERROR_EXIT();
  }

  LoggerFeature* logger = ApplicationServer::getFeature<LoggerFeature>("Logger");
  logger->setBackgrounded(true);

  // make the pid filename absolute
  int err = 0;
  std::string currentDir = FileUtils::currentDirectory(&err);

  char* absoluteFile =
    TRI_GetAbsolutePath(_pidFile.c_str(), currentDir.c_str());

  if (absoluteFile != nullptr) {
    _pidFile = std::string(absoluteFile);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, absoluteFile);
    LOG(DEBUG) << "using absolute pid file '" << _pidFile << "'";
  } else {
    LOG(FATAL) << "cannot determine absolute path";
    FATAL_ERROR_EXIT();
  }
}

void DaemonFeature::daemonize() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::daemonize";

  if (!_daemon) {
    return;
  }

  LOG_TOPIC(INFO, Logger::STARTUP) << "starting up in daemon mode";

  checkPidFile();

  int pid = forkProcess();

  // main process
  if (pid != 0) {
    TRI_SetProcessTitle("arangodb [daemon]");
    writePidFile(pid);

    int result = waitForChildProcess(pid);

    exit(result);
  }

  // child process
  else {
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "daemon mode continuing within child";
  }
}

void DaemonFeature::unprepare() {
  if (!_daemon) {
    return;
  }

  // remove pid file
  if (!FileUtils::remove(_pidFile)) {
    LOG(ERR) << "cannot remove pid file '" << _pidFile << "'";
  }
}

void DaemonFeature::checkPidFile() {
  // check if the pid-file exists
  if (!_pidFile.empty()) {
    if (FileUtils::isDirectory(_pidFile)) {
      LOG(FATAL) << "pid-file '" << _pidFile << "' is a directory";
      FATAL_ERROR_EXIT();
    } else if (FileUtils::exists(_pidFile) && FileUtils::size(_pidFile) > 0) {
      LOG_TOPIC(INFO, Logger::STARTUP) << "pid-file '" << _pidFile
                                       << "' already exists, verifying pid";

      std::ifstream f(_pidFile.c_str());

      // file can be opened
      if (f) {
        TRI_pid_t oldPid;

        f >> oldPid;

        if (oldPid == 0) {
          LOG(FATAL) << "pid-file '" << _pidFile << "' is unreadable";
          FATAL_ERROR_EXIT();
        }

        LOG_TOPIC(DEBUG, Logger::STARTUP) << "found old pid: " << oldPid;

        int r = kill(oldPid, 0);

        if (r == 0 || errno == EPERM) {
          LOG(FATAL) << "pid-file '" << _pidFile
                     << "' exists and process with pid " << oldPid
                     << " is still running, refusing to start twice";
          FATAL_ERROR_EXIT();
        } else if (errno == ESRCH) {
          LOG_TOPIC(ERR, Logger::STARTUP) << "pid-file '" << _pidFile
                                          << " exists, but no process with pid "
                                          << oldPid << " exists";

          if (!FileUtils::remove(_pidFile)) {
            LOG(FATAL) << "pid-file '" << _pidFile
                       << "' exists, no process with pid " << oldPid
                       << " exists, but pid-file cannot be removed";
            FATAL_ERROR_EXIT();
          }

          LOG_TOPIC(INFO, Logger::STARTUP) << "removed stale pid-file '"
                                           << _pidFile << "'";
        } else {
          LOG(FATAL) << "pid-file '" << _pidFile << "' exists and kill "
                     << oldPid << " failed";
          FATAL_ERROR_EXIT();
        }
      }

      // failed to open file
      else {
        LOG(FATAL) << "pid-file '" << _pidFile
                   << "' exists, but cannot be opened";
        FATAL_ERROR_EXIT();
      }
    }

    LOG_TOPIC(DEBUG, Logger::STARTUP) << "using pid-file '" << _pidFile << "'";
  }
}

int DaemonFeature::forkProcess() {
  // fork off the parent process
  TRI_pid_t pid = fork();

  if (pid < 0) {
    LOG(FATAL) << "cannot fork";
    FATAL_ERROR_EXIT();
  }

  // Upon successful completion, fork() shall return 0 to the child process and
  // shall return the process ID of the child process to the parent process.

  // if we got a good PID, then we can exit the parent process
  if (pid > 0) {
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "started child process with pid "
                                      << pid;
    return pid;
  }

  // change the file mode mask
  umask(0);

  // create a new SID for the child process
  TRI_pid_t sid = setsid();

  if (sid < 0) {
    LOG(FATAL) << "cannot create sid";
    FATAL_ERROR_EXIT();
  }

  // store current working directory
  int err = 0;
  _current = FileUtils::currentDirectory(&err);

  if (err != 0) {
    LOG(FATAL) << "cannot get current directory";
    FATAL_ERROR_EXIT();
  }

  // change the current working directory
  if (!_workingDirectory.empty()) {
    if (!FileUtils::changeDirectory(_workingDirectory)) {
      LOG(FATAL) << "cannot change into working directory '"
                 << _workingDirectory << "'";
      FATAL_ERROR_EXIT();
    } else {
      LOG(INFO) << "changed working directory for child process to '"
                << _workingDirectory << "'";
    }
  }

  // we're a daemon so there won't be a terminal attached
  // close the standard file descriptors and re-open them mapped to /dev/null
  int fd = open("/dev/null", O_RDWR | O_CREAT, 0644);

  if (fd < 0) {
    LOG(FATAL) << "cannot open /dev/null";
    FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDIN_FILENO) < 0) {
    LOG(FATAL) << "cannot re-map stdin to /dev/null";
    FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDOUT_FILENO) < 0) {
    LOG(FATAL) << "cannot re-map stdout to /dev/null";
    FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDERR_FILENO) < 0) {
    LOG(FATAL) << "cannot re-map stderr to /dev/null";
    FATAL_ERROR_EXIT();
  }

  close(fd);

  return 0;
}

void DaemonFeature::writePidFile(int pid) {
  std::ofstream out(_pidFile.c_str(), std::ios::trunc);

  if (!out) {
    LOG(FATAL) << "cannot write pid-file '" << _pidFile << "'";
    FATAL_ERROR_EXIT();
  }

  out << pid;
}

int DaemonFeature::waitForChildProcess(int pid) {
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
      LOG(ERR)
          << "unable to start arangod. please check the logfiles for errors";
      return EXIT_FAILURE;
    }

    // sleep a while and retry
    usleep(500 * 1000);
  }

  // enough time has elapsed... we now abort our loop
  return EXIT_SUCCESS;
}
