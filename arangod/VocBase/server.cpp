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

#include "server.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <regex.h>

#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/conversions.h"
#include "Basics/Exceptions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "Basics/locks.h"
#include "Basics/logging.h"
#include "Basics/memory-map.h"
#include "Basics/MutexLocker.h"
#include "Basics/random.h"
#include "Basics/SpinLock.h"
#include "Basics/SpinLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Utils/CursorRepository.h"
#include "VocBase/auth.h"
#include "VocBase/replication-applier.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief page size
////////////////////////////////////////////////////////////////////////////////

size_t PageSize;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief mask value for significant bits of server id
////////////////////////////////////////////////////////////////////////////////

#define SERVER_ID_MASK 0x0000FFFFFFFFFFFFULL

////////////////////////////////////////////////////////////////////////////////
/// @brief interval for database manager activity
////////////////////////////////////////////////////////////////////////////////

#define DATABASE_MANAGER_INTERVAL (500 * 1000)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for serializing the creation of database
////////////////////////////////////////////////////////////////////////////////

static triagens::basics::Mutex DatabaseCreateLock;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                               server id functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a new server id
///
/// TODO: generate a real UUID instead of the 2 random values
////////////////////////////////////////////////////////////////////////////////

static int GenerateServerId (void) {
  uint64_t randomValue = 0ULL; // init for our friend Valgrind
  uint32_t value1, value2;

  // save two uint32_t values
  value1 = TRI_UInt32Random();
  value2 = TRI_UInt32Random();

  // use the lower 6 bytes only
  randomValue = (((uint64_t) value1) << 32) | ((uint64_t) value2);
  randomValue &= SERVER_ID_MASK;

  ServerId = (TRI_server_id_t) randomValue;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads server id from file
////////////////////////////////////////////////////////////////////////////////

static int ReadServerId (char const* filename) {
  TRI_json_t* idString;
  TRI_server_id_t foundId;

  TRI_ASSERT(filename != nullptr);

  if (! TRI_ExistsFile(filename)) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  TRI_json_t* json = TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, filename, nullptr);

  if (! TRI_IsObjectJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return TRI_ERROR_INTERNAL;
  }

  idString = TRI_LookupObjectJson(json, "serverId");

  if (! TRI_IsStringJson(idString)) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    return TRI_ERROR_INTERNAL;
  }

  foundId = TRI_UInt64String2(idString->_value._string.data,
                              idString->_value._string.length - 1);

  LOG_TRACE("using existing server id: %llu", (unsigned long long) foundId);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (foundId == 0) {
    return TRI_ERROR_INTERNAL;
  }

  ServerId = foundId;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes server id to file
////////////////////////////////////////////////////////////////////////////////

