////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "DaemonFeature.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/Exceptions.h"
#include "Basics/FileResult.h"
#include "Basics/FileResultString.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Basics/operating-system.h"
#include "Basics/process-utils.h"
#include "Basics/system-functions.h"
#include "Basics/threads.h"
#include "Logger/LogAppender.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef TRI_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

DaemonFeature::DaemonFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Daemon") {
  setOptional(true);
  startsAfter<GreetingsFeaturePhase>();

#ifndef _WIN32
  _workingDirectory = "/var/tmp";
#endif
}

void DaemonFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--daemon", "background the server, running it as daemon",
                     new BooleanParameter(&_daemon),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsLinux, arangodb::options::Flags::OsMac, arangodb::options::Flags::Hidden));

  options->addOption("--pid-file", "pid-file in daemon mode",
                     new StringParameter(&_pidFile),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsLinux, arangodb::options::Flags::OsMac, arangodb::options::Flags::Hidden));

  options->addOption("--working-directory", "working directory in daemon mode",
                     new StringParameter(&_workingDirectory),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsLinux, arangodb::options::Flags::OsMac, arangodb::options::Flags::Hidden));
}

void DaemonFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_daemon) {
    return;
  }

  if (_pidFile.empty()) {
    LOG_TOPIC("9d6ba", FATAL, arangodb::Logger::FIXME)
        << "need --pid-file in --daemon mode";
    FATAL_ERROR_EXIT();
  }

  LoggerFeature& logger = server().getFeature<LoggerFeature>();
  logger.setBackgrounded(true);

  // make the pid filename absolute
  std::string currentDir = FileUtils::currentDirectory().result();
  std::string absoluteFile = TRI_GetAbsolutePath(_pidFile, currentDir);

  if (!absoluteFile.empty()) {
    _pidFile = absoluteFile;
    LOG_TOPIC("79662", DEBUG, arangodb::Logger::FIXME)
        << "using absolute pid file '" << _pidFile << "'";
  } else {
    LOG_TOPIC("24de9", FATAL, arangodb::Logger::FIXME)
        << "cannot determine absolute path";
    FATAL_ERROR_EXIT();
  }
}

void DaemonFeature::daemonize() {
  LOG_TOPIC("71164", TRACE, Logger::STARTUP) << name() << "::daemonize";

  if (!_daemon) {
    return;
  }

  LOG_TOPIC("480d4", INFO, Logger::STARTUP) << "starting up in daemon mode";

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
    LOG_TOPIC("0b126", DEBUG, Logger::STARTUP) << "daemon mode continuing within child";
  }
}

void DaemonFeature::unprepare() {
  if (!_daemon) {
    return;
  }

  // remove pid file
  if (!FileUtils::remove(_pidFile)) {
    LOG_TOPIC("1b46c", ERR, arangodb::Logger::FIXME)
        << "cannot remove pid file '" << _pidFile << "'";
  }
}

void DaemonFeature::checkPidFile() {
  // check if the pid-file exists
  if (!_pidFile.empty()) {
    if (FileUtils::isDirectory(_pidFile)) {
      LOG_TOPIC("6b3c0", FATAL, arangodb::Logger::FIXME) << "pid-file '" << _pidFile << "' is a directory";
      FATAL_ERROR_EXIT();
    } else if (FileUtils::exists(_pidFile) && FileUtils::size(_pidFile) > 0) {
      LOG_TOPIC("cf10a", INFO, Logger::STARTUP)
          << "pid-file '" << _pidFile << "' already exists, verifying pid";
      std::string oldPidS;
      try {
        oldPidS = arangodb::basics::FileUtils::slurp(_pidFile);
      } catch (arangodb::basics::Exception const& ex) {
        LOG_TOPIC("4aadd", FATAL, arangodb::Logger::FIXME)
            << "Couldn't read PID file '" << _pidFile << "' - " << ex.what();
        FATAL_ERROR_EXIT();
      }

      basics::StringUtils::trimInPlace(oldPidS);

      if (!oldPidS.empty()) {
        TRI_pid_t oldPid;

        try {
          oldPid = std::stol(oldPidS);
        } catch (std::invalid_argument const& ex) {
          LOG_TOPIC("bd20c", FATAL, arangodb::Logger::FIXME)
              << "pid-file '" << _pidFile << "' doesn't contain a number.";
          FATAL_ERROR_EXIT();
        }
        if (oldPid == 0) {
          LOG_TOPIC("aef5d", FATAL, arangodb::Logger::FIXME)
              << "pid-file '" << _pidFile << "' is unreadable";
          FATAL_ERROR_EXIT();
        }

        LOG_TOPIC("ecac1", DEBUG, Logger::STARTUP) << "found old pid: " << oldPid;

        int r = kill(oldPid, 0);

        if (r == 0 || errno == EPERM) {
          LOG_TOPIC("5fa62", FATAL, arangodb::Logger::FIXME)
              << "pid-file '" << _pidFile << "' exists and process with pid "
              << oldPid << " is still running, refusing to start twice";
          FATAL_ERROR_EXIT();
        } else if (errno == ESRCH) {
          LOG_TOPIC("a9576", ERR, Logger::STARTUP)
              << "pid-file '" << _pidFile << " exists, but no process with pid "
              << oldPid << " exists";

          if (!FileUtils::remove(_pidFile)) {
            LOG_TOPIC("fddfc", FATAL, arangodb::Logger::FIXME)
                << "pid-file '" << _pidFile << "' exists, no process with pid "
                << oldPid << " exists, but pid-file cannot be removed";
            FATAL_ERROR_EXIT();
          }

          LOG_TOPIC("1f3e6", INFO, Logger::STARTUP) << "removed stale pid-file '" << _pidFile << "'";
        } else {
          LOG_TOPIC("180c0", FATAL, arangodb::Logger::FIXME)
              << "pid-file '" << _pidFile << "' exists and kill " << oldPid << " failed";
          FATAL_ERROR_EXIT();
        }
      }

      // failed to open file
      else {
        LOG_TOPIC("ab3fe", FATAL, arangodb::Logger::FIXME)
            << "pid-file '" << _pidFile << "' exists, but cannot be opened";
        FATAL_ERROR_EXIT();
      }
    }

    LOG_TOPIC("1589d", DEBUG, Logger::STARTUP) << "using pid-file '" << _pidFile << "'";
  }
}

