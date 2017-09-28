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
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>

using namespace arangodb;

/// @brief replication applier for a single database
DatabaseReplicationApplier::DatabaseReplicationApplier(ReplicationApplierConfiguration const& configuration,
                                                       TRI_vocbase_t* vocbase)
    : ReplicationApplier(configuration), _vocbase(vocbase) {}

DatabaseReplicationApplier::~DatabaseReplicationApplier() {}
  
/// @brief load the applier state from persistent storage
void DatabaseReplicationApplier::loadState() {}
  
/// @brief store the applier state in persistent storage
void DatabaseReplicationApplier::persistState() {}
 
/// @brief store the current applier state in the passed vpack builder 
void DatabaseReplicationApplier::toVelocyPack(arangodb::velocypack::Builder& result) const {}

/// @brief start the applier
void DatabaseReplicationApplier::start() {}
  
/// @brief stop the applier
void DatabaseReplicationApplier::stop() {}
