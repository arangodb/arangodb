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

#include "MMFilesEngine.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/CleanupThread.h"
#include "VocBase/collection.h"
#include "VocBase/compactor.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBIndex.h"
#endif

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

namespace {

/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
/// a number, and type and ending are arbitrary letters
static uint64_t GetNumericFilenamePartFromDatafile(std::string const& filename) {
  char const* pos1 = strrchr(filename.c_str(), '.');

  if (pos1 == nullptr) {
    return 0;
  }

  char const* pos2 = strrchr(filename.c_str(), '-');

  if (pos2 == nullptr || pos2 > pos1) {
    return 0;
  }

  return basics::StringUtils::uint64(pos2 + 1, pos1 - pos2 - 1);
}

/// @brief extract the numeric part from a filename
static uint64_t GetNumericFilenamePartFromDatabase(std::string const& filename) {
  char const* pos = strrchr(filename.c_str(), '-');

  if (pos == nullptr) {
    return 0;
  }

  return basics::StringUtils::uint64(pos + 1);
}

/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort database filenames on startup
struct DatafileIdStringComparator {
  bool operator()(std::string const& lhs, std::string const& rhs) const {
    return GetNumericFilenamePartFromDatafile(lhs) < GetNumericFilenamePartFromDatafile(rhs);
  }
};

/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort database filenames on startup
struct DatabaseIdStringComparator {
  bool operator()(std::string const& lhs, std::string const& rhs) const {
    return GetNumericFilenamePartFromDatabase(lhs) < GetNumericFilenamePartFromDatabase(rhs);
  }
};

}

std::string const MMFilesEngine::EngineName("MMFiles");

// create the storage engine
MMFilesEngine::MMFilesEngine(application_features::ApplicationServer* server)
    : StorageEngine(server, EngineName),
      _iterateMarkersOnOpen(true),
      _isUpgrade(false),
      _maxTick(0) {
}

MMFilesEngine::~MMFilesEngine() {
}

// add the storage engine's specifc options to the global list of options
void MMFilesEngine::collectOptions(std::shared_ptr<options::ProgramOptions>) {
}
  
// validate the storage engine's specific options
void MMFilesEngine::validateOptions(std::shared_ptr<options::ProgramOptions>) {
}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void MMFilesEngine::prepare() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE = this);

  // get base path from DatabaseServerFeature 
  auto database = application_features::ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _basePath = database->directory();
  _databasePath += database->subdirectoryName("databases") + TRI_DIR_SEPARATOR_CHAR;
  
  TRI_ASSERT(!_basePath.empty());
  TRI_ASSERT(!_databasePath.empty());
}

// initialize engine
void MMFilesEngine::start() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE = this);
  
  // test if the "databases" directory is present and writable
  verifyDirectories();
  
  int res = TRI_ERROR_NO_ERROR;
  
  // get names of all databases
  std::vector<std::string> names(getDatabaseNames());

  if (names.empty()) {
    // no databases found, i.e. there is no system database!
    // create a database for the system database
    res = createDatabaseDirectory(TRI_NewTickServer(), TRI_VOC_SYSTEM_DATABASE);
    _iterateMarkersOnOpen = false;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "unable to initialize databases: " << TRI_errno_string(res);
    THROW_ARANGO_EXCEPTION(res);
  }
  
  if (_iterateMarkersOnOpen) {
    LOG(WARN) << "no shutdown info found. scanning datafiles for last tick...";
  }
}

// stop the storage engine. this can be used to flush all data to disk,
// shutdown threads etc. it is guaranteed that there will be no read and
// write requests to the storage engine after this call
void MMFilesEngine::stop() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE = this);
}

