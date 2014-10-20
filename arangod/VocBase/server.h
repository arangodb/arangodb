////////////////////////////////////////////////////////////////////////////////
/// @brief database server functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_SERVER_H
#define ARANGODB_VOC_BASE_SERVER_H 1

#include "Basics/Common.h"
#include "Basics/associative.h"
#include "Basics/locks.h"
#include "Basics/threads.h"
#include "Basics/vector.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase-defaults.h"

struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief server structure
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_server_s {
  TRI_associative_pointer_t   _databases;
  TRI_associative_pointer_t   _coordinatorDatabases;
  TRI_read_write_lock_t       _databasesLock;

  TRI_mutex_t                 _createLock;
  TRI_thread_t                _databaseManager;
  TRI_vector_pointer_t        _droppedDatabases;
  bool                        _shutdown;

  TRI_vocbase_defaults_t      _defaults;
  void*                       _applicationEndpointServer; // ptr to C++ object

  char*                       _basePath;
  char*                       _databasePath;
  char*                       _lockFilename;
  char*                       _serverIdFilename;

  char*                       _appPath;
  char*                       _devAppPath;

  bool                        _disableReplicationAppliers;
  bool                        _iterateMarkersOnOpen;
  bool                        _hasCreatedSystemDatabase;

  bool                        _initialised;
  void*                       _queryRegistry;
}
TRI_server_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief page size
////////////////////////////////////////////////////////////////////////////////

extern size_t PageSize;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a server instance
////////////////////////////////////////////////////////////////////////////////

TRI_server_t* TRI_CreateServer (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a server instance with configuration
////////////////////////////////////////////////////////////////////////////////

int TRI_InitServer (TRI_server_t* server,
                    void* databasePath,
                    char const* basePath,
                    char const* appPath,
                    char const* devappPath,
                    TRI_vocbase_defaults_t const*,
                    bool disableReplicationApplier,
                    bool iterateMarkersOnOpen);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a server instance
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a server instance
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise globals
////////////////////////////////////////////////////////////////////////////////

void TRI_InitServerGlobals (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief de-initialise globals
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeServerGlobals (void);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the global server id
////////////////////////////////////////////////////////////////////////////////

TRI_server_id_t TRI_GetIdServer (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief start the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StartServer (TRI_server_t*,
                     bool checkVersion,
                     bool performUpgrade);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises all databases
////////////////////////////////////////////////////////////////////////////////

int TRI_InitDatabasesServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StopServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication appliers
////////////////////////////////////////////////////////////////////////////////

void TRI_StopReplicationAppliersServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateCoordinatorDatabaseServer (TRI_server_t*,
                                         TRI_voc_tick_t,
                                         char const*,
                                         TRI_vocbase_defaults_t const*,
                                         struct TRI_vocbase_s**);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateDatabaseServer (TRI_server_t*,
                              TRI_voc_tick_t,
                              char const*,
                              TRI_vocbase_defaults_t const*,
                              struct TRI_vocbase_s**,
                              bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the ids of all local coordinator databases
/// the caller is responsible for freeing the result
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t* TRI_GetIdsCoordinatorDatabaseServer (TRI_server_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdCoordinatorDatabaseServer (TRI_server_t*,
                                           TRI_voc_tick_t,
                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropCoordinatorDatabaseServer (TRI_server_t*,
                                       char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropDatabaseServer (TRI_server_t*,
                            char const*,
                            bool,
                            bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdDatabaseServer (TRI_server_t*,
                                TRI_voc_tick_t,
                                bool,
                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a coordinator database by its id
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_s* TRI_UseByIdCoordinatorDatabaseServer (TRI_server_t*,
                                                            TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief use a coordinator database by its name
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_s* TRI_UseCoordinatorDatabaseServer (TRI_server_t*,
                                                        char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief use a database by its name
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_s* TRI_UseDatabaseServer (TRI_server_t*,
                                             char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a database by its id
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_s* TRI_LookupDatabaseByIdServer (TRI_server_t*,
                                                    TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a database by its name
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_s* TRI_LookupDatabaseByNameServer (TRI_server_t*,
                                                      char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief use a database by its id
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_s* TRI_UseDatabaseByIdServer (TRI_server_t*,
                                                 TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief release a previously used database
/// this will decrease the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseDatabaseServer (TRI_server_t*,
                                struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a database exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExistsDatabaseByIdServer (TRI_server_t*,
                                   TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all databases a user can see
////////////////////////////////////////////////////////////////////////////////

int TRI_GetUserDatabasesServer (TRI_server_t*,
                                char const*,
                                char const*,
                                TRI_vector_string_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all database names
////////////////////////////////////////////////////////////////////////////////

int TRI_GetDatabaseNamesServer (TRI_server_t*,
                                TRI_vector_string_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the defaults into the target
////////////////////////////////////////////////////////////////////////////////

void TRI_GetDatabaseDefaultsServer (TRI_server_t*,
                                    TRI_vocbase_defaults_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                    tick functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new tick
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NewTickServer (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, with lock
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTickServer (TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, without lock - only use at startup!!
////////////////////////////////////////////////////////////////////////////////

void TRI_FastUpdateTickServer (TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current tick counter
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_CurrentTickServer (void);

// -----------------------------------------------------------------------------
// --SECTION--                                                   other functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief msyncs a memory block between begin (incl) and end (excl)
////////////////////////////////////////////////////////////////////////////////

bool TRI_MSync (int,
                void*,
                char const*,
                char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the current operation mode of the server
////////////////////////////////////////////////////////////////////////////////

int TRI_ChangeOperationModeServer (TRI_vocbase_operationmode_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current operation mode of the server
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_operationmode_e TRI_GetOperationModeServer ();

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
