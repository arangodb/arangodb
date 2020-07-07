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
#include "Basics/exitcodes.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
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
      _allowDeprecatedDeployments(false) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

void EngineSelectorFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOption("--server.storage-engine", "storage engine type "
                     "(note that the mmfiles engine is unavailable since v3.7.0 and cannot be used anymore)",
                     new DiscreteValuesParameter<StringParameter>(&_engine, availableEngineNames()));
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
    if (_engine == "mmfiles") {
      LOG_TOPIC("10eb6", FATAL, Logger::STARTUP)
          << "the mmfiles storage engine is unavailable from version v3.7.0 onwards";
    } else {
      // should not happen
      LOG_TOPIC("3e975", FATAL, Logger::STARTUP)
          << "unable to determine storage engine '" << _engine << "'";
    }
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_UNSUPPORTED_STORAGE_ENGINE);
  }

  if (selected->second.deprecated) {
    if (!selected->second.allowNewDeployments) {
      LOG_TOPIC("23562", ERR, arangodb::Logger::STARTUP)
          << "The " << _engine << " storage engine is deprecated and unsupported and will be removed in a future version. "
          << "Please plan for a migration to a different ArangoDB storage engine.";

      if (!ServerState::instance()->isCoordinator() &&
          !basics::FileUtils::isRegularFile(_engineFilePath) &&
          !_allowDeprecatedDeployments) {
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

template <typename As, typename std::enable_if<std::is_base_of<StorageEngine, As>::value, int>::type>
As& EngineSelectorFeature::engine() {
  return *static_cast<As*>(ENGINE);
}
template ClusterEngine& EngineSelectorFeature::engine<ClusterEngine>();
template RocksDBEngine& EngineSelectorFeature::engine<RocksDBEngine>();

std::string const& EngineSelectorFeature::engineName() {
  return ENGINE->typeName();
}

std::string const& EngineSelectorFeature::defaultEngine() {
  return RocksDBEngine::EngineName;
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