void MMFilesEngine::recoveryDone(TRI_vocbase_t* vocbase) {    
  DatabaseFeature* databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");

  if (!databaseFeature->checkVersion() && !databaseFeature->upgrade()) {
    // start compactor thread
    TRI_ASSERT(!vocbase->_hasCompactor);
    LOG(TRACE) << "starting compactor for database '" << vocbase->name() << "'";

    TRI_InitThread(&vocbase->_compactor);
    TRI_StartThread(&vocbase->_compactor, nullptr, "Compactor",
                    TRI_CompactorVocBase, vocbase);
    vocbase->_hasCompactor = true;
  }

  // delete all collection files from collections marked as deleted
  for (auto& it : _deleted) {
    std::string const& name = it.first;
    std::string const& file = it.second;

    LOG(DEBUG) << "collection '" << name << "' was deleted, wiping it";

    int res = TRI_RemoveDirectory(file.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(WARN) << "cannot wipe deleted collection '" << name << "': " << TRI_errno_string(res);
    }
  }
  _deleted.clear();
}

// fill the Builder object with an array of databases that were detected
// by the storage engine. this method must sort out databases that were not
// fully created (see "createDatabase" below). called at server start only
void MMFilesEngine::getDatabases(arangodb::velocypack::Builder& result) {
  result.openArray();

  // open databases in defined order
  std::vector<std::string> files = TRI_FilesDirectory(_databasePath.c_str());
  std::sort(files.begin(), files.end(), DatabaseIdStringComparator());

  for (auto const& name : files) {
    TRI_ASSERT(!name.empty());
    
    TRI_voc_tick_t id = GetNumericFilenamePartFromDatabase(name);

    if (id == 0) {
      // invalid id
      continue;
    }

    // construct and validate path
    std::string const directory(basics::FileUtils::buildFilename(_databasePath, name));

    if (!TRI_IsDirectory(directory.c_str())) {
      continue;
    }

    if (!basics::StringUtils::isPrefix(name, "database-") ||
        basics::StringUtils::isSuffix(name, ".tmp")) {
      LOG_TOPIC(TRACE, Logger::DATAFILES) << "ignoring file '" << name << "'";
      continue;
    }

    // we have a directory...

    if (!TRI_IsWritable(directory.c_str())) {
      // the database directory we found is not writable for the current user
      // this can cause serious trouble so we will abort the server start if we
      // encounter this situation
      LOG(ERR) << "database directory '" << directory
               << "' is not writable for current user";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
    }

    // we have a writable directory...
    std::string const tmpfile(basics::FileUtils::buildFilename(directory, ".tmp"));

    if (TRI_ExistsFile(tmpfile.c_str())) {
      // still a temporary... must ignore
      LOG(TRACE) << "ignoring temporary directory '" << tmpfile << "'";
      continue;
    }

    // a valid database directory

    // now read data from parameter.json file
    std::string const file = parametersFile(id);

    if (!TRI_ExistsFile(file.c_str())) {
      // no parameter.json file
      
      if (TRI_FilesDirectory(directory.c_str()).empty()) {
        // directory is otherwise empty, continue!
        LOG(WARN) << "ignoring empty database directory '" << directory
                  << "' without parameters file";
        continue;
      } 
        
      // abort
      LOG(ERR) << "database directory '" << directory
               << "' does not contain parameters file or parameters file cannot be read";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    LOG(DEBUG) << "reading database parameters from file '" << file << "'";
    std::shared_ptr<VPackBuilder> builder;
    try {
      builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(file);
    } catch (...) {
      LOG(ERR) << "database directory '" << directory
               << "' does not contain a valid parameters file";

      // abort
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    VPackSlice parameters = builder->slice();
    std::string const parametersString = parameters.toJson();

    LOG(DEBUG) << "database parameters: " << parametersString;
      
    VPackSlice idSlice = parameters.get("id");
    
    if (!idSlice.isString() ||
        id != static_cast<TRI_voc_tick_t>(basics::StringUtils::uint64(idSlice.copyString()))) {
      LOG(ERR) << "database directory '" << directory
               << "' does not contain a valid parameters file";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }
    
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted", false)) {
      // database is deleted, skip it!
      LOG(DEBUG) << "found dropped database in directory '" << directory << "'";
      LOG(DEBUG) << "removing superfluous database directory '" << directory << "'";

#ifdef ARANGODB_ENABLE_ROCKSDB
      // delete persistent indexes for this database
      TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
          basics::StringUtils::uint64(idSlice.copyString()));
      RocksDBFeature::dropDatabase(id);
#endif

      dropDatabaseDirectory(directory);
      continue;
    }

    VPackSlice nameSlice = parameters.get("name");

    if (!nameSlice.isString()) {
      LOG(ERR) << "database directory '" << directory << "' does not contain a valid parameters file";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    result.add(parameters);
  }

  result.close();
}

// fill the Builder object with an array of collections (and their corresponding
// indexes) that were detected by the storage engine. called at server start only
int MMFilesEngine::getCollectionsAndIndexes(TRI_vocbase_t* vocbase, 
                                            arangodb::velocypack::Builder& result,
                                            bool wasCleanShutdown,
                                            bool isUpgrade) {
  if (!wasCleanShutdown) {
    LOG(TRACE) << "scanning all collection markers in database '" << vocbase->name() << "'";
  }

  result.openArray();

  std::string const path = databaseDirectory(vocbase->_id);
  std::vector<std::string> files = TRI_FilesDirectory(path.c_str());

  for (auto const& name : files) {
    TRI_ASSERT(!name.empty());

    if (!StringUtils::isPrefix(name, "collection-") ||
        StringUtils::isSuffix(name, ".tmp")) {
      // no match, ignore this file
      continue;
    }

    std::string const directory = FileUtils::buildFilename(path, name);

    if (!TRI_IsDirectory(directory.c_str())) {
      LOG(DEBUG) << "ignoring non-directory '" << directory << "'";
      continue;
    }

    if (!TRI_IsWritable(directory.c_str())) {
      // the collection directory we found is not writable for the current
      // user
      // this can cause serious trouble so we will abort the server start if
      // we
      // encounter this situation
      LOG(ERR) << "database subdirectory '" << directory
                << "' is not writable for current user";

      return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
    }

    int res = TRI_ERROR_NO_ERROR;

    try {
      arangodb::VocbaseCollectionInfo info =
          arangodb::VocbaseCollectionInfo::fromFile(directory, vocbase,
                                                    "",  // Name is unused
                                                    true);
     
      if (info.deleted()) {
        _deleted.emplace_back(std::make_pair(info.name(), directory));
        continue;
      }

      if (info.version() < TRI_COL_VERSION && !isUpgrade) {
        // collection is too "old"
        LOG(ERR) << "collection '" << info.name()
                 << "' has a too old version. Please start the server "
                    "with the --database.auto-upgrade option.";

        return TRI_ERROR_FAILED;
      }

      // add collection info
      result.openObject();
      info.toVelocyPack(result);
      result.add("path", VPackValue(directory));
      result.close();

    } catch (arangodb::basics::Exception const& e) {
      std::string tmpfile = FileUtils::buildFilename(directory, ".tmp");

      if (TRI_ExistsFile(tmpfile.c_str())) {
        LOG(TRACE) << "ignoring temporary directory '" << tmpfile << "'";
        // temp file still exists. this means the collection was not created
        // fully and needs to be ignored
        continue;  // ignore this directory
      }

      res = e.code();

      LOG(ERR) << "cannot read collection info file in directory '"
                << directory << "': " << TRI_errno_string(res);

      return res;
    }
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

// determine the maximum revision id previously handed out by the storage
// engine. this value is used as a lower bound for further HLC values handed out by
// the server. called at server start only, after getDatabases() and getCollectionsAndIndexes()
uint64_t MMFilesEngine::getMaxRevision() {
  return _maxTick;
}
  
TRI_vocbase_t* MMFilesEngine::openDatabase(VPackSlice const& parameters, bool isUpgrade) {
  VPackSlice idSlice = parameters.get("id");
  TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(basics::StringUtils::uint64(idSlice.copyString()));
  std::string const name = parameters.get("name").copyString();
  
  bool const wasCleanShutdown = arangodb::wal::LogfileManager::instance()->hasFoundLastTick();
  return openExistingDatabase(id, name, wasCleanShutdown, isUpgrade);
}

// asks the storage engine to create a database as specified in the VPack
// Slice object and persist the creation info. It is guaranteed by the server that 
// no other active database with the same name and id exists when this function
// is called. If this operation fails somewhere in the middle, the storage 
// engine is required to fully clean up the creation and throw only then, 
// so that subsequent database creation requests will not fail.
// the WAL entry for the database creation will be written *after* the call
// to "createDatabase" returns
TRI_vocbase_t* MMFilesEngine::createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& data) {
  std::string const name = data.get("name").copyString();

  waitUntilDeletion(id, true);
  
  int res = createDatabaseDirectory(id, name);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return openExistingDatabase(id, name, true, false);
}

// asks the storage engine to drop the specified database and persist the 
// deletion info. Note that physical deletion of the database data must not 
// be carried out by this call, as there may still be readers of the database's data. 
// It is recommended that this operation only sets a deletion flag for the database 
// but let's an async task perform the actual deletion. 
// the WAL entry for database deletion will be written *after* the call
// to "prepareDropDatabase" returns
int MMFilesEngine::prepareDropDatabase(TRI_vocbase_t* vocbase) {
  return saveDatabaseParameters(vocbase->_id, vocbase->name(), true);
}

// perform a physical deletion of the database      
int MMFilesEngine::dropDatabase(TRI_vocbase_t* vocbase) {
  return dropDatabaseDirectory(databaseDirectory(vocbase->_id));
}

/// @brief wait until a database directory disappears
int MMFilesEngine::waitUntilDeletion(TRI_voc_tick_t id, bool force) {
  std::string const path = databaseDirectory(id);
  
  int iterations = 0;
  // wait for at most 30 seconds for the directory to be removed
  while (TRI_IsDirectory(path.c_str())) {
    if (iterations == 0) {
      LOG(TRACE) << "waiting for deletion of database directory '" << path << "'";
    } else if (iterations >= 30 * 20) {
      LOG(WARN) << "unable to remove database directory '" << path << "'";

      if (force) {
        LOG(WARN) << "forcefully deleting database directory '" << path << "'";
        return dropDatabaseDirectory(path);
      }
      return TRI_ERROR_INTERNAL;
    }

    if (iterations == 5 * 20) {
      LOG(INFO) << "waiting for deletion of database directory '" << path << "'";
    }

    ++iterations;
    usleep(50000);
  }

  return TRI_ERROR_NO_ERROR;
}


// asks the storage engine to create a collection as specified in the VPack
// Slice object and persist the creation info. It is guaranteed by the server 
// that no other active collection with the same name and id exists in the same
// database when this function is called. If this operation fails somewhere in 
// the middle, the storage engine is required to fully clean up the creation 
// and throw only then, so that subsequent collection creation requests will not fail.
// the WAL entry for the collection creation will be written *after* the call
// to "createCollection" returns
void MMFilesEngine::createCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                     arangodb::velocypack::Slice const& data) {
}

// asks the storage engine to drop the specified collection and persist the 
// deletion info. Note that physical deletion of the collection data must not 
// be carried out by this call, as there may
// still be readers of the collection's data. It is recommended that this operation
// only sets a deletion flag for the collection but let's an async task perform
// the actual deletion.
// the WAL entry for collection deletion will be written *after* the call
// to "dropCollection" returns
void MMFilesEngine::dropCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id, 
                                   std::function<bool()> const& canRemovePhysically) {
}

