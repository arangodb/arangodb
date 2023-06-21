////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb;
using namespace arangodb::options;

namespace {

using EngineResolver = StorageEngine& (*)(ArangodServer&);

struct EngineInfo {
  EngineResolver resolver;
  // whether or not the engine is deprecated
  bool deprecated;
  // whether or not new deployments with this engine are allowed
  bool allowNewDeployments;
};

constexpr std::array<std::pair<std::string_view, EngineInfo>, 1> kEngines{
    {{RocksDBEngine::kEngineName,
      {[](ArangodServer& server) -> StorageEngine& {
         return server.getFeature<RocksDBEngine>();
       },
       false, true}}}};

}  // namespace

namespace arangodb {

EngineSelectorFeature::EngineSelectorFeature(Server& server)
    : ArangodFeature{server, *this},
      _engine(nullptr),
      _engineName("auto"),
      _selected(false),
      _allowDeprecatedDeployments(false) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

void EngineSelectorFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options
      ->addOption("--server.storage-engine",
                  "The storage engine type "
                  "(note that the MMFiles engine is unavailable since "
                  "v3.7.0 and cannot be used anymore).",
                  new DiscreteValuesParameter<StringParameter>(
                      &_engineName, availableEngineNames()))
      .setLongDescription(R"(ArangoDB's storage engine is based on RocksDB, see
http://rocksdb.org. It is the only available engine from ArangoDB v3.7 onwards.

The storage engine type needs to be the same for an entire deployment.
Live switching of storage engines on already installed systems isn't supported.
Configuring the wrong engine (not matching the previously used one) results
in the server refusing to start. You may use `auto` to let ArangoDB choose the
previously used one.)");
}

void EngineSelectorFeature::prepare() {
#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (_selected.load()) {
    // already set in the test code
    return;
  }
#endif
  // read engine from file in database_directory ENGINE (mmfiles/rocksdb)
  auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
  auto path = databasePathFeature.directory();
  _engineFilePath = basics::FileUtils::buildFilename(path, "ENGINE");

  // fail if engine value in file does not match command-line option
  if (!ServerState::instance()->isCoordinator() &&
      basics::FileUtils::isRegularFile(_engineFilePath)) {
    LOG_TOPIC("98b5c", DEBUG, Logger::STARTUP)
        << "looking for previously selected engine in file '" << _engineFilePath
        << "'";
    try {
      std::string content =
          basics::StringUtils::trim(basics::FileUtils::slurp(_engineFilePath));
      if (content != _engineName && _engineName != "auto") {
        LOG_TOPIC("cd6d8", FATAL, Logger::STARTUP)
            << "content of 'ENGINE' file '" << _engineFilePath
            << "' and command-line/configuration option value do not match: '"
            << content << "' != '" << _engineName
            << "'. please validate the command-line/configuration option value "
               "of '--server.storage-engine' or use a different database "
               "directory if the change is intentional";
        FATAL_ERROR_EXIT();
      }
      _engineName = content;
    } catch (std::exception const& ex) {
      LOG_TOPIC("23ec1", FATAL, Logger::STARTUP)
          << "unable to read content of 'ENGINE' file '" << _engineFilePath
          << "': " << ex.what()
          << ". please make sure the file/directory is readable for the "
             "arangod process and user";
      FATAL_ERROR_EXIT();
    }
  }

  if (_engineName == "auto") {
    _engineName = defaultEngine();
  }

  TRI_ASSERT(_engineName != "auto");

  auto const selected =
      std::find_if(std::begin(kEngines), std::end(kEngines),
                   [this](auto& info) { return _engineName == info.first; });
  if (selected == std::end(kEngines)) {
    if (_engineName == "mmfiles") {
      LOG_TOPIC("10eb6", FATAL, Logger::STARTUP)
          << "the mmfiles storage engine is unavailable from version v3.7.0 "
             "onwards";
    } else {
      // should not happen
      LOG_TOPIC("3e975", FATAL, Logger::STARTUP)
          << "unable to determine storage engine '" << _engineName << "'";
    }
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_UNSUPPORTED_STORAGE_ENGINE);
  }

  if (selected->second.deprecated) {
    if (!selected->second.allowNewDeployments) {
      LOG_TOPIC("23562", ERR, arangodb::Logger::STARTUP)
          << "The " << _engineName
          << " storage engine is deprecated and unsupported and will be "
             "removed in a future version. "
          << "Please plan for a migration to a different ArangoDB storage "
             "engine.";

      if (!ServerState::instance()->isCoordinator() &&
          !basics::FileUtils::isRegularFile(_engineFilePath) &&
          !_allowDeprecatedDeployments) {
        LOG_TOPIC("ca0a7", FATAL, Logger::STARTUP)
            << "The " << _engineName
            << " storage engine cannot be used for new deployments.";
        FATAL_ERROR_EXIT();
      }
    } else {
      LOG_TOPIC("80866", WARN, arangodb::Logger::STARTUP)
          << "The " << _engineName
          << " storage engine is deprecated and will be removed in a future "
             "version. "
          << "Please plan for a migration to a different ArangoDB storage "
             "engine.";
    }
  }

  if (ServerState::instance()->isCoordinator()) {
    ClusterEngine& ce = server().getFeature<ClusterEngine>();
    _engine = &ce;

    for (auto& engine : kEngines) {
      StorageEngine& e = engine.second.resolver(server());
      // turn off all other storage engines
      LOG_TOPIC("001b6", TRACE, Logger::STARTUP)
          << "disabling storage engine " << engine.first;
      e.disable();
      if (engine.first == _engineName) {
        LOG_TOPIC("4a3fc", DEBUG, Logger::STARTUP)
            << "using storage engine " << engine.first;
        ce.setActualEngine(&e);
      }
    }

  } else {
    // deactivate all engines but the selected one
    for (auto& engine : kEngines) {
      StorageEngine& e = engine.second.resolver(server());

      if (engine.first == _engineName) {
        // this is the selected engine
        LOG_TOPIC("144fe", DEBUG, Logger::STARTUP)
            << "using storage engine '" << engine.first << "'";
        e.enable();

        // register storage engine
        TRI_ASSERT(_engine == nullptr);
        _engine = &e;
      } else {
        // turn off all other storage engines
        LOG_TOPIC("14a9e", TRACE, Logger::STARTUP)
            << "disabling storage engine '" << engine.first << "'";
        e.disable();
      }
    }
  }

  if (_engine == nullptr) {
    LOG_TOPIC("9cb11", FATAL, Logger::STARTUP)
        << "unable to figure out storage engine from selection '" << _engineName
        << "'. please use the '--server.storage-engine' option to select an "
           "existing storage engine";
    FATAL_ERROR_EXIT();
  }

  _selected.store(true);
}

