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

#include "server.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "ApplicationFeatures/PageSizeFeature.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Utils/CursorRepository.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/replication-applier.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBIndex.h"
#endif

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief interval for database manager activity
////////////////////////////////////////////////////////////////////////////////

#define DATABASE_MANAGER_INTERVAL (500 * 1000)

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for serializing the creation of database
////////////////////////////////////////////////////////////////////////////////

static arangodb::Mutex DatabaseCreateLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief variable protecting the server shutdown
////////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> ServerShutdown;

////////////////////////////////////////////////////////////////////////////////
/// @brief server operation mode (e.g. read-only, normal etc).
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_operationmode_e Mode = TRI_VOCBASE_MODE_NORMAL;

////////////////////////////////////////////////////////////////////////////////
/// @brief close all opened databases
////////////////////////////////////////////////////////////////////////////////

static int CloseDatabases(TRI_server_t* server) {
  MUTEX_LOCKER(mutexLocker,
               server->_databasesMutex);  // Only one should do this at a time
  // No need for the thread protector here, because we have the mutex
  // Note however, that somebody could still read the lists concurrently,
  // therefore we first install a new value, call scan() on the protector
  // and only then really destroy the vocbases:

  // Build the new value:
  auto oldList = server->_databasesLists.load();
  decltype(oldList) newList = nullptr;
  try {
    newList = new DatabasesLists();
    newList->_droppedDatabases =
        server->_databasesLists.load()->_droppedDatabases;
  } catch (...) {
    delete newList;
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // Replace the old by the new:
  server->_databasesLists = newList;
  server->_databasesProtector.scan();

  // Now it is safe to destroy the old databases and the old lists struct:
  for (auto& p : oldList->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);

    TRI_DestroyVocBase(vocbase);

    delete vocbase;
  }

  for (auto& p : oldList->_coordinatorDatabases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR);

    delete vocbase;
  }

  delete oldList;  // Note that this does not delete the TRI_vocbase_t pointers!

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes all databases
////////////////////////////////////////////////////////////////////////////////

