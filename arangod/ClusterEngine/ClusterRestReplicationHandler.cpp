////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterRestReplicationHandler.h"
#include "Basics/StaticStrings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Replication/InitialSyncer.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

ClusterRestReplicationHandler::ClusterRestReplicationHandler(GeneralRequest* request,
                                                             GeneralResponse* response)
    : RestReplicationHandler(request, response) {}

void ClusterRestReplicationHandler::handleCommandBatch() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterRestReplicationHandler::handleCommandBarrier() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterRestReplicationHandler::handleCommandLoggerFollow() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief run the command that determines which transactions were open at
/// a given tick value
/// this is an internal method use by ArangoDB's replication that should not
/// be called by client drivers directly
void ClusterRestReplicationHandler::handleCommandDetermineOpenTransactions() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterRestReplicationHandler::handleCommandInventory() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief produce list of keys for a specific collection
void ClusterRestReplicationHandler::handleCommandCreateKeys() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief returns all key ranges
void ClusterRestReplicationHandler::handleCommandGetKeys() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief returns date for a key range
void ClusterRestReplicationHandler::handleCommandFetchKeys() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterRestReplicationHandler::handleCommandRemoveKeys() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void ClusterRestReplicationHandler::handleCommandDump() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
