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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/build.h"
#include "Basics/encoding.h"
#include "Basics/files.h"
#include "MMFiles/MMFilesAqlFunctions.h"
#include "MMFiles/MMFilesCleanupThread.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesCompactorThread.h"
#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesIndexFactory.h"
#include "MMFiles/MMFilesIncrementalSync.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesOptimizerRules.h"
#include "MMFiles/MMFilesPersistentIndex.h"
#include "MMFiles/MMFilesPersistentIndexFeature.h"
#include "MMFiles/MMFilesRestHandlers.h"
#include "MMFiles/MMFilesTransactionCollection.h"
#include "MMFiles/MMFilesTransactionContextData.h"
#include "MMFiles/MMFilesTransactionManager.h"
#include "MMFiles/MMFilesTransactionState.h"
#include "MMFiles/MMFilesV8Functions.h"
#include "MMFiles/MMFilesView.h"
#include "MMFiles/MMFilesWalRecoveryFeature.h"
#include "MMFiles/mmfiles-replication-dump.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
/// @brief collection meta info filename
static constexpr char const* parametersFilename() { return "parameter.json"; }

/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
/// a number, and type and ending are arbitrary letters
static uint64_t getNumericFilenamePartFromDatafile(
    std::string const& filename) {
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
static uint64_t getNumericFilenamePartFromDatabase(
    std::string const& filename) {
  char const* pos = strrchr(filename.c_str(), '-');

  if (pos == nullptr) {
    return 0;
  }

  return basics::StringUtils::uint64(pos + 1);
}

static uint64_t getNumericFilenamePartFromDatafile(
    MMFilesDatafile const* datafile) {
  return getNumericFilenamePartFromDatafile(datafile->getName());
}

struct DatafileComparator {
  bool operator()(MMFilesDatafile const* lhs,
                  MMFilesDatafile const* rhs) const {
    return getNumericFilenamePartFromDatafile(lhs) <
           getNumericFilenamePartFromDatafile(rhs);
  }
};

/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort database filenames on startup
struct DatafileIdStringComparator {
  bool operator()(std::string const& lhs, std::string const& rhs) const {
    return getNumericFilenamePartFromDatafile(lhs) <
           getNumericFilenamePartFromDatafile(rhs);
  }
};

/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort database filenames on startup
struct DatabaseIdStringComparator {
  bool operator()(std::string const& lhs, std::string const& rhs) const {
    return getNumericFilenamePartFromDatabase(lhs) <
           getNumericFilenamePartFromDatabase(rhs);
  }
};
}

std::string const MMFilesEngine::EngineName("mmfiles");
std::string const MMFilesEngine::FeatureName("MMFilesEngine");

// create the storage engine
MMFilesEngine::MMFilesEngine(application_features::ApplicationServer* server)
    : StorageEngine(server, EngineName, FeatureName, new MMFilesIndexFactory()),
      _isUpgrade(false),
      _maxTick(0) {
  startsAfter("MMFilesPersistentIndex"); // yes, intentional!
    
  server->addFeature(new MMFilesWalRecoveryFeature(server));
  server->addFeature(new MMFilesLogfileManager(server));
  server->addFeature(new MMFilesPersistentIndexFeature(server));
}

MMFilesEngine::~MMFilesEngine() {}

// perform a physical deletion of the database
Result MMFilesEngine::dropDatabase(TRI_vocbase_t* database) {
  // delete persistent indexes for this database
  MMFilesPersistentIndexFeature::dropDatabase(database->id());

  // To shutdown the database (which destroys all LogicalCollection
  // objects of all collections) we need to make sure that the
  // Collector does not interfere. Therefore we execute the shutdown
  // in a phase in which the collector thread does not have any
  // queued operations, a service which it offers:
  auto callback = [&database]() {
    database->shutdown();
    usleep(10000);
  };
  while (
      !MMFilesLogfileManager::instance()->executeWhileNothingQueued(callback)) {
    LOG_TOPIC(TRACE, Logger::FIXME)
        << "Trying to shutdown dropped database, waiting for phase in which "
           "the collector thread does not have queued operations.";
    usleep(500000);
  }
  // stop compactor thread
  shutdownDatabase(database);

  {
    WRITE_LOCKER(locker, _pathsLock);
    _collectionPaths.erase(database->id());
  }

  return dropDatabaseDirectory(databaseDirectory(database->id()));
}

// add the storage engine's specifc options to the global list of options
void MMFilesEngine::collectOptions(std::shared_ptr<options::ProgramOptions>) {}

// validate the storage engine's specific options
void MMFilesEngine::validateOptions(std::shared_ptr<options::ProgramOptions>) {}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void MMFilesEngine::prepare() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);

  // get base path from DatabaseServerFeature
  auto databasePathFeature =
      application_features::ApplicationServer::getFeature<DatabasePathFeature>(
          "DatabasePath");
  _basePath = databasePathFeature->directory();
  _databasePath += databasePathFeature->subdirectoryName("databases");
  if (_databasePath.empty() || _databasePath.back() != TRI_DIR_SEPARATOR_CHAR) {
    _databasePath.push_back(TRI_DIR_SEPARATOR_CHAR);
  }

  TRI_ASSERT(!_basePath.empty());
  TRI_ASSERT(!_databasePath.empty());
}

// initialize engine
void MMFilesEngine::start() {
  if (!isEnabled()) {
    return;
  }

  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);

  // test if the "databases" directory is present and writable
  verifyDirectories();

  // get names of all databases
  std::vector<std::string> names(getDatabaseNames());

  if (names.empty()) {
    // no databases found, i.e. there is no system database!
    // create a database for the system database
    int res =
        createDatabaseDirectory(TRI_NewTickServer(), TRI_VOC_SYSTEM_DATABASE);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "unable to initialize databases: " << TRI_errno_string(res);
      THROW_ARANGO_EXCEPTION(res);
    }
  }
}

// stop the storage engine. this can be used to flush all data to disk,
// shutdown threads etc. it is guaranteed that there will be no read and
// write requests to the storage engine after this call
void MMFilesEngine::stop() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);

  if (!inRecovery()) {
    auto logfileManager = MMFilesLogfileManager::instance();
    logfileManager->flush(true, true, false);
    logfileManager->waitForCollector();
  }
}
  
TransactionManager* MMFilesEngine::createTransactionManager() {
  return new MMFilesTransactionManager();
}

transaction::ContextData* MMFilesEngine::createTransactionContextData() {
  return new MMFilesTransactionContextData();
}

TransactionState* MMFilesEngine::createTransactionState(
    TRI_vocbase_t* vocbase, transaction::Options const& options) {
  return new MMFilesTransactionState(vocbase, options);
}

TransactionCollection* MMFilesEngine::createTransactionCollection(
    TransactionState* state, TRI_voc_cid_t cid, AccessMode::Type accessType,
    int nestingLevel) {
  return new MMFilesTransactionCollection(state, cid, accessType, nestingLevel);
}

// create storage-engine specific collection
PhysicalCollection* MMFilesEngine::createPhysicalCollection(
    LogicalCollection* collection, VPackSlice const& info) {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);
  return new MMFilesCollection(collection, info);
}

// create storage-engine specific view
PhysicalView* MMFilesEngine::createPhysicalView(LogicalView* view,
                                                VPackSlice const& info) {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);
  return new MMFilesView(view, info);
}

void MMFilesEngine::recoveryDone(TRI_vocbase_t* vocbase) {
  DatabaseFeature* databaseFeature =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database");

  if (!databaseFeature->checkVersion() && !databaseFeature->upgrade()) {
    // start compactor thread
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "starting compactor for database '" << vocbase->name() << "'";

    startCompactor(vocbase);
  }

  // delete all collection files from collections marked as deleted
  for (auto& it : _deleted) {
    std::string const& name = it.first;
    std::string const& file = it.second;

    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "collection/view '" << name
                                              << "' was deleted, wiping it";

    int res = TRI_RemoveDirectory(file.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "cannot wipe deleted collection/view '" << name
          << "': " << TRI_errno_string(res);
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

    TRI_voc_tick_t id = getNumericFilenamePartFromDatabase(name);

    if (id == 0) {
      // invalid id
      continue;
    }

    TRI_UpdateTickServer(id);

    // construct and validate path
    std::string const directory(
        basics::FileUtils::buildFilename(_databasePath, name));

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
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "database directory '" << directory
          << "' is not writable for current user";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
    }

    // we have a writable directory...
    std::string const tmpfile(
        basics::FileUtils::buildFilename(directory, ".tmp"));

    if (TRI_ExistsFile(tmpfile.c_str())) {
      // still a temporary... must ignore
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "ignoring temporary directory '" << tmpfile << "'";
      continue;
    }

    // a valid database directory

    // now read data from parameter.json file
    std::string const file = databaseParametersFilename(id);

    if (!TRI_ExistsFile(file.c_str())) {
      // no parameter.json file

      if (TRI_FilesDirectory(directory.c_str()).empty()) {
        // directory is otherwise empty, continue!
        LOG_TOPIC(WARN, arangodb::Logger::FIXME)
            << "ignoring empty database directory '" << directory
            << "' without parameters file";
        continue;
      }

      // abort
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "database directory '" << directory
          << "' does not contain parameters file or parameters file cannot be "
             "read";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "reading database parameters from file '" << file << "'";
    std::shared_ptr<VPackBuilder> builder;
    try {
      builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(file);
    } catch (...) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "database directory '" << directory
          << "' does not contain a valid parameters file";

      // abort
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    VPackSlice parameters = builder->slice();
    std::string const parametersString = parameters.toJson();

    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "database parameters: "
                                              << parametersString;

    VPackSlice idSlice = parameters.get("id");

    if (!idSlice.isString() ||
        id != static_cast<TRI_voc_tick_t>(
                  basics::StringUtils::uint64(idSlice.copyString()))) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "database directory '" << directory
          << "' does not contain a valid parameters file. database id is not a "
             "string";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters,
                                                            "deleted", false)) {
      // database is deleted, skip it!
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
          << "found dropped database in directory '" << directory << "'";
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
          << "removing superfluous database directory '" << directory << "'";

      // delete persistent indexes for this database
      TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
          basics::StringUtils::uint64(idSlice.copyString()));
      MMFilesPersistentIndexFeature::dropDatabase(id);

      dropDatabaseDirectory(directory);
      continue;
    }

    VPackSlice nameSlice = parameters.get("name");

    if (!nameSlice.isString()) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "database directory '" << directory
          << "' does not contain a valid parameters file";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }

    result.add(parameters);
  }

  result.close();
}