int TRI_InitDatabasesServer(TRI_server_t* server) {
  auto unuser(server->_databasesProtector.use());
  auto theLists = server->_databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    // iterate over all databases
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);

    // start the compactor for the database
    TRI_StartCompactorVocBase(vocbase);

    // start the replication applier
    TRI_ASSERT(vocbase->_replicationApplier != nullptr);

    if (vocbase->_replicationApplier->_configuration._autoStart) {
      if (server->_disableReplicationAppliers) {
        LOG(INFO) << "replication applier explicitly deactivated for database '"
                  << vocbase->_name << "'";
      } else {
        int res = vocbase->_replicationApplier->start(0, false, 0);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "unable to start replication applier for database '"
                    << vocbase->_name << "': " << TRI_errno_string(res);
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the ids of all local coordinator databases
/// the caller is responsible for freeing the result
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_voc_tick_t> TRI_GetIdsCoordinatorDatabaseServer(
    TRI_server_t* server, bool includeSystem) {
  std::vector<TRI_voc_tick_t> v;
  {
    auto unuser(server->_databasesProtector.use());
    auto theLists = server->_databasesLists.load();

    for (auto& p : theLists->_coordinatorDatabases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);

      if (includeSystem ||
          !TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE)) {
        v.emplace_back(vocbase->_id);
      }
    }
  }
  return v;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a coordinator database by its id
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseByIdCoordinatorDatabaseServer(TRI_server_t* server,
                                                    TRI_voc_tick_t id) {
  auto unuser(server->_databasesProtector.use());
  auto theLists = server->_databasesLists.load();

  for (auto& p : theLists->_coordinatorDatabases) {
    TRI_vocbase_t* vocbase = p.second;

    if (vocbase->_id == id) {
      bool result TRI_UNUSED = TRI_UseVocBase(vocbase);

      // if we got here, no one else can have deleted the database
      TRI_ASSERT(result == true);
      return vocbase;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a coordinator database by its name
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseCoordinatorDatabaseServer(TRI_server_t* server,
                                                char const* name) {
  auto unuser(server->_databasesProtector.use());
  auto theLists = server->_databasesLists.load();
  auto it = theLists->_coordinatorDatabases.find(std::string(name));
  TRI_vocbase_t* vocbase = nullptr;

  if (it != theLists->_coordinatorDatabases.end()) {
    vocbase = it->second;
    TRI_UseVocBase(vocbase);
  }

  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a database by its name
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseDatabaseServer(TRI_server_t* server, char const* name) {
  auto unuser(server->_databasesProtector.use());
  auto theLists = server->_databasesLists.load();
  auto it = theLists->_databases.find(std::string(name));
  TRI_vocbase_t* vocbase = nullptr;

  if (it != theLists->_databases.end()) {
    vocbase = it->second;
    TRI_UseVocBase(vocbase);
  }

  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a database by its name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_LookupDatabaseByNameServer(TRI_server_t* server,
                                              char const* name) {
  auto unuser(server->_databasesProtector.use());
  auto theLists = server->_databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;

    if (TRI_EqualString(vocbase->_name, name)) {
      return vocbase;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a database by its id
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseDatabaseByIdServer(TRI_server_t* server,
                                         TRI_voc_tick_t id) {
  auto unuser(server->_databasesProtector.use());
  auto theLists = server->_databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;

    if (vocbase->_id == id) {
      TRI_UseVocBase(vocbase);
      return vocbase;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release a previously used database
/// this will decrease the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseDatabaseServer(TRI_server_t* server, TRI_vocbase_t* vocbase) {
  TRI_ReleaseVocBase(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all databases a user can see
////////////////////////////////////////////////////////////////////////////////

int TRI_GetUserDatabasesServer(TRI_server_t* server, char const* username,
                               std::vector<std::string>& names) {
  int res = TRI_ERROR_NO_ERROR;

  {
    auto unuser(server->_databasesProtector.use());
    auto theLists = server->_databasesLists.load();

    for (auto& p : theLists->_databases) {
      char const* dbName = p.second->_name;
      TRI_ASSERT(dbName != nullptr);

      auto level =
          GeneralServerFeature::AUTH_INFO.canUseDatabase(username, dbName);

      if (level == AuthLevel::NONE) {
        continue;
      }

      try {
        names.emplace_back(dbName);
      } catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  std::sort(
      names.begin(), names.end(),
      [](std::string const& l, std::string const& r) -> bool { return l < r; });

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all database names
////////////////////////////////////////////////////////////////////////////////

int TRI_GetDatabaseNamesServer(TRI_server_t* server,
                               std::vector<std::string>& names) {
  int res = TRI_ERROR_NO_ERROR;

  {
    auto unuser(server->_databasesProtector.use());
    auto theLists = server->_databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      TRI_ASSERT(vocbase->_name != nullptr);

      try {
        names.emplace_back(vocbase->_name);
      } catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  std::sort(
      names.begin(), names.end(),
      [](std::string const& l, std::string const& r) -> bool { return l < r; });

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the current operation mode of the server
////////////////////////////////////////////////////////////////////////////////

int TRI_ChangeOperationModeServer(TRI_vocbase_operationmode_e mode) {
  Mode = mode;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current operation server of the server
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_operationmode_e TRI_GetOperationModeServer() { return Mode; }

TRI_server_t::TRI_server_t()
    : _databasesLists(new DatabasesLists()),
      _queryRegistry(nullptr),
      _basePath(nullptr),
      _databasePath(nullptr),
      _disableReplicationAppliers(false),
      _disableCompactor(false),
      _iterateMarkersOnOpen(false),
      _initialized(false) {}

TRI_server_t::~TRI_server_t() {
  if (_initialized) {
    CloseDatabases(this);

    auto p = _databasesLists.load();
    delete p;

    TRI_Free(TRI_CORE_MEM_ZONE, _databasePath);
    TRI_Free(TRI_CORE_MEM_ZONE, _basePath);
  }
}
