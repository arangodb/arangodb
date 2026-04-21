////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

using namespace arangodb;
using namespace arangodb::options;

namespace arangodb {

EngineSelectorFeature::EngineSelectorFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature{server, *this}, _engine(nullptr), _selected(false) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

void EngineSelectorFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addObsoleteOption("--server.storage-engine",
                             "The storage engine type", true);
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
      std::filesystem::is_regular_file(_engineFilePath)) {
    LOG_TOPIC("98b5c", DEBUG, Logger::STARTUP)
        << "looking for previously selected engine in file '" << _engineFilePath
        << "'";
    try {
      std::string content =
          basics::StringUtils::trim(basics::FileUtils::slurp(_engineFilePath));
      if (content != RocksDBEngine::kEngineName) {
        LOG_TOPIC("cd6d8", FATAL, Logger::STARTUP)
            << "'ENGINE' file '" << _engineFilePath
            << " indicates storage engine '" << content
            << "', but currently only '" << RocksDBEngine::kEngineName
            << "' is supported.";
        FATAL_ERROR_EXIT();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("23ec1", FATAL, Logger::STARTUP)
          << "unable to read content of 'ENGINE' file '" << _engineFilePath
          << "': " << ex.what()
          << ". please make sure the file/directory is readable for the "
             "arangod process and user";
      FATAL_ERROR_EXIT();
    }
  }

  if (ServerState::instance()->isCoordinator()) {
    ClusterEngine& ce = server().getFeature<ClusterEngine>();
    _engine = &ce;

    StorageEngine& e = server().getFeature<RocksDBEngine>();
    LOG_TOPIC("001b6", TRACE, Logger::STARTUP) << "disabling storage engine";
    e.disable();
    ce.setActualEngine(&e);
  } else {
    StorageEngine& e = server().getFeature<RocksDBEngine>();
    e.enable();
    _engine = &e;
  }

  TRI_ASSERT(_engine != nullptr);

  _selected.store(true);
}

void EngineSelectorFeature::start() {
  TRI_ASSERT(_engine != nullptr);

  // write engine File
  if (!ServerState::instance()->isCoordinator() &&
      !std::filesystem::is_regular_file(_engineFilePath)) {
    try {
      basics::FileUtils::spit(_engineFilePath, std::string{defaultEngine()},
                              true);
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

StorageEngine& EngineSelectorFeature::engine() {
  if (!selected()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  return *_engine;
}

template<typename As, typename std::enable_if<
                          std::is_base_of<StorageEngine, As>::value, int>::type>
As& EngineSelectorFeature::engine() {
  TRI_ASSERT(dynamic_cast<As*>(_engine) != nullptr);
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