// fills the provided builder with information about the collection
void MMFilesEngine::getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                                      arangodb::velocypack::Builder& builder,
                                      bool includeIndexes,
                                      TRI_voc_tick_t maxTick) {
  std::string const path = collectionDirectory(vocbase->id(), id);

  builder.openObject();

  std::shared_ptr<VPackBuilder> fileInfoBuilder =
      arangodb::basics::VelocyPackHelper::velocyPackFromFile(
          basics::FileUtils::buildFilename(path, parametersFilename()));
  builder.add("parameters", fileInfoBuilder->slice());

  if (includeIndexes) {
    // dump index information
    builder.add("indexes", VPackValue(VPackValueType::Array));

    std::vector<std::string> files = TRI_FilesDirectory(path.c_str());

    // sort by index id
    std::sort(files.begin(), files.end(), DatafileIdStringComparator());

    for (auto const& file : files) {
      if (StringUtils::isPrefix(file, "index-") &&
          StringUtils::isSuffix(file, ".json")) {
        std::string const filename =
            basics::FileUtils::buildFilename(path, file);
        std::shared_ptr<VPackBuilder> indexVPack =
            arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);

        VPackSlice const indexSlice = indexVPack->slice();
        VPackSlice const id = indexSlice.get("id");

        if (id.isNumber()) {
          uint64_t iid = id.getNumericValue<uint64_t>();
          if (iid <= static_cast<uint64_t>(maxTick)) {
            // convert "id" to string
            VPackBuilder toMerge;
            {
              VPackObjectBuilder b(&toMerge);
              toMerge.add("id", VPackValue(std::to_string(iid)));
            }
            VPackBuilder mergedBuilder =
                VPackCollection::merge(indexSlice, toMerge.slice(), false);
            builder.add(mergedBuilder.slice());
          }
        } else if (id.isString()) {
          std::string data = id.copyString();
          uint64_t iid = StringUtils::uint64(data);
          if (iid <= static_cast<uint64_t>(maxTick)) {
            builder.add(indexSlice);
          }
        }
      }
    }
    builder.close();
  }

  builder.close();
}

// fill the Builder object with an array of collections (and their corresponding
// indexes) that were detected by the storage engine. called at server start
// only
int MMFilesEngine::getCollectionsAndIndexes(
    TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result,
    bool wasCleanShutdown, bool isUpgrade) {
  result.openArray();

  std::string const path = databaseDirectory(vocbase->id());
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
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "ignoring non-directory '"
                                                << directory << "'";
      continue;
    }

    if (!TRI_IsWritable(directory.c_str())) {
      // the collection directory we found is not writable for the current
      // user. this can cause serious trouble so we will abort the server start
      // if
      // we encounter this situation
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "database subdirectory '" << directory
          << "' is not writable for current user";

      return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
    }
    
    std::vector<std::string> files = TRI_FilesDirectory(directory.c_str());
    if (files.empty()) {
      // the list always contains the empty string as its first element
      // if the list is empty otherwise, this means the directory is also empty and
      // we can ignore it
      LOG_TOPIC(TRACE, Logger::FIXME) << "ignoring empty collection directory '" << directory << "'";
      continue;
    }

    int res = TRI_ERROR_NO_ERROR;

    try {
      LOG_TOPIC(TRACE, Logger::FIXME) << "loading collection info from directory '" << directory << "'";
      VPackBuilder builder = loadCollectionInfo(vocbase, directory);
      VPackSlice info = builder.slice();

      if (VelocyPackHelper::readBooleanValue(info, "deleted", false)) {
        std::string name = VelocyPackHelper::getStringValue(info, "name", "");
        _deleted.emplace_back(std::make_pair(name, directory));
        continue;
      }
      // add collection info
      result.add(info);
    } catch (arangodb::basics::Exception const& e) {
      std::string tmpfile = FileUtils::buildFilename(directory, ".tmp");

      if (TRI_ExistsFile(tmpfile.c_str())) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
            << "ignoring temporary directory '" << tmpfile << "'";
        // temp file still exists. this means the collection was not created
        // fully and needs to be ignored
        continue;  // ignore this directory
      }

      res = e.code();

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot read collection info file in directory '" << directory
          << "': " << TRI_errno_string(res);

      return res;
    }
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

int MMFilesEngine::getViews(TRI_vocbase_t* vocbase,
                            arangodb::velocypack::Builder& result) {
  result.openArray();

  std::string const path = databaseDirectory(vocbase->id());
  std::vector<std::string> files = TRI_FilesDirectory(path.c_str());

  for (auto const& name : files) {
    TRI_ASSERT(!name.empty());

    if (!StringUtils::isPrefix(name, "view-") ||
        StringUtils::isSuffix(name, ".tmp")) {
      // no match, ignore this file
      continue;
    }

    std::string const directory = FileUtils::buildFilename(path, name);

    if (!TRI_IsDirectory(directory.c_str())) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "ignoring non-directory '"
                                                << directory << "'";
      continue;
    }

    if (!TRI_IsWritable(directory.c_str())) {
      // the collection directory we found is not writable for the current
      // user. this can cause serious trouble so we will abort the server start
      // if
      // we encounter this situation
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "database subdirectory '" << directory
          << "' is not writable for current user";

      return TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE;
    }

    int res = TRI_ERROR_NO_ERROR;

    try {
      VPackBuilder builder = loadViewInfo(vocbase, directory);
      VPackSlice info = builder.slice();

      LOG_TOPIC(TRACE, Logger::FIXME) << "got view slice: " << info.toJson();

      if (VelocyPackHelper::readBooleanValue(info, "deleted", false)) {
        std::string name = VelocyPackHelper::getStringValue(info, "name", "");
        _deleted.emplace_back(std::make_pair(name, directory));
        continue;
      }
      // add view info
      result.add(info);
    } catch (arangodb::basics::Exception const& e) {
      std::string tmpfile = FileUtils::buildFilename(directory, ".tmp");

      if (TRI_ExistsFile(tmpfile.c_str())) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
            << "ignoring temporary directory '" << tmpfile << "'";
        // temp file still exists. this means the view was not created
        // fully and needs to be ignored
        continue;  // ignore this directory
      }

      res = e.code();

      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot read view info file in directory '" << directory
          << "': " << TRI_errno_string(res);

      return res;
    }
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

void MMFilesEngine::waitForSync(TRI_voc_tick_t tick) {
  if (application_features::ApplicationServer::isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  MMFilesLogfileManager::instance()->slots()->waitForTick(tick);
}

TRI_vocbase_t* MMFilesEngine::openDatabase(
    arangodb::velocypack::Slice const& args, bool isUpgrade, int& status) {
  VPackSlice idSlice = args.get("id");
  TRI_voc_tick_t id = static_cast<TRI_voc_tick_t>(
      basics::StringUtils::uint64(idSlice.copyString()));
  std::string const name = args.get("name").copyString();

  bool const wasCleanShutdown =
      MMFilesLogfileManager::hasFoundLastTick();
  status = TRI_ERROR_NO_ERROR;
      
  return openExistingDatabase(id, name, wasCleanShutdown, isUpgrade);
}

TRI_vocbase_t* MMFilesEngine::createDatabaseMMFiles(
    TRI_voc_tick_t id, arangodb::velocypack::Slice const& data) {
  std::string const name = data.get("name").copyString();

  int res = 0;
  waitUntilDeletion(id, true, res);

  res = createDatabaseDirectory(id, name);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return openExistingDatabase(id, name, true, false);
}

void MMFilesEngine::prepareDropDatabase(TRI_vocbase_t* vocbase,
                                        bool useWriteMarker, int& status) {
  // signal the compactor thread to finish
  beginShutdownCompactor(vocbase);

  status = saveDatabaseParameters(vocbase->id(), vocbase->name(), true);

  if (status == TRI_ERROR_NO_ERROR) {
    if (useWriteMarker) {
      // TODO: what shall happen in case writeDropMarker() fails?
      writeDropMarker(vocbase->id());
    }
  }
}

/// @brief wait until a database directory disappears
void MMFilesEngine::waitUntilDeletion(TRI_voc_tick_t id, bool force,
                                      int& status) {
  std::string const path = databaseDirectory(id);

  int iterations = 0;
  // wait for at most 30 seconds for the directory to be removed
  while (TRI_IsDirectory(path.c_str())) {
    if (iterations == 0) {
      if (TRI_FilesDirectory(path.c_str()).empty()) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
            << "deleting empty database directory '" << path << "'";
        status = dropDatabaseDirectory(path);
        return;
      }

      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "waiting for deletion of database directory '" << path << "'";
    } else if (iterations >= 30 * 20) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "timed out waiting for deletion of database directory '" << path << "'";

      if (force) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME)
            << "forcefully deleting database directory '" << path << "'";
        status = dropDatabaseDirectory(path);
        return;
      }
      status = TRI_ERROR_INTERNAL;
      return;
    }

    if (iterations == 5 * 20) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME)
          << "waiting for deletion of database directory '" << path << "'";
    }

    ++iterations;
    usleep(50000);
  }

  status = TRI_ERROR_NO_ERROR;
  return;
}

// asks the storage engine to create a collection as specified in the VPack
// Slice object and persist the creation info. It is guaranteed by the server
// that no other active collection with the same name and id exists in the same
// database when this function is called. If this operation fails somewhere in
// the middle, the storage engine is required to fully clean up the creation
// and throw only then, so that subsequent collection creation requests will not
// fail.
// the WAL entry for the collection creation will be written *after* the call
// to "createCollection" returns
std::string MMFilesEngine::createCollection(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
    arangodb::LogicalCollection const* parameters) {
  std::string const path = databasePath(vocbase);

  // sanity check
  if (sizeof(MMFilesDatafileHeaderMarker) + sizeof(MMFilesDatafileFooterMarker) >
      static_cast<MMFilesCollection*>(parameters->getPhysical())->journalSize()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create datafile '" << parameters->name() << "' in '" << path
        << "', journal size '" << static_cast<MMFilesCollection*>(parameters->getPhysical())->journalSize()
        << "' is too small";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATAFILE_FULL);
  }

  if (!TRI_IsDirectory(path.c_str())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create collection '" << path
        << "', database path is not a directory";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_INVALID);
  }

  TRI_ASSERT(id != 0);
  std::string const dirname = createCollectionDirectoryName(path, id);

  registerCollectionPath(vocbase->id(), id, dirname);

  // directory must not exist
  if (TRI_ExistsFile(dirname.c_str())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create collection '" << parameters->name()
        << "' in directory '" << dirname << "': directory already exists";
    THROW_ARANGO_EXCEPTION(
        TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS);
  }

  // use a temporary directory first. this saves us from leaving an empty
  // directory behind, and the server refusing to start
  std::string const tmpname = dirname + ".tmp";

  // create directory
  std::string errorMessage;
  long systemError;
  int res = TRI_CreateDirectory(tmpname.c_str(), systemError, errorMessage);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create collection '" << parameters->name()
        << "' in directory '" << path << "': " << TRI_errno_string(res) << " - "
        << systemError << " - " << errorMessage;
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_IF_FAILURE("CreateCollection::tempDirectory") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // create a temporary file (.tmp)
  std::string const tmpfile(
      arangodb::basics::FileUtils::buildFilename(tmpname, ".tmp"));
  res = TRI_WriteFile(tmpfile.c_str(), "", 0);

  // this file will be renamed to this filename later...
  std::string const tmpfile2(
      arangodb::basics::FileUtils::buildFilename(dirname, ".tmp"));

  TRI_IF_FAILURE("CreateCollection::tempFile") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create collection '" << parameters->name()
        << "' in directory '" << path << "': " << TRI_errno_string(res) << " - "
        << systemError << " - " << errorMessage;
    TRI_RemoveDirectory(tmpname.c_str());
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_IF_FAILURE("CreateCollection::renameDirectory") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  res = TRI_RenameFile(tmpname.c_str(), dirname.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create collection '" << parameters->name()
        << "' in directory '" << path << "': " << TRI_errno_string(res) << " - "
        << systemError << " - " << errorMessage;
    TRI_RemoveDirectory(tmpname.c_str());
    THROW_ARANGO_EXCEPTION(res);
  }

  // now we have the collection directory in place with the correct name and a
  // .tmp file in it

  // delete .tmp file
  TRI_UnlinkFile(tmpfile2.c_str());

  // save the parameters file
  bool const doSync =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database")
          ->forceSyncProperties();
  saveCollectionInfo(vocbase, id, parameters, doSync);

  return dirname;
}