int DaemonFeature::forkProcess() {
  // fork off the parent process
  TRI_pid_t pid = fork();

  if (pid < 0) {
    LOG_TOPIC("fd0f9", FATAL, arangodb::Logger::FIXME) << "cannot fork";
    FATAL_ERROR_EXIT();
  }

  // Upon successful completion, fork() shall return 0 to the child process and
  // shall return the process ID of the child process to the parent process.

  // if we got a good PID, then we can exit the parent process
  if (pid > 0) {
    LOG_TOPIC("89e55", DEBUG, Logger::STARTUP) << "started child process with pid " << pid;
    return pid;
  }

  TRI_ASSERT(pid == 0);  // we are in the child

  // child
  LogAppender::allowStdLogging(false);
  Logger::clearCachedPid();

  // change the file mode mask
  umask(0);

  // create a new SID for the child process
  TRI_pid_t sid = setsid();

  if (sid < 0) {
    LOG_TOPIC("e9093", FATAL, arangodb::Logger::FIXME) << "cannot create sid";
    FATAL_ERROR_EXIT();
  }

  // store current working directory
  FileResultString cwd = FileUtils::currentDirectory();

  if (!cwd.ok()) {
    LOG_TOPIC("a681c", FATAL, arangodb::Logger::FIXME)
        << "cannot get current directory: " << cwd.errorMessage();
    FATAL_ERROR_EXIT();
  }

  _current = cwd.result();

  // change the current working directory
  if (!_workingDirectory.empty()) {
    FileResult res = FileUtils::changeDirectory(_workingDirectory);

    if (!res.ok()) {
      LOG_TOPIC("d9f9d", FATAL, arangodb::Logger::STARTUP)
          << "cannot change into working directory '" << _workingDirectory
          << "': " << res.errorMessage();
      FATAL_ERROR_EXIT();
    } else {
      LOG_TOPIC("ae8be", INFO, arangodb::Logger::STARTUP)
          << "changed working directory for child process to '"
          << _workingDirectory << "'";
    }
  }

  remapStandardFileDescriptors();

  return pid;
}

void DaemonFeature::remapStandardFileDescriptors() {
  // we're a daemon so there won't be a terminal attached
  // close the standard file descriptors and re-open them mapped to /dev/null

  // close all descriptors
  for (int i = getdtablesize(); i >= 0; --i) {
    close(i);
  }

  // open fd /dev/null
  int fd = open("/dev/null", O_RDWR | O_CREAT, 0644);

  if (fd < 0) {
    LOG_TOPIC("92755", FATAL, arangodb::Logger::FIXME) << "cannot open /dev/null";
    FATAL_ERROR_EXIT();
  }

  // the following calls silently close and repoen the given fds
  // to avoid concurrency issues
  if (dup2(fd, STDIN_FILENO) != STDIN_FILENO) {
    LOG_TOPIC("3d2ca", FATAL, arangodb::Logger::FIXME)
        << "cannot re-map stdin to /dev/null";
    FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDOUT_FILENO) != STDOUT_FILENO) {
    LOG_TOPIC("4d834", FATAL, arangodb::Logger::FIXME)
        << "cannot re-map stdout to /dev/null";
    FATAL_ERROR_EXIT();
  }

  if (dup2(fd, STDERR_FILENO) != STDERR_FILENO) {
    LOG_TOPIC("39cac", FATAL, arangodb::Logger::FIXME)
        << "cannot re-map stderr to /dev/null";
    FATAL_ERROR_EXIT();
  }

  // Do not close one of the recently opened fds
  if (fd > 2) {
    close(fd);
  }
}

void DaemonFeature::writePidFile(int pid) {
  try {
    arangodb::basics::FileUtils::spit(_pidFile, std::to_string(pid), true);
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC("c2741", FATAL, arangodb::Logger::FIXME)
        << "cannot write pid-file '" << _pidFile << "' - " << ex.what();
  }
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
      LOG_TOPIC("dce6d", ERR, arangodb::Logger::FIXME)
          << "unable to start arangod. please check the logfiles for errors";
      return EXIT_FAILURE;
    }

    // sleep a while and retry
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // enough time has elapsed... we now abort our loop
  return EXIT_SUCCESS;
}

}  // namespace arangodb
