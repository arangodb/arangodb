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

#include "Aql/QueryRegistry.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/json.h"
#include "Basics/locks.h"
#include "Basics/logging.h"
#include "Basics/memory-map.h"
#include "Basics/random.h"
#include "Basics/tri-strings.h"
#include "Basics/JsonHelper.h"
#include "Ahuacatl/ahuacatl-statementlist.h"
#include "Utils/Exception.h"
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

class DatabaseReadLocker {
  public:
    DatabaseReadLocker (DatabaseReadLocker const&) = delete;
    DatabaseReadLocker& operator= (DatabaseReadLocker const&) = delete;

    DatabaseReadLocker (TRI_read_write_lock_t* lock)
      : _lock(lock) {
      TRI_ReadLockReadWriteLock(_lock);
    }

    ~DatabaseReadLocker () {
      TRI_ReadUnlockReadWriteLock(_lock);
    }

  private:

    TRI_read_write_lock_t* _lock;
};

class DatabaseWriteLocker {
  public:
    DatabaseWriteLocker (DatabaseWriteLocker const&) = delete;
    DatabaseWriteLocker& operator= (DatabaseWriteLocker const&) = delete;

    DatabaseWriteLocker (TRI_read_write_lock_t* lock)
      : _lock(lock) {
       while (! TRI_TryWriteLockReadWriteLock(lock)) {
         usleep(1000);
      }
    }

    ~DatabaseWriteLocker () {
      TRI_WriteUnlockReadWriteLock(_lock);
    }

  private:

    TRI_read_write_lock_t* _lock;
};

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

static uint64_t CurrentTick = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief tick lock
////////////////////////////////////////////////////////////////////////////////

static TRI_spin_t TickLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief the server's global id
////////////////////////////////////////////////////////////////////////////////