// asks the storage engine to persist the collection.
// After this call the collection is persisted over recovery.
// This call will write wal markers.
arangodb::Result MMFilesEngine::persistCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection) {
  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(vocbase != nullptr);
  if (inRecovery()) {
    // Nothing to do. In recovery we do not write markers.
    return {};
  }
  VPackBuilder builder =
      collection->toVelocyPackIgnore({"path", "statusString"}, true, false);
  VPackSlice const slice = builder.slice();

  auto cid = collection->cid();
  TRI_ASSERT(cid != 0);
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(cid));

  int res = TRI_ERROR_NO_ERROR;

  try {
    MMFilesCollectionMarker marker(TRI_DF_MARKER_VPACK_CREATE_COLLECTION,
                                   vocbase->id(), cid, slice);

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return {};
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "could not save collection create marker in log: "
      << TRI_errno_string(res);

  return {res, TRI_errno_string(res)};
}

// asks the storage engine to drop the specified collection and persist the
// deletion info. Note that physical deletion of the collection data must not
// be carried out by this call, as there may
// still be readers of the collection's data.
// This call will write the WAL entry for collection deletion
arangodb::Result MMFilesEngine::dropCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
  if (inRecovery()) {
    // nothing to do here
    return {};
  }
  int res = TRI_ERROR_NO_ERROR;

  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("id", VPackValue(collection->cid_as_string()));
    builder.add("name", VPackValue(collection->name()));
    builder.close();

    MMFilesCollectionMarker marker(TRI_DF_MARKER_VPACK_DROP_COLLECTION,
                                   vocbase->id(), collection->cid(),
                                   builder.slice());

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "could not save collection drop marker in log: "
        << TRI_errno_string(res);
  }

  return {res, TRI_errno_string(res)};
}

// perform a physical deletion of the collection
// After this call data of this collection is corrupted, only perform if
// assured that no one is using the collection anymore
void MMFilesEngine::destroyCollection(TRI_vocbase_t* vocbase,
                                      arangodb::LogicalCollection* collection) {
  std::string const name(collection->name());
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  unregisterCollectionPath(vocbase->id(), collection->cid());

  // delete persistent indexes
  MMFilesPersistentIndexFeature::dropCollection(vocbase->id(),
                                                collection->cid());

  // rename collection directory
  if (physical->path().empty()) {
    return;
  }

  std::string const collectionPath = physical->path();

#ifdef _WIN32
  size_t pos = collectionPath.find_last_of('\\');
#else
  size_t pos = collectionPath.find_last_of('/');
#endif

  bool invalid = false;

  if (pos == std::string::npos || pos + 1 >= collectionPath.size()) {
    invalid = true;
  }

  std::string path;
  std::string relName;
  if (!invalid) {
    // extract path part
    if (pos > 0) {
      path = collectionPath.substr(0, pos);
    }

    // extract relative filename
    relName = collectionPath.substr(pos + 1);

    if (!StringUtils::isPrefix(relName, "collection-") ||
        StringUtils::isSuffix(relName, ".tmp")) {
      invalid = true;
    }
  }

  if (invalid) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot rename dropped collection '" << name << "': unknown path '"
        << physical->path() << "'";
  } else {
    // prefix the collection name with "deleted-"

    std::string const newFilename = FileUtils::buildFilename(
        path, "deleted-" + relName.substr(std::string("collection-").size()));

    // check if target directory already exists
    if (TRI_IsDirectory(newFilename.c_str())) {
      // remove existing target directory
      TRI_RemoveDirectory(newFilename.c_str());
    }

    // perform the rename
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "renaming collection directory from '" << physical->path()
        << "' to '" << newFilename << "'";

    std::string systemError;
    int res = TRI_RenameFile(physical->path().c_str(), newFilename.c_str(),
                             nullptr, &systemError);

    if (res != TRI_ERROR_NO_ERROR) {
      if (!systemError.empty()) {
        systemError = ", error details: " + systemError;
      }
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot rename directory of dropped collection '" << name
          << "' from '" << physical->path() << "' to '" << newFilename
          << "': " << TRI_errno_string(res) << systemError;
    } else {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "wiping dropped collection '"
                                                << name << "' from disk";

      res = TRI_RemoveDirectory(newFilename.c_str());

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "cannot wipe dropped collection '" << name
            << "' from disk: " << TRI_errno_string(res);
      }
    }
  }
}

// asks the storage engine to change properties of the collection as specified
// in
// the VPack Slice object and persist them. If this operation fails
// somewhere in the middle, the storage engine is required to fully revert the
// property changes and throw only then, so that subsequent operations will not
// fail.
// the WAL entry for the propery change will be written *after* the call
// to "changeCollection" returns
void MMFilesEngine::changeCollection(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
    arangodb::LogicalCollection const* parameters, bool doSync) {
  saveCollectionInfo(vocbase, id, parameters, doSync);
}

// asks the storage engine to persist renaming of a collection
// This will write a renameMarker if not in recovery
Result MMFilesEngine::renameCollection(
    TRI_vocbase_t* vocbase, arangodb::LogicalCollection const* collection,
    std::string const& oldName) {
  if (inRecovery()) {
    // Nothing todo. Marker already there
    return {};
  }
  int res = TRI_ERROR_NO_ERROR;
  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("id", VPackValue(collection->cid_as_string()));
    builder.add("oldName", VPackValue(oldName));
    builder.add("name", VPackValue(collection->name()));
    builder.close();

    MMFilesCollectionMarker marker(TRI_DF_MARKER_VPACK_RENAME_COLLECTION,
                                   vocbase->id(), collection->cid(),
                                   builder.slice());

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    res = TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "could not save collection rename marker in log: "
        << TRI_errno_string(res);
  }
  return {res, TRI_errno_string(res)};
}

void MMFilesEngine::createView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalView const* parameters) {
  std::string const path = databasePath(vocbase);

  if (!TRI_IsDirectory(path.c_str())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create view '" << path
        << "', database path is not a directory";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_INVALID);
  }

  TRI_ASSERT(id != 0);
  std::string const dirname = createViewDirectoryName(path, id);

  registerViewPath(vocbase->id(), id, dirname);

  // directory must not exist
  if (TRI_ExistsFile(dirname.c_str())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create view '" << parameters->name() << "' in directory '"
        << dirname << "': directory already exists";
    THROW_ARANGO_EXCEPTION(
        TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS);  // TODO: change
                                                                // error code
  }

  // use a temporary directory first. this saves us from leaving an empty
  // directory behind, and the server refusing to start
  std::string const tmpname = dirname + ".tmp";

  // create directory
  std::string errorMessage;
  long systemError;
  int res = TRI_CreateDirectory(tmpname.c_str(), systemError, errorMessage);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create view '" << parameters->name() << "' in directory '"
        << path << "': " << TRI_errno_string(res) << " - " << systemError
        << " - " << errorMessage;
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_IF_FAILURE("CreateView::tempDirectory") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // create a temporary file (.tmp)
  std::string const tmpfile(
      arangodb::basics::FileUtils::buildFilename(tmpname, ".tmp"));
  res = TRI_WriteFile(tmpfile.c_str(), "", 0);

  // this file will be renamed to this filename later...
  std::string const tmpfile2(
      arangodb::basics::FileUtils::buildFilename(dirname, ".tmp"));

  TRI_IF_FAILURE("CreateView::tempFile") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create view '" << parameters->name() << "' in directory '"
        << path << "': " << TRI_errno_string(res) << " - " << systemError
        << " - " << errorMessage;
    TRI_RemoveDirectory(tmpname.c_str());
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_IF_FAILURE("CreateView::renameDirectory") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  res = TRI_RenameFile(tmpname.c_str(), dirname.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot create view '" << parameters->name() << "' in directory '"
        << path << "': " << TRI_errno_string(res) << " - " << systemError
        << " - " << errorMessage;
    TRI_RemoveDirectory(tmpname.c_str());
    THROW_ARANGO_EXCEPTION(res);
  }

  // now we have the directory in place with the correct name and a .tmp file in
  // it

  // delete .tmp file
  TRI_UnlinkFile(tmpfile2.c_str());

  // save the parameters file
  bool const doSync =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database")
          ->forceSyncProperties();
  saveViewInfo(vocbase, id, parameters, doSync);
}

arangodb::Result MMFilesEngine::persistView(TRI_vocbase_t* vocbase,
                                            arangodb::LogicalView const* view) {
  TRI_ASSERT(view != nullptr);
  TRI_ASSERT(vocbase != nullptr);
  if (inRecovery()) {
    // Nothing to do. In recovery we do not write markers.
    return {};
  }

  VPackBuilder builder;
  builder.openObject();
  view->toVelocyPack(builder, true, true);
  builder.close();

  VPackSlice const slice = builder.slice();

  auto id = view->id();
  TRI_ASSERT(id != 0);
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

  int res = TRI_ERROR_NO_ERROR;

  try {
    MMFilesViewMarker marker(TRI_DF_MARKER_VPACK_CREATE_VIEW, vocbase->id(), id,
                             slice);

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return {};
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "could not save view create marker in log: " << TRI_errno_string(res);

  return {res, TRI_errno_string(res)};
}

arangodb::Result MMFilesEngine::dropView(TRI_vocbase_t* vocbase,
                                         arangodb::LogicalView* view) {
  if (inRecovery()) {
    // nothing to do here
    return {};
  }
  int res = TRI_ERROR_NO_ERROR;

  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("id", VPackValue(std::to_string(view->id())));
    builder.add("name", VPackValue(view->name()));
    builder.close();

    MMFilesViewMarker marker(TRI_DF_MARKER_VPACK_DROP_VIEW, vocbase->id(),
                             view->id(), builder.slice());

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "could not save view drop marker in log: " << TRI_errno_string(res);
  }

  return {res, TRI_errno_string(res)};
}

