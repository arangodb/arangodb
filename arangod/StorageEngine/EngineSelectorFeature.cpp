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
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "MMFiles/MMFilesEngine.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb::options;

namespace {
struct EngineInfo {
  std::type_index type;
  // whether or not the engine is deprecated
  bool deprecated;
  // whether or not new deployments with this engine are allowed
  bool allowNewDeployments;
};

std::unordered_map<std::string, EngineInfo> createEngineMap() {
  std::unordered_map<std::string, EngineInfo> map;
  // mmfiles is deprecated since v3.6.0
  map.try_emplace(arangodb::MMFilesEngine::EngineName,
                  EngineInfo{ std::type_index(typeid(arangodb::MMFilesEngine)), true, false });
  // rocksdb is not deprecated and the engine of choice
  map.try_emplace(arangodb::RocksDBEngine::EngineName,
                  EngineInfo{ std::type_index(typeid(arangodb::RocksDBEngine)), false, true });
  return map;
}
}

namespace arangodb {

StorageEngine* EngineSelectorFeature::ENGINE = nullptr;

EngineSelectorFeature::EngineSelectorFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "EngineSelector"), 
      _engine("auto"), 
      _selected(false),
      _allowDeprecated(false) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

void EngineSelectorFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOption("--server.storage-engine", "storage engine type "
                     "(the mmfiles engine is deprecated and can only be used for existing deployments)",
                     new DiscreteValuesParameter<StringParameter>(&_engine, availableEngineNames()));
 
  // whether or not we want to allow deprecated storage engines for _new_ deployments
  // this is a hidden option that is there only for running tests for deprecated engines
  options->addOption("--server.allow-deprecated-storage-engine", "allow deprecated storage engines for new deployments "
                     "(only useful for testing - do not use in production)",
                     new BooleanParameter(&_allowDeprecated),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30700);
}

void EngineSelectorFeature::prepare() {
#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (_selected.load()) {
    // already set in the test code
    return;
  }
#endif
  auto engines = ::createEngineMap();

  // read engine from file in database_directory ENGINE (mmfiles/rocksdb)
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  auto path = databasePathFeature.directory();
  _engineFilePath = basics::FileUtils::buildFilename(path, "ENGINE");

  // fail if engine value in file does not match command-line option
  if (!ServerState::instance()->isCoordinator() &&
      basics::FileUtils::isRegularFile(_engineFilePath)) {
    LOG_TOPIC("98b5c", DEBUG, Logger::STARTUP)
        << "looking for previously selected engine in file '" << _engineFilePath << "'";
    try {
      std::string content =
          basics::StringUtils::trim(basics::FileUtils::slurp(_engineFilePath));
      if (content != _engine && _engine != "auto") {
        LOG_TOPIC("cd6d8", FATAL, Logger::STARTUP)
            << "content of 'ENGINE' file '" << _engineFilePath
            << "' and command-line/configuration option value do not match: '"
            << content << "' != '" << _engine
            << "'. please validate the command-line/configuration option value "
               "of '--server.storage-engine' or use a different database "
               "directory if the change is intentional";
        FATAL_ERROR_EXIT();
      }
      _engine = content;
    } catch (std::exception const& ex) {
      LOG_TOPIC("23ec1", FATAL, Logger::STARTUP)
          << "unable to read content of 'ENGINE' file '" << _engineFilePath
          << "': " << ex.what()
          << ". please make sure the file/directory is readable for the "
             "arangod process and user";
      FATAL_ERROR_EXIT();
    }
  }

  if (_engine == "auto") {
    _engine = defaultEngine();
  }

  TRI_ASSERT(_engine != "auto");

  auto selected = engines.find(_engine);
  if (selected == engines.end()) {
    // should not happen
    LOG_TOPIC("3e975", FATAL, Logger::STARTUP)
        << "unable to determine storage engine";
    FATAL_ERROR_EXIT();
  }

  if (selected->second.deprecated) {
    if (!selected->second.allowNewDeployments) {
      LOG_TOPIC("23562", ERR, arangodb::Logger::STARTUP)
          << "The " << _engine << " storage engine is deprecated and unsupported and will be removed in a future version. "
          << "Please plan for a migration to a different ArangoDB storage engine.";

      if (!ServerState::instance()->isCoordinator() &&
          !basics::FileUtils::isRegularFile(_engineFilePath) &&
          !_allowDeprecated) {
        LOG_TOPIC("ca0a7", FATAL, Logger::STARTUP)
            << "The " << _engine << " storage engine cannot be used for new deployments.";
         FATAL_ERROR_EXIT();
      }
    } else {
      LOG_TOPIC("80866", WARN, arangodb::Logger::STARTUP)
          << "The " << _engine << " storage engine is deprecated and will be removed in a future version. "
          << "Please plan for a migration to a different ArangoDB storage engine.";
    }
  }

  if (ServerState::instance()->isCoordinator()) {
    ClusterEngine& ce = server().getFeature<ClusterEngine>();
    ENGINE = &ce;

    for (auto& engine : engines) {
      StorageEngine& e = server().getFeature<StorageEngine>(engine.second.type);
      // turn off all other storage engines
      LOG_TOPIC("001b6", TRACE, Logger::STARTUP) << "disabling storage engine " << engine.first;
      e.disable();
      if (engine.first == _engine) {
        LOG_TOPIC("4a3fc", INFO, Logger::FIXME) << "using storage engine " << engine.first;
        ce.setActualEngine(&e);
      }
    }

  } else {
    // deactivate all engines but the selected one
    for (auto& engine : engines) {
      auto& e = server().getFeature<StorageEngine>(engine.second.type);

      if (engine.first == _engine) {
        // this is the selected engine
        LOG_TOPIC("144fe", INFO, Logger::FIXME)
            << "using storage engine '" << engine.first << "'";
        e.enable();

        // register storage engine
        TRI_ASSERT(ENGINE == nullptr);
        ENGINE = &e;
      } else {
        // turn off all other storage engines
        LOG_TOPIC("14a9e", TRACE, Logger::STARTUP)
            << "disabling storage engine '" << engine.first << "'";
        e.disable();
      }
    }
  }

  if (ENGINE == nullptr) {
    LOG_TOPIC("9cb11", FATAL, Logger::STARTUP)
        << "unable to figure out storage engine from selection '" << _engine
        << "'. please use the '--server.storage-engine' option to select an "
           "existing storage engine";
    FATAL_ERROR_EXIT();
  }

  _selected.store(true);
}

