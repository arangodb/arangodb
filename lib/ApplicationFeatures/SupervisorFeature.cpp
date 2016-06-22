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
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

static bool DONE = false;
static int CLIENT_PID = false;

static void StopHandler(int) {
  LOG_TOPIC(INFO, Logger::STARTUP) << "received SIGINT for supervisor";
  kill(CLIENT_PID, SIGTERM);
  DONE = true;
}

SupervisorFeature::SupervisorFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Supervisor"), _supervisor(false) {
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
      LOG(FATAL) << "daemon mode not available, cannot start supervisor";
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

  std::vector<ApplicationFeature*> supervisorFeatures(_supervisorStart.size());

  std::transform(_supervisorStart.begin(), _supervisorStart.end(),
                 supervisorFeatures.begin(), [](std::string const& name) {
                  try {
                    return ApplicationServer::getFeature<ApplicationFeature>(name);
                  } catch (...) { 
                    LOG_TOPIC(FATAL, Logger::STARTUP)
                        << "unknown feature '" << name << "', giving up";
                    FATAL_ERROR_EXIT();
                  }
                 });

  while (!done) {
    // fork of the server
    _clientPid = fork();

    if (_clientPid < 0) {
      LOG_TOPIC(FATAL, Logger::STARTUP) << "fork failed, giving up";
      FATAL_ERROR_EXIT();
    }

    // parent (supervisor)
    if (0 < _clientPid) {
      TRI_SetProcessTitle("arangodb [supervisor]");

      std::for_each(supervisorFeatures.begin(), supervisorFeatures.end(),
                    [](ApplicationFeature* feature) {
                      LoggerFeature* logger =
                          dynamic_cast<LoggerFeature*>(feature);

                      if (logger != nullptr) {
                        logger->setSupervisor(true);
                        logger->disableThreaded();
                      }
                    });

      std::for_each(supervisorFeatures.begin(), supervisorFeatures.end(),
                    [](ApplicationFeature* feature) { feature->prepare(); });

      std::for_each(supervisorFeatures.begin(), supervisorFeatures.end(),
                    [](ApplicationFeature* feature) { feature->start(); });

      LOG_TOPIC(DEBUG, Logger::STARTUP) << "supervisor mode: within parent";

      ArangoGlobalContext::CONTEXT->unmaskStandardSignals();
      signal(SIGINT, StopHandler);
      signal(SIGTERM, StopHandler);

      CLIENT_PID = _clientPid;
      DONE = false;

      int status;
      int res = waitpid(_clientPid, &status, 0);
      bool horrible = true;

      if (!DONE) {
        done = true;
        horrible = false;
      }
      else {
        LOG_TOPIC(DEBUG, Logger::STARTUP) << "waitpid woke up with return value "
                                          << res << " and status " << status;

        if (WIFEXITED(status)) {
          // give information about cause of death
          if (WEXITSTATUS(status) == 0) {
            LOG_TOPIC(INFO, Logger::STARTUP) << "child " << _clientPid
                                             << " died of natural causes";
            done = true;
            horrible = false;
          } else {
            t = time(0) - startTime;

            LOG_TOPIC(ERR, Logger::STARTUP)
                << "child " << _clientPid
                << " died a horrible death, exit status " << WEXITSTATUS(status);

            if (t < MIN_TIME_ALIVE_IN_SEC) {
              LOG_TOPIC(ERR, Logger::STARTUP)
                  << "child only survived for " << t
                  << " seconds, this will not work - please fix the error "
                     "first";
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
              LOG_TOPIC(INFO, Logger::STARTUP)
                  << "child " << _clientPid
                  << " died of natural causes, exit status " << WTERMSIG(status);
              done = true;
              horrible = false;
              break;

            default:
              t = time(0) - startTime;

              LOG_TOPIC(ERR, Logger::STARTUP) << "child " << _clientPid
                                              << " died a horrible death, signal "
                                              << WTERMSIG(status);

              if (t < MIN_TIME_ALIVE_IN_SEC) {
                LOG_TOPIC(ERR, Logger::STARTUP)
                    << "child only survived for " << t
                    << " seconds, this will not work - please fix the "
                       "error first";
                done = true;

#ifdef WCOREDUMP
                if (WCOREDUMP(status)) {
                  LOG_TOPIC(WARN, Logger::STARTUP) << "child process "
                                                   << _clientPid
                                                   << " produced a core dump";
                }
#endif
              } else {
                done = false;
              }

              break;
          }
        } else {
          LOG_TOPIC(ERR, Logger::STARTUP)
              << "child " << _clientPid
              << " died a horrible death, unknown cause";
          done = false;
        }
      }

      if (horrible) {
        result = EXIT_FAILURE;
      }
    }

    // child - run the normal boot sequence
    else {
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

  std::for_each(supervisorFeatures.rbegin(), supervisorFeatures.rend(),
                [](ApplicationFeature* feature) { feature->stop(); });

  std::for_each(supervisorFeatures.rbegin(), supervisorFeatures.rend(),
                [](ApplicationFeature* feature) { feature->unprepare(); });

  exit(result);
}