static TRI_server_id_t ServerId;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                           database hash functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the database name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementDatabaseName (TRI_associative_pointer_t* array,
                                         void const* element) {
  TRI_vocbase_t const* e = static_cast<TRI_vocbase_t const*>(element);

  return TRI_FnvHashString((char const*) e->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a database name and a database
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyDatabaseName (TRI_associative_pointer_t* array,
                                  void const* key,
                                  void const* element) {
  char const* k = static_cast<char const*>(key);
  TRI_vocbase_t const* e = static_cast<TRI_vocbase_t const*>(element);

  return TRI_EqualString(k, e->_name);
}

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

  if (! TRI_IsArrayJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return TRI_ERROR_INTERNAL;
  }

  idString = TRI_LookupArrayJson(json, "serverId");

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
  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json == nullptr) {
    // out of memory
    LOG_ERROR("cannot save server id in file '%s': out of memory", filename);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_ASSERT(ServerId != 0);

  idString = TRI_StringUInt64((uint64_t) ServerId);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "serverId", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, idString));
  TRI_FreeString(TRI_CORE_MEM_ZONE, idString);

  tt = time(0);
  TRI_gmtime(tt, &tb);
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "createdTime", TRI_CreateString2CopyJson(TRI_CORE_MEM_ZONE, buffer, len));

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
// --SECTION--                                                    tick functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current tick value, without using a lock
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t GetTick (void) {
  return (ServerIdentifier | (CurrentTick << 16));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, without using a lock
////////////////////////////////////////////////////////////////////////////////

static inline void UpdateTick (TRI_voc_tick_t tick) {
  TRI_voc_tick_t s = tick >> 16;

  if (CurrentTick < s) {
    CurrentTick = s;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                database functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a user can see a database
////////////////////////////////////////////////////////////////////////////////

static bool CanUseDatabase (TRI_vocbase_t* vocbase,
                            char const* username,
                            char const* password) {
  bool mustChange;

  if (! vocbase->_settings.requireAuthentication) {
    // authentication is turned off
    return true;
  }

  return TRI_CheckAuthenticationAuthInfo(vocbase, nullptr, username, password, &mustChange);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator for database names
////////////////////////////////////////////////////////////////////////////////

static int DatabaseNameComparator (const void* lhs, const void* rhs) {
  const char* l = *((char**) lhs);
  const char* r = *((char**) rhs);

  return strcmp(l, r);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a list of database names
////////////////////////////////////////////////////////////////////////////////

static void SortDatabaseNames (TRI_vector_string_t* names) {
  qsort(names->_buffer, names->_length, sizeof(char*), &DatabaseNameComparator);
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
      res = TRI_CreateDirectory(path);

      if (res == TRI_ERROR_NO_ERROR) {
        LOG_INFO("created base application directory '%s'",
                 path);
      }
      else {
        LOG_ERROR("unable to create base application directory '%s': %s",
                  path,
                  TRI_errno_string(res));
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
  char* path = TRI_Concatenate3File(basePath, "databases", name);

  if (path != nullptr) {
    if (! TRI_IsDirectory(path)) {
      res = TRI_CreateDirectory(path);

      if (res == TRI_ERROR_NO_ERROR) {
        LOG_INFO("created application directory '%s' for database '%s'",
                 path,
                 name);
      }
      else {
        LOG_ERROR("unable to create application directory '%s' for database '%s': %s",
                  path,
                  name,
                  TRI_errno_string(res));
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
                          bool isUpgrade) {
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

  for (size_t i = 0;  i < n;  ++i) {
    TRI_vocbase_t* vocbase;
    TRI_json_t* json;
    TRI_json_t const* deletedJson;
    TRI_json_t const* nameJson;
    TRI_json_t const* idJson;
    TRI_voc_tick_t id;
    TRI_vocbase_defaults_t defaults;
    char const* name;
    char* databaseDirectory;
    char* parametersFile;
    char* databaseName;
    void const* found;

    name = files._buffer[i];
    TRI_ASSERT(name != nullptr);

    // .............................................................................
    // construct and validate path
    // .............................................................................

    databaseDirectory = TRI_Concatenate2File(server->_databasePath, name);

    if (databaseDirectory == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    if (! TRI_IsDirectory(databaseDirectory)) {
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

    // .............................................................................
    // read parameter.json file
    // .............................................................................

    // now read data from parameter.json file
    parametersFile = TRI_Concatenate2File(databaseDirectory, TRI_VOC_PARAMETER_FILE);

    if (parametersFile == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    if (! TRI_ExistsFile(parametersFile)) {
      // no parameter.json file
      LOG_ERROR("database directory '%s' does not contain parameters file",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, parametersFile);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      // skip this database
      continue;
    }

    LOG_DEBUG("reading database parameters from file '%s'",
              parametersFile);

    json = TRI_JsonFile(TRI_CORE_MEM_ZONE, parametersFile, nullptr);

    if (json == nullptr) {
      LOG_ERROR("database directory '%s' does not contain a valid parameters file",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, parametersFile);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      // skip this database
      continue;
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, parametersFile);

    deletedJson = TRI_LookupArrayJson(json, "deleted");

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

    idJson = TRI_LookupArrayJson(json, "id");

    if (! TRI_IsStringJson(idJson)) {
      LOG_ERROR("database directory '%s' does not contain a valid parameters file",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      // skip this database
      continue;
    }

    id = (TRI_voc_tick_t) TRI_UInt64String(idJson->_value._string.data);

    nameJson = TRI_LookupArrayJson(json, "name");

    if (! TRI_IsStringJson(nameJson)) {
      LOG_ERROR("database directory '%s' does not contain a valid parameters file",
                databaseDirectory);

      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      // skip this database
      continue;
    }

    databaseName = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE,
                                         nameJson->_value._string.data,
                                         nameJson->_value._string.length);

    // .............................................................................
    // setup defaults
    // .............................................................................

    // use defaults and blend them with parameters found in file
    TRI_GetDatabaseDefaultsServer(server, &defaults);
    // TODO: decide which parameter from the command-line should win vs. parameter.json
    // TRI_FromJsonVocBaseDefaults(&defaults, TRI_LookupArrayJson(json, "properties"));

    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    if (databaseName == nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      res = TRI_ERROR_OUT_OF_MEMORY;
      break;
    }

    // .............................................................................
    // create app directories
    // .............................................................................

    res = CreateApplicationDirectory(databaseName, server->_appPath);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseName);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      break;
    }

    res = CreateApplicationDirectory(databaseName, server->_devAppPath);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseName);
      TRI_FreeString(TRI_CORE_MEM_ZONE, databaseDirectory);
      break;
    }

    // .............................................................................
    // open the database and scan collections in it
    // .............................................................................

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

    res = TRI_InsertKeyAssociativePointer2(&server->_databases,
                                           vocbase->_name,
                                           vocbase,
                                           &found);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("could not add database '%s': out of memory",
                name);

      break;
    }

    // should never have a duplicate database name
    TRI_ASSERT(found == nullptr);

    LOG_INFO("loaded database '%s' from '%s'",
             vocbase->_name,
             vocbase->_path);
  }

  TRI_DestroyVectorString(&files);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close all opened databases
////////////////////////////////////////////////////////////////////////////////

static int CloseDatabases (TRI_server_t* server) {
  DatabaseWriteLocker locker(&server->_databasesLock);

  size_t n = server->_databases._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

    if (vocbase != nullptr) {
      TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);

      TRI_DestroyVocBase(vocbase);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);

      // clear to avoid potential double freeing
      server->_databases._table[i] = nullptr;
    }
  }

  n = server->_coordinatorDatabases._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_coordinatorDatabases._table[i]);

    if (vocbase != nullptr) {
      TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR);

      TRI_DestroyInitialVocBase(vocbase);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);

      // clear to avoid potential double freeing
      server->_coordinatorDatabases._table[i] = nullptr;
    }
  }

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

  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

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

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "id", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, tickString));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "name", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, name));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "deleted", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, deleted));
  // TODO: save properties later when it is clear what they will be used
  // TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "properties", properties);

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

  res = TRI_CreateDirectory(file);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, file);
    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);

    return res;
  }

  res = SaveDatabaseParameters(tick, databaseName, false, defaults, file);
  TRI_FreeString(TRI_CORE_MEM_ZONE, file);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

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
/// @brief initialise the list of databases
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
  catch (triagens::arango::Exception const& ex) {
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
  catch (triagens::arango::Exception const& ex) {
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

  while (true) {
    TRI_LockMutex(&server->_createLock);
    bool shutdown = server->_shutdown;
    TRI_UnlockMutex(&server->_createLock);

    // check if we have to drop some database
    TRI_vocbase_t* database = nullptr;

    {
      DatabaseReadLocker locker(&server->_databasesLock);

      size_t const n = server->_droppedDatabases._length;

      for (size_t i = 0; i < n; ++i) {
        TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_AtVectorPointer(&server->_droppedDatabases, i));

        if (! TRI_CanRemoveVocBase(vocbase)) {
          continue;
        }

        // found a database to delete
        database = static_cast<TRI_vocbase_t*>(TRI_RemoveVectorPointer(&server->_droppedDatabases, i));
        break;
      }
    }

    if (database != nullptr) {
      if (database->_type == TRI_VOCBASE_TYPE_COORDINATOR) {
        // coordinator database
        // ---------------------------

        TRI_DestroyInitialVocBase(database);
      }
      else {
        // regular database
        // ---------------------------

        // remember the database path
        char* path;

        LOG_TRACE("physically removing database directory '%s' of database '%s'",
                  database->_path,
                  database->_name);

        // remove apps directory for database
        if (database->_isOwnAppsDirectory && strlen(server->_appPath) > 0) {
          path = TRI_Concatenate3File(server->_appPath, "databases", database->_name);

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

        // remove dev-apps directory for database
        if (database->_isOwnAppsDirectory && strlen(server->_devAppPath) > 0) {
          path = TRI_Concatenate3File(server->_devAppPath, "databases", database->_name);

          if (path != nullptr) {
            if (TRI_IsDirectory(path)) {
              LOG_TRACE("removing dev-app directory '%s' of database '%s'",
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

        TRI_Free(TRI_UNKNOWN_MEM_ZONE, database);
      }

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
    }

    // next iteration
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a server instance
////////////////////////////////////////////////////////////////////////////////

TRI_server_t* TRI_CreateServer () {
  return static_cast<TRI_server_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_server_t), true));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a server instance with configuration
////////////////////////////////////////////////////////////////////////////////

int TRI_InitServer (TRI_server_t* server,
                    void* applicationEndpointServer,
                    char const* basePath,
                    char const* appPath,
                    char const* devAppPath,
                    TRI_vocbase_defaults_t const* defaults,
                    bool disableAppliers,
                    bool iterateMarkersOnOpen) {

  TRI_ASSERT(server != nullptr);
  TRI_ASSERT(basePath != nullptr);

  server->_iterateMarkersOnOpen = iterateMarkersOnOpen;
  server->_hasCreatedSystemDatabase = false;

  // c++ object, may be null in console mode
  server->_applicationEndpointServer = applicationEndpointServer;

  // .............................................................................
  // set up paths and filenames
  // .............................................................................

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

  server->_devAppPath = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, devAppPath);

  if (server->_devAppPath == nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, server->_appPath);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_serverIdFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_lockFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_databasePath);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_basePath);

    return TRI_ERROR_OUT_OF_MEMORY;
  }


  // .............................................................................
  // server defaults
  // .............................................................................

  memcpy(&server->_defaults, defaults, sizeof(TRI_vocbase_defaults_t));

  // .............................................................................
  // database hashes and vectors
  // .............................................................................

  TRI_InitAssociativePointer(&server->_databases,
                             TRI_UNKNOWN_MEM_ZONE,
                             &TRI_HashStringKeyAssociativePointer,
                             HashElementDatabaseName,
                             EqualKeyDatabaseName,
                             nullptr);

  TRI_InitAssociativePointer(&server->_coordinatorDatabases,
                             TRI_UNKNOWN_MEM_ZONE,
                             &TRI_HashStringKeyAssociativePointer,
                             HashElementDatabaseName,
                             EqualKeyDatabaseName,
                             nullptr);

  TRI_InitReadWriteLock(&server->_databasesLock);

  TRI_InitVectorPointer2(&server->_droppedDatabases, TRI_UNKNOWN_MEM_ZONE, 64);

  TRI_InitMutex(&server->_createLock);

  server->_disableReplicationAppliers = disableAppliers;

  server->_queryRegistry = nullptr;   // will be filled in later

  server->_initialised = true;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a server instance
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyServer (TRI_server_t* server) {
  if (server->_initialised) {
    CloseDatabases(server);

    TRI_DestroyMutex(&server->_createLock);
    TRI_DestroyVectorPointer(&server->_droppedDatabases);
    TRI_DestroyReadWriteLock(&server->_databasesLock);
    TRI_DestroyAssociativePointer(&server->_coordinatorDatabases);
    TRI_DestroyAssociativePointer(&server->_databases);

    TRI_Free(TRI_CORE_MEM_ZONE, server->_devAppPath);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_appPath);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_serverIdFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_lockFilename);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_databasePath);
    TRI_Free(TRI_CORE_MEM_ZONE, server->_basePath);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a server instance
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeServer (TRI_server_t* server) {
  TRI_DestroyServer(server);
  TRI_Free(TRI_CORE_MEM_ZONE, server);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise globals
////////////////////////////////////////////////////////////////////////////////

void TRI_InitServerGlobals () {
  ServerIdentifier = TRI_UInt16Random();
  PageSize = (size_t) getpagesize();

  memset(&ServerId, 0, sizeof(TRI_server_id_t));

  TRI_InitSpin(&TickLock);
  TRI_GlobalInitStatementListAql();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief de-initialise globals
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeServerGlobals () {
  TRI_GlobalFreeStatementListAql();
  TRI_DestroySpin(&TickLock);
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

  // .............................................................................
  // check that the database is not locked and lock it
  // .............................................................................

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

  // .............................................................................
  // read the server id
  // .............................................................................

  res = DetermineServerId(server, checkVersion);

  if (res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
    return res;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("reading/creating server file failed: %s",
              TRI_errno_string(res));

    return res;
  }

  // .............................................................................
  // verify existence of "databases" subdirectory
  // .............................................................................

  if (! TRI_IsDirectory(server->_databasePath)) {
    res = TRI_CreateDirectory(server->_databasePath);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to create database directory '%s': %s",
                server->_databasePath,
                TRI_errno_string(res));

      return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
    }

    server->_iterateMarkersOnOpen = false;
  }

  if (! TRI_IsWritable(server->_databasePath)) {
    LOG_ERROR("database directory '%s' is not writable",
              server->_databasePath);

    return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
  }


  // .............................................................................
  // perform an eventual migration of the databases.
  // .............................................................................

  res = InitDatabases(server, checkVersion, performUpgrade);

  if (res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
    return res;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("unable to initialise databases: %s",
              TRI_errno_string(res));
    return res;
  }

  // .............................................................................
  // create shared application directories
  // .............................................................................

  if (server->_appPath != nullptr &&
      strlen(server->_appPath) > 0 &&
      ! TRI_IsDirectory(server->_appPath)) {
    if (! performUpgrade) {
      LOG_ERROR("specified --javascript.app-path directory '%s' does not exist. "
                "Please start again with --upgrade option to create it.",
                server->_appPath);
      return TRI_ERROR_BAD_PARAMETER;
    }

    res = TRI_CreateDirectory(server->_appPath);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to create --javascript.app-path directory '%s': %s",
                server->_appPath,
                TRI_errno_string(res));
      return res;
    }
  }

  if (server->_devAppPath != nullptr &&
      strlen(server->_devAppPath) > 0 &&
      ! TRI_IsDirectory(server->_devAppPath)) {
    if (! performUpgrade) {
      LOG_ERROR("specified --javascript.dev-app-path directory '%s' does not exist. "
                "Please start again with --upgrade option to create it.",
                server->_devAppPath);
      return TRI_ERROR_BAD_PARAMETER;
    }

    res = TRI_CreateDirectory(server->_devAppPath);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("unable to create --javascript.dev-app-path directory '%s': %s",
                server->_devAppPath,
                TRI_errno_string(res));
      return res;
    }
  }

  // create subdirectories if not yet present
  res = CreateBaseApplicationDirectory(server->_appPath, "databases");

  // system directory is in a read-only location
