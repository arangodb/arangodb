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

#include <thread>

#include "ConsoleFeature.h"

#include "Basics/messages.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ServerFeature.h"

#include <iostream>

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ConsoleFeature::ConsoleFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Console"),
      _operationMode(OperationMode::MODE_SERVER),
      _consoleThread(nullptr) {
  startsAfter("AgencyPhase");
}

void ConsoleFeature::start() {
  auto server = ApplicationServer::getFeature<ServerFeature>("Server");

  _operationMode = server->operationMode();

  if (_operationMode != OperationMode::MODE_CONSOLE) {
    return;
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "server operation mode: CONSOLE";

  auto* sysDbFeature = arangodb::application_features::ApplicationServer::getFeature<
    arangodb::SystemDatabaseFeature
  >();
  auto database = sysDbFeature->use();

  _consoleThread.reset(
    new ConsoleThread(ApplicationFeature::server(), database.get())
  );
  _consoleThread->start();
}

void ConsoleFeature::unprepare() {
  if (_operationMode != OperationMode::MODE_CONSOLE) {
    return;
  }

  _consoleThread->userAbort();
  _consoleThread->beginShutdown();

  int iterations = 0;

  while (_consoleThread->isRunning() && ++iterations < 30) {
    std::this_thread::sleep_for(std::chrono::microseconds(100 * 1000));  // spin while console is still needed
  }

  std::cout << std::endl << TRI_BYE_MESSAGE << std::endl;
}

} // arangodb
