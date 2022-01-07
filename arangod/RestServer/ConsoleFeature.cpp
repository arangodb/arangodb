////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <thread>

#include "ConsoleFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/debugging.h"
#include "Basics/messages.h"
#include "FeaturePhases/AgencyFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/SystemDatabaseFeature.h"

#include <iostream>

#ifndef _WIN32
#include <sys/ioctl.h>
#include <unistd.h>
#endif

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ConsoleFeature::ConsoleFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Console"),
      _operationMode(OperationMode::MODE_SERVER) {
  startsAfter<AgencyFeaturePhase>();
}

void ConsoleFeature::start() {
  auto& serverFeature = server().getFeature<ServerFeature>();

  _operationMode = serverFeature.operationMode();

  if (_operationMode != OperationMode::MODE_CONSOLE) {
    return;
  }

  LOG_TOPIC("a4313", TRACE, Logger::STARTUP)
      << "server operation mode: CONSOLE";

  auto& sysDbFeature = server().getFeature<arangodb::SystemDatabaseFeature>();
  auto database = sysDbFeature.use();

  _consoleThread = std::make_unique<ConsoleThread>(server(), database.get());
  _consoleThread->start();
}

void ConsoleFeature::beginShutdown() {
  if (_operationMode != OperationMode::MODE_CONSOLE) {
    return;
  }

  TRI_ASSERT(_consoleThread != nullptr);

  _consoleThread->userAbort();

#ifndef _WIN32
  if (isatty(STDIN_FILENO)) {
    char c = '\n';
    // send ourselves a character, so that we can get out of the blocking
    // linenoise function that reads a character from the terminal
    [[maybe_unused]] int res = ioctl(STDIN_FILENO, TIOCSTI, &c);
  }
#endif
}

void ConsoleFeature::unprepare() {
  if (_operationMode != OperationMode::MODE_CONSOLE) {
    return;
  }

  _consoleThread->userAbort();
  _consoleThread->beginShutdown();

  int iterations = 0;

  while (_consoleThread->isRunning() && ++iterations < 30) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // sleep while console is still needed
  }

  std::cout << std::endl << TRI_BYE_MESSAGE << std::endl;
}

}  // namespace arangodb