#if 0
  if (res == TRI_ERROR_NO_ERROR) {
    res = CreateBaseApplicationDirectory(server->_appPath, "system");
  }
#endif

  if (res == TRI_ERROR_NO_ERROR) {
    res = CreateBaseApplicationDirectory(server->_devAppPath, "databases");
  }

  // system directory is in a read-only location
#if 0
  if (res == TRI_ERROR_NO_ERROR) {
    res = CreateBaseApplicationDirectory(server->_devAppPath, "system");
  }
#endif

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("unable to initialise databases: %s",
              TRI_errno_string(res));
    return res;
  }


  // .............................................................................
  // open and scan all databases
  // .............................................................................

  // scan all databases
  res = OpenDatabases(server, performUpgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not iterate over all databases: %s",
              TRI_errno_string(res));

    return res;
  }

  // we don't yet need the lock here as this is called during startup and no races
  // are possible. however, this may be changed in the future
  TRI_LockMutex(&server->_createLock);
  server->_shutdown = false;
  TRI_UnlockMutex(&server->_createLock);

  // start dbm thread
  TRI_InitThread(&server->_databaseManager);
  TRI_StartThread(&server->_databaseManager, nullptr, "[databases]", DatabaseManager, server);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises all databases
////////////////////////////////////////////////////////////////////////////////

