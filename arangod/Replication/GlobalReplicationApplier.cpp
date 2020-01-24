////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "GlobalReplicationApplier.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalTailingSyncer.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <velocypack/Builder.h>

using namespace arangodb;

/// @brief server-global replication applier for all databases
GlobalReplicationApplier::GlobalReplicationApplier(ReplicationApplierConfiguration const& configuration)
    : ReplicationApplier(configuration, "global database") {}

GlobalReplicationApplier::~GlobalReplicationApplier() {
  try {
    stop();
  } catch (...) {
  }
}

/// @brief stop the applier and "forget" everything
void GlobalReplicationApplier::forget() {
  stopAndJoin();

  removeState();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->removeReplicationApplierConfiguration();
  _configuration.reset();
}

/// @brief store the configuration for the applier
void GlobalReplicationApplier::storeConfiguration(bool doSync) {
  VPackBuilder builder;
  builder.openObject();
  _configuration.toVelocyPack(builder, true, true);
  builder.close();

  LOG_TOPIC("f270b", DEBUG, Logger::REPLICATION)
      << "storing applier configuration " << builder.slice().toJson() << " for "
      << _databaseName;

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = engine->saveReplicationApplierConfiguration(builder.slice(), doSync);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief load a persisted configuration for the applier
ReplicationApplierConfiguration GlobalReplicationApplier::loadConfiguration() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = TRI_ERROR_INTERNAL;
  VPackBuilder builder = engine->getReplicationApplierConfiguration(res);

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // file not found
    TRI_ASSERT(builder.isEmpty());
    return ReplicationApplierConfiguration(engine->server());
  }

  TRI_ASSERT(!builder.isEmpty());

  return ReplicationApplierConfiguration::fromVelocyPack(engine->server(),
                                                         builder.slice(), std::string());
}

std::shared_ptr<InitialSyncer> GlobalReplicationApplier::buildInitialSyncer() const {
  return std::make_shared<arangodb::GlobalInitialSyncer>(_configuration);
}

std::shared_ptr<TailingSyncer> GlobalReplicationApplier::buildTailingSyncer(
    TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) const {
  return std::make_shared<arangodb::GlobalTailingSyncer>(_configuration, initialTick,
                                                         useTick, barrierId);
}

std::string GlobalReplicationApplier::getStateFilename() const {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto& sysDbFeature =
      _configuration._server.getFeature<arangodb::SystemDatabaseFeature>();
  auto vocbase = sysDbFeature.use();

  std::string const path = engine->databasePath(vocbase.get());
  if (path.empty()) {
    return std::string();
  }

  return arangodb::basics::FileUtils::buildFilename(
      path, "GLOBAL-REPLICATION-APPLIER-STATE");
}