// asks the storage engine to rename the collection as specified in the VPack
// Slice object and persist the renaming info. It is guaranteed by the server 
// that no other active collection with the same name and id exists in the same
// database when this function is called. If this operation fails somewhere in 
// the middle, the storage engine is required to fully revert the rename operation
// and throw only then, so that subsequent collection creation/rename requests will 
// not fail. the WAL entry for the rename will be written *after* the call
// to "renameCollection" returns
void MMFilesEngine::renameCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                     arangodb::velocypack::Slice const& data) {
}

// asks the storage engine to change properties of the collection as specified in 
// the VPack Slice object and persist them. If this operation fails 
// somewhere in the middle, the storage engine is required to fully revert the 
// property changes and throw only then, so that subsequent operations will not fail.
// the WAL entry for the propery change will be written *after* the call
// to "changeCollection" returns
void MMFilesEngine::changeCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                     arangodb::velocypack::Slice const& data) {
}

// asks the storage engine to create an index as specified in the VPack
// Slice object and persist the creation info. The database id, collection id 
// and index data are passed in the Slice object. Note that this function
// is not responsible for inserting the individual documents into the index.
// If this operation fails somewhere in the middle, the storage engine is required 
// to fully clean up the creation and throw only then, so that subsequent index 
// creation requests will not fail.
// the WAL entry for the index creation will be written *after* the call
// to "createIndex" returns
void MMFilesEngine::createIndex(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) {
}