void MMFilesEngine::destroyView(TRI_vocbase_t* vocbase,
                                arangodb::LogicalView* view) {
  std::string const name(view->name());
  auto physical = static_cast<MMFilesView*>(view->getPhysical());
  TRI_ASSERT(physical != nullptr);

  // rename view directory
  if (physical->path().empty()) {
    return;
  }

  std::string const viewPath = physical->path();

#ifdef _WIN32
  size_t pos = viewPath.find_last_of('\\');
#else
  size_t pos = viewPath.find_last_of('/');
#endif

  bool invalid = false;

  if (pos == std::string::npos || pos + 1 >= viewPath.size()) {
    invalid = true;
  }

  std::string path;
  std::string relName;
  if (!invalid) {
    // extract path part
    if (pos > 0) {
      path = viewPath.substr(0, pos);
    }

    // extract relative filename
    relName = viewPath.substr(pos + 1);

    if (!StringUtils::isPrefix(relName, "view-") ||
        StringUtils::isSuffix(relName, ".tmp")) {
      invalid = true;
    }
  }

  if (invalid) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot rename dropped view '"
                                            << name << "': unknown path '"
                                            << physical->path() << "'";
  } else {
    // prefix the collection name with "deleted-"

    std::string const newFilename = FileUtils::buildFilename(
        path, "deleted-" + relName.substr(std::string("view-").size()));

    // check if target directory already exists
    if (TRI_IsDirectory(newFilename.c_str())) {
      // remove existing target directory
      TRI_RemoveDirectory(newFilename.c_str());
    }

    // perform the rename
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "renaming view directory from '" << physical->path() << "' to '"
        << newFilename << "'";

    std::string systemError;
    int res = TRI_RenameFile(physical->path().c_str(), newFilename.c_str(),
                             nullptr, &systemError);

    if (res != TRI_ERROR_NO_ERROR) {
      if (!systemError.empty()) {
        systemError = ", error details: " + systemError;
      }
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot rename directory of dropped view '" << name << "' from '"
          << physical->path() << "' to '" << newFilename
          << "': " << TRI_errno_string(res) << systemError;
    } else {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "wiping dropped view '"
                                                << name << "' from disk";

      res = TRI_RemoveDirectory(newFilename.c_str());

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "cannot wipe dropped view '" << name
            << "' from disk: " << TRI_errno_string(res);
      }
    }
  }
}

void MMFilesEngine::saveViewInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                                 arangodb::LogicalView const* view,
                                 bool forceSync) const {
  std::string const filename = viewParametersFilename(vocbase->id(), id);

  VPackBuilder builder;
  builder.openObject();
  view->toVelocyPack(builder, true, true);
  builder.close();

  LOG_TOPIC(TRACE, Logger::FIXME) << "storing view properties in file '" << filename << "': " << builder.slice().toJson();

  bool ok =
      VelocyPackHelper::velocyPackToFile(filename, builder.slice(), forceSync);

  if (!ok) {
    int res = TRI_errno();
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res,
        std::string("cannot save view properties file '") + filename +
            "': " + TRI_errno_string(res));
  }
}

// asks the storage engine to change properties of the view as specified in
// the VPack Slice object and persist them. If this operation fails
// somewhere in the middle, the storage engine is required to fully revert the
// property changes and throw only then, so that subsequent operations will not
// fail.
// the WAL entry for the propery change will be written *after* the call
// to "changeView" returns
void MMFilesEngine::changeView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalView const* view, bool doSync) {
  saveViewInfo(vocbase, id, view, doSync);
}

// asks the storage engine to create an index as specified in the VPack
// Slice object and persist the creation info. The database id, collection id
// and index data are passed in the Slice object. Note that this function
// is not responsible for inserting the individual documents into the index.
// If this operation fails somewhere in the middle, the storage engine is
// required
// to fully clean up the creation and throw only then, so that subsequent index
// creation requests will not fail.
// the WAL entry for the index creation will be written *after* the call
// to "createIndex" returns
void MMFilesEngine::createIndex(TRI_vocbase_t* vocbase,
                                TRI_voc_cid_t collectionId, TRI_idx_iid_t id,
                                arangodb::velocypack::Slice const& data) {
  // construct filename
  std::string const filename = indexFilename(vocbase->id(), collectionId, id);

  // and save
  bool const doSync =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database")
          ->forceSyncProperties();
  bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(filename, data,
                                                                 doSync);

  if (!ok) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot save index definition: "
                                            << TRI_last_error();
    THROW_ARANGO_EXCEPTION(TRI_errno());
  }
}

// asks the storage engine to drop the specified index and persist the deletion
// info. Note that physical deletion of the index must not be carried out by
// this call,
// as there may still be users of the index. It is recommended that this
// operation
// only sets a deletion flag for the index but let's an async task perform
// the actual deletion.
// the WAL entry for index deletion will be written *after* the call
// to "dropIndex" returns
void MMFilesEngine::dropIndex(TRI_vocbase_t* vocbase,
                              TRI_voc_cid_t collectionId, TRI_idx_iid_t id) {
  // construct filename
  std::string const filename = indexFilename(vocbase->id(), collectionId, id);

  int res = TRI_UnlinkFile(filename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "cannot remove index definition in file '" << filename
        << "': " << TRI_errno_string(res);
  }
}

void MMFilesEngine::dropIndexWalMarker(TRI_vocbase_t* vocbase,
                                       TRI_voc_cid_t collectionId,
                                       arangodb::velocypack::Slice const& data,
                                       bool writeMarker, int& error) {
  error = TRI_ERROR_NO_ERROR;
  if (!writeMarker) {
    return;
  }

  try {
    MMFilesCollectionMarker marker(TRI_DF_MARKER_VPACK_DROP_INDEX,
                                   vocbase->id(), collectionId, data);

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);
    error = slotInfo.errorCode;
  } catch (arangodb::basics::Exception const& ex) {
    error = ex.code();
  } catch (...) {
    error = TRI_ERROR_INTERNAL;
  }
}

/// @brief callback for unloading a collection
static bool UnloadCollectionCallback(LogicalCollection* collection) {
  TRI_ASSERT(collection != nullptr);

  WRITE_LOCKER_EVENTUAL(locker, collection->lock());

  if (collection->status() != TRI_VOC_COL_STATUS_UNLOADING) {
    return false;
  }

  auto ditches =
      arangodb::MMFilesCollection::toMMFilesCollection(collection)->ditches();

  if (ditches->contains(arangodb::MMFilesDitch::TRI_DITCH_DOCUMENT) ||
      ditches->contains(arangodb::MMFilesDitch::TRI_DITCH_REPLICATION) ||
      ditches->contains(arangodb::MMFilesDitch::TRI_DITCH_COMPACTION)) {
    locker.unlock();

    // still some ditches left...
    // as the cleanup thread has already popped the unload ditch from the
    // ditches list,
    // we need to insert a new one to really execute the unload
    collection->vocbase()->unloadCollection(collection, false);
    return false;
  }

  int res = collection->close();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to close collection '"
                                            << collection->name() << "': " << TRI_errno_string(res);

    collection->setStatus(TRI_VOC_COL_STATUS_CORRUPTED);
  } else {
    collection->setStatus(TRI_VOC_COL_STATUS_UNLOADED);
  }

  return true;
}

void MMFilesEngine::unloadCollection(TRI_vocbase_t* vocbase,
                                     LogicalCollection* collection) {
  // add callback for unload
  arangodb::MMFilesCollection::toMMFilesCollection(collection)
      ->ditches()
      ->createMMFilesUnloadCollectionDitch(collection, UnloadCollectionCallback,
                                           __FILE__, __LINE__);

  signalCleanup(vocbase);
}

void MMFilesEngine::signalCleanup(TRI_vocbase_t* vocbase) {
  MUTEX_LOCKER(locker, _threadsLock);

  auto it = _cleanupThreads.find(vocbase);

  if (it == _cleanupThreads.end()) {
    return;
  }

  (*it).second->signal();
}

/// @brief scans a collection and locates all files
MMFilesEngineCollectionFiles MMFilesEngine::scanCollectionDirectory(
    std::string const& path) {
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
  std::sort(structure.journals.begin(), structure.journals.end(),
            DatafileIdStringComparator());
  std::sort(structure.compactors.begin(), structure.compactors.end(),
            DatafileIdStringComparator());
  std::sort(structure.datafiles.begin(), structure.datafiles.end(),
            DatafileIdStringComparator());
  std::sort(structure.indexes.begin(), structure.indexes.end(),
            DatafileIdStringComparator());

  return structure;
}

void MMFilesEngine::verifyDirectories() {
  if (!TRI_IsDirectory(_basePath.c_str())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "database path '" << _basePath
                                            << "' is not a directory";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_INVALID);
  }

  if (!TRI_IsWritable(_basePath.c_str())) {
    // database directory is not writable for the current user... bad luck
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "database directory '" << _basePath
        << "' is not writable for current user";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
  }

  // verify existence of "databases" subdirectory
  if (!TRI_IsDirectory(_databasePath.c_str())) {
    long systemError;
    std::string errorMessage;
    int res =
        TRI_CreateDirectory(_databasePath.c_str(), systemError, errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "unable to create database directory '" << _databasePath
          << "': " << errorMessage;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
    }
  }

  if (!TRI_IsWritable(_databasePath.c_str())) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "database directory '" << _databasePath << "' is not writable";

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
    std::string const dname(
        arangodb::basics::FileUtils::buildFilename(_databasePath, name));

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
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "failed to create database directory: " << errorMessage;
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
    std::string const tmpfile(
        arangodb::basics::FileUtils::buildFilename(dirname, ".tmp"));
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
  std::string const file = databaseParametersFilename(id);

  if (!arangodb::basics::VelocyPackHelper::velocyPackToFile(
          file, builder.slice(), true)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot save database information in file '" << file << "'";
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

std::string MMFilesEngine::versionFilename(TRI_voc_tick_t id) const {
  return databaseDirectory(id) + TRI_DIR_SEPARATOR_CHAR + "VERSION";
}

std::string MMFilesEngine::databaseDirectory(TRI_voc_tick_t id) const {
  return _databasePath + "database-" + std::to_string(id);
}

std::string MMFilesEngine::databaseParametersFilename(TRI_voc_tick_t id) const {
  return basics::FileUtils::buildFilename(databaseDirectory(id),
                                          parametersFilename());
}

std::string MMFilesEngine::collectionDirectory(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t id) const {
  READ_LOCKER(locker, _pathsLock);

  auto it = _collectionPaths.find(databaseId);

  if (it == _collectionPaths.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "trying to determine directory for unknown database");
  }

  auto it2 = (*it).second.find(id);

  if (it2 == (*it).second.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "trying to determine directory for unknown collection");
  }
  return (*it2).second;
}

