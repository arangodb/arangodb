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

#include "SupervisorFeature.h"

#include "ApplicationFeatures/DaemonFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
static bool DONE = false;
static int CLIENT_PID = false;

static char const* restartMessage = "will now start a new child process";
static char const* noRestartMessage = "will intentionally not start a new child process";
static char const* fixErrorMessage = "please check what causes the child process to fail and fix the error first";

static char const* translateSignal(int signal) {
  if (signal >= 128) {
    signal -= 128;
  }
  switch (signal) {
#ifdef SIGHUP
    case SIGHUP:  return "SIGHUP";
#endif
#ifdef SIGINT
    case SIGINT:  return "SIGINT";
#endif
#ifdef SIGQUIT
    case SIGQUIT: return "SIGQUIT";
#endif
#ifdef SIGKILL
    case SIGILL:  return "SIGILL";
#endif
#ifdef SIGTRAP
    case SIGTRAP: return "SIGTRAP";
#endif
#ifdef SIGABRT
    case SIGABRT: return "SIGABRT";
#endif
#ifdef SIGBUS
    case SIGBUS:  return "SIGBUS";
#endif
#ifdef SIGFPE
    case SIGFPE:  return "SIGFPE";
#endif
#ifdef SIGKILL
    case SIGKILL: return "SIGKILL";
#endif
#ifdef SIGSEGV
    case SIGSEGV: return "SIGSEGV";
#endif
#ifdef SIGPIPE
    case SIGPIPE: return "SIGPIPE";
#endif
#ifdef SIGTERM
    case SIGTERM: return "SIGTERM";
#endif
#ifdef SIGCONT
    case SIGCONT: return "SIGCONT";
#endif
#ifdef SIGSTOP
    case SIGSTOP: return "SIGSTOP";
#endif
    default:      return "unknown";
  }
}

static void StopHandler(int) {
  LOG_TOPIC(INFO, Logger::STARTUP) << "received SIGINT for supervisor; commanding client [" << CLIENT_PID << "] to shut down.";
  int rc = kill(CLIENT_PID, SIGTERM);
  if (rc < 0) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "commanding client [" << CLIENT_PID << "] to shut down failed: [" << errno << "] " << strerror(errno);
  }
  DONE = true;
}

}

static void HUPHandler(int) {
  LOG_TOPIC(INFO, Logger::STARTUP) << "received SIGHUP for supervisor; commanding client [" << CLIENT_PID << "] to logrotate.";
  int rc = kill(CLIENT_PID, SIGHUP);
  if (rc < 0) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "commanding client [" << CLIENT_PID << "] to logrotate failed: [" << errno << "] " << strerror(errno);
  }
}

SupervisorFeature::SupervisorFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Supervisor"), _supervisor(false), _clientPid(0) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Daemon");
  startsAfter("Logger");
  startsAfter("WorkMonitor");
}

void SupervisorFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addHiddenOption("--supervisor",
                           "background the server, starts a supervisor",
                           new BooleanParameter(&_supervisor));
}

void SupervisorFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (_supervisor) {
    try {
      DaemonFeature* daemon = ApplicationServer::getFeature<DaemonFeature>("Daemon");

      // force daemon mode
      daemon->setDaemon(true);

      // revalidate options
      daemon->validateOptions(options);
    } catch (...) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "daemon mode not available, cannot start supervisor";
      FATAL_ERROR_EXIT();
    }
  }
}