static int WriteServerId (char const* filename) {
  char* idString;
  char buffer[32];
  size_t len;
  time_t tt;
  struct tm tb;

  TRI_ASSERT(filename != nullptr);

  // create a json object
  TRI_json_t* json = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE);

  if (json == nullptr) {
    // out of memory
    LOG_ERROR("cannot save server id in file '%s': out of memory", filename);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_ASSERT(ServerId != 0);

  idString = TRI_StringUInt64((uint64_t) ServerId);
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "serverId", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, idString, strlen(idString)));
  TRI_FreeString(TRI_CORE_MEM_ZONE, idString);

  tt = time(0);
  TRI_gmtime(tt, &tb);
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "createdTime", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, buffer, len));

  // save json info to file
  LOG_DEBUG("Writing server id to file '%s'", filename);
  bool ok = TRI_SaveJson(filename, json, true);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (! ok) {
    LOG_ERROR("could not save server id in file '%s': %s", filename, TRI_last_error());

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read / create the server id on startup
////////////////////////////////////////////////////////////////////////////////

static int DetermineServerId (TRI_server_t* server, bool checkVersion) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                database functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a user can see a database
/// note: "seeing" here does not necessarily mean the user can access the db.
/// it only means there is a user account (with whatever password) present
/// in the database
////////////////////////////////////////////////////////////////////////////////

static bool CanUseDatabase (TRI_vocbase_t* vocbase,
                            char const* username) {
  if (! vocbase->_settings.requireAuthentication) {
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

static uint64_t GetNumericFilenamePart (const char* filename) {
  char const* pos = strrchr(filename, '-');

  if (pos == nullptr) {
    return 0;
  }

  return TRI_UInt64String(pos + 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort database filenames on startup
////////////////////////////////////////////////////////////////////////////////

static int DatabaseIdComparator (const void* lhs, const void* rhs) {
  const char* l = *((char**) lhs);
  const char* r = *((char**) rhs);

  const uint64_t numLeft  = GetNumericFilenamePart(l);
  const uint64_t numRight = GetNumericFilenamePart(r);

  if (numLeft != numRight) {
    return numLeft < numRight ? -1 : 1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create base app directory
////////////////////////////////////////////////////////////////////////////////

static int CreateBaseApplicationDirectory (char const* basePath,
                                           char const* type) {
  if (basePath == nullptr || strlen(basePath) == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;
  char* path = TRI_Concatenate2File(basePath, type);

  if (path != nullptr) {
    if (! TRI_IsDirectory(path)) {
      std::string errorMessage;
      long systemError;
      res = TRI_CreateDirectory(path, systemError, errorMessage);

      if (res == TRI_ERROR_NO_ERROR) {
        LOG_INFO("created base application directory '%s'",
                 path);
      }
      else {
        if ((res != TRI_ERROR_FILE_EXISTS) || (! TRI_IsDirectory(path))) {
          LOG_ERROR("unable to create base application directory %s",
                    errorMessage.c_str());
        }
        else {
          LOG_INFO("otherone created base application directory '%s'",
                   path);
          res = TRI_ERROR_NO_ERROR;
        }
      }
    }

    TRI_Free(TRI_CORE_MEM_ZONE, path);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create app subdirectory for a database
////////////////////////////////////////////////////////////////////////////////

static int CreateApplicationDirectory (char const* name,
                                       char const* basePath) {
  if (basePath == nullptr || strlen(basePath) == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;
  char* path = TRI_Concatenate3File(basePath, "_db", name);

  if (path != nullptr) {
    if (! TRI_IsDirectory(path)) {
      long systemError;
      std::string errorMessage;
      res = TRI_CreateDirectory(path, systemError, errorMessage);

      if (res == TRI_ERROR_NO_ERROR) {
        if (triagens::wal::LogfileManager::instance()->isInRecovery()) {
          LOG_TRACE("created application directory '%s' for database '%s'",
                    path,
                    name);
        }
        else {
          LOG_INFO("created application directory '%s' for database '%s'",
                   path,
                   name);
        }
      }
      else if (res == TRI_ERROR_FILE_EXISTS) {
        LOG_INFO("unable to create application directory '%s' for database '%s': %s",
                 path,
                 name,
                 errorMessage.c_str());
        res = TRI_ERROR_NO_ERROR;
      }
      else {
        LOG_ERROR("unable to create application directory '%s' for database '%s': %s",
                  path,
                  name,
                  errorMessage.c_str());
      }
    }

    TRI_Free(TRI_CORE_MEM_ZONE, path);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all databases in the databases directory and open them
////////////////////////////////////////////////////////////////////////////////

static int OpenDatabases (TRI_server_t* server,
                          regex_t* regex,  
                          bool isUpgrade) {
  regmatch_t matches[2];

  if (server->_iterateMarkersOnOpen && ! server->_hasCreatedSystemDatabase) {
    LOG_WARNING("no shutdown info found. scanning datafiles for last tick...");
  }

  TRI_vector_string_t files;
  files = TRI_FilesDirectory(server->_databasePath);

  int res = TRI_ERROR_NO_ERROR;
  size_t n = files._length;

  // open databases in defined order
  if (n > 1) {
    qsort(files._buffer, n, sizeof(char**), &DatabaseIdComparator);
  }

  MUTEX_LOCKER(server->_databasesMutex);
  auto oldLists = server->_databasesLists.load();
  auto newLists = new DatabasesLists(*oldLists);
  // No try catch here, if we crash here because out of memory...

  for (size_t i = 0;  i < n;  ++i) {
    TRI_vocbase_t* vocbase;
    TRI_json_t* json;
    TRI_json_t const* deletedJson;
    TRI_json_t const* nameJson;
    TRI_json_t const* idJson;
    TRI_voc_tick_t id;
    TRI_vocbase_defaults_t defaults;
    char* parametersFile;
    char* databaseName;
    char const* name = files._buffer[i];
    TRI_ASSERT(name != nullptr);

    // .........................................................................
    // construct and validate path
    // .........................................................................

    char* databaseDirectory = TRI_Concatenate2File(server->_databasePath, name);

    if (databaseDirectory == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    if (! TRI_IsDirectory(databaseDirectory)) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      continue;
    }
   
    if (regexec(regex, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
      // name does not match the pattern, ignore this directory

      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      continue;
    }

    // we have a directory...

    if (! TRI_IsWritable(databaseDirectory)) {
      // the database directory we found is not writable for the current user
      // this can cause serious trouble so we will abort the server start if we
      // encounter this situation
      LOG_ERROR("database directory '%s' is not writable for current user",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      res = TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
      break;
    }

    // we have a writable directory...
    
    char* tmpfile = TRI_Concatenate2File(databaseDirectory, ".tmp");

    if (TRI_ExistsFile(tmpfile)) {
      // still a temporary... must ignore
      LOG_TRACE("ignoring temporary directory '%s'", tmpfile);
      TRI_FreeString(TRI_CORE_MEM_ZONE, tmpfile);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      continue;
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpfile);

    // a valid database directory

    // .........................................................................
    // read parameter.json file
    // .........................................................................

    // now read data from parameter.json file
    parametersFile = TRI_Concatenate2File(databaseDirectory, TRI_VOC_PARAMETER_FILE);

    if (parametersFile == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    if (! TRI_ExistsFile(parametersFile)) {
      // no parameter.json file
      LOG_ERROR("database directory '%s' does not contain parameters file or parameters file cannot be read",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, parametersFile);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      // abort
      res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      break;
    }

    LOG_DEBUG("reading database parameters from file '%s'",
              parametersFile);

    json = TRI_JsonFile(TRI_CORE_MEM_ZONE, parametersFile, nullptr);

    if (json == nullptr) {
      LOG_ERROR("database directory '%s' does not contain a valid parameters file",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, parametersFile);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      // abort
      res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      break;
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, parametersFile);

    deletedJson = TRI_LookupObjectJson(json, "deleted");

    if (TRI_IsBooleanJson(deletedJson)) {
      if (deletedJson->_value._boolean) {
        // database is deleted, skip it!
        LOG_INFO("found dropped database in directory '%s'",
                 databaseDirectory);

        LOG_INFO("removing superfluous database directory '%s'",
                 databaseDirectory);

        TRI_RemoveDirectory(databaseDirectory);

        TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        continue;
      }
    }

    idJson = TRI_LookupObjectJson(json, "id");

    if (! TRI_IsStringJson(idJson)) {
      LOG_ERROR("database directory '%s' does not contain a valid parameters file",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      break;
    }

    id = (TRI_voc_tick_t) TRI_UInt64String(idJson->_value._string.data);

    nameJson = TRI_LookupObjectJson(json, "name");

    if (! TRI_IsStringJson(nameJson)) {
      LOG_ERROR("database directory '%s' does not contain a valid parameters file",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      res = TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE;
      break;
    }

    databaseName = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE,
                                         nameJson->_value._string.data,
                                         nameJson->_value._string.length - 1);

    // .........................................................................
    // setup defaults
    // .........................................................................

    // use defaults and blend them with parameters found in file
    TRI_GetDatabaseDefaultsServer(server, &defaults);
    // TODO: decide which parameter from the command-line should win vs. parameter.json
    // TRI_FromJsonVocBaseDefaults(&defaults, TRI_LookupObjectJson(json, "properties"));

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    if (databaseName == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    // .........................................................................
    // create app directories
    // .........................................................................

    res = CreateApplicationDirectory(databaseName, server->_appPath);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseName);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      break;
    }

    // .........................................................................
    // open the database and scan collections in it
    // .........................................................................

    // try to open this database
    vocbase = TRI_OpenVocBase(server,
                              databaseDirectory,
                              id,
                              databaseName,
                              &defaults,
                              isUpgrade,
                              server->_iterateMarkersOnOpen);

    TRI_FreeString(TRI_CORE_MEM_ZONE, databaseName);

    if (vocbase == nullptr) {
      // grab last error
      res = TRI_errno();

      if (res == TRI_ERROR_NO_ERROR) {
        // but we must have an error...
        res = TRI_ERROR_INTERNAL;
      }

      LOG_ERROR("could not process database directory '%s' for database '%s': %s",
                databaseDirectory,
                name,
                TRI_errno_string(res));

      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      break;
    }

    // we found a valid database
    TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
    
    void const* TRI_UNUSED found = nullptr;

    try {
      auto pair = newLists->_databases.insert(
          std::make_pair(std::string(vocbase->_name), vocbase));
      if (! pair.second) {
        found = pair.first->second;
      }
    }
    catch (...) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      LOG_ERROR("could not add database '%s': out of memory",
                name);
      break;
    }

    TRI_ASSERT(found == nullptr);

    LOG_INFO("loaded database '%s' from '%s'",
             vocbase->_name,
             vocbase->_path);
  }

  server->_databasesLists = newLists;
  server->_databasesProtector.scan();
  delete oldLists;

  TRI_DestroyVectorString(&files);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close all opened databases
////////////////////////////////////////////////////////////////////////////////

static int CloseDatabases (TRI_server_t* server) {
  MUTEX_LOCKER(server->_databasesMutex);  // Only one should do this at a time
  // No need for the thread protector here, because we have the mutex
  // Note however, that somebody could still read the lists concurrently,
  // therefore we first install a new value, call scan() on the protector
  // and only then really destroy the vocbases:

  // Build the new value:
  auto oldList = server->_databasesLists.load();
  decltype(oldList) newList = nullptr;
  try {
    newList = new DatabasesLists();
    newList->_droppedDatabases = server->_databasesLists.load()->_droppedDatabases;
  }
  catch (...) {
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

  delete oldList; // Note that this does not delete the TRI_vocbase_t pointers!

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close all opened databases
////////////////////////////////////////////////////////////////////////////////

static int CloseDroppedDatabases (TRI_server_t* server) {
  MUTEX_LOCKER(server->_databasesMutex);

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
    newList->_coordinatorDatabases = server->_databasesLists.load()->_coordinatorDatabases;
  }
  catch (...) {
    delete newList;
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // Replace the old by the new:
  server->_databasesLists = newList;
  server->_databasesProtector.scan();

  // Now it is safe to destroy the old dropped databases and the old lists struct:
  for (TRI_vocbase_t* vocbase : oldList->_droppedDatabases) {
    TRI_ASSERT(vocbase != nullptr);

    if (vocbase->_type == TRI_VOCBASE_TYPE_NORMAL) {
      TRI_DestroyVocBase(vocbase);
      delete vocbase;
    }
    else if (vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR) {
      delete vocbase;
    }
    else {
      LOG_ERROR("unknown database type %d %s - close doing nothing.",
                vocbase->_type,
                vocbase->_name);
    }
  }

  delete oldList; // Note that this does not delete the TRI_vocbase_t pointers!

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the names of all databases in the ArangoDB 1.4 layout
////////////////////////////////////////////////////////////////////////////////

static int GetDatabases (TRI_server_t* server,
                         TRI_vector_string_t* databases) {
  regmatch_t matches[2];

  TRI_ASSERT(server != nullptr);

  regex_t re;
  int res = regcomp(&re, "^database-([0-9][0-9]*)$", REG_EXTENDED);

  if (res != 0) {
    LOG_ERROR("unable to compile regular expression");

    return TRI_ERROR_INTERNAL;
  }

  TRI_vector_string_t files;
  files = TRI_FilesDirectory(server->_databasePath);

  res = TRI_ERROR_NO_ERROR;
  size_t const n = files._length;

  for (size_t i = 0;  i < n;  ++i) {
    char const* name = files._buffer[i];
    TRI_ASSERT(name != nullptr);

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
      // found some other file
      continue;
    }

    // found a database name
    char* dname = TRI_Concatenate2File(server->_databasePath, name);

    if (dname == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    if (TRI_IsDirectory(dname)) {
      TRI_PushBackVectorString(databases, TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, name));
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  // sort by id
  qsort(databases->_buffer, databases->_length, sizeof(char*), &DatabaseIdComparator);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move the VERSION file from the main data directory into the _system
/// database subdirectory
////////////////////////////////////////////////////////////////////////////////

static int MoveVersionFile (TRI_server_t* server,
                            char const* systemName) {
  char* oldName = TRI_Concatenate2File(server->_basePath, "VERSION");

  if (oldName == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  char* targetName = TRI_Concatenate3File(server->_databasePath, systemName, "VERSION");

  if (targetName == nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res = TRI_ERROR_NO_ERROR;
  if (TRI_ExistsFile(oldName)) {
    res = TRI_RenameFile(oldName, targetName);
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
  TRI_FreeString(TRI_CORE_MEM_ZONE, targetName);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if there are "old" collections
////////////////////////////////////////////////////////////////////////////////

static bool HasOldCollections (TRI_server_t* server) {
  regex_t re;
  regmatch_t matches[2];
  TRI_vector_string_t files;
  bool found;
  size_t i, n;

  TRI_ASSERT(server != nullptr);

  if (regcomp(&re, "^collection-([0-9][0-9]*)$", REG_EXTENDED) != 0) {
    LOG_ERROR("unable to compile regular expression");

    return false;
  }

  found = false;
  files = TRI_FilesDirectory(server->_basePath);
  n = files._length;

  for (i = 0;  i < n;  ++i) {
    char const* name = files._buffer[i];
    TRI_ASSERT(name != nullptr);

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      // found "collection-xxxx". we can ignore the rest
      found = true;
      break;
    }
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move collections from the main data directory into the _system
/// database subdirectory
////////////////////////////////////////////////////////////////////////////////

static int MoveOldCollections (TRI_server_t* server,
                               char const* systemName) {
  regex_t re;
  regmatch_t matches[2];
  TRI_vector_string_t files;
  int res;
  size_t i, n;

  TRI_ASSERT(server != nullptr);
  TRI_ASSERT(systemName != nullptr);

  // first move the VERSION file
  MoveVersionFile(server, systemName);

  res = regcomp(&re, "^collection-([0-9][0-9]*)$", REG_EXTENDED);

  if (res != 0) {
    LOG_ERROR("unable to compile regular expression");

    return TRI_ERROR_INTERNAL;
  }

  res = TRI_ERROR_NO_ERROR;
  files = TRI_FilesDirectory(server->_basePath);
  n = files._length;

  for (i = 0;  i < n;  ++i) {
    char const* name;
    char* oldName;
    char* targetName;

    name = files._buffer[i];
    TRI_ASSERT(name != nullptr);

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) != 0) {
      // found something else than "collection-xxxx". we can ignore these files/directories
      continue;
    }

    oldName = TRI_Concatenate2File(server->_basePath, name);

    if (oldName == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    if (! TRI_IsDirectory(oldName)) {
      // not a directory
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
      continue;
    }

    // move into system database directory

    targetName = TRI_Concatenate3File(server->_databasePath, systemName, name);

    if (targetName == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    LOG_INFO("moving standalone collection directory from '%s' to system database directory '%s'",
             oldName,
             targetName);

    // rename directory
    res = TRI_RenameFile(oldName, targetName);

    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
    TRI_FreeString(TRI_CORE_MEM_ZONE, targetName);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("moving collection directory failed: %s",
                TRI_errno_string(res));
      break;
    }
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a parameter.json file for a database
////////////////////////////////////////////////////////////////////////////////

static int SaveDatabaseParameters (TRI_voc_tick_t id,
                                   char const* name,
                                   bool deleted,
                                   TRI_vocbase_defaults_t const* defaults,
                                   char const* directory) {
  // TRI_json_t* properties;

  TRI_ASSERT(id > 0);
  TRI_ASSERT(name != nullptr);
  TRI_ASSERT(directory != nullptr);

  char* file = TRI_Concatenate2File(directory, TRI_VOC_PARAMETER_FILE);

  if (file == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  char* tickString = TRI_StringUInt64((uint64_t) id);

  if (tickString == nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_json_t* json = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE);

  if (json == nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tickString);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // TODO
  /*
  properties = TRI_JsonVocBaseDefaults(TRI_CORE_MEM_ZONE, defaults);

  if (properties == nullptr) {
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tickString);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);

    return TRI_ERROR_OUT_OF_MEMORY;
  }
  */

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "id", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, tickString, strlen(tickString)));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "name", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, name, strlen(name)));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "deleted", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, deleted));
  // TODO: save properties later when it is clear what they will be used
  // TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "properties", properties);

  TRI_FreeString(TRI_CORE_MEM_ZONE, tickString);

  if (! TRI_SaveJson(file, json, true)) {
    LOG_ERROR("cannot save database information in file '%s'", file);

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);

    return TRI_ERROR_INTERNAL;
  }

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
  TRI_FreeString(TRI_CORE_MEM_ZONE, file);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database directory and return its name
////////////////////////////////////////////////////////////////////////////////

static int CreateDatabaseDirectory (TRI_server_t* server,
                                    TRI_voc_tick_t tick,
                                    char const* databaseName,
                                    TRI_vocbase_defaults_t const* defaults,
                                    char** name) {
  char* tickString;
  char* dname;
  char* file;
  int res;

  TRI_ASSERT(server != nullptr);
  TRI_ASSERT(databaseName != nullptr);

  tickString = TRI_StringUInt64(tick);

  if (tickString == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  dname = TRI_Concatenate2String("database-", tickString);
  TRI_FreeString(TRI_CORE_MEM_ZONE, tickString);

  if (dname == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  file = TRI_Concatenate2File(server->_databasePath, dname);

  if (file == nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

    
  // use a temporary directory first. otherwise, if creation fails, the server
  // might be left with an empty database directory at restart, and abort.
  char* tmpname = TRI_Concatenate2String(file, ".tmp");

  if (TRI_IsDirectory(tmpname)) {
    TRI_RemoveDirectory(tmpname);
  }

  std::string errorMessage;
  long systemError;
    
  res = TRI_CreateDirectory(tmpname, systemError, errorMessage);

  if (res != TRI_ERROR_NO_ERROR){
    if (res != TRI_ERROR_FILE_EXISTS) {
      LOG_ERROR("failed to create database directory: %s", errorMessage.c_str());
    }
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    return res;
  }
  
  TRI_IF_FAILURE("CreateDatabase::tempDirectory") {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    return TRI_ERROR_DEBUG;
  }

  char* tmpfile = TRI_Concatenate2File(tmpname, ".tmp");
  res = TRI_WriteFile(tmpfile, "", 0);
  TRI_FreeString(TRI_CORE_MEM_ZONE, tmpfile);
  
  TRI_IF_FAILURE("CreateDatabase::tempFile") {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    return TRI_ERROR_DEBUG;
  }
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_RemoveDirectory(tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    return res;
  }
  
  // finally rename
  res = TRI_RenameFile(tmpname, file);
  
  TRI_IF_FAILURE("CreateDatabase::renameDirectory") {
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    return TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_RemoveDirectory(tmpname); // clean up
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    return res;
  }
    
  TRI_FreeString(TRI_CORE_MEM_ZONE, tmpname);

  // now everything is valid

  res = SaveDatabaseParameters(tick, databaseName, false, defaults, file);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    return res;
  }

  // finally remove the .tmp file
  {
    char* tmpfile = TRI_Concatenate2File(file, ".tmp");
    TRI_UnlinkFile(tmpfile);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tmpfile);
  }
    
  TRI_FreeString(TRI_CORE_MEM_ZONE, file);

  // takes ownership of the string
  *name = dname;
    
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move 1.4-alpha database directories around until they are matching
/// the final ArangoDB 1.4 filename layout
////////////////////////////////////////////////////////////////////////////////

static int Move14AlphaDatabases (TRI_server_t* server) {
  regex_t re;
  regmatch_t matches[2];
  TRI_vector_string_t files;
  int res;
  size_t i, n;

  TRI_ASSERT(server != nullptr);

  res = regcomp(&re, "^database-([0-9][0-9]*)$", REG_EXTENDED);

  if (res != 0) {
    LOG_ERROR("unable to compile regular expression");

    return TRI_ERROR_INTERNAL;
  }

  res = TRI_ERROR_NO_ERROR;
  files = TRI_FilesDirectory(server->_databasePath);
  n = files._length;

  for (i = 0;  i < n;  ++i) {
    char const* name;
    char* tickString;
    char* dname;
    char* targetName;
    char* oldName;
    TRI_voc_tick_t tick;

    name = files._buffer[i];
    TRI_ASSERT(name != nullptr);

    if (regexec(&re, name, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
      // found "database-xxxx". this is the desired format already
      continue;
    }

    // found some other format. we need to adjust the name

    oldName = TRI_Concatenate2File(server->_databasePath, name);

    if (oldName == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    if (! TRI_IsDirectory(oldName)) {
      // found a non-directory
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
      continue;
    }

    tick = TRI_NewTickServer();
    tickString = TRI_StringUInt64(tick);

    if (tickString == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    dname = TRI_Concatenate2String("database-", tickString);
    TRI_FreeString(TRI_CORE_MEM_ZONE, tickString);

    if (dname == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    targetName = TRI_Concatenate2File(server->_databasePath, dname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);

    if (targetName == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    res = SaveDatabaseParameters(tick, name, false, &server->_defaults, oldName);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, targetName);
      TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
      break;
    }

    LOG_INFO("renaming database directory from '%s' to '%s'",
             oldName,
             targetName);

    res = TRI_RenameFile(oldName, targetName);

    TRI_FreeString(TRI_CORE_MEM_ZONE, oldName);
    TRI_FreeString(TRI_CORE_MEM_ZONE, targetName);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("renaming database failed: %s",
                TRI_errno_string(res));
      break;
    }
  }

  TRI_DestroyVectorString(&files);
  regfree(&re);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the list of databases
////////////////////////////////////////////////////////////////////////////////

static int InitDatabases (TRI_server_t* server,
                          bool checkVersion,
                          bool performUpgrade) {

  TRI_ASSERT(server != nullptr);

  TRI_vector_string_t names;
  TRI_InitVectorString(&names, TRI_CORE_MEM_ZONE);

  int res = GetDatabases(server, &names);

  if (res == TRI_ERROR_NO_ERROR) {
    if (names._length == 0) {
      char* name;

      if (! performUpgrade && HasOldCollections(server)) {
        LOG_ERROR("no databases found. Please start the server with the --upgrade option");

        return TRI_ERROR_ARANGO_DATADIR_INVALID;
      }

      // no databases found, i.e. there is no system database!
      // create a database for the system database
      res = CreateDatabaseDirectory(server, TRI_NewTickServer(), TRI_VOC_SYSTEM_DATABASE, &server->_defaults, &name);

      if (res == TRI_ERROR_NO_ERROR) {
        if (TRI_PushBackVectorString(&names, name) != TRI_ERROR_NO_ERROR) {
          TRI_FreeString(TRI_CORE_MEM_ZONE, name);
          res = TRI_ERROR_OUT_OF_MEMORY;
        }
      }

      server->_hasCreatedSystemDatabase = true;
    }

    if (res == TRI_ERROR_NO_ERROR && performUpgrade) {
      char const* systemName;

      TRI_ASSERT(names._length > 0);

      systemName = names._buffer[0];

      // this performs a migration of the collections of the "only" pre-1.4
      // database into the system database and its own directory
      res = MoveOldCollections(server, systemName);

      if (res == TRI_ERROR_NO_ERROR) {
        // this renames database directories created with 1.4-alpha from the
        // database name to "database-xxx"
        res = Move14AlphaDatabases(server);
      }
    }
  }

  TRI_DestroyVectorString(&names);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a create-database marker into the log
////////////////////////////////////////////////////////////////////////////////

static int WriteCreateMarker (TRI_voc_tick_t id,
                              TRI_json_t const* json) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    triagens::wal::CreateDatabaseMarker marker(id, triagens::basics::JsonHelper::toString(json));
    triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      // throw an exception which is caught at the end of this function
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  }
  catch (triagens::basics::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("could not save create database marker in log: %s", TRI_errno_string(res));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a drop-database marker into the log
////////////////////////////////////////////////////////////////////////////////

static int WriteDropMarker (TRI_voc_tick_t id) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    triagens::wal::DropDatabaseMarker marker(id);
    triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      // throw an exception which is caught at the end of this function
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  }
  catch (triagens::basics::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("could not save drop database marker in log: %s", TRI_errno_string(res));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief database manager thread main loop
/// the purpose of this thread is to physically remove directories of databases
/// that have been dropped
////////////////////////////////////////////////////////////////////////////////

static void DatabaseManager (void* data) {
  TRI_server_t* server = static_cast<TRI_server_t*>(data);
  int cleanupCycles = 0;

  while (true) {
    bool shutdown = ServerShutdown.load(std::memory_order_relaxed);

    // check if we have to drop some database
    TRI_vocbase_t* database = nullptr;

    {
      auto unuser(server->_databasesProtector.use());
      auto theLists = server->_databasesLists.load();
      
      for (TRI_vocbase_t* vocbase : theLists->_droppedDatabases) {
        if (! TRI_CanRemoveVocBase(vocbase)) {
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
        MUTEX_LOCKER(server->_databasesMutex);

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
        }
        catch (...) {
          delete newLists;
          continue;   // try again later
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

        // remember the database path
        char* path;

        LOG_TRACE("physically removing database directory '%s' of database '%s'",
                  database->_path,
                  database->_name);

        // remove apps directory for database
        if (database->_isOwnAppsDirectory && strlen(server->_appPath) > 0) {
          path = TRI_Concatenate3File(server->_appPath, "_db", database->_name);

          if (path != nullptr) {
            if (TRI_IsDirectory(path)) {
              LOG_TRACE("removing app directory '%s' of database '%s'",
                        path,
                        database->_name);

              TRI_RemoveDirectory(path);
            }

            TRI_Free(TRI_CORE_MEM_ZONE, path);
          }
        }

        path = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, database->_path);

        TRI_DestroyVocBase(database);

        // remove directory
        if (path != nullptr) {
          TRI_RemoveDirectory(path);
          TRI_FreeString(TRI_CORE_MEM_ZONE, path);
        }
      }
        
      delete database;

      // directly start next iteration
    }
    else {
      if (shutdown) {
        // done
        break;
      }

      usleep(DATABASE_MANAGER_INTERVAL);
      // The following is only necessary after a wait:
      auto queryRegistry = static_cast<triagens::aql::QueryRegistry*>
                                      (server->_queryRegistry);
      if (queryRegistry != nullptr) {
        queryRegistry->expireQueries();
      }
  
      // on a coordinator, we have no cleanup threads for the databases
      // so we have to do cursor cleanup here 
      if (triagens::arango::ServerState::instance()->isCoordinator() && 
          ++cleanupCycles == 10) { 
        cleanupCycles = 0;

        auto unuser(server->_databasesProtector.use());
        auto theLists = server->_databasesLists.load();
      
        for (auto& p : theLists->_coordinatorDatabases) {
          TRI_vocbase_t* vocbase = p.second;
          TRI_ASSERT(vocbase != nullptr);
          auto cursorRepository = static_cast<triagens::arango::CursorRepository*>(vocbase->_cursorRepository);

          try {
            cursorRepository->garbageCollect(false);
          }
          catch (...) {
          }
        }
      }
    }

    // next iteration
  }

  CloseDroppedDatabases(server);
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a server instance with configuration
////////////////////////////////////////////////////////////////////////////////

int TRI_InitServer (TRI_server_t* server,
                    triagens::rest::ApplicationEndpointServer* applicationEndpointServer,
                    triagens::basics::ThreadPool* indexPool,
                    char const* basePath,
                    char const* appPath,
                    TRI_vocbase_defaults_t const* defaults,
                    bool disableAppliers,
                    bool iterateMarkersOnOpen) {

  TRI_ASSERT(server != nullptr);
  TRI_ASSERT(basePath != nullptr);

  server->_iterateMarkersOnOpen = iterateMarkersOnOpen;
  server->_hasCreatedSystemDatabase = false;

  // c++ object, may be null in console mode
  server->_applicationEndpointServer = applicationEndpointServer;

  server->_indexPool                 = indexPool;

  // ...........................................................................
  // set up paths and filenames
  // ...........................................................................

  server->_basePath = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, basePath);

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

  server->_appPath = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, appPath);

  if (server->_appPath == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, server->_serverIdFilename);
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

  server->_initialized = true;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize globals
////////////////////////////////////////////////////////////////////////////////

void TRI_InitServerGlobals () {
  ServerIdentifier = TRI_UInt16Random();
  PageSize = (size_t) getpagesize();

  memset(&ServerId, 0, sizeof(TRI_server_id_t));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the global server id
////////////////////////////////////////////////////////////////////////////////

TRI_server_id_t TRI_GetIdServer () {
  return ServerId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StartServer (TRI_server_t* server,
                     bool checkVersion,
                     bool performUpgrade) {
  int res;

  if (! TRI_IsDirectory(server->_basePath)) {
    LOG_ERROR("database path '%s' is not a directory",
              server->_basePath);

    return TRI_ERROR_ARANGO_DATADIR_INVALID;
  }

  if (! TRI_IsWritable(server->_basePath)) {
    // database directory is not writable for the current user... bad luck
    LOG_ERROR("database directory '%s' is not writable for current user",
              server->_basePath);

    return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
  }

  // ...........................................................................
  // check that the database is not locked and lock it
  // ...........................................................................

  res = TRI_VerifyLockFile(server->_lockFilename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("database is locked, please check the lock file '%s'",
              server->_lockFilename);

    return TRI_ERROR_ARANGO_DATADIR_LOCKED;
  }

  if (TRI_ExistsFile(server->_lockFilename)) {
    TRI_UnlinkFile(server->_lockFilename);
  }

  res = TRI_CreateLockFile(server->_lockFilename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot lock the database directory, please check the lock file '%s': %s",
              server->_lockFilename,
              TRI_errno_string(res));

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
    LOG_ERROR("reading/creating server file failed: %s",
              TRI_errno_string(res));

    return res;
  }

  // ...........................................................................
  // verify existence of "databases" subdirectory
  // ...........................................................................

  if (! TRI_IsDirectory(server->_databasePath)) {
    long systemError;
    std::string errorMessage;
    res = TRI_CreateDirectory(server->_databasePath, systemError, errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to create database directory '%s': %s",
                server->_databasePath,
                errorMessage.c_str());

      return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
    }

    server->_iterateMarkersOnOpen = false;
  }

  if (! TRI_IsWritable(server->_databasePath)) {
    LOG_ERROR("database directory '%s' is not writable",
              server->_databasePath);

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
    LOG_ERROR("unable to initialize databases: %s",
              TRI_errno_string(res));
    return res;
  }

  // ...........................................................................
  // create shared application directories
  // ...........................................................................

  if (server->_appPath != nullptr &&
      strlen(server->_appPath) > 0 &&
      ! TRI_IsDirectory(server->_appPath)) {

    long systemError;
    std::string errorMessage;
    bool res = TRI_CreateRecursiveDirectory(server->_appPath, systemError, errorMessage);

    if (res) {
      LOG_INFO("created --javascript.app-path directory '%s'.",
               server->_appPath);
    }
    else {
      LOG_ERROR("unable to create --javascript.app-path directory '%s': %s",
                server->_appPath,
                errorMessage.c_str());
      return TRI_ERROR_SYS_ERROR;
    }
  }

  // create subdirectories if not yet present
  res = CreateBaseApplicationDirectory(server->_appPath, "_db");

  // system directory is in a read-only location
#if 0
  if (res == TRI_ERROR_NO_ERROR) {
    res = CreateBaseApplicationDirectory(server->_appPath, "system");
  }
#endif

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("unable to initialize databases: %s",
              TRI_errno_string(res));
    return res;
  }


  // ...........................................................................
  // open and scan all databases
  // ...........................................................................

  regex_t regex;
  res = regcomp(&regex, "^database-([0-9][0-9]*)$", REG_EXTENDED);

  if (res != 0) {
    LOG_ERROR("unable to compile regular expression");

    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  // scan all databases
  res = OpenDatabases(server, &regex, performUpgrade);
  
  regfree(&regex);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not iterate over all databases: %s",
              TRI_errno_string(res));

    return res;
  }

  // start dbm thread
  TRI_InitThread(&server->_databaseManager);
  TRI_StartThread(&server->_databaseManager, nullptr, "[databases]", DatabaseManager, server);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes all databases
////////////////////////////////////////////////////////////////////////////////

int TRI_InitDatabasesServer (TRI_server_t* server) {
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
        LOG_INFO("replication applier explicitly deactivated for database '%s'", vocbase->_name);
      }
      else {
        int res = vocbase->_replicationApplier->start(0, false);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_WARNING("unable to start replication applier for database '%s': %s",
                      vocbase->_name,
                      TRI_errno_string(res));
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the server
////////////////////////////////////////////////////////////////////////////////

int TRI_StopServer (TRI_server_t* server) {
  // set shutdown flag
  ServerShutdown.store(true);

  // stop dbm thread
  int res = TRI_JoinThread(&server->_databaseManager);

  CloseDatabases(server);

  TRI_DestroyLockFile(server->_lockFilename);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication appliers
////////////////////////////////////////////////////////////////////////////////

void TRI_StopReplicationAppliersServer (TRI_server_t* server) {
  MUTEX_LOCKER(server->_databasesMutex);  // Only one should do this at a time
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

int TRI_CreateCoordinatorDatabaseServer (TRI_server_t* server,
                                         TRI_voc_tick_t tick,
                                         char const* name,
                                         TRI_vocbase_defaults_t const* defaults,
                                         TRI_vocbase_t** database) {
  if (! TRI_IsAllowedNameVocBase(true, name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  MUTEX_LOCKER(DatabaseCreateLock);

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

  TRI_vocbase_t* vocbase = TRI_CreateInitialVocBase(server, TRI_VOCBASE_TYPE_COORDINATOR, "none", tick, name, defaults);

  if (vocbase == nullptr) {
    // grab last error
    int res = TRI_errno();

    if (res != TRI_ERROR_NO_ERROR) {
      // but we must have an error...
      res = TRI_ERROR_INTERNAL;
    }

    LOG_ERROR("could not create database '%s': %s",
              name,
              TRI_errno_string(res));

    return res;
  }

  TRI_ASSERT(vocbase != nullptr);

  vocbase->_replicationApplier = TRI_CreateReplicationApplier(server, vocbase);

  if (vocbase->_replicationApplier == nullptr) {
    delete vocbase;

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // TODO: create application directories??
//  CreateApplicationDirectory(vocbase->_name, server->_appPath);

  // increase reference counter
  TRI_UseVocBase(vocbase);
  vocbase->_state = (sig_atomic_t) TRI_VOCBASE_STATE_NORMAL;

  {
    MUTEX_LOCKER(server->_databasesMutex);
    auto oldLists = server->_databasesLists.load();
    decltype(oldLists) newLists = nullptr;
    try {
      newLists = new DatabasesLists(*oldLists);
      newLists->_coordinatorDatabases.emplace(std::string(vocbase->_name), vocbase);
    }
    catch (...) {
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

int TRI_CreateDatabaseServer (TRI_server_t* server,
                              TRI_voc_tick_t databaseId,
                              char const* name,
                              TRI_vocbase_defaults_t const* defaults,
                              TRI_vocbase_t** database,
                              bool writeMarker) {

  if (! TRI_IsAllowedNameVocBase(false, name)) {
    return TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;
  }

  TRI_vocbase_t* vocbase = nullptr;
  TRI_json_t* json = nullptr;
  int res;

  // the create lock makes sure no one else is creating a database while we're inside
  // this function
  MUTEX_LOCKER(DatabaseCreateLock);
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

    // name not yet in use
    json = TRI_JsonVocBaseDefaults(TRI_UNKNOWN_MEM_ZONE, defaults);

    if (json == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // create the database directory
    char* file;

    if (databaseId == 0) {
      databaseId = TRI_NewTickServer();
    }

    res = CreateDatabaseDirectory(server, databaseId, name, defaults, &file);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      return res;
    }

    char* path = TRI_Concatenate2File(server->_databasePath, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);

    if (triagens::wal::LogfileManager::instance()->isInRecovery()) {
      LOG_TRACE("creating database '%s', directory '%s'",
                name,
                path);
    }
    else {
      LOG_INFO("creating database '%s', directory '%s'",
              name,
              path);
    }

    vocbase = TRI_OpenVocBase(server, path, databaseId, name, defaults, false, false);
    TRI_FreeString(TRI_CORE_MEM_ZONE, path);

    if (vocbase == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      // grab last error
      res = TRI_errno();

      if (res != TRI_ERROR_NO_ERROR) {
        // but we must have an error...
        res = TRI_ERROR_INTERNAL;
      }

      LOG_ERROR("could not create database '%s': %s",
                name,
                TRI_errno_string(res));

      return res;
    }

    TRI_ASSERT(vocbase != nullptr);
    
    char* tickString = TRI_StringUInt64(databaseId);
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, tickString, strlen(tickString)));
    TRI_FreeString(TRI_CORE_MEM_ZONE, tickString);
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "name", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, name, strlen(name)));


    // create application directories
    CreateApplicationDirectory(vocbase->_name, server->_appPath);

    if (! triagens::wal::LogfileManager::instance()->isInRecovery()) {
      TRI_ReloadAuthInfo(vocbase);
      TRI_StartCompactorVocBase(vocbase);

      // start the replication applier
      if (vocbase->_replicationApplier->_configuration._autoStart) {
        if (server->_disableReplicationAppliers) {
          LOG_INFO("replication applier explicitly deactivated for database '%s'", name);
        }
        else {
          res = vocbase->_replicationApplier->start(0, false);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_WARNING("unable to start replication applier for database '%s': %s",
                        name,
                        TRI_errno_string(res));
          }
        }
      }

      // increase reference counter
      TRI_UseVocBase(vocbase);
    }

    {
      MUTEX_LOCKER(server->_databasesMutex);
      auto oldLists = server->_databasesLists.load();
      decltype(oldLists) newLists = nullptr;
      try {
        newLists = new DatabasesLists(*oldLists);
        newLists->_databases.insert(
              std::make_pair(std::string(vocbase->_name), vocbase));
      }
      catch (...) {
        LOG_ERROR("Out of memory for putting new database into list!");
        // This is bad, but at least we do not crash!
      }
      if (newLists != nullptr) {
        server->_databasesLists = newLists;
        server->_databasesProtector.scan();
        delete oldLists;
      }
    }

  } // release DatabaseCreateLock

  // write marker into log
  if (writeMarker) {
    res = WriteCreateMarker(vocbase->_id, json);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  *database = vocbase;

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the ids of all local coordinator databases
/// the caller is responsible for freeing the result
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_voc_tick_t> TRI_GetIdsCoordinatorDatabaseServer (TRI_server_t* server) {
  std::vector<TRI_voc_tick_t> v;
  {
    auto unuser(server->_databasesProtector.use());
    auto theLists = server->_databasesLists.load();

    for (auto& p : theLists->_coordinatorDatabases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);

      if (! TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE)) {
        v.emplace_back(vocbase->_id);
      }
    }
  }
  return v;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdCoordinatorDatabaseServer (TRI_server_t* server,
                                           TRI_voc_tick_t id,
                                           bool force) {
  int res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;

  MUTEX_LOCKER(server->_databasesMutex);
  auto oldLists = server->_databasesLists.load();
  decltype(oldLists) newLists = nullptr;
  TRI_vocbase_t* vocbase = nullptr;
  try {
    newLists = new DatabasesLists(*oldLists);

    for (auto it = newLists->_coordinatorDatabases.begin();
         it != newLists->_coordinatorDatabases.end(); it++) {
      vocbase = it->second;

      if (vocbase->_id == id &&
          (force || ! TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE))) {
        newLists->_droppedDatabases.emplace(vocbase);
        newLists->_coordinatorDatabases.erase(it);
        break;
      }
      vocbase = nullptr;
    }
  }
  catch (...) {
    delete newLists;
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  if (vocbase != nullptr) {
    server->_databasesLists = newLists;
    server->_databasesProtector.scan();
    delete oldLists;

    if (TRI_DropVocBase(vocbase)) {
      LOG_INFO("dropping coordinator database '%s'", vocbase->_name);
      res = TRI_ERROR_NO_ERROR;
    }
  }
  else {
    delete newLists;
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropDatabaseServer (TRI_server_t* server,
                            char const* name,
                            bool removeAppsDirectory,
                            bool writeMarker) {
  if (TRI_EqualString(name, TRI_VOC_SYSTEM_DATABASE)) {
    // prevent deletion of system database
    return TRI_ERROR_FORBIDDEN;
  }

  MUTEX_LOCKER(server->_databasesMutex);

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
    }
    else {
      vocbase = it->second;
      // mark as deleted
      TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);

      newLists->_databases.erase(it);
      newLists->_droppedDatabases.insert(vocbase);
    }
  }
  catch (...) {
    delete newLists;
    return TRI_ERROR_OUT_OF_MEMORY;
  }
    
  server->_databasesLists = newLists;
  server->_databasesProtector.scan();
  delete oldLists;

  vocbase->_isOwnAppsDirectory = removeAppsDirectory;

  // invalidate all entries for the database
  triagens::aql::QueryCache::instance()->invalidate(vocbase);

  int res = TRI_ERROR_NO_ERROR;

  if (TRI_DropVocBase(vocbase)) {
    if (triagens::wal::LogfileManager::instance()->isInRecovery()) {
      LOG_TRACE("dropping database '%s', directory '%s'",
                vocbase->_name,
                vocbase->_path);
    }
    else {
      LOG_INFO("dropping database '%s', directory '%s'",
               vocbase->_name,
               vocbase->_path);
    }

    res = SaveDatabaseParameters(vocbase->_id,
                                 vocbase->_name,
                                 true,
                                 &vocbase->_settings,
                                 vocbase->_path);
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

int TRI_DropByIdDatabaseServer (TRI_server_t* server,
                                TRI_voc_tick_t id,
                                bool removeAppsDirectory,
                                bool writeMarker) {
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
  
  return TRI_DropDatabaseServer(server, name.c_str(), removeAppsDirectory, writeMarker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a coordinator database by its id
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseByIdCoordinatorDatabaseServer (TRI_server_t* server,
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

TRI_vocbase_t* TRI_UseCoordinatorDatabaseServer (TRI_server_t* server,
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

TRI_vocbase_t* TRI_UseDatabaseServer (TRI_server_t* server,
                                      char const* name) {
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

TRI_vocbase_t* TRI_LookupDatabaseByNameServer (TRI_server_t* server,
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

TRI_vocbase_t* TRI_UseDatabaseByIdServer (TRI_server_t* server,
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

void TRI_ReleaseDatabaseServer (TRI_server_t* server,
                                TRI_vocbase_t* vocbase) {

  TRI_ReleaseVocBase(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all databases a user can see
////////////////////////////////////////////////////////////////////////////////

int TRI_GetUserDatabasesServer (TRI_server_t* server,
                                char const* username,
                                std::vector<std::string>& names) {

  int res = TRI_ERROR_NO_ERROR;

  {
    auto unuser(server->_databasesProtector.use());
    auto theLists = server->_databasesLists.load();

    for (auto& p : theLists->_databases) {
      TRI_vocbase_t* vocbase = p.second;
      TRI_ASSERT(vocbase != nullptr);
      TRI_ASSERT(vocbase->_name != nullptr);

      if (! CanUseDatabase(vocbase, username)) {
        // user cannot see database
        continue;
      }

      try {
        names.emplace_back(vocbase->_name);
      }
      catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  std::sort(names.begin(), names.end(), [](std::string const& l, std::string const& r) -> bool {
    return l < r;
  });

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all database names
////////////////////////////////////////////////////////////////////////////////

int TRI_GetDatabaseNamesServer (TRI_server_t* server,
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
      }
      catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  std::sort(names.begin(), names.end(), [](std::string const& l, std::string const& r) -> bool {
    return l < r;
  });

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the defaults into the target
////////////////////////////////////////////////////////////////////////////////

void TRI_GetDatabaseDefaultsServer (TRI_server_t* server,
                                    TRI_vocbase_defaults_t* target) {
  // copy defaults into target
  memcpy(target, &server->_defaults, sizeof(TRI_vocbase_defaults_t));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    tick functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new tick
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_NewTickServer () {
  uint64_t tick = ServerIdentifier;

  tick |= (++CurrentTick) << 16;

  return tick;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, with lock
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTickServer (TRI_voc_tick_t tick) {
  TRI_voc_tick_t t = tick >> 16;

  auto expected = CurrentTick.load(std::memory_order_relaxed);
 
  // only update global tick if less than the specified value...
  while (expected < t &&
         ! CurrentTick.compare_exchange_weak(expected, t, std::memory_order_release, std::memory_order_relaxed)) {
    expected = CurrentTick.load(std::memory_order_relaxed);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current tick counter
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_CurrentTickServer () {
  return (ServerIdentifier | (CurrentTick << 16));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   other functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief msyncs a memory block between begin (incl) and end (excl)
////////////////////////////////////////////////////////////////////////////////

bool TRI_MSync (int fd,
                char const* begin,
                char const* end) {
  uintptr_t p = (intptr_t) begin;
  uintptr_t q = (intptr_t) end;
  uintptr_t g = (intptr_t) PageSize;

  char* b = (char*)( (p / g) * g );
  char* e = (char*)( ((q + g - 1) / g) * g );

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

int TRI_ChangeOperationModeServer (TRI_vocbase_operationmode_e mode) {
  Mode = mode;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current operation server of the server
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_operationmode_e TRI_GetOperationModeServer () {
  return Mode;
}
// -----------------------------------------------------------------------------
// --SECTION--                                                      TRI_server_t
// -----------------------------------------------------------------------------

TRI_server_t::TRI_server_t ()
  : _databasesLists(new DatabasesLists()),
    _applicationEndpointServer(nullptr),
    _indexPool(nullptr),
    _queryRegistry(nullptr),
    _basePath(nullptr),
    _databasePath(nullptr),
    _lockFilename(nullptr),
    _serverIdFilename(nullptr),
    _appPath(nullptr),
    _disableReplicationAppliers(false),
    _iterateMarkersOnOpen(false),
    _hasCreatedSystemDatabase(false),
    _initialized(false) {

}

TRI_server_t::~TRI_server_t () {
  if (_initialized) {
    CloseDatabases(this);

    auto p = _databasesLists.load();
    delete p;

    TRI_Free(TRI_CORE_MEM_ZONE, _appPath);
    TRI_Free(TRI_CORE_MEM_ZONE, _serverIdFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, _lockFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, _databasePath);
    TRI_Free(TRI_CORE_MEM_ZONE, _basePath);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