/// @brief build a parameters filename (absolute path)
std::string MMFilesEngine::collectionParametersFilename(
    TRI_voc_tick_t databaseId, TRI_voc_cid_t id) const {
  return basics::FileUtils::buildFilename(collectionDirectory(databaseId, id),
                                          parametersFilename());
}

std::string MMFilesEngine::viewDirectory(TRI_voc_tick_t databaseId,
                                         TRI_voc_cid_t id) const {
  READ_LOCKER(locker, _pathsLock);

  auto it = _viewPaths.find(databaseId);

  if (it == _viewPaths.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "trying to determine directory for unknown database");
  }

  auto it2 = (*it).second.find(id);

  if (it2 == (*it).second.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "trying to determine directory for unknown view");
  }
  return (*it2).second;
}

/// @brief build a parameters filename (absolute path)
std::string MMFilesEngine::viewParametersFilename(TRI_voc_tick_t databaseId,
                                                  TRI_voc_cid_t id) const {
  return basics::FileUtils::buildFilename(viewDirectory(databaseId, id),
                                          parametersFilename());
}

/// @brief build an index filename (absolute path)
std::string MMFilesEngine::indexFilename(TRI_voc_tick_t databaseId,
                                         TRI_voc_cid_t collectionId,
                                         TRI_idx_iid_t id) const {
  return basics::FileUtils::buildFilename(
      collectionDirectory(databaseId, collectionId), indexFilename(id));
}

/// @brief build an index filename (relative path)
std::string MMFilesEngine::indexFilename(TRI_idx_iid_t id) const {
  return std::string("index-") + std::to_string(id) + ".json";
}

/// @brief open an existing database. internal function
TRI_vocbase_t* MMFilesEngine::openExistingDatabase(TRI_voc_tick_t id,
                                                   std::string const& name,
                                                   bool wasCleanShutdown,
                                                   bool isUpgrade) {
  auto vocbase =
      std::make_unique<TRI_vocbase_t>(TRI_VOCBASE_TYPE_NORMAL, id, name);

  // scan the database path for views
  try {
    VPackBuilder builder;
    int res = getViews(vocbase.get(), builder);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());
  
    ViewTypesFeature* viewTypesFeature =
        application_features::ApplicationServer::getFeature<ViewTypesFeature>(
            "ViewTypes");

    for (auto const& it : VPackArrayIterator(slice)) {
      // we found a view that is still active
      LOG_TOPIC(TRACE, Logger::FIXME) << "processing view: " << it.toJson();

      std::string type = it.get("type").copyString();
      // will throw if type is invalid
      ViewCreator& creator = viewTypesFeature->creator(type);

      TRI_ASSERT(!it.get("id").isNone());

      std::shared_ptr<LogicalView> view =
          std::make_shared<arangodb::LogicalView>(vocbase.get(), it);
      
      StorageEngine::registerView(vocbase.get(), view);

      auto physical = static_cast<MMFilesView*>(view->getPhysical());
      TRI_ASSERT(physical != nullptr);

      registerViewPath(vocbase->id(), view->id(), physical->path());

      view->spawnImplementation(creator, it, false);
      view->getImplementation()->open();
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error while opening database views: "
                                            << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "error while opening database views: unknown exception";
    throw;
  }

  // scan the database path for collections
  try {
    VPackBuilder builder;
    int res = getCollectionsAndIndexes(vocbase.get(), builder, wasCleanShutdown,
                                       isUpgrade);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    VPackSlice slice = builder.slice();
    TRI_ASSERT(slice.isArray());

    for (auto const& it : VPackArrayIterator(slice)) {
      LOG_TOPIC(TRACE, Logger::FIXME) << "processing collection: " << it.toJson();

      // we found a collection that is still active
      TRI_ASSERT(!it.get("id").isNone() || !it.get("cid").isNone());
      auto uniqCol =
          std::make_unique<arangodb::LogicalCollection>(vocbase.get(), it);
      auto collection = uniqCol.get();
      TRI_ASSERT(collection != nullptr);
      StorageEngine::registerCollection(vocbase.get(), uniqCol.get());
      // The vocbase has taken over control
      uniqCol.release();

      auto physical =
          static_cast<MMFilesCollection*>(collection->getPhysical());
      TRI_ASSERT(physical != nullptr);

      registerCollectionPath(vocbase->id(), collection->cid(),
                             physical->path());

      if (!wasCleanShutdown) {
        // iterating markers may be time-consuming. we'll only do it if
        // we have to
        LOG_TOPIC(WARN, arangodb::Logger::FIXME)
            << "no shutdown info found. scanning all collection markers in "
               "collection '"
            << collection->name() << "', database '" << vocbase->name() << "'";
        findMaxTickInJournals(physical->path());
      }

      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "added document collection '"
                                                << collection->name() << "'";
    }

    // start cleanup thread
    startCleanup(vocbase.get());

    return vocbase.release();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error while opening database collections: "
                                            << ex.what();
    throw;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "error while opening database collections: unknown exception";
    throw;
  }
}

/// @brief physically erases the database directory
int MMFilesEngine::dropDatabaseDirectory(std::string const& path) {
  // first create a .tmp file in the directory that will help us recover when we
  // crash
  // before the directory deletion is completed
  std::string const tmpfile(
      arangodb::basics::FileUtils::buildFilename(path, ".tmp"));
  // ignore errors from writing this file...
  TRI_WriteFile(tmpfile.c_str(), "", 0);

  return TRI_RemoveDirectoryDeterministic(path.c_str());
}

/// @brief iterate over a set of datafiles, identified by filenames
/// note: the files will be opened and closed
bool MMFilesEngine::iterateFiles(std::vector<std::string> const& files) {
  /// @brief this iterator is called on startup for journal and compactor file
  /// of a collection
  /// it will check the ticks of all markers and update the internal tick
  /// counter accordingly. this is done so we'll not re-assign an already used
  /// tick value
  auto cb = [this](MMFilesMarker const* marker,
                   MMFilesDatafile* datafile) -> bool {
    TRI_voc_tick_t markerTick = marker->getTick();

    if (markerTick > _maxTick) {
      _maxTick = markerTick;
    }
    return true;
  };

  for (auto const& filename : files) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "iterating over collection journal file '" << filename << "'";

    std::unique_ptr<MMFilesDatafile> datafile(
        MMFilesDatafile::open(filename, true));

    if (datafile != nullptr) {
      TRI_IterateDatafile(datafile.get(), cb);
    }
  }

  return true;
}

/// @brief iterate over the markers in the collection's journals
/// this function is called on server startup for all collections. we do this
/// to get the last tick used in a collection
bool MMFilesEngine::findMaxTickInJournals(std::string const& path) {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "iterating ticks of journal '"
                                            << path << "'";
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

/// @brief create a full directory name for a view
std::string MMFilesEngine::createViewDirectoryName(std::string const& basePath,
                                                   TRI_voc_cid_t id) {
  std::string filename("view-");
  filename.append(std::to_string(id));
  filename.push_back('-');
  filename.append(std::to_string(RandomGenerator::interval(UINT32_MAX)));

  return arangodb::basics::FileUtils::buildFilename(basePath, filename);
}

/// @brief create a full directory name for a collection
std::string MMFilesEngine::createCollectionDirectoryName(
    std::string const& basePath, TRI_voc_cid_t cid) {
  std::string filename("collection-");
  filename.append(std::to_string(cid));
  filename.push_back('-');
  filename.append(std::to_string(RandomGenerator::interval(UINT32_MAX)));

  return arangodb::basics::FileUtils::buildFilename(basePath, filename);
}

void MMFilesEngine::registerCollectionPath(TRI_voc_tick_t databaseId,
                                           TRI_voc_cid_t id,
                                           std::string const& path) {
  WRITE_LOCKER(locker, _pathsLock);

  auto it = _collectionPaths.find(databaseId);

  if (it == _collectionPaths.end()) {
    it = _collectionPaths
             .emplace(databaseId,
                      std::unordered_map<TRI_voc_cid_t, std::string>())
             .first;
  }
  (*it).second[id] = path;
}

void MMFilesEngine::unregisterCollectionPath(TRI_voc_tick_t databaseId,
                                             TRI_voc_cid_t id) {
  /*
  WRITE_LOCKER(locker, _pathsLock);

  auto it = _collectionPaths.find(databaseId);

  if (it == _collectionPaths.end()) {
    return;
  }
  (*it).second.erase(id);
  */
}

void MMFilesEngine::registerViewPath(TRI_voc_tick_t databaseId,
                                     TRI_voc_cid_t id,
                                     std::string const& path) {
  WRITE_LOCKER(locker, _pathsLock);

  auto it = _viewPaths.find(databaseId);

  if (it == _viewPaths.end()) {
    it = _viewPaths
             .emplace(databaseId,
                      std::unordered_map<TRI_voc_cid_t, std::string>())
             .first;
  }
  (*it).second[id] = path;
}

void MMFilesEngine::saveCollectionInfo(
    TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
    arangodb::LogicalCollection const* parameters, bool forceSync) const {
  std::string const filename = collectionParametersFilename(vocbase->id(), id);

  VPackBuilder builder =
      parameters->toVelocyPackIgnore({"path", "statusString"}, true, false);
  TRI_ASSERT(id != 0);

  bool ok =
      VelocyPackHelper::velocyPackToFile(filename, builder.slice(), forceSync);

  if (!ok) {
    int res = TRI_errno();
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res,
        std::string("cannot save collection properties file '") + filename +
            "': " + TRI_errno_string(res));
  }
}

