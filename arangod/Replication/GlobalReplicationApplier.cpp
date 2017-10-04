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
#include "Logger/Logger.h"
#include "Replication/GlobalTailingSyncer.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <velocypack/Builder.h>

using namespace arangodb;

/// @brief server-global replication applier for all databases
GlobalReplicationApplier::GlobalReplicationApplier(ReplicationApplierConfiguration const& configuration)
      : ReplicationApplier(configuration, "global database") {}

GlobalReplicationApplier::~GlobalReplicationApplier() {
  try {
    stop(true);
  } catch (...) {}
}

/// @brief stop the applier and "forget" everything
void GlobalReplicationApplier::forget() {
  stopAndJoin(true);

  removeState();

// TODO: _vocbase!
//  StorageEngine* engine = EngineSelectorFeature::ENGINE;
//  engine->removeReplicationApplierConfiguration(_vocbase);
  _configuration.reset();
}
  
/// @brief store the configuration for the applier
void GlobalReplicationApplier::storeConfiguration(bool doSync) {
  VPackBuilder builder;
  builder.openObject();
  _configuration.toVelocyPack(builder, true);
  builder.close();

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "storing applier configuration " << builder.slice().toJson();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = engine->saveReplicationApplierConfiguration(builder.slice(), doSync);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief load a persisted configuration for the applier
ReplicationApplierConfiguration GlobalReplicationApplier::loadConfiguration() {
  // TODO: move to ReplicationApplier 
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = TRI_ERROR_INTERNAL;
  VPackBuilder builder = engine->getReplicationApplierConfiguration(res);

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // file not found
    TRI_ASSERT(builder.isEmpty());
    return ReplicationApplierConfiguration();
  }

  TRI_ASSERT(!builder.isEmpty());

  return ReplicationApplierConfiguration::fromVelocyPack(builder.slice(), std::string());
}

std::unique_ptr<TailingSyncer> GlobalReplicationApplier::buildSyncer(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) {
  return std::make_unique<arangodb::GlobalTailingSyncer>(_configuration, initialTick, useTick, barrierId);
}

std::string GlobalReplicationApplier::getStateFilename() const {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  // TODO: fix storage path
  TRI_vocbase_t* vocbase = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database")->systemDatabase();

  return arangodb::basics::FileUtils::buildFilename(engine->databasePath(vocbase), "REPLICATION-APPLIER-STATE");
}
