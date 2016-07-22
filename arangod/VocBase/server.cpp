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
#include "Basics/HybridLogicalClock.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RestServer/RestServerFeature.h"
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
/// @brief current tick identifier (48 bit)
////////////////////////////////////////////////////////////////////////////////

static std::atomic<uint64_t> CurrentTick(0);

////////////////////////////////////////////////////////////////////////////////
/// @brief a hybrid logical clock
////////////////////////////////////////////////////////////////////////////////

static HybridLogicalClock hybridLogicalClock;

////////////////////////////////////////////////////////////////////////////////
/// @brief create app subdirectory for a database
////////////////////////////////////////////////////////////////////////////////

static int CreateApplicationDirectory(std::string const& name, std::string const& basePath) {
  if (basePath.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  std::string const path = basics::FileUtils::buildFilename(basics::FileUtils::buildFilename(basePath, "db"), name);
  int res = TRI_ERROR_NO_ERROR;

  if (!TRI_IsDirectory(path.c_str())) {
    long systemError;
    std::string errorMessage;
    res = TRI_CreateRecursiveDirectory(path.c_str(), systemError, errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      if (arangodb::wal::LogfileManager::instance()->isInRecovery()) {
        LOG(TRACE) << "created application directory '" << path
                    << "' for database '" << name << "'";
      } else {
        LOG(INFO) << "created application directory '" << path
                  << "' for database '" << name << "'";
      }
    } else if (res == TRI_ERROR_FILE_EXISTS) {
      LOG(INFO) << "unable to create application directory '" << path
                << "' for database '" << name << "': " << errorMessage;
      res = TRI_ERROR_NO_ERROR;
    } else {
      LOG(ERR) << "unable to create application directory '" << path
                << "' for database '" << name << "': " << errorMessage;
    }
  }

  return res;
}

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
/// @brief save a parameter.json file for a database
////////////////////////////////////////////////////////////////////////////////

static int SaveDatabaseParameters(TRI_voc_tick_t id, char const* name,
                                  bool deleted,
                                  char const* directory) {
  TRI_ASSERT(id > 0);
  TRI_ASSERT(name != nullptr);
  TRI_ASSERT(directory != nullptr);

  std::string const file = arangodb::basics::FileUtils::buildFilename(
      directory, TRI_VOC_PARAMETER_FILE);

  // Build the VelocyPack to store
  VPackBuilder builder;
  try {
    builder.openObject();
    builder.add("id", VPackValue(std::to_string(id)));
    builder.add("name", VPackValue(name));
    builder.add("deleted", VPackValue(deleted));
    builder.close();
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (!arangodb::basics::VelocyPackHelper::velocyPackToFile(
          file.c_str(), builder.slice(), true)) {
    LOG(ERR) << "cannot save database information in file '" << file << "'";

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database directory and return its name
////////////////////////////////////////////////////////////////////////////////

static int CreateDatabaseDirectory(TRI_server_t* server, TRI_voc_tick_t tick,
                                   char const* databaseName,
                                   std::string& name) {
  TRI_ASSERT(server != nullptr);
  TRI_ASSERT(databaseName != nullptr);

  std::string const dname("database-" + std::to_string(tick));
  std::string const dirname(arangodb::basics::FileUtils::buildFilename(
      server->_databasePath, dname.c_str()));

  // use a temporary directory first. otherwise, if creation fails, the server
  // might be left with an empty database directory at restart, and abort.

  std::string const tmpname(dirname + ".tmp");

  if (TRI_IsDirectory(tmpname.c_str())) {
    TRI_RemoveDirectory(tmpname.c_str());
  }

  std::string errorMessage;
  long systemError;

  int res = TRI_CreateDirectory(tmpname.c_str(), systemError, errorMessage);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res != TRI_ERROR_FILE_EXISTS) {
      LOG(ERR) << "failed to create database directory: " << errorMessage;
    }
    return res;
  }

  TRI_IF_FAILURE("CreateDatabase::tempDirectory") { return TRI_ERROR_DEBUG; }

  std::string const tmpfile(
      arangodb::basics::FileUtils::buildFilename(tmpname, ".tmp"));
  res = TRI_WriteFile(tmpfile.c_str(), "", 0);

  TRI_IF_FAILURE("CreateDatabase::tempFile") { return TRI_ERROR_DEBUG; }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_RemoveDirectory(tmpname.c_str());
    return res;
  }

  // finally rename
  res = TRI_RenameFile(tmpname.c_str(), dirname.c_str());

  TRI_IF_FAILURE("CreateDatabase::renameDirectory") { return TRI_ERROR_DEBUG; }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_RemoveDirectory(tmpname.c_str());  // clean up
    return res;
  }

  // now everything is valid

  res = SaveDatabaseParameters(tick, databaseName, false, dirname.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // finally remove the .tmp file
  {
    std::string const tmpfile(
        arangodb::basics::FileUtils::buildFilename(dirname, ".tmp"));
    TRI_UnlinkFile(tmpfile.c_str());
  }

  // takes ownership of the string
  name = dname;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a create-database marker into the log
////////////////////////////////////////////////////////////////////////////////

static int WriteCreateMarker(TRI_voc_tick_t id, VPackSlice const& slice) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::DatabaseMarker marker(TRI_DF_MARKER_VPACK_CREATE_DATABASE,
                                         id, slice);
    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      // throw an exception which is caught at the end of this function
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "could not save create database marker in log: "
              << TRI_errno_string(res);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a drop-database marker into the log
////////////////////////////////////////////////////////////////////////////////

static int WriteDropMarker(TRI_voc_tick_t id) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("id", VPackValue(std::to_string(id)));
    builder.close();

    arangodb::wal::DatabaseMarker marker(TRI_DF_MARKER_VPACK_DROP_DATABASE, id,
                                         builder.slice());

    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      // throw an exception which is caught at the end of this function
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "could not save drop database marker in log: "
              << TRI_errno_string(res);
  }

  return res;
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
/// @brief create a new database
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateCoordinatorDatabaseServer(TRI_server_t* server,
                                        TRI_voc_tick_t tick, char const* name,
                                        TRI_vocbase_t** database) {
  if (!TRI_IsAllowedNameVocBase(true, name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  MUTEX_LOCKER(mutexLocker, DatabaseCreateLock);

  {
    auto unuser(server->_databasesProtector.use());
    auto theLists = server->_databasesLists.load();

    auto it = theLists->_coordinatorDatabases.find(std::string(name));
    if (it != theLists->_coordinatorDatabases.end()) {
      // name already in use
      return TRI_ERROR_ARANGO_DUPLICATE_NAME;
    }
  }

  // name not yet in use, release the read lock

  TRI_vocbase_t* vocbase = TRI_CreateInitialVocBase(
      server, TRI_VOCBASE_TYPE_COORDINATOR, "none", tick, name);

  if (vocbase == nullptr) {
    // grab last error
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      // but we must have an error...
      res = TRI_ERROR_INTERNAL;
    }

    LOG(ERR) << "could not create database '" << name
             << "': " << TRI_errno_string(res);

    return res;
  }

  TRI_ASSERT(vocbase != nullptr);

  vocbase->_replicationApplier = TRI_CreateReplicationApplier(server, vocbase);

  if (vocbase->_replicationApplier == nullptr) {
    delete vocbase;

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // increase reference counter
  TRI_UseVocBase(vocbase);
  vocbase->_state = (sig_atomic_t)TRI_VOCBASE_STATE_NORMAL;

  {
    MUTEX_LOCKER(mutexLocker, server->_databasesMutex);
    auto oldLists = server->_databasesLists.load();
    decltype(oldLists) newLists = nullptr;
    try {
      newLists = new DatabasesLists(*oldLists);
      newLists->_coordinatorDatabases.emplace(std::string(vocbase->_name),
                                              vocbase);
    } catch (...) {
      delete newLists;
      delete vocbase;
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    server->_databasesLists = newLists;
    server->_databasesProtector.scan();
    delete oldLists;
  }

  *database = vocbase;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateDatabaseServer(TRI_server_t* server, TRI_voc_tick_t databaseId,
                             char const* name,
                             TRI_vocbase_t** database, bool writeMarker) {
  if (!TRI_IsAllowedNameVocBase(false, name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  TRI_vocbase_t* vocbase = nullptr;
  VPackBuilder builder;
  int res;

  // the create lock makes sure no one else is creating a database while we're
  // inside
  // this function
  MUTEX_LOCKER(mutexLocker, DatabaseCreateLock);
  {
    {
      auto unuser(server->_databasesProtector.use());
      auto theLists = server->_databasesLists.load();

      auto it = theLists->_databases.find(std::string(name));
      if (it != theLists->_databases.end()) {
        // name already in use
        return TRI_ERROR_ARANGO_DUPLICATE_NAME;
      }
    }

    // create the database directory
    if (databaseId == 0) {
      databaseId = TRI_NewTickServer();
    }

    if (writeMarker) {
      try {
        builder.openObject();
        builder.add("database", VPackValue(databaseId));
      } catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

    std::string dirname;
    res = CreateDatabaseDirectory(server, databaseId, name, dirname);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    std::string const path(arangodb::basics::FileUtils::buildFilename(
        server->_databasePath, dirname.c_str()));

    if (arangodb::wal::LogfileManager::instance()->isInRecovery()) {
      LOG(TRACE) << "creating database '" << name << "', directory '" << path
                 << "'";
    } else {
      LOG(INFO) << "creating database '" << name << "', directory '" << path
                << "'";
    }

    vocbase = TRI_OpenVocBase(server, path.c_str(), databaseId, name, false, false);

    if (vocbase == nullptr) {
      // grab last error
      res = TRI_errno();

      if (res != TRI_ERROR_NO_ERROR) {
        // but we must have an error...
        res = TRI_ERROR_INTERNAL;
      }

      LOG(ERR) << "could not create database '" << name
               << "': " << TRI_errno_string(res);

      return res;
    }

    TRI_ASSERT(vocbase != nullptr);

    if (writeMarker) {
      try {
        builder.add("id", VPackValue(std::to_string(databaseId)));
        builder.add("name", VPackValue(name));
      } catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

    // create application directories
    V8DealerFeature* dealer =
        ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
    auto appPath = dealer->appPath();

    CreateApplicationDirectory(vocbase->_name, appPath);

    if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
      TRI_StartCompactorVocBase(vocbase);

      // start the replication applier
      if (vocbase->_replicationApplier->_configuration._autoStart) {
        if (server->_disableReplicationAppliers) {
          LOG(INFO)
              << "replication applier explicitly deactivated for database '"
              << name << "'";
        } else {
          res = vocbase->_replicationApplier->start(0, false, 0);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG(WARN) << "unable to start replication applier for database '"
                      << name << "': " << TRI_errno_string(res);
          }
        }
      }

      // increase reference counter
      TRI_UseVocBase(vocbase);
    }

    {
      MUTEX_LOCKER(mutexLocker, server->_databasesMutex);
      auto oldLists = server->_databasesLists.load();
      decltype(oldLists) newLists = nullptr;
      try {
        newLists = new DatabasesLists(*oldLists);
        newLists->_databases.insert(
            std::make_pair(std::string(vocbase->_name), vocbase));
      } catch (...) {
        LOG(ERR) << "Out of memory for putting new database into list!";
        // This is bad, but at least we do not crash!
      }
      if (newLists != nullptr) {
        server->_databasesLists = newLists;
        server->_databasesProtector.scan();
        delete oldLists;
      }
    }

  }  // release DatabaseCreateLock

  // write marker into log
  if (writeMarker) {
    builder.close();
    res = WriteCreateMarker(databaseId, builder.slice());
  }

  *database = vocbase;

  return res;
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
/// @brief drops an existing coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdCoordinatorDatabaseServer(TRI_server_t* server,
                                          TRI_voc_tick_t id, bool force) {
  int res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;

  MUTEX_LOCKER(mutexLocker, server->_databasesMutex);
  auto oldLists = server->_databasesLists.load();
  decltype(oldLists) newLists = nullptr;
  TRI_vocbase_t* vocbase = nullptr;
  try {
    newLists = new DatabasesLists(*oldLists);

    for (auto it = newLists->_coordinatorDatabases.begin();
         it != newLists->_coordinatorDatabases.end(); it++) {
      vocbase = it->second;

      if (vocbase->_id == id &&
          (force ||
           !TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE))) {
        newLists->_droppedDatabases.emplace(vocbase);
        newLists->_coordinatorDatabases.erase(it);
        break;
      }
      vocbase = nullptr;
    }
  } catch (...) {
    delete newLists;
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  if (vocbase != nullptr) {
    server->_databasesLists = newLists;
    server->_databasesProtector.scan();
    delete oldLists;

    if (TRI_DropVocBase(vocbase)) {
      LOG(INFO) << "dropping coordinator database '" << vocbase->_name << "'";
      res = TRI_ERROR_NO_ERROR;
    }
  } else {
    delete newLists;
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropDatabaseServer(TRI_server_t* server, char const* name,
                           bool removeAppsDirectory, bool writeMarker) {
  if (TRI_EqualString(name, TRI_VOC_SYSTEM_DATABASE)) {
    // prevent deletion of system database
    return TRI_ERROR_FORBIDDEN;
  }

  MUTEX_LOCKER(mutexLocker, server->_databasesMutex);

  auto oldLists = server->_databasesLists.load();
  decltype(oldLists) newLists = nullptr;
  TRI_vocbase_t* vocbase = nullptr;
  try {
    newLists = new DatabasesLists(*oldLists);

    auto it = newLists->_databases.find(std::string(name));
    if (it == newLists->_databases.end()) {
      // not found
      delete newLists;
      return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
    } else {
      vocbase = it->second;
      // mark as deleted
      TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);

      newLists->_databases.erase(it);
      newLists->_droppedDatabases.insert(vocbase);
    }
  } catch (...) {
    delete newLists;
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  server->_databasesLists = newLists;
  server->_databasesProtector.scan();
  delete oldLists;

  vocbase->_isOwnAppsDirectory = removeAppsDirectory;

  // invalidate all entries for the database
  arangodb::aql::QueryCache::instance()->invalidate(vocbase);

  int res = TRI_ERROR_NO_ERROR;

  if (TRI_DropVocBase(vocbase)) {
    if (arangodb::wal::LogfileManager::instance()->isInRecovery()) {
      LOG(TRACE) << "dropping database '" << vocbase->_name << "', directory '"
                 << vocbase->_path << "'";
    } else {
      LOG(INFO) << "dropping database '" << vocbase->_name << "', directory '"
                << vocbase->_path << "'";
    }

    res = SaveDatabaseParameters(vocbase->_id, vocbase->_name, true, vocbase->_path);
    // TODO: what to do here in case of error?

    if (writeMarker) {
      WriteDropMarker(vocbase->_id);
    }
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdDatabaseServer(TRI_server_t* server, TRI_voc_tick_t id,
                               bool removeAppsDirectory, bool writeMarker) {
  std::string name;

  {
    auto unuser(server->_databasesProtector.use());
    auto theLists = server->_databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;

      if (vocbase->_id == id) {
        name = vocbase->_name;
        break;
      }
    }
  }

  return TRI_DropDatabaseServer(server, name.c_str(), removeAppsDirectory,
                                writeMarker);
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

      auto level = RestServerFeature::AUTH_INFO.canUseDatabase(username, dbName);

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
/// @brief create a new tick
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NewTickServer() { return ++CurrentTick; }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new tick, using a hybrid logical clock
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_HybridLogicalClock(void) {
  return hybridLogicalClock.getTimeStamp();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new tick, using a hybrid logical clock, this variant
/// is supposed to be called when a time stamp is received in network
/// communications.
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_HybridLogicalClock(TRI_voc_tick_t received) {
  return hybridLogicalClock.getTimeStamp(received);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, with lock
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTickServer(TRI_voc_tick_t tick) {
  TRI_voc_tick_t t = tick;

  auto expected = CurrentTick.load(std::memory_order_relaxed);

  // only update global tick if less than the specified value...
  while (expected < t &&
         !CurrentTick.compare_exchange_weak(expected, t,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) {
    expected = CurrentTick.load(std::memory_order_relaxed);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current tick counter
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_CurrentTickServer() { return CurrentTick; }

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