VPackBuilder MMFilesEngine::loadCollectionInfo(TRI_vocbase_t* vocbase,
                                               std::string const& path) {
  // find parameter file
  std::string filename =
      arangodb::basics::FileUtils::buildFilename(path, parametersFilename());

  if (!TRI_ExistsFile(filename.c_str())) {
    filename += ".tmp";  // try file with .tmp extension
    if (!TRI_ExistsFile(filename.c_str())) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "collection directory '" << path << " ' does not contain a "
          << "parameters file '" << filename.substr(0, filename.size() - 4) << "'";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }
  }
      
  std::shared_ptr<VPackBuilder> content;
  VPackSlice slice;
  
  try {
    content =
      arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
    slice = content->slice();
  } catch (...) {
    // ignore errors right now but re-throw with the following exception
  }

  if (!slice.isObject()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot open '" << filename
        << "', collection parameters are not readable";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }
      
  if (filename.substr(filename.size() - 4, 4) == ".tmp") {
    // we got a tmp file. Now try saving the original file
    std::string const original(filename.substr(0, filename.size() - 4));
    bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(original,
                                                                   slice, true);

    if (!ok) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot store collection parameters in file '" << original << "'";
    }
  }
  
  // fiddle "isSystem" value, which is not contained in the JSON file
  bool isSystemValue = false;
  if (slice.hasKey("name")) {
    auto name = slice.get("name").copyString();
    if (!name.empty()) {
      isSystemValue = name[0] == '_';
    }
  }

  VPackBuilder patch;
  patch.openObject();
  patch.add("isSystem", VPackValue(isSystemValue));
  patch.add("path", VPackValue(path));

  // auto-magic version detection to disambiguate collections from 3.0 and from
  // 3.1
  if (slice.hasKey("version") && slice.get("version").isNumber() &&
      slice.get("version").getNumber<int>() == LogicalCollection::VERSION_30 &&
      slice.hasKey("allowUserKeys") && slice.hasKey("replicationFactor") &&
      slice.hasKey("numberOfShards")) {
    // these attributes were added to parameter.json in 3.1. so this is a 3.1
    // collection already
    // fix version number
    patch.add("version", VPackValue(LogicalCollection::VERSION_31));
  }

  patch.close();
  VPackBuilder b2 = VPackCollection::merge(slice, patch.slice(), false);
  slice = b2.slice();

  // handle indexes
  std::unordered_set<uint64_t> foundIds;
  VPackBuilder indexesPatch;
  indexesPatch.openObject();
  indexesPatch.add("indexes", VPackValue(VPackValueType::Array));

  // merge indexes into the collection structure
  VPackSlice indexes = slice.get("indexes");
  if (indexes.isArray()) {
    // simply copy over existing index definitions
    for (auto const& it : VPackArrayIterator(indexes)) {
      indexesPatch.add(it);
      VPackSlice id = it.get("id");
      if (id.isString()) {
        foundIds.emplace(basics::StringUtils::uint64(id.copyString()));
      }
    }
  }

  // check files within the directory and find index definitions
  std::vector<std::string> files = TRI_FilesDirectory(path.c_str());

  for (auto const& file : files) {
    std::vector<std::string> parts = StringUtils::split(file, '.');

    if (parts.size() < 2 || parts.size() > 3 || parts[0].empty()) {
      continue;
    }

    std::vector<std::string> next = StringUtils::split(parts[0], "-");
    if (next.size() < 2) {
      continue;
    }

    if (next[0] == "index" && parts[1] == "json") {
      std::string filename =
          arangodb::basics::FileUtils::buildFilename(path, file);
      std::shared_ptr<VPackBuilder> content =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
      VPackSlice indexSlice = content->slice();
      if (!indexSlice.isObject()) {
        // invalid index definition
        continue;
      }

      VPackSlice id = indexSlice.get("id");
      if (id.isString()) {
        auto idxId = basics::StringUtils::uint64(id.copyString());
        if (foundIds.find(idxId) == foundIds.end()) {
          foundIds.emplace(idxId);
          indexesPatch.add(indexSlice);
        }
      }
    }
  }

  indexesPatch.close();
  indexesPatch.close();

  return VPackCollection::merge(slice, indexesPatch.slice(), false);
}

VPackBuilder MMFilesEngine::loadViewInfo(TRI_vocbase_t* vocbase,
                                         std::string const& path) {
  // find parameter file
  std::string filename =
      arangodb::basics::FileUtils::buildFilename(path, parametersFilename());

  if (!TRI_ExistsFile(filename.c_str())) {
    filename += ".tmp";  // try file with .tmp extension
    if (!TRI_ExistsFile(filename.c_str())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
    }
  }

  std::shared_ptr<VPackBuilder> content =
      arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
  VPackSlice slice = content->slice();
  if (!slice.isObject()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot open '" << filename << "', view parameters are not readable";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  if (filename.substr(filename.size() - 4, 4) == ".tmp") {
    // we got a tmp file. Now try saving the original file
    std::string const original(filename.substr(0, filename.size() - 4));
    bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(original,
                                                                   slice, true);

    if (!ok) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot store view parameters in file '" << original << "'";
    }
  }

  VPackBuilder patch;
  patch.openObject();
  patch.add("path", VPackValue(path));
  patch.close();
  return VPackCollection::merge(slice, patch.slice(), false);
}

/// @brief remove data of expired compaction blockers
bool MMFilesEngine::cleanupCompactionBlockers(TRI_vocbase_t* vocbase) {
  // check if we can instantly acquire the lock
  TRY_WRITE_LOCKER(locker, _compactionBlockersLock);

  if (!locker.isLocked()) {
    // couldn't acquire lock
    return false;
  }

  auto it = _compactionBlockers.find(vocbase);

  if (it == _compactionBlockers.end()) {
    // no entry for this database
    return true;
  }

  // we are now holding the write lock
  double now = TRI_microtime();

  size_t n = (*it).second.size();

  for (size_t i = 0; i < n; /* no hoisting */) {
    auto& blocker = (*it).second[i];

    if (blocker._expires < now) {
      (*it).second.erase((*it).second.begin() + i);
      n--;
    } else {
      i++;
    }
  }

  if ((*it).second.empty()) {
    // remove last element
    _compactionBlockers.erase(it);
  }

  return true;
}

/// @brief insert a compaction blocker
int MMFilesEngine::insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl,
                                           TRI_voc_tick_t& id) {
  id = 0;

  if (ttl <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  CompactionBlocker blocker(TRI_NewTickServer(), TRI_microtime() + ttl);

  {
    WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock);

    auto it = _compactionBlockers.find(vocbase);

    if (it == _compactionBlockers.end()) {
      it =
          _compactionBlockers.emplace(vocbase, std::vector<CompactionBlocker>())
              .first;
    }

    (*it).second.emplace_back(blocker);
  }

  id = blocker._id;

  return TRI_ERROR_NO_ERROR;
}

