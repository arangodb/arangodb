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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterRestReplicationHandler.h"

#include "Futures/Future.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

ClusterRestReplicationHandler::ClusterRestReplicationHandler(
    ArangodServer& server, GeneralRequest* request, GeneralResponse* response)
    : RestReplicationHandler(server, request, response) {}

futures::Future<futures::Unit>
ClusterRestReplicationHandler::handleCommandBatch() {
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
futures::Future<futures::Unit>
ClusterRestReplicationHandler::handleCommandCreateKeys() {
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

void ClusterRestReplicationHandler::handleCommandRevisionTree() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
