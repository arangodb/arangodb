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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "EngineSelectorFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "StorageEngine/MMFilesEngine.h"
#include "StorageEngine/OtherEngine.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb;
using namespace arangodb::options;

StorageEngine* EngineSelectorFeature::ENGINE = nullptr;

EngineSelectorFeature::EngineSelectorFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "EngineSelector"), _engine(MMFilesEngine::EngineName) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void EngineSelectorFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOption("--server.storage-engine", 
                     "storage engine type",
                     new DiscreteValuesParameter<StringParameter>(&_engine, availableEngines()));
}

void EngineSelectorFeature::prepare() {
  // deactivate all engines but the selected one
  for (auto& engine : availableEngines()) {
    StorageEngine* e = application_features::ApplicationServer::getFeature<StorageEngine>(engine);

    if (engine == _engine) {
      // this is the selected engine
      LOG_TOPIC(TRACE, Logger::STARTUP) << "enabling storage engine " << engine;
      e->enable();

      // register storage engine
      ENGINE = e;
    } else {
      // turn off all other storage engines
      LOG_TOPIC(TRACE, Logger::STARTUP) << "disabling storage engine " << engine;
      e->disable();
    }
  }

  TRI_ASSERT(ENGINE != nullptr);
}

void EngineSelectorFeature::unprepare() {
  // unregister storage engine
  ENGINE = nullptr;
}

// return all available storage engines
std::unordered_set<std::string> EngineSelectorFeature::availableEngines() { 
  return std::unordered_set<std::string>{
    MMFilesEngine::EngineName, 
    OtherEngine::EngineName 
  };
}