void EngineSelectorFeature::start() {
  TRI_ASSERT(_engine != nullptr);

  // write engine File
  if (!ServerState::instance()->isCoordinator() &&
      !basics::FileUtils::isRegularFile(_engineFilePath)) {
    try {
      basics::FileUtils::spit(_engineFilePath, _engineName, true);
    } catch (std::exception const& ex) {
      LOG_TOPIC("4ff0f", FATAL, Logger::STARTUP)
          << "unable to write 'ENGINE' file '" << _engineFilePath
          << "': " << ex.what()
          << ". please make sure the file/directory is writable for the "
             "arangod process and user";
      FATAL_ERROR_EXIT();
    }
  }
}

void EngineSelectorFeature::unprepare() {
  // unregister storage engine
  _selected.store(false);
  _engine = nullptr;

  if (ServerState::instance()->isCoordinator()) {
#ifdef ARANGODB_USE_GOOGLE_TESTS
    if (!arangodb::ClusterEngine::Mocking) {
#endif
      ClusterEngine& ce = server().getFeature<ClusterEngine>();
      ce.setActualEngine(nullptr);
#ifdef ARANGODB_USE_GOOGLE_TESTS
    }
#endif
  }
}

// return the names of all available storage engines
std::unordered_set<std::string> EngineSelectorFeature::availableEngineNames() {
  std::unordered_set<std::string> result{"auto"};
  for (auto const& it : kEngines) {
    result.emplace(it.first);
  }
  return result;
}

StorageEngine& EngineSelectorFeature::engine() {
  if (!selected()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  return *_engine;
}

template<typename As, typename std::enable_if<
                          std::is_base_of<StorageEngine, As>::value, int>::type>
As& EngineSelectorFeature::engine() {
  return *static_cast<As*>(_engine);
}
template ClusterEngine& EngineSelectorFeature::engine<ClusterEngine>();
template RocksDBEngine& EngineSelectorFeature::engine<RocksDBEngine>();

std::string_view EngineSelectorFeature::engineName() const {
  return _engine->typeName();
}

std::string_view EngineSelectorFeature::defaultEngine() {
  return RocksDBEngine::kEngineName;
}

bool EngineSelectorFeature::isRocksDB() {
  return engineName() == RocksDBEngine::kEngineName;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void EngineSelectorFeature::setEngineTesting(StorageEngine* input) {
  TRI_ASSERT((input == nullptr) != (_engine == nullptr));
  _selected.store(input != nullptr);
  _engine = input;
}
#endif

}  // namespace arangodb
