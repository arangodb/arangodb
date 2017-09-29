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

#include <velocypack/Builder.h>

using namespace arangodb;

/// @brief server-global replication applier for all databases
GlobalReplicationApplier::GlobalReplicationApplier(ReplicationApplierConfiguration const& configuration)
      : ReplicationApplier(configuration, "global database") {}

GlobalReplicationApplier::~GlobalReplicationApplier() {
  stop(true, false);
}

/// @brief stop the applier and "forget" everything
void GlobalReplicationApplier::forget() {
  stop(true, true);

  removeState();

// TODO: _vocbase!
//  StorageEngine* engine = EngineSelectorFeature::ENGINE;
//  engine->removeReplicationApplierConfiguration(_vocbase);
  _configuration.reset();
}

/// @brief start the replication applier
void GlobalReplicationApplier::start(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) {}
  
/// @brief stop the replication applier
void GlobalReplicationApplier::stop(bool resetError, bool joinThread) {}

/// @brief shuts down the replication applier
void GlobalReplicationApplier::shutdown() {
  // simply fall back to base class implementation
  ReplicationApplier::shutdown();
}
  
/// @brief configure the replication applier
void GlobalReplicationApplier::reconfigure(ReplicationApplierConfiguration const& configuration) {}
  
/// @brief remove the replication application state file
void GlobalReplicationApplier::removeState() {}
  
/// @brief load the applier state from persistent storage
/// returns whether a previous state was found
bool GlobalReplicationApplier::loadState() { return false; }
  
/// @brief store the applier state in persistent storage
void GlobalReplicationApplier::persistState(bool doSync) {}
 
/// @brief store the current applier state in the passed vpack builder 
void GlobalReplicationApplier::toVelocyPack(arangodb::velocypack::Builder& result) const {}

/// @brief return the current configuration
ReplicationApplierConfiguration GlobalReplicationApplier::configuration() const {
  return _configuration;
}