/// @brief touch an existing compaction blocker
int MMFilesEngine::extendCompactionBlocker(TRI_vocbase_t* vocbase,
                                           TRI_voc_tick_t id, double ttl) {
  if (ttl <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock);

  auto it = _compactionBlockers.find(vocbase);

  if (it == _compactionBlockers.end()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  for (auto& blocker : (*it).second) {
    if (blocker._id == id) {
      blocker._expires = TRI_microtime() + ttl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

/// @brief remove an existing compaction blocker
int MMFilesEngine::removeCompactionBlocker(TRI_vocbase_t* vocbase,
                                           TRI_voc_tick_t id) {
  WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock);

  auto it = _compactionBlockers.find(vocbase);

  if (it == _compactionBlockers.end()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  size_t const n = (*it).second.size();

  for (size_t i = 0; i < n; ++i) {
    auto& blocker = (*it).second[i];
    if (blocker._id == id) {
      (*it).second.erase((*it).second.begin() + i);

      if ((*it).second.empty()) {
        // remove last item
        _compactionBlockers.erase(it);
      }
      return TRI_ERROR_NO_ERROR;
    }
  }

  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

void MMFilesEngine::preventCompaction(
    TRI_vocbase_t* vocbase,
    std::function<void(TRI_vocbase_t*)> const& callback) {
  WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock);
  callback(vocbase);
}

bool MMFilesEngine::tryPreventCompaction(
    TRI_vocbase_t* vocbase, std::function<void(TRI_vocbase_t*)> const& callback,
    bool checkForActiveBlockers) {
  TRY_WRITE_LOCKER(locker, _compactionBlockersLock);

  if (locker.isLocked()) {
    if (checkForActiveBlockers) {
      double const now = TRI_microtime();

      // check if we have a still-valid compaction blocker
      auto it = _compactionBlockers.find(vocbase);

      if (it != _compactionBlockers.end()) {
        for (auto const& blocker : (*it).second) {
          if (blocker._expires > now) {
            // found a compaction blocker
            return false;
          }
        }
      }
    }
    callback(vocbase);
    return true;
  }
  return false;
}

int MMFilesEngine::shutdownDatabase(TRI_vocbase_t* vocbase) {
  try {
    stopCompactor(vocbase);
    return stopCleanup(vocbase);
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

// start the cleanup thread for the database
int MMFilesEngine::startCleanup(TRI_vocbase_t* vocbase) {
  std::unique_ptr<MMFilesCleanupThread> thread;

  {
    MUTEX_LOCKER(locker, _threadsLock);

    thread.reset(new MMFilesCleanupThread(vocbase));
    _cleanupThreads.emplace(vocbase, thread.get());
  }

  TRI_ASSERT(thread != nullptr);

  if (!thread->start()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not start cleanup thread";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  thread.release();
  return TRI_ERROR_NO_ERROR;
}

// stop and delete the cleanup thread for the database
int MMFilesEngine::stopCleanup(TRI_vocbase_t* vocbase) {
  MMFilesCleanupThread* thread = nullptr;

  {
    MUTEX_LOCKER(locker, _threadsLock);

    auto it = _cleanupThreads.find(vocbase);

    if (it == _cleanupThreads.end()) {
      // already stopped
      return TRI_ERROR_NO_ERROR;
    }

    thread = (*it).second;
    _cleanupThreads.erase(it);
  }

  TRI_ASSERT(thread != nullptr);

  thread->beginShutdown();
  thread->signal();

  while (thread->isRunning()) {
    usleep(5000);
  }

  delete thread;

  return TRI_ERROR_NO_ERROR;
}

// start the compactor thread for the database
int MMFilesEngine::startCompactor(TRI_vocbase_t* vocbase) {
  std::unique_ptr<MMFilesCompactorThread> thread;

  {
    MUTEX_LOCKER(locker, _threadsLock);

    auto it = _compactorThreads.find(vocbase);

    if (it != _compactorThreads.end()) {
      return TRI_ERROR_INTERNAL;
    }

    thread.reset(new MMFilesCompactorThread(vocbase));
    _compactorThreads.emplace(vocbase, thread.get());
  }

  TRI_ASSERT(thread != nullptr);

  if (!thread->start()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "could not start compactor thread";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  thread.release();
  return TRI_ERROR_NO_ERROR;
}

// signal the compactor thread to stop
int MMFilesEngine::beginShutdownCompactor(TRI_vocbase_t* vocbase) {
  MMFilesCompactorThread* thread = nullptr;

  {
    MUTEX_LOCKER(locker, _threadsLock);

    auto it = _compactorThreads.find(vocbase);

    if (it == _compactorThreads.end()) {
      // already stopped
      return TRI_ERROR_NO_ERROR;
    }

    thread = (*it).second;
  }

  TRI_ASSERT(thread != nullptr);

  thread->beginShutdown();
  thread->signal();

  return TRI_ERROR_NO_ERROR;
}

// stop and delete the compactor thread for the database
int MMFilesEngine::stopCompactor(TRI_vocbase_t* vocbase) {
  MMFilesCompactorThread* thread = nullptr;

  {
    MUTEX_LOCKER(locker, _threadsLock);

    auto it = _compactorThreads.find(vocbase);

    if (it == _compactorThreads.end()) {
      // already stopped
      return TRI_ERROR_NO_ERROR;
    }

    thread = (*it).second;
    _compactorThreads.erase(it);
  }

  TRI_ASSERT(thread != nullptr);

  thread->beginShutdown();
  thread->signal();

  while (thread->isRunning()) {
    usleep(5000);
  }

  delete thread;

  return TRI_ERROR_NO_ERROR;
}

/// @brief: check the initial markers in a datafile
bool MMFilesEngine::checkDatafileHeader(MMFilesDatafile* datafile,
                                        std::string const& filename) const {
  TRI_ASSERT(datafile != nullptr);

  // check the document header
  char const* ptr = datafile->data();

  // skip the datafile header
  ptr += encoding::alignedSize<size_t>(sizeof(MMFilesDatafileHeaderMarker));
  MMFilesCollectionHeaderMarker const* cm =
      reinterpret_cast<MMFilesCollectionHeaderMarker const*>(ptr);

  if (cm->base.getType() != TRI_DF_MARKER_COL_HEADER) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "collection header mismatch in file '" << filename
        << "', expected TRI_DF_MARKER_COL_HEADER, found " << cm->base.getType();
    return false;
  }

  return true;
}

/// @brief checks a collection
int MMFilesEngine::openCollection(TRI_vocbase_t* vocbase,
                                  LogicalCollection* collection,
                                  bool ignoreErrors) {
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  LOG_TOPIC(TRACE, Logger::DATAFILES) << "check collection directory '"
                                      << physical->path() << "'";

  std::vector<MMFilesDatafile*> all;
  std::vector<MMFilesDatafile*> compactors;
  std::vector<MMFilesDatafile*> datafiles;
  std::vector<MMFilesDatafile*> journals;
  std::vector<MMFilesDatafile*> sealed;
  bool stop = false;
  int result = TRI_ERROR_NO_ERROR;

  TRI_ASSERT(collection->cid() != 0);

  // check files within the directory
  std::vector<std::string> files = TRI_FilesDirectory(physical->path().c_str());

  for (auto const& file : files) {
    std::vector<std::string> parts = StringUtils::split(file, '.');

    if (parts.size() < 2 || parts.size() > 3 || parts[0].empty()) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string extension = parts[1];
    std::string isDead = (parts.size() > 2) ? parts[2] : "";

    std::vector<std::string> next = StringUtils::split(parts[0], "-");

    if (next.size() < 2) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string filename = FileUtils::buildFilename(physical->path(), file);
    std::string filetype = next[0];
    next.erase(next.begin());
    std::string qualifier = StringUtils::join(next, '-');

    // .............................................................................
    // file is dead
    // .............................................................................

    if (!isDead.empty() || filetype == "temp") {
      if (isDead == "dead" || filetype == "temp") {
        LOG_TOPIC(TRACE, Logger::DATAFILES)
            << "found temporary file '" << filename
            << "', which is probably a left-over. deleting it";
        FileUtils::remove(filename);
      } else {
        LOG_TOPIC(DEBUG, Logger::DATAFILES)
            << "ignoring file '" << file
            << "' because it does not look like a datafile";
      }
      continue;
    }

    // file is an index. indexes are handled elsewhere
    if (filetype == "index" && extension == "json") {
      continue;
    }

    // file is a journal or datafile, open the datafile
    if (extension == "db") {
      // found a compaction file. now rename it back
      if (filetype == "compaction") {
        std::string relName = "datafile-" + qualifier + "." + extension;
        std::string newName =
            FileUtils::buildFilename(physical->path(), relName);

        if (FileUtils::exists(newName)) {
          // we have a compaction-xxxx and a datafile-xxxx file. we'll keep
          // the datafile
          FileUtils::remove(filename);

          LOG_TOPIC(WARN, Logger::DATAFILES)
              << "removing unfinished compaction file '" << filename << "'";
          continue;
        } else {
          // this should fail, but shouldn't do any harm either...
          FileUtils::remove(newName);

          int res = TRI_RenameFile(filename.c_str(), newName.c_str());

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_TOPIC(ERR, Logger::DATAFILES)
                << "unable to rename compaction file '" << filename << "' to '"
                << newName << "'";
            result = res;
            stop = true;
            break;
          }
        }

        // reuse newName
        filename = std::move(newName);
      }

      TRI_set_errno(TRI_ERROR_NO_ERROR);

      std::unique_ptr<MMFilesDatafile> df(
          MMFilesDatafile::open(filename, ignoreErrors));

      if (df == nullptr) {
        LOG_TOPIC(ERR, Logger::DATAFILES) << "cannot open datafile '"
                                          << filename
                                          << "': " << TRI_last_error();

        result = TRI_errno();
        stop = true;
        break;
      }

      all.emplace_back(df.get());
      MMFilesDatafile* datafile = df.release();

      if (!checkDatafileHeader(datafile, filename)) {
        result = TRI_ERROR_ARANGO_CORRUPTED_DATAFILE;
        stop = true;
        break;
      }

      // file is a journal
      if (filetype == "journal") {
        if (datafile->isSealed()) {
          if (datafile->state() != TRI_DF_STATE_READ) {
            LOG_TOPIC(WARN, Logger::DATAFILES)
                << "strange, journal '" << filename
                << "' is already sealed; must be a left over; will use "
                   "it as datafile";
          }

          sealed.emplace_back(datafile);
        } else {
          journals.emplace_back(datafile);
        }
      }

      // file is a compactor
      else if (filetype == "compactor") {
        // ignore
      }

      // file is a datafile (or was a compaction file)
      else if (filetype == "datafile" || filetype == "compaction") {
        if (!datafile->isSealed()) {
          LOG_TOPIC(DEBUG, Logger::DATAFILES)
              << "datafile '" << filename
              << "' is not sealed, this should not happen under normal circumstances";
        } 
        datafiles.emplace_back(datafile);
      }

      else {
        LOG_TOPIC(ERR, Logger::DATAFILES) << "unknown file '" << file
                                          << "'";
      }
    } else {
      LOG_TOPIC(ERR, Logger::DATAFILES) << "unknown file '" << file << "'";
    }
  }

  // convert the sealed journals into datafiles
  if (!stop) {
    for (auto& datafile : sealed) {
      std::string dname("datafile-" + std::to_string(datafile->fid()) + ".db");
      std::string filename =
          arangodb::basics::FileUtils::buildFilename(physical->path(), dname);

      int res = datafile->rename(filename);

      if (res == TRI_ERROR_NO_ERROR) {
        datafiles.emplace_back(datafile);
        LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
            << "renamed sealed journal to '" << filename << "'";
      } else {
        result = res;
        stop = true;
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "cannot rename sealed journal to '" << filename
            << "': " << TRI_errno_string(res);
        break;
      }
    }
  }

  // stop if necessary
  if (stop) {
    for (auto& datafile : all) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "closing datafile '"
                                                << datafile->getName() << "'";
      delete datafile;
    }

    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }
    return TRI_ERROR_INTERNAL;
  }

  // sort the datafiles
  // this allows us to iterate them in the correct order later
  std::sort(datafiles.begin(), datafiles.end(), DatafileComparator());
  std::sort(journals.begin(), journals.end(), DatafileComparator());
  std::sort(compactors.begin(), compactors.end(), DatafileComparator());

  if (journals.size() > 1) {
    LOG_TOPIC(DEBUG, Logger::FIXME) << "found more than a single journal for collection '" << collection->name() << "'. now turning extra journals into datafiles";

    MMFilesDatafile* journal = journals.back();
    journals.pop_back();
    
    // got more than one journal. now add all the journals but the last one as datafiles
    for (auto& it : journals) {
      std::string dname("datafile-" + std::to_string(it->fid()) + ".db");
      std::string filename =
          arangodb::basics::FileUtils::buildFilename(physical->path(), dname);

      int res = it->rename(filename);

      if (res == TRI_ERROR_NO_ERROR) {
        datafiles.emplace_back(it);
        LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
            << "renamed extra journal to '" << filename << "'";
      } else {
        result = res;
        stop = true;
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "cannot rename extra journal to '" << filename
            << "': " << TRI_errno_string(res);
        break;
      }
    }

    journals.clear();
    journals.emplace_back(journal);

    TRI_ASSERT(journals.size() == 1);

    // sort datafiles again
    std::sort(datafiles.begin(), datafiles.end(), DatafileComparator());
  }
  
  // stop if necessary
  if (stop) {
    for (auto& datafile : all) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "closing datafile '"
                                                << datafile->getName() << "'";
      delete datafile;
    }

    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }
    return TRI_ERROR_INTERNAL;
  }
  
  LOG_TOPIC(DEBUG, Logger::FIXME) << "collection inventory for '" 
                                  << collection->name() << "': datafiles: " 
                                  << datafiles.size() << ", journals: " 
                                  << journals.size() << ", compactors: " 
                                  << compactors.size();
    

  // add the datafiles and journals
  physical->setInitialFiles(std::move(datafiles), std::move(journals), std::move(compactors));

  return TRI_ERROR_NO_ERROR;
}

/// @brief transfer markers into a collection, actual work
/// the collection must have been prepared to call this function
int MMFilesEngine::transferMarkers(LogicalCollection* collection,
                                   MMFilesCollectorCache* cache,
                                   MMFilesOperationsType const& operations) {
  int res = transferMarkersWorker(collection, cache, operations);

  TRI_IF_FAILURE("transferMarkersCrash") {
    // intentionally kill the server
    TRI_SegfaultDebugging("CollectorThreadTransfer");
  }

  if (res == TRI_ERROR_NO_ERROR && !cache->operations->empty()) {
    // now sync the datafile
    res = syncJournalCollection(collection);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  return res;
}

/// @brief Add engine-specific AQL functions.
void MMFilesEngine::addAqlFunctions() {
  MMFilesAqlFunctions::registerResources();
}

/// @brief Add engine-specific optimizer rules
void MMFilesEngine::addOptimizerRules() {
  MMFilesOptimizerRules::registerResources();
}

/// @brief Add engine-specific V8 functions
void MMFilesEngine::addV8Functions() {
  MMFilesV8Functions::registerResources();
}

/// @brief Add engine-specific REST handlers
void MMFilesEngine::addRestHandlers(rest::RestHandlerFactory* handlerFactory) {
  MMFilesRestHandlers::registerResources(handlerFactory);
}

/// @brief transfer markers into a collection, actual work
/// the collection must have been prepared to call this function
int MMFilesEngine::transferMarkersWorker(
    LogicalCollection* collection, MMFilesCollectorCache* cache,
    MMFilesOperationsType const& operations) {
  // used only for crash / recovery tests
  int numMarkers = 0;

  MMFilesCollection* mmfiles =
      static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(mmfiles);
  TRI_voc_tick_t const minTransferTick = mmfiles->maxTick();
  TRI_ASSERT(!operations.empty());

  for (auto it2 = operations.begin(); it2 != operations.end(); ++it2) {
    MMFilesMarker const* source = (*it2);
    TRI_voc_tick_t const tick = source->getTick();

    if (tick <= minTransferTick) {
      // we have already transferred this marker in a previous run, nothing
      // to
      // do
      continue;
    }

    TRI_IF_FAILURE("CollectorThreadTransfer") {
      if (++numMarkers > 5) {
        // intentionally kill the server
        TRI_SegfaultDebugging("CollectorThreadTransfer");
      }
    }

    MMFilesMarkerType const type = source->getType();

    if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
        type == TRI_DF_MARKER_VPACK_REMOVE) {
      TRI_voc_size_t const size = source->getSize();

      char* dst = nextFreeMarkerPosition(collection, tick, type, size, cache);

      if (dst == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      auto& dfi = cache->getDfi(cache->lastFid);
      dfi.numberUncollected++;

      memcpy(dst, source, size);

      finishMarker(reinterpret_cast<char const*>(source), dst, collection, tick,
                   cache);
    }
  }

  TRI_IF_FAILURE("CollectorThreadTransferFinal") {
    // intentionally kill the server
    TRI_SegfaultDebugging("CollectorThreadTransferFinal");
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief get the next position for a marker of the specified size
char* MMFilesEngine::nextFreeMarkerPosition(LogicalCollection* collection,
                                            TRI_voc_tick_t tick,
                                            MMFilesMarkerType type,
                                            TRI_voc_size_t size,
                                            MMFilesCollectorCache* cache) {
  // align the specified size
  size = encoding::alignedSize<TRI_voc_size_t>(size);

  char* dst = nullptr;  // will be modified by reserveJournalSpace()
  MMFilesDatafile* datafile =
      nullptr;  // will be modified by reserveJournalSpace()
  int res = static_cast<MMFilesCollection*>(collection->getPhysical())
                ->reserveJournalSpace(tick, size, dst, datafile);

  if (res != TRI_ERROR_NO_ERROR) {
    // could not reserve space, for whatever reason
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_NO_JOURNAL);
  }

  // if we get here, we successfully reserved space in the datafile

  TRI_ASSERT(datafile != nullptr);

  if (cache->lastFid != datafile->fid()) {
    if (cache->lastFid > 0) {
      // rotated the existing journal... now update the old journal's stats
      auto& dfi = cache->createDfi(cache->lastFid);
      static_cast<MMFilesCollection*>(collection->getPhysical())
          ->_datafileStatistics.increaseUncollected(cache->lastFid,
                                                    dfi.numberUncollected);
      // and reset them afterwards
      dfi.numberUncollected = 0;
    }

    // reset datafile in cache
    cache->lastDatafile = datafile;
    cache->lastFid = datafile->fid();

    // create a local datafile info struct
    cache->createDfi(datafile->fid());

    // we only need the ditches when we are outside the recovery
    // the compactor will not run during recovery
    auto ditch = arangodb::MMFilesCollection::toMMFilesCollection(collection)
                     ->ditches()
                     ->createMMFilesDocumentDitch(false, __FILE__, __LINE__);

    if (ditch == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    cache->addDitch(ditch);
  }

  TRI_ASSERT(dst != nullptr);

  MMFilesDatafileHelper::InitMarker(reinterpret_cast<MMFilesMarker*>(dst),
                                    type, size);

  return dst;
}

/// @brief set the tick of a marker and calculate its CRC value
void MMFilesEngine::finishMarker(char const* walPosition,
                                 char* datafilePosition,
                                 LogicalCollection* collection,
                                 TRI_voc_tick_t tick,
                                 MMFilesCollectorCache* cache) {
  MMFilesMarker* marker =
      reinterpret_cast<MMFilesMarker*>(datafilePosition);

  MMFilesDatafile* datafile = cache->lastDatafile;
  TRI_ASSERT(datafile != nullptr);

  // update ticks
  TRI_UpdateTicksDatafile(datafile, marker);

  MMFilesCollection* mmfiles =
      static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(mmfiles);
  TRI_ASSERT(mmfiles->maxTick() < tick);
  mmfiles->maxTick(tick);

  cache->operations->emplace_back(MMFilesCollectorOperation(
      datafilePosition, marker->getSize(), walPosition, cache->lastFid));
}

/// @brief sync all journals of a collection
int MMFilesEngine::syncJournalCollection(LogicalCollection* collection) {
  TRI_IF_FAILURE("CollectorThread::syncDatafileCollection") {
    return TRI_ERROR_DEBUG;
  }

  return static_cast<MMFilesCollection*>(collection->getPhysical())
      ->syncActiveJournal();
}

/// @brief writes a drop-database marker into the log
int MMFilesEngine::writeDropMarker(TRI_voc_tick_t id) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("id", VPackValue(std::to_string(id)));
    builder.close();

    MMFilesDatabaseMarker marker(TRI_DF_MARKER_VPACK_DROP_DATABASE, id,
                                 builder.slice());

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

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
    LOG_TOPIC(WARN, Logger::FIXME)
        << "could not save drop database marker in log: "
        << TRI_errno_string(res);
  }

  return res;
}

bool MMFilesEngine::inRecovery() {
  return MMFilesLogfileManager::instance()->isInRecovery();
}

/// @brief writes a create-database marker into the log
int MMFilesEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                             VPackSlice const& slice) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    MMFilesDatabaseMarker marker(TRI_DF_MARKER_VPACK_CREATE_DATABASE, id,
                                 slice);
    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

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
    LOG_TOPIC(WARN, Logger::FIXME)
        << "could not save create database marker in log: "
        << TRI_errno_string(res);
  }

  return res;
}
  