void SupervisorFeature::daemonize() {
  static time_t const MIN_TIME_ALIVE_IN_SEC = 30;

  if (!_supervisor) {
    return;
  }

  time_t startTime = time(0);
  time_t t;
  bool done = false;
  int result = EXIT_SUCCESS;

  // will be reseted in SchedulerFeature
  ArangoGlobalContext::CONTEXT->unmaskStandardSignals();

  LoggerFeature* logger = nullptr;
      
  try {
    logger = ApplicationServer::getFeature<LoggerFeature>("Logger");
  } catch (...) { 
    LOG_TOPIC(FATAL, Logger::STARTUP)
      << "unknown feature 'Logger', giving up";
    FATAL_ERROR_EXIT();
  }

  logger->setSupervisor(true);
  logger->prepare();

  LOG_TOPIC(DEBUG, Logger::STARTUP) << "starting supervisor loop";

  while (!done) {
    logger->setSupervisor(false);

    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "supervisor will now try to fork a new child process";

    // fork of the server
    _clientPid = fork();

    if (_clientPid < 0) {
      LOG_TOPIC(FATAL, Logger::STARTUP) << "fork failed, giving up";
      FATAL_ERROR_EXIT();
    }

    // parent (supervisor)
    if (0 < _clientPid) {
      signal(SIGINT, StopHandler);
      signal(SIGTERM, StopHandler);
      signal(SIGHUP, HUPHandler);

      LOG_TOPIC(INFO, Logger::STARTUP) << "supervisor has forked a child process with pid " << _clientPid;

      TRI_SetProcessTitle("arangodb [supervisor]");

      LOG_TOPIC(DEBUG, Logger::STARTUP) << "supervisor mode: within parent";

      CLIENT_PID = _clientPid;
      DONE = false;

      int status;
      int res = waitpid(_clientPid, &status, 0);
      bool horrible = true;

      LOG_TOPIC(INFO, Logger::STARTUP) << "waitpid woke up with return value "
                                       << res << " and status " << status
                                       << " and DONE = " << (DONE ? "true" : "false");

      if (DONE) {
        // signal handler for SIGINT or SIGTERM was invoked
        done = true;
        horrible = false;
      }
      else {
        TRI_ASSERT(horrible);

        if (WIFEXITED(status)) {
          // give information about cause of death
          if (WEXITSTATUS(status) == 0) {
            LOG_TOPIC(INFO, Logger::STARTUP) << "child process " << _clientPid
                                             << " terminated normally. " << noRestartMessage;

            done = true;
            horrible = false;
          } else {
            t = time(0) - startTime;

            if (t < MIN_TIME_ALIVE_IN_SEC) {
              LOG_TOPIC(ERR, Logger::STARTUP)
                    << "child process " << _clientPid << " terminated unexpectedly, exit status "
                    << WEXITSTATUS(status) << ". the child process only survived for " << t
                    << " seconds. this is lower than the minimum threshold value of " << MIN_TIME_ALIVE_IN_SEC
                    << " s. " << noRestartMessage << ". " << fixErrorMessage;

              TRI_ASSERT(horrible);
              done = true;
            } else {
              LOG_TOPIC(ERR, Logger::STARTUP)
                  << "child process " << _clientPid
                  << " terminated unexpectedly, exit status " << WEXITSTATUS(status)
                  << ". " << restartMessage;

              done = false;
            }
          }
        } else if (WIFSIGNALED(status)) {
          int const s = WTERMSIG(status);
          switch (s) {
            case 2: // SIGINT
            case 9: // SIGKILL
            case 15: // SIGTERM
              LOG_TOPIC(INFO, Logger::STARTUP)
                  << "child process " << _clientPid
                  << " terminated normally, exit status " << s
                  << " (" << translateSignal(s) << "). " << noRestartMessage;

              done = true;
              horrible = false;
              break;

            default:
              TRI_ASSERT(horrible);
              t = time(0) - startTime;

              if (t < MIN_TIME_ALIVE_IN_SEC) {
                LOG_TOPIC(ERR, Logger::STARTUP)
                    << "child process " << _clientPid << " terminated unexpectedly, signal "
                    << s << " (" << translateSignal(s) << "). the child process only survived for " << t
                    << " seconds. this is lower than the minimum threshold value of " << MIN_TIME_ALIVE_IN_SEC
                    << " s. " << noRestartMessage << ". " << fixErrorMessage;

                done = true;

#ifdef WCOREDUMP
                if (WCOREDUMP(status)) {
                  LOG_TOPIC(WARN, Logger::STARTUP) << "child process "
                                                   << _clientPid
                                                   << " also produced a core dump";
                }
#endif
              } else {
                LOG_TOPIC(ERR, Logger::STARTUP) << "child process " << _clientPid
                                                << " terminated unexpectedly, signal "
                                                << s << " (" << translateSignal(s) << "). "
                                                << restartMessage;

                done = false;
              }

              break;
          }
        } else {
          LOG_TOPIC(ERR, Logger::STARTUP)
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

      LOG_TOPIC(DEBUG, Logger::STARTUP) << "supervisor mode: within child";
      TRI_SetProcessTitle("arangodb [server]");

#ifdef TRI_HAVE_PRCTL
      // force child to stop if supervisor dies
      prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);
#endif

      try {
        DaemonFeature* daemon = ApplicationServer::getFeature<DaemonFeature>("Daemon");

        // disable daemon mode
        daemon->setDaemon(false);
      } catch (...) {
      }

      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::STARTUP) << "supervisor mode: finished (exit " << result << ")";

  Logger::flush();
  Logger::shutdown();

  exit(result);
}
