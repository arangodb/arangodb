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

#include "ServerFeature.h"

#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/application-exit.h"
#include "Basics/process-utils.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/SslServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace arangodb {

ServerFeature::ServerFeature(application_features::ApplicationServer& server, int* res)
    : ApplicationFeature(server, "Server"),
      _result(res),
      _operationMode(OperationMode::MODE_SERVER)
#if _WIN32
      ,
      _codePage(65001),  // default to UTF8
      _originalCodePage(UINT16_MAX)
#endif
{
  setOptional(true);
  startsAfter<AqlFeaturePhase>();

  startsAfter<StatisticsFeature>();
  startsAfter<UpgradeFeature>();
}

void ServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--console", "start a JavaScript emergency console",
                     new BooleanParameter(&_console));

  options->addSection("server", "Server features");

  options->addOption("--server.rest-server", "start a rest-server",
                     new BooleanParameter(&_restServer),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
  
  options->addOption("--server.validate-utf8-strings", "perform UTF-8 string validation for incoming JSON and VelocyPack data",
                     new BooleanParameter(&_validateUtf8Strings),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden)).setIntroducedIn(30700);

  options->addSection("javascript", "Configure the JavaScript engine");

  options->addOption("--javascript.script", "run scripts and exit",
                     new VectorParameter<StringParameter>(&_scripts));

#if _WIN32
  options->addOption("--console.code-page",
                     "Windows code page to use; defaults to UTF8",
                     new UInt16Parameter(&_codePage),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
#endif

  // add several obsoleted options here
  options->addSection("vst", "Configure the VelocyStream protocol");
  options->addObsoleteOption("--vst.maxsize", "maximal size (in bytes) "
                             "for a VelocyPack chunk", true);
  
  options->addObsoleteOption(
      "--server.session-timeout",
      "timeout of web interface server sessions (in seconds)", true);

  // add obsolete MMFiles WAL options (obsoleted in 3.7)
  options->addSection("wal", "Configure the WAL of the MMFiles engine");
  options->addObsoleteOption("--wal.allow-oversize-entries",
                             "allow entries that are bigger than '--wal.logfile-size'", false);
  options->addObsoleteOption("--wal.use-mlock",
                             "mlock WAL logfiles in memory (may require elevated privileges or limits)", false);
  options->addObsoleteOption("--wal.directory", "logfile directory", true);
  options->addObsoleteOption("--wal.historic-logfiles", "maximum number of historic logfiles to keep after collection", true);
  options->addObsoleteOption("--wal.ignore-logfile-errors", 
                             "ignore logfile errors. this will read recoverable data from corrupted logfiles but ignore any unrecoverable data", false);
  options->addObsoleteOption("--wal.ignore-recovery-errors", "continue recovery even if re-applying operations fails", false);
  options->addObsoleteOption("--wal.flush-timeout", "flush timeout (in milliseconds)", true);
  options->addObsoleteOption("--wal.logfile-size", "size of each logfile (in bytes)", true);
  options->addObsoleteOption("--wal.open-logfiles", "maximum number of parallel open logfiles", true);
  options->addObsoleteOption("--wal.reserve-logfiles", "maximum number of reserve logfiles to maintain", true);
  options->addObsoleteOption("--wal.slots", "number of logfile slots to use", true);
  options->addObsoleteOption("--wal.sync-interval", "interval for automatic, non-requested disk syncs (in milliseconds)", true);
  options->addObsoleteOption("--wal.throttle-when-pending", 
                             "throttle writes when at least this many operations are waiting for collection (set to 0 to deactivate write-throttling)", true);
  options->addObsoleteOption("--wal.throttle-wait", "maximum wait time per operation when write-throttled (in milliseconds)", true);
}

void ServerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  int count = 0;

  if (_console) {
    _operationMode = OperationMode::MODE_CONSOLE;
    ++count;
  }

  if (!_scripts.empty()) {
    _operationMode = OperationMode::MODE_SCRIPT;
    ++count;
  }

  if (1 < count) {
    LOG_TOPIC("353cd", FATAL, arangodb::Logger::FIXME)
        << "cannot combine '--console', '--javascript.unit-tests' and "
        << "'--javascript.script'";
    FATAL_ERROR_EXIT();
  }

  if (_operationMode == OperationMode::MODE_SERVER && !_restServer) {
    LOG_TOPIC("8daab", FATAL, arangodb::Logger::FIXME)
        << "need at least '--console', '--javascript.unit-tests' or"
        << "'--javascript.script if rest-server is disabled";
    FATAL_ERROR_EXIT();
  }

  V8DealerFeature& v8dealer = server().getFeature<V8DealerFeature>();

  if (v8dealer.isEnabled()) {
    if (_operationMode == OperationMode::MODE_SCRIPT) {
      v8dealer.setMinimumContexts(2);
    } else {
      v8dealer.setMinimumContexts(1);
    }
  } else if (_operationMode != OperationMode::MODE_SERVER) {
    LOG_TOPIC("a114b", FATAL, arangodb::Logger::FIXME)
        << "Options '--console', '--javascript.unit-tests'"
        << " or '--javascript.script' are not supported without V8";
    FATAL_ERROR_EXIT();
  }

  if (!_restServer) {
    server().disableFeatures(
        std::vector<std::type_index>{std::type_index(typeid(DaemonFeature)),
                                     std::type_index(typeid(HttpEndpointProvider)),
                                     std::type_index(typeid(GeneralServerFeature)),
                                     std::type_index(typeid(SslServerFeature)),
                                     std::type_index(typeid(StatisticsFeature)),
                                     std::type_index(typeid(SupervisorFeature))});

    if (!options->processingResult().touched("replication.auto-start")) {
      // turn off replication applier when we do not have a rest server
      // but only if the config option is not explicitly set (the recovery
      // test want the applier to be enabled for testing it)
      ReplicationFeature& replicationFeature = server().getFeature<ReplicationFeature>();
      replicationFeature.disableReplicationApplier();
    }
  }

  if (_operationMode == OperationMode::MODE_CONSOLE) {
    server().disableFeatures(
        std::vector<std::type_index>{std::type_index(typeid(DaemonFeature)),
                                     std::type_index(typeid(SupervisorFeature))});
    v8dealer.setMinimumContexts(2);
  }

  if (_operationMode == OperationMode::MODE_SERVER ||
      _operationMode == OperationMode::MODE_CONSOLE) {
    server().getFeature<ShutdownFeature>().disable();
  }
}

void ServerFeature::prepare() {
  // adjust global settings for UTF-8 string validation
  basics::VelocyPackHelper::strictRequestValidationOptions.validateUtf8Strings = _validateUtf8Strings;
}

void ServerFeature::start() {
#if _WIN32
  _originalCodePage = GetConsoleOutputCP();
  if (IsValidCodePage(_codePage)) {
    SetConsoleOutputCP(_codePage);
  }
#endif

  waitForHeartbeat();

  *_result = EXIT_SUCCESS;

  switch (_operationMode) {
    case OperationMode::MODE_SCRIPT:
    case OperationMode::MODE_CONSOLE:
      break;

    case OperationMode::MODE_SERVER:
      LOG_TOPIC("7031b", TRACE, Logger::STARTUP) << "server operation mode: SERVER";
      break;
  }

  // flush all log output before we go on... this is sensible because any
  // of the following options may print or prompt, and pending log entries
  // might overwrite that
  Logger::flush();

  if (!isConsoleMode()) {
    // install CTRL-C handlers
    server().registerStartupCallback([this]() {
      server().getFeature<SchedulerFeature>().buildControlCHandler();
    });
  }
}

void ServerFeature::stop() {
#if _WIN32
  if (IsValidCodePage(_originalCodePage)) {
    SetConsoleOutputCP(_originalCodePage);
  }
#endif
}

void ServerFeature::beginShutdown() {
  std::string msg =
      ArangoGlobalContext::CONTEXT->binaryName() + " [shutting down]";
  TRI_SetProcessTitle(msg.c_str());
  _isStopping = true;
}

void ServerFeature::waitForHeartbeat() {
  if (!ServerState::instance()->isCoordinator()) {
    // waiting for the heartbeart thread is necessary on coordinator only
    return;
  }

  while (true) {
    if (HeartbeatThread::hasRunOnce()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

std::string ServerFeature::operationModeString(OperationMode mode) {
  switch (mode) {
    case OperationMode::MODE_CONSOLE:
      return "console";
    case OperationMode::MODE_SCRIPT:
      return "script";
    case OperationMode::MODE_SERVER:
      return "server";
    default:
      return "unknown";
  }
}

}  // namespace arangodb