std::shared_ptr<arangodb::velocypack::Builder> MMFilesEngine::getReplicationApplierConfiguration(TRI_vocbase_t* vocbase, int& status) {
  std::string const filename = arangodb::basics::FileUtils::buildFilename(databasePath(vocbase), "REPLICATION-APPLIER-CONFIG");

  std::shared_ptr<VPackBuilder> builder;

  if (!TRI_ExistsFile(filename.c_str())) {
    status = TRI_ERROR_FILE_NOT_FOUND;
    return builder;
  }

  try {
    builder = VelocyPackHelper::velocyPackFromFile(filename);
    if (builder->slice().isObject()) {
      status = TRI_ERROR_NO_ERROR;
    } else {
      LOG_TOPIC(ERR, Logger::REPLICATION)
          << "unable to read replication applier configuration from file '"
          << filename << "'";
      status = TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION;
    } 
  } catch (...) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "unable to read replication applier configuration from file '"
        << filename << "'";
    status = TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION;
  }

  return builder;
}

int MMFilesEngine::removeReplicationApplierConfiguration(TRI_vocbase_t* vocbase) {
  std::string const filename = arangodb::basics::FileUtils::buildFilename(databasePath(vocbase), "REPLICATION-APPLIER-CONFIG");

  if (TRI_ExistsFile(filename.c_str())) {
    return TRI_UnlinkFile(filename.c_str());
  }

  return TRI_ERROR_NO_ERROR;
}
 
int MMFilesEngine::saveReplicationApplierConfiguration(TRI_vocbase_t* vocbase, arangodb::velocypack::Slice slice, bool doSync) { 
  std::string const filename = arangodb::basics::FileUtils::buildFilename(databasePath(vocbase), "REPLICATION-APPLIER-CONFIG");

  if (!VelocyPackHelper::velocyPackToFile(filename, slice, doSync)) {
    return TRI_errno();
  } 
  
  return TRI_ERROR_NO_ERROR;
}

int MMFilesEngine::handleSyncKeys(arangodb::InitialSyncer& syncer,
                          arangodb::LogicalCollection* col,
                          std::string const& keysId,
                          std::string const& cid,
                          std::string const& collectionName,
                          TRI_voc_tick_t maxTick,
                          std::string& errorMsg) {
  return handleSyncKeysMMFiles(syncer, col, keysId, cid, collectionName,maxTick, errorMsg);
}
  
Result MMFilesEngine::createLoggerState(TRI_vocbase_t* vocbase, VPackBuilder& builder){
  MMFilesLogfileManagerState const s = MMFilesLogfileManager::instance()->state();
  builder.openObject();  // Base
  // "state" part
  builder.add("state", VPackValue(VPackValueType::Object));  // open
  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(s.lastCommittedTick)));
  builder.add("lastUncommittedLogTick", VPackValue(std::to_string(s.lastAssignedTick)));
  builder.add("totalEvents", VPackValue(static_cast<double>(s.numEvents + s.numEventsSync)));  // s.numEvents + s.numEventsSync
  builder.add("time", VPackValue(s.timeString));
  builder.close();

  // "server" part
  builder.add("server", VPackValue(VPackValueType::Object));  // open
  builder.add("version", VPackValue(ARANGODB_VERSION));
  builder.add("serverId", VPackValue(std::to_string(ServerIdFeature::getId())));
  builder.close();

  // "clients" part
  builder.add("clients", VPackValue(VPackValueType::Array));  // open
  if (vocbase != nullptr) {                                   // add clients
    auto allClients = vocbase->getReplicationClients();
    for (auto& it : allClients) {
      // One client
      builder.add(VPackValue(VPackValueType::Object));
      builder.add("serverId", VPackValue(std::to_string(std::get<0>(it))));

      char buffer[21];
      TRI_GetTimeStampReplication(std::get<1>(it), &buffer[0], sizeof(buffer));
      builder.add("time", VPackValue(buffer));

      builder.add("lastServedTick",
                  VPackValue(std::to_string(std::get<2>(it))));

      builder.close();
    }
  }
  builder.close();  // clients

  builder.close();  // base

  return Result();
}

Result MMFilesEngine::createTickRanges(VPackBuilder& builder) {
    auto const& ranges = MMFilesLogfileManager::instance()->ranges();
    builder.openArray();
    for (auto& it : ranges) {
      builder.openObject();
      //filename and state are already of type string
      builder.add("datafile", VPackValue(it.filename));
      builder.add("state", VPackValue(it.state));
      builder.add("tickMin", VPackValue(std::to_string(it.tickMin)));
      builder.add("tickMax", VPackValue(std::to_string(it.tickMax)));
      builder.close();
    }
    builder.close();
    return Result{};
}

Result MMFilesEngine::firstTick(uint64_t& tick){
  auto const& ranges = MMFilesLogfileManager::instance()->ranges();
  for (auto& it : ranges) {
    if (it.tickMin == 0) {
      continue;
    }
    if (it.tickMin < tick) {
      tick = it.tickMin;
    }
  }
  return Result{};
};

Result MMFilesEngine::lastLogger(TRI_vocbase_t* /*vocbase*/, std::shared_ptr<transaction::Context> transactionContext, uint64_t tickStart, uint64_t tickEnd,  std::shared_ptr<VPackBuilder>& builderSPtr) {
  Result res{};
  std::shared_ptr<transaction::StandaloneContext> scontext =
    std::dynamic_pointer_cast<transaction::StandaloneContext>(transactionContext);
  TRI_ASSERT(scontext);
  MMFilesReplicationDumpContext dump(scontext, 0, true, 0);
  int r = MMFilesDumpLogReplication(&dump, std::unordered_set<TRI_voc_tid_t>(),
                                      0, tickStart, tickEnd, true);
  if (r != TRI_ERROR_NO_ERROR) {
    res.reset(r);
    return res;
  }
  // parsing JSON
  VPackParser parser;
  parser.parse(dump._buffer->_buffer);
  builderSPtr = parser.steal();
  return res;
}
