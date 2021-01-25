////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cstdint>

#include "SupervisorFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/operating-system.h"
#include "Basics/process-utils.h"
#include "Basics/signals.h"
#include "Logger/LogAppender.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef TRI_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
static bool DONE = false;
static int CLIENT_PID = false;

static char const* restartMessage = "will now start a new child process";
static char const* noRestartMessage =
    "will intentionally not start a new child process";
static char const* fixErrorMessage =
    "please check what causes the child process to fail and fix the error "
    "first";

static void StopHandler(int) {
  LOG_TOPIC("3ca0f", INFO, Logger::STARTUP)
      << "received SIGINT for supervisor; commanding client [" << CLIENT_PID
      << "] to shut down.";
  int rc = kill(CLIENT_PID, SIGTERM);
  if (rc < 0) {
    LOG_TOPIC("cf204", ERR, Logger::STARTUP)
        << "commanding client [" << CLIENT_PID << "] to shut down failed: ["
        << errno << "] " << strerror(errno);
  }
  DONE = true;
}

}  // namespace

static void HUPHandler(int) {
  LOG_TOPIC("a7bac", INFO, Logger::STARTUP)
      << "received SIGHUP for supervisor; commanding client [" << CLIENT_PID
      << "] to logrotate.";
  int rc = kill(CLIENT_PID, SIGHUP);
  if (rc < 0) {
    LOG_TOPIC("e7d53", ERR, Logger::STARTUP)
        << "commanding client [" << CLIENT_PID << "] to logrotate failed: ["
        << errno << "] " << strerror(errno);
  }
}

SupervisorFeature::SupervisorFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Supervisor"), _supervisor(false), _clientPid(0) {
  setOptional(true);
  startsAfter<GreetingsFeaturePhase>();
  startsAfter<DaemonFeature>();
}

void SupervisorFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--supervisor",
                     "background the server, starts a supervisor",
                     new BooleanParameter(&_supervisor),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

void SupervisorFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_supervisor) {
    try {
      DaemonFeature& daemon = server().getFeature<DaemonFeature>();

      // force daemon mode
      daemon.setDaemon(true);

      // revalidate options
      daemon.validateOptions(options);
    } catch (...) {
      LOG_TOPIC("9207d", FATAL, arangodb::Logger::FIXME)
          << "daemon mode not available, cannot start supervisor";
      FATAL_ERROR_EXIT();
    }
  }
}

