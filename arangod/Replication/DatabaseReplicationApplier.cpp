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

#include "DatabaseReplicationApplier.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseTailingSyncer.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

/// @brief replication applier for a single database, without configuration
DatabaseReplicationApplier::DatabaseReplicationApplier(TRI_vocbase_t& vocbase)
    : DatabaseReplicationApplier(ReplicationApplierConfiguration(), vocbase) {}

/// @brief replication applier for a single database, with configuration
DatabaseReplicationApplier::DatabaseReplicationApplier(
    ReplicationApplierConfiguration const& configuration,
    TRI_vocbase_t& vocbase
): ReplicationApplier(configuration, std::string("database '") + vocbase.name() + "'"), 
   _vocbase(vocbase) {
}

DatabaseReplicationApplier::~DatabaseReplicationApplier() {
  try {
    stop();
  } catch (...) {}
}
  
/// @brief execute the check condition
bool DatabaseReplicationApplier::applies() const {
  return (_vocbase.type() == TRI_VOCBASE_TYPE_NORMAL);
}

/// @brief configure the replication applier
void DatabaseReplicationApplier::reconfigure(ReplicationApplierConfiguration const& configuration) {
  if (configuration._database.empty()) {
    // no database
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "no database configured");
  }

  ReplicationApplier::reconfigure(configuration); 
}

/// @brief stop the applier and "forget" everything
void DatabaseReplicationApplier::forget() {
  if (!applies()) {
    // unsupported
    return;
  }
  // TODO: move to ReplicationApplier

  stopAndJoin();

  removeState();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  engine->removeReplicationApplierConfiguration(_vocbase);
  _configuration.reset();
}

/// @brief factory function for creating a database-specific replication applier
/*static*/ DatabaseReplicationApplier* DatabaseReplicationApplier::create(
    TRI_vocbase_t& vocbase
) {
  std::unique_ptr<DatabaseReplicationApplier> applier;

  if (vocbase.type() == TRI_VOCBASE_TYPE_NORMAL) {
    applier = std::make_unique<DatabaseReplicationApplier>(
      DatabaseReplicationApplier::loadConfiguration(vocbase), vocbase
    );
    applier->loadState();
  } else {
    applier = std::make_unique<DatabaseReplicationApplier>(vocbase);
  }

  return applier.release();
}

/// @brief load a persisted configuration for the applier
ReplicationApplierConfiguration DatabaseReplicationApplier::loadConfiguration(
    TRI_vocbase_t& vocbase
) {
  // TODO: move to ReplicationApplier 
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = TRI_ERROR_INTERNAL;
  VPackBuilder builder = engine->getReplicationApplierConfiguration(vocbase, res);

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    // file not found
    TRI_ASSERT(builder.isEmpty());
    return ReplicationApplierConfiguration();
  }

  TRI_ASSERT(!builder.isEmpty());

  return ReplicationApplierConfiguration::fromVelocyPack(
    builder.slice(), vocbase.name()
  );
}

/// @brief store the configuration for the applier
void DatabaseReplicationApplier::storeConfiguration(bool doSync) {
  if (!applies()) {
    return;
  }

  VPackBuilder builder;

  builder.openObject();
  _configuration.toVelocyPack(builder, true, true);
  builder.close();

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "storing applier configuration " << builder.slice().toJson() << " for " << _databaseName;

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = engine->saveReplicationApplierConfiguration(
    _vocbase, builder.slice(), doSync
  );

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

std::shared_ptr<InitialSyncer> DatabaseReplicationApplier::buildInitialSyncer() const {
  return std::make_shared<arangodb::DatabaseInitialSyncer>(_vocbase, _configuration);
}

std::shared_ptr<TailingSyncer> DatabaseReplicationApplier::buildTailingSyncer(TRI_voc_tick_t initialTick,
                                                                              bool useTick, TRI_voc_tick_t barrierId) const {
  return std::make_shared<arangodb::DatabaseTailingSyncer>(_vocbase, _configuration,
                                                           initialTick, useTick, barrierId);
}

std::string DatabaseReplicationApplier::getStateFilename() const {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  
  std::string const path = engine->databasePath(&_vocbase);
  if (path.empty()) {
    return std::string();
  }
  return arangodb::basics::FileUtils::buildFilename(path, "REPLICATION-APPLIER-STATE");
}

} // arangodb