// asks the storage engine to drop the specified index and persist the deletion 
// info. Note that physical deletion of the index must not be carried out by this call, 
// as there may still be users of the index. It is recommended that this operation
// only sets a deletion flag for the index but let's an async task perform
// the actual deletion.
// the WAL entry for index deletion will be written *after* the call
// to "dropIndex" returns
void MMFilesEngine::dropIndex(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                              TRI_idx_iid_t id) {
}

// iterate all documents of the underlying collection
// this is called when a collection is openend, and all its documents need to be added to
// indexes etc.
void MMFilesEngine::iterateDocuments(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                     std::function<void(arangodb::velocypack::Slice const&)> const& cb) {
}

// adds a document to the storage engine
// this will be called by the WAL collector when surviving documents are being moved
// into the storage engine's realm
void MMFilesEngine::addDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                        arangodb::velocypack::Slice const& document) {
}

// removes a document from the storage engine
// this will be called by the WAL collector when non-surviving documents are being removed
// from the storage engine's realm
void MMFilesEngine::removeDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                           arangodb::velocypack::Slice const& document) {
}

/// @brief scans a collection and locates all files
MMFilesEngineCollectionFiles MMFilesEngine::scanCollectionDirectory(std::string const& path) {
  LOG_TOPIC(TRACE, Logger::DATAFILES) << "scanning collection directory '"
                                      << path << "'";

  MMFilesEngineCollectionFiles structure;

  // check files within the directory
  std::vector<std::string> files = TRI_FilesDirectory(path.c_str());

  for (auto const& file : files) {
    std::vector<std::string> parts = StringUtils::split(file, '.');

    if (parts.size() < 2 || parts.size() > 3 || parts[0].empty()) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string filename = FileUtils::buildFilename(path, file);
    std::string extension = parts[1];
    std::string isDead = (parts.size() > 2) ? parts[2] : "";

    std::vector<std::string> next = StringUtils::split(parts[0], "-");

    if (next.size() < 2) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string filetype = next[0];
    next.erase(next.begin());
    std::string qualifier = StringUtils::join(next, '-');

    // file is dead
    if (!isDead.empty()) {
      if (isDead == "dead") {
        FileUtils::remove(filename);
      } else {
        LOG_TOPIC(DEBUG, Logger::DATAFILES)
            << "ignoring file '" << file
            << "' because it does not look like a datafile";
      }

      continue;
    }

    // file is an index
    if (filetype == "index" && extension == "json") {
      structure.indexes.emplace_back(filename);
      continue;
    }

    // file is a journal or datafile
    if (extension == "db") {
      // file is a journal
      if (filetype == "journal") {
        structure.journals.emplace_back(filename);
      }

      // file is a datafile
      else if (filetype == "datafile") {
        structure.datafiles.emplace_back(filename);
      }

      // file is a left-over compaction file. rename it back
      else if (filetype == "compaction") {
        std::string relName = "datafile-" + qualifier + "." + extension;
        std::string newName = FileUtils::buildFilename(path, relName);

        if (FileUtils::exists(newName)) {
          // we have a compaction-xxxx and a datafile-xxxx file. we'll keep
          // the datafile

          FileUtils::remove(filename);

          LOG_TOPIC(WARN, Logger::DATAFILES)
              << "removing left-over compaction file '" << filename << "'";

          continue;
        } else {
          // this should fail, but shouldn't do any harm either...
          FileUtils::remove(newName);

          // rename the compactor to a datafile
          int res = TRI_RenameFile(filename.c_str(), newName.c_str());

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_TOPIC(ERR, Logger::DATAFILES)
                << "unable to rename compaction file '" << filename << "'";
            continue;
          }
        }

        structure.datafiles.emplace_back(filename);
      }

      // temporary file, we can delete it!
      else if (filetype == "temp") {
        LOG_TOPIC(WARN, Logger::DATAFILES)
            << "found temporary file '" << filename
            << "', which is probably a left-over. deleting it";
        FileUtils::remove(filename);
      }

      // ups, what kind of file is that
      else {
        LOG_TOPIC(ERR, Logger::DATAFILES) << "unknown datafile type '" << file
                                          << "'";
      }
    }
  }

  // now sort the files in the structures that we created.
  // the sorting allows us to iterate the files in the correct order
  std::sort(structure.journals.begin(), structure.journals.end(), DatafileIdStringComparator());
  std::sort(structure.compactors.begin(), structure.compactors.end(), DatafileIdStringComparator());
  std::sort(structure.datafiles.begin(), structure.datafiles.end(), DatafileIdStringComparator());
  std::sort(structure.indexes.begin(), structure.indexes.end(), DatafileIdStringComparator());

  return structure;
}
  
