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

#ifndef ARANGOD_VOC_BASE_SERVER_H
#define ARANGOD_VOC_BASE_SERVER_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/DataProtector.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
class QueryRegistry;
}
namespace rest {
class ApplicationEndpointServer;
}
}

/// @brief server structure
struct TRI_server_t {
  TRI_server_t();
  ~TRI_server_t();

  std::atomic<DatabasesLists*> _databasesLists;
  // TODO: Make this again a template once everybody has gcc >= 4.9.2
  // arangodb::basics::DataProtector<64>
  arangodb::basics::DataProtector _databasesProtector;
  arangodb::Mutex _databasesMutex;

  std::atomic<arangodb::aql::QueryRegistry*> _queryRegistry;

  char* _basePath;
  char* _databasePath;

  bool _disableReplicationAppliers;
  bool _disableCompactor;
  bool _iterateMarkersOnOpen;
  bool _initialized;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes all databases
////////////////////////////////////////////////////////////////////////////////

int TRI_InitDatabasesServer(TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the current operation mode of the server
////////////////////////////////////////////////////////////////////////////////

int TRI_ChangeOperationModeServer(TRI_vocbase_operationmode_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current operation mode of the server
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_operationmode_e TRI_GetOperationModeServer();

#endif