void SupervisorFeature::daemonize() {
  static time_t const MIN_TIME_ALIVE_IN_SEC = 30;

  if (!_supervisor) {
    return;
  }

  time_t startTime = time(nullptr);
  time_t t;
  bool done = false;
  int result = EXIT_SUCCESS;

  // will be reseted in SchedulerFeature
  arangodb::signals::unmaskAllSignals();

  if (!server().hasFeature<LoggerFeature>()) {
    LOG_TOPIC("4e6ee", FATAL, Logger::STARTUP) << "unknown feature 'Logger', giving up";
    FATAL_ERROR_EXIT();
  }

  LoggerFeature& logger = server().getFeature<LoggerFeature>();
  logger.setSupervisor(true);
  logger.prepare();

  LOG_TOPIC("47d80", DEBUG, Logger::STARTUP) << "starting supervisor loop";

  while (!done) {
    logger.setSupervisor(false);

    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    LOG_TOPIC("a3331", DEBUG, Logger::STARTUP)
        << "supervisor will now try to fork a new child process";

    // fork of the server
    _clientPid = fork();

    if (_clientPid < 0) {
      LOG_TOPIC("dc0e1", FATAL, Logger::STARTUP) << "fork failed, giving up";
      FATAL_ERROR_EXIT();
    }

    // parent (supervisor)
    if (0 < _clientPid) {
      signal(SIGINT, StopHandler);
      signal(SIGTERM, StopHandler);
      signal(SIGHUP, HUPHandler);

      LOG_TOPIC("ba799", INFO, Logger::STARTUP)
          << "supervisor has forked a child process with pid " << _clientPid;

      TRI_SetProcessTitle("arangodb [supervisor]");

      LOG_TOPIC("639f2", DEBUG, Logger::STARTUP) << "supervisor mode: within parent";

      CLIENT_PID = _clientPid;
      DONE = false;

      int status;
      int res = waitpid(_clientPid, &status, 0);
      bool horrible = true;

      LOG_TOPIC("a7a71", INFO, Logger::STARTUP)
          << "waitpid woke up with return value " << res << " and status "
          << status << " and DONE = " << (DONE ? "true" : "false");

      if (DONE) {
        // signal handler for SIGINT or SIGTERM was invoked
        done = true;
        horrible = false;
      } else {
        TRI_ASSERT(horrible);

        if (WIFEXITED(status)) {
          // give information about cause of death
          if (WEXITSTATUS(status) == 0) {
            LOG_TOPIC("61ac2", INFO, Logger::STARTUP)
                << "child process " << _clientPid << " terminated normally. "
                << noRestartMessage;

            done = true;
            horrible = false;
          } else {
            t = time(nullptr) - startTime;

            if (t < MIN_TIME_ALIVE_IN_SEC) {
              LOG_TOPIC("9db96", ERR, Logger::STARTUP)
                  << "child process " << _clientPid
                  << " terminated unexpectedly, exit status " << WEXITSTATUS(status)
                  << ". the child process only survived for " << t
                  << " seconds. this is lower than the minimum threshold value "
                     "of "
                  << MIN_TIME_ALIVE_IN_SEC << " s. " << noRestartMessage << ". "
                  << fixErrorMessage;

              TRI_ASSERT(horrible);
              done = true;
            } else {
              LOG_TOPIC("1ae4a", ERR, Logger::STARTUP)
                  << "child process " << _clientPid << " terminated unexpectedly, exit status "
                  << WEXITSTATUS(status) << ". " << restartMessage;

              done = false;
            }
          }
        } else if (WIFSIGNALED(status)) {
          int const s = WTERMSIG(status);
          switch (s) {
            case 2:   // SIGINT
            case 9:   // SIGKILL
            case 15:  // SIGTERM
              LOG_TOPIC("50f4e", INFO, Logger::STARTUP)
                  << "child process " << _clientPid << " terminated normally, exit status "
                  << s << " (" << arangodb::signals::name(s) << "). " << noRestartMessage;

              done = true;
              horrible = false;
              break;

            default:
              TRI_ASSERT(horrible);
              t = time(nullptr) - startTime;

              if (t < MIN_TIME_ALIVE_IN_SEC) {
                LOG_TOPIC("4a3a6", ERR, Logger::STARTUP)
                    << "child process " << _clientPid << " terminated unexpectedly, signal "
                    << s << " (" << arangodb::signals::name(s)
                    << "). the child process only survived for " << t
                    << " seconds. this is lower than the minimum threshold "
                       "value of "
                    << MIN_TIME_ALIVE_IN_SEC << " s. " << noRestartMessage
                    << ". " << fixErrorMessage;

                done = true;

#ifdef WCOREDUMP
                if (WCOREDUMP(status)) {
                  LOG_TOPIC("195c5", WARN, Logger::STARTUP)
                      << "child process " << _clientPid << " also produced a core dump";
                }
#endif
              } else {
                LOG_TOPIC("97c53", ERR, Logger::STARTUP)
                    << "child process " << _clientPid << " terminated unexpectedly, signal "
                    << s << " (" << arangodb::signals::name(s) << "). " << restartMessage;

                done = false;
              }

              break;
          }
        } else {
          LOG_TOPIC("0f028", ERR, Logger::STARTUP)
              << "child process " << _clientPid
              << " terminated unexpectedly, unknown cause. " << restartMessage;

          done = false;
        }
      }

      if (horrible) {
        result = EXIT_FAILURE;
      } else {
        result = EXIT_SUCCESS;
      }
    }

    // child - run the normal boot sequence
    else {
      Logger::shutdown();

      LogAppender::allowStdLogging(false);
      DaemonFeature::remapStandardFileDescriptors();

      LOG_TOPIC("abe90", DEBUG, Logger::STARTUP) << "supervisor mode: within child";
      TRI_SetProcessTitle("arangodb [server]");

#ifdef TRI_HAVE_PRCTL
      // force child to stop if supervisor dies
      prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);
#endif

      try {
        DaemonFeature& daemon = server().getFeature<DaemonFeature>();

        // disable daemon mode
        daemon.setDaemon(false);
      } catch (...) {
      }

      return;
    }
  }

  LOG_TOPIC("85f0b", DEBUG, Logger::STARTUP) << "supervisor mode: finished (exit " << result << ")";

  Logger::flush();
  Logger::shutdown();

  exit(result);
}