void MMFilesEngine::verifyDirectories() {
  if (!TRI_IsDirectory(_basePath.c_str())) {
    LOG(ERR) << "database path '" << _basePath << "' is not a directory";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_INVALID);
  }

  if (!TRI_IsWritable(_basePath.c_str())) {
    // database directory is not writable for the current user... bad luck
    LOG(ERR) << "database directory '" << _basePath
             << "' is not writable for current user";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
  }

  // verify existence of "databases" subdirectory
  if (!TRI_IsDirectory(_databasePath.c_str())) {
    long systemError;
    std::string errorMessage;
    int res = TRI_CreateDirectory(_databasePath.c_str(), systemError, errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to create database directory '"
               << _databasePath << "': " << errorMessage;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
    }
  }

  if (!TRI_IsWritable(_databasePath.c_str())) {
    LOG(ERR) << "database directory '" << _databasePath << "' is not writable";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
  }
}

/// @brief get the names of all databases 
std::vector<std::string> MMFilesEngine::getDatabaseNames() const {
  std::vector<std::string> databases;

  for (auto const& name : TRI_FilesDirectory(_databasePath.c_str())) {
    TRI_ASSERT(!name.empty());

    if (!basics::StringUtils::isPrefix(name, "database-")) {
      // found some other file
      continue;
    }

    // found a database name
    std::string const dname(arangodb::basics::FileUtils::buildFilename(_databasePath, name));

    if (TRI_IsDirectory(dname.c_str())) {
      databases.emplace_back(name);
    }
  }

  // sort by id
  std::sort(databases.begin(), databases.end(), DatabaseIdStringComparator());

  return databases;
}