int TRI_InitDatabasesServer (TRI_server_t* server) {
  DatabaseReadLocker locker(&server->_databasesLock);

  size_t const n = server->_databases._nrAlloc;

  // iterate over all databases
  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

    if (vocbase != nullptr) {
      TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);

      // initialise the authentication data for the database
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
          int res = TRI_StartReplicationApplier(vocbase->_replicationApplier, 0, false);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_WARNING("unable to start replication applier for database '%s': %s",
                        vocbase->_name,
                        TRI_errno_string(res));
          }
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
  int res;

  // set shutdown flag
  TRI_LockMutex(&server->_createLock);
  server->_shutdown = true;
  TRI_UnlockMutex(&server->_createLock);

  // stop dbm thread
  res = TRI_JoinThread(&server->_databaseManager);

  CloseDatabases(server);

  TRI_DestroyLockFile(server->_lockFilename);

  return res;
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

  TRI_LockMutex(&server->_createLock);

  {
    DatabaseReadLocker locker(&server->_databasesLock);

    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_LookupByKeyAssociativePointer(&server->_coordinatorDatabases, name));

    if (vocbase != nullptr) {
      // name already in use
      TRI_UnlockMutex(&server->_createLock);

      return TRI_ERROR_ARANGO_DUPLICATE_NAME;
    }
  }

  // name not yet in use, release the read lock

  TRI_vocbase_t* vocbase = TRI_CreateInitialVocBase(server, TRI_VOCBASE_TYPE_COORDINATOR, "none", tick, name, defaults);

  if (vocbase == nullptr) {
    TRI_UnlockMutex(&server->_createLock);

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
    TRI_DestroyInitialVocBase(vocbase);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, vocbase);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // TODO: create application directories??
