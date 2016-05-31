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
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Utils/CursorRepository.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/auth.h"
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
/// @brief page size
////////////////////////////////////////////////////////////////////////////////

size_t PageSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief mask value for significant bits of server id
////////////////////////////////////////////////////////////////////////////////

#define SERVER_ID_MASK 0x0000FFFFFFFFFFFFULL

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
/// @brief random server identifier (16 bit)
////////////////////////////////////////////////////////////////////////////////

static uint16_t ServerIdentifier = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief current tick identifier (48 bit)
////////////////////////////////////////////////////////////////////////////////

static std::atomic<uint64_t> CurrentTick(0);

////////////////////////////////////////////////////////////////////////////////
/// @brief the server's global id
////////////////////////////////////////////////////////////////////////////////

static TRI_server_id_t ServerId;

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a new server id
////////////////////////////////////////////////////////////////////////////////

static int GenerateServerId(void) {
  do {
    ServerId = RandomGenerator::interval((uint64_t)SERVER_ID_MASK);
  } while (ServerId == 0);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads server id from file
////////////////////////////////////////////////////////////////////////////////

static int ReadServerId(char const* filename) {
  TRI_ASSERT(filename != nullptr);

  if (!TRI_ExistsFile(filename)) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }
  
  TRI_server_id_t foundId;
  try {
    std::string filenameString(filename);
    std::shared_ptr<VPackBuilder> builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(filenameString);
    VPackSlice content = builder->slice();
    if (!content.isObject()) {
      return TRI_ERROR_INTERNAL;
    }
    VPackSlice idSlice = content.get("serverId");
    if (!idSlice.isString()) {
      return TRI_ERROR_INTERNAL;
    }
    foundId = StringUtils::uint64(idSlice.copyString());
  } catch (...) {
    // Nothing to free
    return TRI_ERROR_INTERNAL;
  }

  LOG(TRACE) << "using existing server id: " << foundId;

  if (foundId == 0) {
    return TRI_ERROR_INTERNAL;
  }

  ServerId = foundId;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes server id to file
////////////////////////////////////////////////////////////////////////////////

static int WriteServerId(char const* filename) {
  TRI_ASSERT(filename != nullptr);
  // create a VelocyPackObject
  VPackBuilder builder;
  try {
    builder.openObject();

    TRI_ASSERT(ServerId != 0);
    builder.add("serverId", VPackValue(std::to_string(ServerId)));

    time_t tt = time(0);
    struct tm tb;
    TRI_gmtime(tt, &tb);
    char buffer[32];
    size_t len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
    builder.add("createdTime", VPackValue(std::string(buffer, len)));

    builder.close();
  } catch (...) {
    // out of memory
    LOG(ERR) << "cannot save server id in file '" << filename
             << "': out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // save json info to file
  LOG(DEBUG) << "Writing server id to file '" << filename << "'";
  bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
      filename, builder.slice(), true);

  if (!ok) {
    LOG(ERR) << "could not save server id in file '" << filename
             << "': " << TRI_last_error();

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read / create the server id on startup
////////////////////////////////////////////////////////////////////////////////

static int DetermineServerId(TRI_server_t* server, bool checkVersion) {
  int res = ReadServerId(server->_serverIdFilename);

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    if (checkVersion) {
      return TRI_ERROR_ARANGO_EMPTY_DATADIR;
    }

    // id file does not yet exist. now create it
    res = GenerateServerId();

    if (res == TRI_ERROR_NO_ERROR) {
      // id was generated. now save it
      res = WriteServerId(server->_serverIdFilename);
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a user can see a database
/// note: "seeing" here does not necessarily mean the user can access the db.
/// it only means there is a user account (with whatever password) present
/// in the database
////////////////////////////////////////////////////////////////////////////////

static bool CanUseDatabase(TRI_vocbase_t* vocbase, char const* username) {
  if (!vocbase->_settings.requireAuthentication) {
    // authentication is turned off
    return true;
  }

  if (strlen(username) == 0) {
    // will happen if username is "" (when converting it from a null value)
    // this will happen if authentication is turned off
    return true;
  }

  return TRI_ExistsAuthenticationAuthInfo(vocbase, username);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetNumericFilenamePart(char const* filename) {
  char const* pos = strrchr(filename, '-');

  if (pos == nullptr) {
    return 0;
  }

  return StringUtils::uint64(pos + 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort database filenames on startup
////////////////////////////////////////////////////////////////////////////////

static bool DatabaseIdStringComparator(std::string const& lhs,
                                       std::string const& rhs) {
  uint64_t const numLeft = GetNumericFilenamePart(lhs.c_str());
  uint64_t const numRight = GetNumericFilenamePart(rhs.c_str());

  return numLeft < numRight;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create base app directory
////////////////////////////////////////////////////////////////////////////////

static int CreateBaseApplicationDirectory(char const* basePath,
                                          char const* type) {
  if (basePath == nullptr || strlen(basePath) == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;
  std::string path = arangodb::basics::FileUtils::buildFilename(basePath, type);

  if (!TRI_IsDirectory(path.c_str())) {
    std::string errorMessage;
    long systemError;
    res = TRI_CreateDirectory(path.c_str(), systemError, errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG(INFO) << "created base application directory '" << path << "'";
    } else {
      if ((res != TRI_ERROR_FILE_EXISTS) || (!TRI_IsDirectory(path.c_str()))) {
        LOG(ERR) << "unable to create base application directory "
                 << errorMessage;
      } else {
        LOG(INFO) << "otherone created base application directory '" << path
                  << "'";
        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create app subdirectory for a database
////////////////////////////////////////////////////////////////////////////////

static int CreateApplicationDirectory(char const* name, char const* basePath) {
  if (basePath == nullptr || strlen(basePath) == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;
  char* path = TRI_Concatenate3File(basePath, "_db", name);

  if (path != nullptr) {
    if (!TRI_IsDirectory(path)) {
      long systemError;
      std::string errorMessage;
      res = TRI_CreateRecursiveDirectory(path, systemError, errorMessage);

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

    TRI_Free(TRI_CORE_MEM_ZONE, path);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all databases in the databases directory and open them
////////////////////////////////////////////////////////////////////////////////

static int OpenDatabases(TRI_server_t* server, bool isUpgrade) {
  if (server->_iterateMarkersOnOpen && !server->_hasCreatedSystemDatabase) {
    LOG(WARN) << "no shutdown info found. scanning datafiles for last tick...";
  }

  std::vector<std::string> files = TRI_FilesDirectory(server->_databasePath);

  int res = TRI_ERROR_NO_ERROR;
  size_t n = files.size();

  // open databases in defined order
  if (n > 1) {
    std::sort(files.begin(), files.end(), DatabaseIdStringComparator);
  }

  MUTEX_LOCKER(mutexLocker, server->_databasesMutex);

  auto oldLists = server->_databasesLists.load();
  auto newLists = new DatabasesLists(*oldLists);
  // No try catch here, if we crash here because out of memory...

  for (auto const& name : files) {
    TRI_ASSERT(!name.empty());

    // .........................................................................
    // construct and validate path
    // .........................................................................

    std::string const databaseDirectory(
        arangodb::basics::FileUtils::buildFilename(server->_databasePath,
                                                   name.c_str()));

    if (!TRI_IsDirectory(databaseDirectory.c_str())) {
      continue;
    }

    if (!StringUtils::isPrefix(name, "database-") ||
        StringUtils::isSuffix(name, ".tmp")) {
      LOG_TOPIC(TRACE, Logger::DATAFILES) << "ignoring file '" << name << "'";
      continue;
    }

    // we have a directory...

    if (!TRI_IsWritable(databaseDirectory.c_str())) {
      // the database directory we found is not writable for the current user
      // this can cause serious trouble so we will abort the server start if we
      // encounter this situation
      LOG(ERR) << "database directory '" << databaseDirectory
               << "' is not writable for current user";

      res = TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
      break;
    }

    // we have a writable directory...
    std::string const tmpfile(arangodb::basics::FileUtils::buildFilename(
        databaseDirectory.c_str(), ".tmp"));

    if (TRI_ExistsFile(tmpfile.c_str())) {
      // still a temporary... must ignore
      LOG(TRACE) << "ignoring temporary directory '" << tmpfile << "'";
      continue;
    }

    // a valid database directory

    // .........................................................................
    // read parameter.json file
    // .........................................................................

    // now read data from parameter.json file
    std::string const parametersFile(arangodb::basics::FileUtils::buildFilename(
        databaseDirectory, TRI_VOC_PARAMETER_FILE));

    if (!TRI_ExistsFile(parametersFile.c_str())) {
      // no parameter.json file
      
      if (TRI_FilesDirectory(databaseDirectory.c_str()).empty()) {
        // directory is otherwise empty, continue!
        LOG(WARN) << "ignoring empty database directory '" << databaseDirectory
                  << "' without parameters file";

        res = TRI_ERROR_NO_ERROR;
      } else {
        // abort
        LOG(ERR) << "database directory '" << databaseDirectory
                 << "' does not contain parameters file or parameters file "
                    "cannot be read";

        res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      }
      break;
    }

    LOG(DEBUG) << "reading database parameters from file '" << parametersFile
               << "'";
    std::shared_ptr<VPackBuilder> builder;
    try {
      builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
          parametersFile);
    } catch (...) {
      LOG(ERR) << "database directory '" << databaseDirectory
               << "' does not contain a valid parameters file";

      // abort
      res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      break;
    }
    VPackSlice parameters = builder->slice();
    std::string parametersString = parameters.toJson();

    LOG(DEBUG) << "database parameters: " << parametersString;

    if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters,
                                                            "deleted", false)) {
      // database is deleted, skip it!
      LOG(INFO) << "found dropped database in directory '" << databaseDirectory
                << "'";

      LOG(INFO) << "removing superfluous database directory '"
                << databaseDirectory << "'";

#ifdef ARANGODB_ENABLE_ROCKSDB
      VPackSlice idSlice = parameters.get("id");
      if (idSlice.isString()) {
        // delete persistent indexes for this database
        TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(StringUtils::uint64(idSlice.copyString()));
        RocksDBFeature::dropDatabase(id);
      }
#endif

      TRI_RemoveDirectory(databaseDirectory.c_str());
      continue;
    }
    VPackSlice idSlice = parameters.get("id");

    if (!idSlice.isString()) {
      LOG(ERR) << "database directory '" << databaseDirectory
               << "' does not contain a valid parameters file";
      res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      break;
    }

    TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(StringUtils::uint64(idSlice.copyString()));

    VPackSlice nameSlice = parameters.get("name");

    if (!nameSlice.isString()) {
      LOG(ERR) << "database directory '" << databaseDirectory
               << "' does not contain a valid parameters file";

      res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      break;
    }

    std::string const databaseName = nameSlice.copyString();

    // use defaults
    TRI_vocbase_defaults_t defaults;
    TRI_GetDatabaseDefaultsServer(server, &defaults);

    // .........................................................................
    // create app directories
    // .........................................................................

    V8DealerFeature* dealer = 
        ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
    auto appPath = dealer->appPath();

    res = CreateApplicationDirectory(databaseName.c_str(), appPath.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }

    // .........................................................................
    // open the database and scan collections in it
    // .........................................................................

    // try to open this database
    TRI_vocbase_t* vocbase = TRI_OpenVocBase(
        server, databaseDirectory.c_str(), id, databaseName.c_str(), &defaults,
        isUpgrade, server->_iterateMarkersOnOpen);

    if (vocbase == nullptr) {
      // grab last error
      res = TRI_errno();

      if (res == TRI_ERROR_NO_ERROR) {
        // but we must have an error...
        res = TRI_ERROR_INTERNAL;
      }

      LOG(ERR) << "could not process database directory '" << databaseDirectory
               << "' for database '" << name << "': " << TRI_errno_string(res);
      break;
    }

    // we found a valid database
    void const* TRI_UNUSED found = nullptr;

    try {
      auto pair = newLists->_databases.insert(
          std::make_pair(std::string(vocbase->_name), vocbase));
      if (!pair.second) {
        found = pair.first->second;
      }
    } catch (...) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      LOG(ERR) << "could not add database '" << name << "': out of memory";
      break;
    }

    TRI_ASSERT(found == nullptr);

    LOG(INFO) << "loaded database '" << vocbase->_name << "' from '"
              << vocbase->_path << "'";
  }

  server->_databasesLists = newLists;
  server->_databasesProtector.scan();
  delete oldLists;

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
/// @brief close all opened databases
////////////////////////////////////////////////////////////////////////////////

static int CloseDroppedDatabases(TRI_server_t* server) {
  MUTEX_LOCKER(mutexLocker, server->_databasesMutex);

  // No need for the thread protector here, because we have the mutex
  // Note however, that somebody could still read the lists concurrently,
  // therefore we first install a new value, call scan() on the protector
  // and only then really destroy the vocbases:

  // Build the new value:
  auto oldList = server->_databasesLists.load();
  decltype(oldList) newList = nullptr;
  try {
    newList = new DatabasesLists();
    newList->_databases = server->_databasesLists.load()->_databases;
    newList->_coordinatorDatabases =
        server->_databasesLists.load()->_coordinatorDatabases;
  } catch (...) {
    delete newList;
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // Replace the old by the new:
  server->_databasesLists = newList;
  server->_databasesProtector.scan();

  // Now it is safe to destroy the old dropped databases and the old lists
  // struct:
  for (TRI_vocbase_t* vocbase : oldList->_droppedDatabases) {
    TRI_ASSERT(vocbase != nullptr);

    if (vocbase->_type == TRI_VOCBASE_TYPE_NORMAL) {
      TRI_DestroyVocBase(vocbase);
      delete vocbase;
    } else if (vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR) {
      delete vocbase;
    } else {
      LOG(ERR) << "unknown database type " << vocbase->_type << " "
               << vocbase->_name << " - close doing nothing.";
    }
  }

  delete oldList;  // Note that this does not delete the TRI_vocbase_t pointers!

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the names of all databases in the ArangoDB 1.4 layout
////////////////////////////////////////////////////////////////////////////////

static int GetDatabases(TRI_server_t* server,
                        std::vector<std::string>& databases) {
  TRI_ASSERT(server != nullptr);

  std::vector<std::string> files = TRI_FilesDirectory(server->_databasePath);

  int res = TRI_ERROR_NO_ERROR;

  for (auto const& name : files) {
    TRI_ASSERT(!name.empty());

    if (!StringUtils::isPrefix(name, "database-")) {
      // found some other file
      continue;
    }

    // found a database name
    std::string const dname(arangodb::basics::FileUtils::buildFilename(
        server->_databasePath, name.c_str()));

    if (TRI_IsDirectory(dname.c_str())) {
      databases.push_back(name);
    }
  }

  // sort by id
  std::sort(databases.begin(), databases.end(), DatabaseIdStringComparator);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if there are "old" collections
////////////////////////////////////////////////////////////////////////////////

static bool HasOldCollections(TRI_server_t* server) {
  TRI_ASSERT(server != nullptr);

  bool found = false;
  std::vector<std::string> files = TRI_FilesDirectory(server->_basePath);

  for (auto const& name : files) {
    TRI_ASSERT(!name.empty());

    if (StringUtils::isPrefix(name, "collection-") &&
        !StringUtils::isSuffix(name, ".tmp")) {
      // found "collection-xxxx". we can ignore the rest
      found = true;
      break;
    }
  }

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a parameter.json file for a database
////////////////////////////////////////////////////////////////////////////////

static int SaveDatabaseParameters(TRI_voc_tick_t id, char const* name,
                                  bool deleted,
                                  TRI_vocbase_defaults_t const* defaults,
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
                                   TRI_vocbase_defaults_t const* defaults,
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

  res = SaveDatabaseParameters(tick, databaseName, false, defaults,
                               dirname.c_str());

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
/// @brief initialize the list of databases
////////////////////////////////////////////////////////////////////////////////

static int InitDatabases(TRI_server_t* server, bool checkVersion,
                         bool performUpgrade) {
  TRI_ASSERT(server != nullptr);

  std::vector<std::string> names;
  int res = GetDatabases(server, names);

  if (res == TRI_ERROR_NO_ERROR) {
    if (names.empty()) {
      if (!performUpgrade && HasOldCollections(server)) {
        LOG(ERR) << "no databases found. Please start the server with the "
                    "--database.auto-upgrade option";

        return TRI_ERROR_ARANGO_DATADIR_INVALID;
      }

      // no databases found, i.e. there is no system database!
      // create a database for the system database
      std::string dirname;
      res = CreateDatabaseDirectory(server, TRI_NewTickServer(),
                                    TRI_VOC_SYSTEM_DATABASE, &server->_defaults,
                                    dirname);

      server->_hasCreatedSystemDatabase = true;
    }
  }

  return res;
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
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

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
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

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
/// @brief database manager thread main loop
/// the purpose of this thread is to physically remove directories of databases
/// that have been dropped
////////////////////////////////////////////////////////////////////////////////

static void DatabaseManager(void* data) {
  auto server = static_cast<TRI_server_t*>(data);
  int cleanupCycles = 0;

  while (true) {
    bool shutdown = ServerShutdown.load(std::memory_order_relaxed);

    // check if we have to drop some database
    TRI_vocbase_t* database = nullptr;

    {
      auto unuser(server->_databasesProtector.use());
      auto theLists = server->_databasesLists.load();

      for (TRI_vocbase_t* vocbase : theLists->_droppedDatabases) {
        if (!TRI_CanRemoveVocBase(vocbase)) {
          continue;
        }

        // found a database to delete
        database = vocbase;
        break;
      }
    }

    if (database != nullptr) {
      // found a database to delete, now remove it from the struct

      {
        MUTEX_LOCKER(mutexLocker, server->_databasesMutex);

        // Build the new value:
        auto oldLists = server->_databasesLists.load();
        decltype(oldLists) newLists = nullptr;
        try {
          newLists = new DatabasesLists();
          newLists->_databases = oldLists->_databases;
          newLists->_coordinatorDatabases = oldLists->_coordinatorDatabases;
          for (TRI_vocbase_t* vocbase : oldLists->_droppedDatabases) {
            if (vocbase != database) {
              newLists->_droppedDatabases.insert(vocbase);
            }
          }
        } catch (...) {
          delete newLists;
          continue;  // try again later
        }

        // Replace the old by the new:
        server->_databasesLists = newLists;
        server->_databasesProtector.scan();
        delete oldLists;

        // From now on no other thread can possibly see the old TRI_vocbase_t*,
        // note that there is only one DatabaseManager thread, so it is
        // not possible that another thread has seen this very database
        // and tries to free it at the same time!
      }

      if (database->_type != TRI_VOCBASE_TYPE_COORDINATOR) {
        // regular database
        // ---------------------------

#ifdef ARANGODB_ENABLE_ROCKSDB
        // delete persistent indexes for this database
        RocksDBFeature::dropDatabase(database->_id);
#endif

        LOG(TRACE) << "physically removing database directory '"
                   << database->_path << "' of database '" << database->_name
                   << "'";

        std::string path;

        // remove apps directory for database
        V8DealerFeature* dealer = 
            ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
        auto appPath = dealer->appPath();

        if (database->_isOwnAppsDirectory && !appPath.empty()) {
          path = arangodb::basics::FileUtils::buildFilename(arangodb::basics::FileUtils::buildFilename(appPath, "_db"), database->_name);

          if (TRI_IsDirectory(path.c_str())) {
            LOG(TRACE) << "removing app directory '" << path
                       << "' of database '" << database->_name << "'";

            TRI_RemoveDirectory(path.c_str());
          }
        }

        // remember db path
        path = std::string(database->_path);

        TRI_DestroyVocBase(database);

        // remove directory
        TRI_RemoveDirectory(path.c_str());
      }

      delete database;

      // directly start next iteration
    } else {
      if (shutdown) {
        // done
        break;
      }

      usleep(DATABASE_MANAGER_INTERVAL);
      // The following is only necessary after a wait:
      auto queryRegistry = server->_queryRegistry.load();

      if (queryRegistry != nullptr) {
        queryRegistry->expireQueries();
      }

      // on a coordinator, we have no cleanup threads for the databases
      // so we have to do cursor cleanup here
      if (++cleanupCycles >= 10 &&
          arangodb::ServerState::instance()->isCoordinator()) {
        // note: if no coordinator then cleanupCycles will increase endlessly,
        // but it's only used for the following part
        cleanupCycles = 0;

        auto unuser(server->_databasesProtector.use());
        auto theLists = server->_databasesLists.load();

        for (auto& p : theLists->_coordinatorDatabases) {
          TRI_vocbase_t* vocbase = p.second;
          TRI_ASSERT(vocbase != nullptr);
          auto cursorRepository = vocbase->_cursorRepository;

          try {
            cursorRepository->garbageCollect(false);
          } catch (...) {
          }
        }
      }
    }

    // next iteration
  }

  CloseDroppedDatabases(server);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a server instance with configuration
////////////////////////////////////////////////////////////////////////////////

int TRI_InitServer(TRI_server_t* server,
                   arangodb::basics::ThreadPool* indexPool,
                   char const* basePath, TRI_vocbase_defaults_t const* defaults,
                   bool disableAppliers, bool disableCompactor,
                   bool iterateMarkersOnOpen) {
  TRI_ASSERT(server != nullptr);
  TRI_ASSERT(basePath != nullptr);

  server->_iterateMarkersOnOpen = iterateMarkersOnOpen;
  server->_hasCreatedSystemDatabase = false;
  server->_indexPool = indexPool;
  server->_databaseManagerStarted = false;

  // ...........................................................................
  // set up paths and filenames
  // ...........................................................................

  server->_basePath = TRI_DuplicateString(TRI_CORE_MEM_ZONE, basePath);

  if (server->_basePath == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  server->_databasePath = TRI_Concatenate2File(server->_basePath, "databases");

  if (server->_databasePath == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, server->_basePath);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  server->_lockFilename = TRI_Concatenate2File(server->_basePath, "LOCK");

  if (server->_lockFilename == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, server->_databasePath);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_basePath);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  server->_serverIdFilename = TRI_Concatenate2File(server->_basePath, "SERVER");

  if (server->_serverIdFilename == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, server->_lockFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_databasePath);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_basePath);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // ...........................................................................
  // server defaults
  // ...........................................................................

  memcpy(&server->_defaults, defaults, sizeof(TRI_vocbase_defaults_t));

  // ...........................................................................
  // database hashes and vectors
  // ...........................................................................

  server->_disableReplicationAppliers = disableAppliers;
  server->_disableCompactor = disableCompactor;

  server->_initialized = true;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize globals
////////////////////////////////////////////////////////////////////////////////

void TRI_InitServerGlobals() {
  ServerIdentifier = RandomGenerator::interval((uint16_t)UINT16_MAX);
  PageSize = (size_t)getpagesize();
  memset(&ServerId, 0, sizeof(TRI_server_id_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the global server id
////////////////////////////////////////////////////////////////////////////////

TRI_server_id_t TRI_GetIdServer() { return ServerId; }

////////////////////////////////////////////////////////////////////////////////
/// @brief start the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StartServer(TRI_server_t* server, bool checkVersion,
                    bool performUpgrade) {
  int res;

  if (!TRI_IsDirectory(server->_basePath)) {
    LOG(ERR) << "database path '" << server->_basePath
             << "' is not a directory";

    return TRI_ERROR_ARANGO_DATADIR_INVALID;
  }

  if (!TRI_IsWritable(server->_basePath)) {
    // database directory is not writable for the current user... bad luck
    LOG(ERR) << "database directory '" << server->_basePath
             << "' is not writable for current user";

    return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
  }

  // ...........................................................................
  // check that the database is not locked and lock it
  // ...........................................................................

  res = TRI_VerifyLockFile(server->_lockFilename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "database is locked, please check the lock file '"
             << server->_lockFilename << "'";

    return TRI_ERROR_ARANGO_DATADIR_LOCKED;
  }

  if (TRI_ExistsFile(server->_lockFilename)) {
    TRI_UnlinkFile(server->_lockFilename);
  }

  res = TRI_CreateLockFile(server->_lockFilename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR)
        << "cannot lock the database directory, please check the lock file '"
        << server->_lockFilename << "': " << TRI_errno_string(res);

    return TRI_ERROR_ARANGO_DATADIR_UNLOCKABLE;
  }

  // ...........................................................................
  // read the server id
  // ...........................................................................

  res = DetermineServerId(server, checkVersion);

  if (res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
    return res;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "reading/creating server file failed: "
             << TRI_errno_string(res);

    return res;
  }

  // ...........................................................................
  // verify existence of "databases" subdirectory
  // ...........................................................................

  if (!TRI_IsDirectory(server->_databasePath)) {
    long systemError;
    std::string errorMessage;
    res = TRI_CreateDirectory(server->_databasePath, systemError, errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to create database directory '"
               << server->_databasePath << "': " << errorMessage;

      return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
    }

    server->_iterateMarkersOnOpen = false;
  }

  if (!TRI_IsWritable(server->_databasePath)) {
    LOG(ERR) << "database directory '" << server->_databasePath
             << "' is not writable";

    return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
  }

  // ...........................................................................
  // perform an eventual migration of the databases.
  // ...........................................................................

  res = InitDatabases(server, checkVersion, performUpgrade);

  if (res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
    return res;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "unable to initialize databases: " << TRI_errno_string(res);
    return res;
  }

  // ...........................................................................
  // create shared application directories
  // ...........................................................................

  V8DealerFeature* dealer = 
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
  auto appPath = dealer->appPath();

  if (!appPath.empty() && !TRI_IsDirectory(appPath.c_str())) {
    long systemError;
    std::string errorMessage;
    int res = TRI_CreateRecursiveDirectory(appPath.c_str(), systemError,
                                            errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG(INFO) << "created --javascript.app-path directory '" << appPath
                << "'.";
    } else {
      LOG(ERR) << "unable to create --javascript.app-path directory '"
               << appPath << "': " << errorMessage;
      return res;
    }
  }

  // create subdirectories if not yet present
  res = CreateBaseApplicationDirectory(appPath.c_str(), "_db");

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "unable to initialize databases: " << TRI_errno_string(res);
    return res;
  }

  // ...........................................................................
  // open and scan all databases
  // ...........................................................................

  // scan all databases
  res = OpenDatabases(server, performUpgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "could not iterate over all databases: "
             << TRI_errno_string(res);

    return res;
  }

  // start dbm thread
  TRI_InitThread(&server->_databaseManager);
  TRI_StartThread(&server->_databaseManager, nullptr, "Databases",
                  DatabaseManager, server);
  server->_databaseManagerStarted = true;

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

    // initialize the authentication data for the database
    TRI_ReloadAuthInfo(vocbase);

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
/// @brief stop the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StopServer(TRI_server_t* server) {
  // set shutdown flag
  ServerShutdown.store(true);

  // stop dbm thread
  int res = TRI_ERROR_NO_ERROR;

  if (server->_databaseManagerStarted) {
    TRI_JoinThread(&server->_databaseManager);
    server->_databaseManagerStarted = false;
  }

  CloseDatabases(server);

  TRI_DestroyLockFile(server->_lockFilename);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication appliers
////////////////////////////////////////////////////////////////////////////////

void TRI_StopReplicationAppliersServer(TRI_server_t* server) {
  MUTEX_LOCKER(mutexLocker,
               server->_databasesMutex);  // Only one should do this at a time
  // No need for the thread protector here, because we have the mutex

  for (auto& p : server->_databasesLists.load()->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);
    if (vocbase->_replicationApplier != nullptr) {
      vocbase->_replicationApplier->stop(false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateCoordinatorDatabaseServer(TRI_server_t* server,
                                        TRI_voc_tick_t tick, char const* name,
                                        TRI_vocbase_defaults_t const* defaults,
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
      server, TRI_VOCBASE_TYPE_COORDINATOR, "none", tick, name, defaults);

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
                             TRI_vocbase_defaults_t const* defaults,
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

        // name not yet in use
        defaults->toVelocyPack(builder);
      } catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

    std::string dirname;
    res = CreateDatabaseDirectory(server, databaseId, name, defaults, dirname);

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

    vocbase = TRI_OpenVocBase(server, path.c_str(), databaseId, name, defaults,
                              false, false);

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

    CreateApplicationDirectory(vocbase->_name, appPath.c_str());

    if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
      TRI_ReloadAuthInfo(vocbase);
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
/// @brief activates or deactivates deadlock detection in all existing
/// databases
////////////////////////////////////////////////////////////////////////////////

void TRI_EnableDeadlockDetectionDatabasesServer(TRI_server_t* server) {
  auto unuser(server->_databasesProtector.use());
  auto theLists = server->_databasesLists.load();

  for (auto& p : theLists->_databases) {
    TRI_vocbase_t* vocbase = p.second;
    TRI_ASSERT(vocbase != nullptr);

    vocbase->_deadlockDetector.enabled(true);
  }
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

      if (includeSystem || !TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE)) {
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

    res = SaveDatabaseParameters(vocbase->_id, vocbase->_name, true,
                                 &vocbase->_settings, vocbase->_path);
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
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      TRI_ASSERT(vocbase->_name != nullptr);

      if (!CanUseDatabase(vocbase, username)) {
        // user cannot see database
        continue;
      }

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
/// @brief copies the defaults into the target
////////////////////////////////////////////////////////////////////////////////

void TRI_GetDatabaseDefaultsServer(TRI_server_t* server,
                                   TRI_vocbase_defaults_t* target) {
  // copy defaults into target
  memcpy(target, &server->_defaults, sizeof(TRI_vocbase_defaults_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new tick
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NewTickServer() { return ++CurrentTick; }

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
/// @brief msyncs a memory block between begin (incl) and end (excl)
////////////////////////////////////////////////////////////////////////////////

bool TRI_MSync(int fd, char const* begin, char const* end) {
  uintptr_t p = (intptr_t)begin;
  uintptr_t q = (intptr_t)end;
  uintptr_t g = (intptr_t)PageSize;

  char* b = (char*)((p / g) * g);
  char* e = (char*)(((q + g - 1) / g) * g);

  int res = TRI_FlushMMFile(fd, b, e - b, MS_SYNC);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return false;
  }

  return true;
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
      _databaseManagerStarted(false),
      _indexPool(nullptr),
      _queryRegistry(nullptr),
      _basePath(nullptr),
      _databasePath(nullptr),
      _lockFilename(nullptr),
      _serverIdFilename(nullptr),
      _disableReplicationAppliers(false),
      _disableCompactor(false),
      _iterateMarkersOnOpen(false),
      _hasCreatedSystemDatabase(false),
      _initialized(false) {}

TRI_server_t::~TRI_server_t() {
  if (_initialized) {
    CloseDatabases(this);

    auto p = _databasesLists.load();
    delete p;

    TRI_Free(TRI_CORE_MEM_ZONE, _serverIdFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, _lockFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, _databasePath);
    TRI_Free(TRI_CORE_MEM_ZONE, _basePath);
  }
}