/// @brief create a new database directory 
int MMFilesEngine::createDatabaseDirectory(TRI_voc_tick_t id,
                                           std::string const& name) {
  std::string const dirname = databaseDirectory(id);

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

  res = saveDatabaseParameters(id, name, false);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // finally remove the .tmp file
  {
    std::string const tmpfile(arangodb::basics::FileUtils::buildFilename(dirname, ".tmp"));
    TRI_UnlinkFile(tmpfile.c_str());
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief save a parameter.json file for a database
int MMFilesEngine::saveDatabaseParameters(TRI_voc_tick_t id, 
                                          std::string const& name,
                                          bool deleted) {
  TRI_ASSERT(id > 0);
  TRI_ASSERT(!name.empty());

  VPackBuilder builder = databaseToVelocyPack(id, name, deleted);
  std::string const file = parametersFile(id);

  if (!arangodb::basics::VelocyPackHelper::velocyPackToFile(
          file.c_str(), builder.slice(), true)) {
    LOG(ERR) << "cannot save database information in file '" << file << "'";
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}
  
VPackBuilder MMFilesEngine::databaseToVelocyPack(TRI_voc_tick_t id, 
                                                 std::string const& name, 
                                                 bool deleted) const {
  TRI_ASSERT(id > 0);
  TRI_ASSERT(!name.empty());

  VPackBuilder builder;
  builder.openObject();
  builder.add("id", VPackValue(std::to_string(id)));
  builder.add("name", VPackValue(name));
  builder.add("deleted", VPackValue(deleted));
  builder.close();

  return builder;
}

std::string MMFilesEngine::databaseDirectory(TRI_voc_tick_t id) const {
  return _databasePath + "database-" + std::to_string(id);
}

std::string MMFilesEngine::parametersFile(TRI_voc_tick_t id) const {
  return basics::FileUtils::buildFilename(databaseDirectory(id), TRI_VOC_PARAMETER_FILE);
}

/// @brief open an existing database. internal function
TRI_vocbase_t* MMFilesEngine::openExistingDatabase(TRI_voc_tick_t id, std::string const& name, 
                                                   bool wasCleanShutdown, bool isUpgrade) {
  auto vocbase = std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id, name);

  // scan the database path for collections
  VPackBuilder builder;
  int res = getCollectionsAndIndexes(vocbase.get(), builder, wasCleanShutdown, isUpgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  VPackSlice slice = builder.slice();
  TRI_ASSERT(slice.isArray());

  for (auto const& it : VPackArrayIterator(slice)) {
    arangodb::VocbaseCollectionInfo info(vocbase.get(), it.get("name").copyString(), it, true);
    
    // we found a collection that is still active
    std::string const directory = it.get("path").copyString();
    TRI_vocbase_col_t* c = nullptr;

    try {
      c = StorageEngine::registerCollection(ConditionalWriteLocker::DoLock(), vocbase.get(), info.type(), info.id(), info.name(), info.planId(), directory);
    } catch (...) {
      // if we caught an exception, c is still a nullptr
    }

    if (c == nullptr) {
      LOG(ERR) << "failed to add document collection '" << info.name() << "'";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
    }

    if (!wasCleanShutdown) {
      // iterating markers may be time-consuming. we'll only do it if
      // we have to
      findMaxTickInJournals(directory);
    }

    LOG(DEBUG) << "added document collection '" << info.name() << "'";
  }

  // start cleanup thread
  TRI_ASSERT(vocbase->_cleanupThread == nullptr);
  vocbase->_cleanupThread.reset(new CleanupThread(vocbase.get()));

  if (!vocbase->_cleanupThread->start()) {
    LOG(ERR) << "could not start cleanup thread";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return vocbase.release();
}
      
/// @brief physically erases the database directory
int MMFilesEngine::dropDatabaseDirectory(std::string const& path) {
  return TRI_RemoveDirectory(path.c_str());
}

/// @brief iterate over a set of datafiles, identified by filenames
/// note: the files will be opened and closed
bool MMFilesEngine::iterateFiles(std::vector<std::string> const& files) {
  /// @brief this iterator is called on startup for journal and compactor file
  /// of a collection
  /// it will check the ticks of all markers and update the internal tick
  /// counter accordingly. this is done so we'll not re-assign an already used
  /// tick value
  auto cb = [this](TRI_df_marker_t const* marker, TRI_datafile_t* datafile) -> bool {
    TRI_voc_tick_t markerTick = marker->getTick();
  
    if (markerTick > _maxTick) {
      _maxTick = markerTick;
    }
    return true;
  };

  for (auto const& filename : files) {
    LOG(DEBUG) << "iterating over collection journal file '" << filename << "'";

    TRI_datafile_t* datafile = TRI_OpenDatafile(filename.c_str(), true);

    if (datafile != nullptr) {
      TRI_IterateDatafile(datafile, cb);
      TRI_CloseDatafile(datafile);
      TRI_FreeDatafile(datafile);
    }
  }

  return true;
}

/// @brief iterate over the markers in the collection's journals
/// this function is called on server startup for all collections. we do this
/// to get the last tick used in a collection
bool MMFilesEngine::findMaxTickInJournals(std::string const& path) {
  LOG(TRACE) << "iterating ticks of journal '" << path << "'";
  MMFilesEngineCollectionFiles structure = scanCollectionDirectory(path);

  if (structure.journals.empty()) {
    // no journal found for collection. should not happen normally, but if
    // it does, we need to grab the ticks from the datafiles, too
    return iterateFiles(structure.datafiles);
  }

  // compactor files don't need to be iterated... they just contain data
  // copied from other files, so their tick values will never be any higher
  return iterateFiles(structure.journals);
}