void EngineSelectorFeature::start() {
  TRI_ASSERT(ENGINE != nullptr);

  // write engine File
  if (!ServerState::instance()->isCoordinator() &&
      !basics::FileUtils::isRegularFile(_engineFilePath)) {
    try {
      basics::FileUtils::spit(_engineFilePath, _engine, true);
    } catch (std::exception const& ex) {
      LOG_TOPIC("4ff0f", FATAL, Logger::STARTUP)
          << "unable to write 'ENGINE' file '" << _engineFilePath << "': " << ex.what()
          << ". please make sure the file/directory is writable for the "
             "arangod process and user";
      FATAL_ERROR_EXIT();
    }
  }
}

void EngineSelectorFeature::unprepare() {
  // unregister storage engine
  _selected.store(false);
  ENGINE = nullptr;

  if (ServerState::instance()->isCoordinator()) {
    ClusterEngine& ce = server().getFeature<ClusterEngine>();
    ce.setActualEngine(nullptr);
  }
}

// return the names of all available storage engines
std::unordered_set<std::string> EngineSelectorFeature::availableEngineNames() {
  std::unordered_set<std::string> result;
  for (auto const& it : ::createEngineMap()) {
    result.emplace(it.first);
  }
  result.emplace("auto");
  return result;
}

StorageEngine& EngineSelectorFeature::engine() {
  if (!selected()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  return *ENGINE;
}

std::string const& EngineSelectorFeature::engineName() {
  return ENGINE->typeName();
}

std::string const& EngineSelectorFeature::defaultEngine() {
  return RocksDBEngine::EngineName;
}

bool EngineSelectorFeature::isMMFiles() {
  return engineName() == MMFilesEngine::EngineName;
}

bool EngineSelectorFeature::isRocksDB() {
  return engineName() == RocksDBEngine::EngineName;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void EngineSelectorFeature::setEngineTesting(StorageEngine* input) {
  TRI_ASSERT((input == nullptr) != (ENGINE == nullptr));
  _selected.store(input != nullptr);
  ENGINE = input;
}
#endif

}  // namespace arangodb