//  CreateApplicationDirectory(vocbase->_name, server->_appPath);
//  CreateApplicationDirectory(vocbase->_name, server->_devAppPath);

  // increase reference counter
  TRI_UseVocBase(vocbase);

  {
    DatabaseWriteLocker locker(&server->_databasesLock);
    TRI_InsertKeyAssociativePointer(&server->_coordinatorDatabases, vocbase->_name, vocbase, false);
  }

  TRI_UnlockMutex(&server->_createLock);

  vocbase->_state = (sig_atomic_t) TRI_VOCBASE_STATE_NORMAL;

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

  // the create lock makes sure no one else is creating a database while we're inside
  // this function
  TRI_LockMutex(&server->_createLock);

  {
    DatabaseReadLocker locker(&server->_databasesLock);

    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_LookupByKeyAssociativePointer(&server->_databases, name));

    if (vocbase != nullptr) {
      // name already in use
      TRI_UnlockMutex(&server->_createLock);

      return TRI_ERROR_ARANGO_DUPLICATE_NAME;
    }
  }

  // name not yet in use
  TRI_json_t* json = TRI_JsonVocBaseDefaults(TRI_UNKNOWN_MEM_ZONE, defaults);

  if (json == nullptr) {
    TRI_UnlockMutex(&server->_createLock);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // create the database directory
  char* file;

  if (databaseId == 0) {
    databaseId = TRI_NewTickServer();
  }

  int res = CreateDatabaseDirectory(server, databaseId, name, defaults, &file);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_UnlockMutex(&server->_createLock);

    return res;
  }

  char* path = TRI_Concatenate2File(server->_databasePath, file);
  TRI_FreeString(TRI_CORE_MEM_ZONE, file);

  LOG_INFO("creating database '%s', directory '%s'",
           name,
           path);

  TRI_vocbase_t* vocbase = TRI_OpenVocBase(server, path, databaseId, name, defaults, false, false);
  TRI_FreeString(TRI_CORE_MEM_ZONE, path);

  if (vocbase == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_UnlockMutex(&server->_createLock);

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
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, tickString));
  TRI_FreeString(TRI_CORE_MEM_ZONE, tickString);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "name", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, name));


  // create application directories
  CreateApplicationDirectory(vocbase->_name, server->_appPath);
  CreateApplicationDirectory(vocbase->_name, server->_devAppPath);

  if (! triagens::wal::LogfileManager::instance()->isInRecovery()) {
    TRI_ReloadAuthInfo(vocbase);
    TRI_StartCompactorVocBase(vocbase);

    // start the replication applier
    if (vocbase->_replicationApplier->_configuration._autoStart) {
      if (server->_disableReplicationAppliers) {
        LOG_INFO("replication applier explicitly deactivated for database '%s'", name);
      }
      else {
        res = TRI_StartReplicationApplier(vocbase->_replicationApplier, 0, false);

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
    DatabaseWriteLocker locker(&server->_databasesLock);
    TRI_InsertKeyAssociativePointer(&server->_databases, vocbase->_name, vocbase, false);
  }

  TRI_UnlockMutex(&server->_createLock);

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

TRI_voc_tick_t* TRI_GetIdsCoordinatorDatabaseServer (TRI_server_t* server) {
  TRI_vector_t v;

  TRI_InitVector(&v, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_voc_tick_t));

  {
    DatabaseReadLocker locker(&server->_databasesLock);
    size_t const n = server->_coordinatorDatabases._nrAlloc;

    for (size_t i = 0; i < n; ++i) {
      TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_coordinatorDatabases._table[i]);

      if (vocbase != nullptr &&
          ! TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE)) {
        TRI_PushBackVector(&v, &vocbase->_id);
      }
    }
  }

  // append a 0 as the end marker
  TRI_voc_tick_t zero = 0;
  TRI_PushBackVector(&v, &zero);

  // steal the elements from the vector
  TRI_voc_tick_t* data = (TRI_voc_tick_t*) v._buffer;
  v._buffer = nullptr;

  TRI_DestroyVector(&v);

  return data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropByIdCoordinatorDatabaseServer (TRI_server_t* server,
                                           TRI_voc_tick_t id,
                                           bool force) {
  int res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;

  DatabaseWriteLocker locker(&server->_databasesLock);

  if (TRI_ReserveVectorPointer(&server->_droppedDatabases, 1) != TRI_ERROR_NO_ERROR) {
    // we need space for one more element
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  size_t const n = server->_coordinatorDatabases._nrAlloc;
  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_coordinatorDatabases._table[i]);

    if (vocbase != nullptr &&
        vocbase->_id == id &&
        (force || ! TRI_EqualString(vocbase->_name, TRI_VOC_SYSTEM_DATABASE))) {
      TRI_RemoveKeyAssociativePointer(&server->_coordinatorDatabases, vocbase->_name);

      if (TRI_DropVocBase(vocbase)) {
        LOG_INFO("dropping coordinator database '%s'",
                 vocbase->_name);

        TRI_PushBackVectorPointer(&server->_droppedDatabases, vocbase);
        res = TRI_ERROR_NO_ERROR;
      }
      break;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an existing coordinator database
////////////////////////////////////////////////////////////////////////////////

int TRI_DropCoordinatorDatabaseServer (TRI_server_t* server,
                                       char const* name) {
  if (TRI_EqualString(name, TRI_VOC_SYSTEM_DATABASE)) {
    // prevent deletion of system database
    return TRI_ERROR_FORBIDDEN;
  }

  DatabaseWriteLocker locker(&server->_databasesLock);

  if (TRI_ReserveVectorPointer(&server->_droppedDatabases, 1) != TRI_ERROR_NO_ERROR) {
    // we need space for one more element
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res = TRI_ERROR_INTERNAL;
  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_RemoveKeyAssociativePointer(&server->_coordinatorDatabases, name));

  if (vocbase == nullptr) {
    // not found
    res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }
  else {
    // mark as deleted
    TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR);

    if (TRI_DropVocBase(vocbase)) {
      LOG_INFO("dropping coordinator database '%s'",
               vocbase->_name);

      TRI_PushBackVectorPointer(&server->_droppedDatabases, vocbase);
      res = TRI_ERROR_NO_ERROR;
    }
    else {
      // already deleted
      res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
    }
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

  DatabaseWriteLocker locker(&server->_databasesLock);

  if (TRI_ReserveVectorPointer(&server->_droppedDatabases, 1) != TRI_ERROR_NO_ERROR) {
    // we need space for one more element
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res = TRI_ERROR_INTERNAL;
  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_RemoveKeyAssociativePointer(&server->_databases, name));

  if (vocbase == nullptr) {
    // not found
    res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }
  else {
    // mark as deleted
    TRI_ASSERT(vocbase->_type == TRI_VOCBASE_TYPE_NORMAL);

    vocbase->_isOwnAppsDirectory = removeAppsDirectory;

    if (TRI_DropVocBase(vocbase)) {
      LOG_INFO("dropping database '%s', directory '%s'",
               vocbase->_name,
               vocbase->_path);

      res = SaveDatabaseParameters(vocbase->_id,
                                   vocbase->_name,
                                   true,
                                   &vocbase->_settings,
                                   vocbase->_path);

      TRI_PushBackVectorPointer(&server->_droppedDatabases, vocbase);

      // TODO: what to do in case of error?
      if (writeMarker) {
        WriteDropMarker(vocbase->_id);
      }
    }
    else {
      // already deleted
      res = TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
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
    DatabaseReadLocker locker(&server->_databasesLock);

    size_t const n = server->_databases._nrAlloc;

    for (size_t i = 0; i < n; ++i) {
      TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

      if (vocbase != nullptr && vocbase->_id == id) {
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

  DatabaseReadLocker locker(&server->_databasesLock);
  size_t const n = server->_coordinatorDatabases._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_coordinatorDatabases._table[i]);

    if (vocbase != nullptr && vocbase->_id == id) {
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
  DatabaseReadLocker locker(&server->_databasesLock);

  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_LookupByKeyAssociativePointer(&server->_coordinatorDatabases, name));

  if (vocbase != nullptr) {
    bool result TRI_UNUSED = TRI_UseVocBase(vocbase);

    // if we got here, no one else can have deleted the database
    TRI_ASSERT(result == true);
  }

  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a database by its name
/// this will increase the reference-counter for the database
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_UseDatabaseServer (TRI_server_t* server,
                                      char const* name) {
  DatabaseReadLocker locker(&server->_databasesLock);

  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(TRI_LookupByKeyAssociativePointer(&server->_databases, name));

  if (vocbase != nullptr) {
    bool result TRI_UNUSED = TRI_UseVocBase(vocbase);

    // if we got here, no one else can have deleted the database
    TRI_ASSERT(result == true);
  }

  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a database by its id
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_LookupDatabaseByIdServer (TRI_server_t* server,
                                             TRI_voc_tick_t id) {
  DatabaseReadLocker locker(&server->_databasesLock);
  size_t const n = server->_databases._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

    if (vocbase != nullptr && vocbase->_id == id) {
      return vocbase;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a database by its name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* TRI_LookupDatabaseByNameServer (TRI_server_t* server,
                                               char const* name) {
  DatabaseReadLocker locker(&server->_databasesLock);
  size_t const n = server->_databases._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

    if (vocbase != nullptr && TRI_EqualString(vocbase->_name, name)) {
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
  DatabaseReadLocker locker(&server->_databasesLock);
  size_t const n = server->_databases._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

    if (vocbase != nullptr && vocbase->_id == id) {
      bool result TRI_UNUSED = TRI_UseVocBase(vocbase);

      // if we got here, no one else can have deleted the database
      TRI_ASSERT(result == true);
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
/// @brief get a database by its id
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExistsDatabaseByIdServer (TRI_server_t* server,
                                   TRI_voc_tick_t id) {
  DatabaseReadLocker locker(&server->_databasesLock);
  size_t const n = server->_databases._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

    if (vocbase != nullptr && vocbase->_id == id) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all databases a user can see
////////////////////////////////////////////////////////////////////////////////

int TRI_GetUserDatabasesServer (TRI_server_t* server,
                                char const* username,
                                char const* password,
                                TRI_vector_string_t* names) {

  int res = TRI_ERROR_NO_ERROR;

  {
    DatabaseReadLocker locker(&server->_databasesLock);
    size_t const n = server->_databases._nrAlloc;

    for (size_t i = 0; i < n; ++i) {
      TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

      if (vocbase != nullptr) {
        char* copy;

        TRI_ASSERT(vocbase->_name != nullptr);

        if (! CanUseDatabase(vocbase, username, password)) {
          // user cannot see database
          continue;
        }

        copy = TRI_DuplicateStringZ(names->_memoryZone, vocbase->_name);

        if (copy == nullptr) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          break;
        }

        if (TRI_PushBackVectorString(names, copy) != TRI_ERROR_NO_ERROR) {
          // insertion failed.
          TRI_Free(names->_memoryZone, copy);
          res = TRI_ERROR_OUT_OF_MEMORY;
          break;
        }
      }
    }
  }

  SortDatabaseNames(names);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of all database names
////////////////////////////////////////////////////////////////////////////////

int TRI_GetDatabaseNamesServer (TRI_server_t* server,
                                TRI_vector_string_t* names) {

  int res = TRI_ERROR_NO_ERROR;

  {
    DatabaseReadLocker locker(&server->_databasesLock);
    size_t const n = server->_databases._nrAlloc;

    for (size_t i = 0; i < n; ++i) {
      TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(server->_databases._table[i]);

      if (vocbase != nullptr) {
        char* copy;

        TRI_ASSERT(vocbase->_name != nullptr);

        copy = TRI_DuplicateStringZ(names->_memoryZone, vocbase->_name);

        if (copy == nullptr) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          break;
        }

        if (TRI_PushBackVectorString(names, copy) != TRI_ERROR_NO_ERROR) {
          // insertion failed.
          TRI_Free(names->_memoryZone, copy);
          res = TRI_ERROR_OUT_OF_MEMORY;
          break;
        }
      }
    }
  }

  SortDatabaseNames(names);

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

  TRI_LockSpin(&TickLock);

  tick |= (++CurrentTick) << 16;

  TRI_UnlockSpin(&TickLock);

  return tick;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, with lock
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateTickServer (TRI_voc_tick_t tick) {
  TRI_LockSpin(&TickLock);
  UpdateTick(tick);
  TRI_UnlockSpin(&TickLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the tick counter, without lock - only use at startup!!
////////////////////////////////////////////////////////////////////////////////

void TRI_FastUpdateTickServer (TRI_voc_tick_t tick) {
  UpdateTick(tick);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current tick counter
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tick_t TRI_CurrentTickServer () {
  TRI_voc_tick_t tick;

  TRI_LockSpin(&TickLock);
  tick = GetTick();
  TRI_UnlockSpin(&TickLock);

  return tick;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   other functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief msyncs a memory block between begin (incl) and end (excl)
////////////////////////////////////////////////////////////////////////////////

bool TRI_MSync (int fd,
                void* mmHandle,
                char const* begin,
                char const* end) {
  uintptr_t p = (intptr_t) begin;
  uintptr_t q = (intptr_t) end;
  uintptr_t g = (intptr_t) PageSize;

  char* b = (char*)( (p / g) * g );
  char* e = (char*)( ((q + g - 1) / g) * g );
  int res;

  res = TRI_FlushMMFile(fd, &mmHandle, b, e - b, MS_SYNC);

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
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
