////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBHotBackup.h"
#include "RocksDBHotBackupCoord.h"

#include <ctype.h>
#include <thread>

#include "Agency/TimeString.h"
#include "ApplicationFeatures/RocksDBOptionFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/TransactionManagerFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionManager.h"

#if USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBHotBackupEE.h"
#endif

#include <rocksdb/utilities/checkpoint.h>

namespace {
arangodb::RocksDBHotBackup* toHotBackup(arangodb::RocksDBHotBackup* obj) {
  return static_cast<arangodb::RocksDBHotBackup*>(obj);
}
} //namespace

namespace arangodb {

static constexpr char const* dirCreatingString = {"CREATING"};
static constexpr char const* dirRestoringString = {"RESTORING"};
static constexpr char const* dirDownloadingString = {"DOWNLOADING"};
static constexpr char const* dirFailsafeString = {"FAILSAFE"};

//
// @brief Serial numbers are used to match asynchronous LockCleaner
//        callbacks to current instance of lock holder
static Mutex serialNumberMutex;
static std::atomic<uint64_t> lockingSerialNumber{0}; // zero when no lock held

static std::atomic<uint64_t> nextSerialNumber{1};
static uint64_t getSerialNumber() {
  uint64_t temp;
  temp = ++nextSerialNumber;
  if (0 == temp) {
    temp = ++nextSerialNumber;
  } // if
  return temp;
} // getSerialNumber


//
// @brief static function to pick proper operation object and then have it
//        parse parameters
//
std::shared_ptr<RocksDBHotBackup> RocksDBHotBackup::operationFactory(
  rest::RequestType type,
  std::vector<std::string> const& suffixes,
  VPackSlice body) {
  std::shared_ptr<RocksDBHotBackup> operation;
  bool isCoord(ServerState::instance()->isCoordinator());

  // initial implementation only has single suffix verbs
  if (1 == suffixes.size()) {
    if (0 == suffixes[0].compare("create")) {
      operation.reset((isCoord ? toHotBackup(new RocksDBHotBackupCreateCoord(body))
                       : toHotBackup(new RocksDBHotBackupCreate(body))));
    } else if (0 == suffixes[0].compare("restore")) {
      operation.reset((isCoord ? toHotBackup(new RocksDBHotBackupRestoreCoord(body))
                       : toHotBackup(new RocksDBHotBackupRestore(body))));
    } else if (0 == suffixes[0].compare("list")) {
      operation.reset((isCoord ? toHotBackup(new RocksDBHotBackupListCoord(body))
                       : toHotBackup(new RocksDBHotBackupList(body))));
    } else if (0 == suffixes[0].compare("lock")) {
      operation.reset((isCoord ? toHotBackup(new RocksDBHotBackupLockCoord(body))
                       : toHotBackup(new RocksDBHotBackupLock(body))));

    }
#if USE_ENTERPRISE
    else if (0 == suffixes[0].compare("upload")) {
      operation.reset((isCoord ? toHotBackup(new RocksDBHotBackupUploadCoord(body))
                       : toHotBackup(new RocksDBHotBackupUpload(body))));
    }
    else if (0 == suffixes[0].compare("download")) {
      operation.reset((isCoord ? toHotBackup(new RocksDBHotBackupDownloadCoord(body))
                       : toHotBackup(new RocksDBHotBackupDownload(body))));
    }
#endif   // USE_ENTERPRISE

  }

  // check the parameter once operation selected
  if (operation) {
    operation->parseParameters(type);
  } else {
    // if no operation selected, give base class which defaults to "bad"
    operation.reset(new RocksDBHotBackup(body));
  } // if

  return operation;

} // RocksDBHotBackup::operationFactory


//
// @brief Setup the base object, default is "bad parameters"
//
RocksDBHotBackup::RocksDBHotBackup(VPackSlice body)
  : _body(body), _valid(false), _success(false), _respCode(rest::ResponseCode::BAD),
    _respError(TRI_ERROR_HTTP_BAD_PARAMETER), _timeoutSeconds(10)
{
  _isSingle = ServerState::instance()->isSingleServer();
  return;
}

std::string RocksDBHotBackup::buildDirectoryPath(std::string const& timestamp, std::string const& userString) {
  std::string suffix = getPersistedId();
  suffix += "_";
  suffix += timestamp;

  if (0 != userString.length()) {
    // limit directory name to 254 characters
    suffix += "_";
    suffix.append(userString, 0, 254 - suffix.size());
  }

  // clean up directory name
  for (auto it = suffix.begin(); suffix.end() != it; ) {
    if (isalnum(*it)) {
      ++it;
    } else if (isspace(*it)) {
      *it = '_';
      ++it;
    } else if ('-' == *it || '_' == *it || '.' == *it) {
      ++it;
    } else if (ispunct(*it)) {
      *it='.';
    } else {
      suffix.erase(it, it + 1);
    } // else
  } // for

  return rebuildPath(suffix);

} // RocksDBHotBackup::buildDirectoryPath


std::string RocksDBHotBackup::rebuildPathPrefix() {
  std::string ret_string = getDatabasePath();
  ret_string += TRI_DIR_SEPARATOR_CHAR;
  ret_string += "hotbackups";

  // This prefix path must exist, ignore errors
  long sysError;
  std::string errorStr;
  // TODO: no error handling here. shall we throw if the directory cannot be created?
  TRI_CreateRecursiveDirectory(ret_string.c_str(), sysError, errorStr);

  return ret_string;

} // RocksDBHotBackup::rebuildPathPrefix


std::string RocksDBHotBackup::rebuildPath(std::string const& suffix) {
  std::string ret_string = rebuildPathPrefix();

  ret_string += TRI_DIR_SEPARATOR_CHAR;
  ret_string += suffix;

  return ret_string;

} // RocksDBHotBackup::rebuildPath


//
// @brief Remove file or dir currently occupying "path"
//
bool RocksDBHotBackup::clearPath(std::string const& path) {
  bool retFlag = true;

  if (basics::FileUtils::exists(path)) {
    if (basics::FileUtils::isDirectory(path)) {
      TRI_RemoveDirectory(path.c_str());
    } else {
      basics::FileUtils::remove(path);
    } // else

    // test if still there and error out?
    if (basics::FileUtils::exists(path)) {
      retFlag = false;
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBHotBackup::clearPath: unable to remove previous " << path;
    } // if
  } // if

  return retFlag;

} // RocksDBHotBackup::clearPath


//
// @brief Common routine for retrieving parameter from request body.
//        Assumes caller has maintain state of _body and _valid
//
void RocksDBHotBackup::getParamValue(char const* key, std::string& value, bool required) {
  VPackSlice tempSlice;

  try {
    if (_body.isObject() && _body.hasKey(key)) {
      tempSlice = _body.get(key);
      value = tempSlice.copyString();
    } else if (required) {
      if (_valid) {
        _result.add(VPackValue(VPackValueType::Object));
        _valid = false;
      }
      _result.add(key, VPackValue("parameter required"));
    } // else if
  } catch (VPackException const& vexcept) {
    if (_valid) {
      _result.add(VPackValue(VPackValueType::Object));
      _valid = false;
    }
    _result.add(key, VPackValue(vexcept.what()));
  }

} // RocksDBHotBackup::getParamValue (std::string)


void RocksDBHotBackup::getParamValue(char const* key, bool& value, bool required) {
  VPackSlice tempSlice;

  try {
    if (_body.isObject() && _body.hasKey(key)) {
      tempSlice = _body.get(key);
      value = tempSlice.getBool();
    } else if (required) {
      if (_valid) {
        _result.add(VPackValue(VPackValueType::Object));
        _valid = false;
      }
      _result.add(key, VPackValue("parameter required"));
    } // else if
  } catch (VPackException const& vexcept) {
    if (_valid) {
      _result.add(VPackValue(VPackValueType::Object));
      _valid = false;
    }
    _result.add(key, VPackValue(vexcept.what()));
  };

} // RocksDBHotBackup::getParamValue (bool)


void RocksDBHotBackup::getParamValue(char const* key, unsigned& value, bool required) {
  VPackSlice tempSlice;

  try {
    if (_body.isObject() && _body.hasKey(key)) {
      tempSlice = _body.get(key);
      value = tempSlice.getUInt();
    } else if (required) {
      if (_valid) {
        _result.add(VPackValue(VPackValueType::Object));
        _valid = false;
      }
      _result.add(key, VPackValue("parameter required"));
    } // else if
  } catch (VPackException const& vexcept) {
    if (_valid) {
      _result.add(VPackValue(VPackValueType::Object));
      _valid = false;
    }
    _result.add(key, VPackValue(vexcept.what()));
  };

} // RocksDBHotBackup::getParamValue (unsigned)

void RocksDBHotBackup::getParamValue(char const* key, VPackSlice& value, bool required) {
  VPackSlice tempSlice;

  try {
    if (_body.isObject() && _body.hasKey(key)) {
      value = _body.get(key);
    } else if (required) {
      if (_valid) {
        _result.add(VPackValue(VPackValueType::Object));
        _valid = false;
      }
      _result.add(key, VPackValue("parameter required"));
    } // else if
  } catch (VPackException const& vexcept) {
    if (_valid) {
      _result.add(VPackValue(VPackValueType::Object));
      _valid = false;
    }
    _result.add(key, VPackValue(vexcept.what()));
  };

}


//
// @brief Wrapper for ServerState::instance()->getPersistedId() to simplify
//        unit testing
//
std::string RocksDBHotBackup::getPersistedId() {

  // single server does not have UUID file by default, might need to force the issue
  if (ServerState::instance()->isSingleServer()) {
    if (!ServerState::instance()->hasPersistedId()) {
      ServerState::instance()->generatePersistedId(ServerState::ROLE_SINGLE);
    } // if
  } // if

  return ServerState::instance()->getPersistedId();

} // RocksDBHotBackup::getPersistedId


//
// @brief Wrapper for getFeature<DatabasePathFeature> to simplify
//        unit testing
//
std::string RocksDBHotBackup::getDatabasePath() {
  // get base path from DatabaseServerFeature
  auto databasePathFeature =
      application_features::ApplicationServer::getFeature<DatabasePathFeature>(
          "DatabasePath");
  return databasePathFeature->directory();

} // RocksDBHotBackup::getDatabasePath


std::string RocksDBHotBackup::getRocksDBPath() {
  std::string engineDir = getDatabasePath();
  engineDir += TRI_DIR_SEPARATOR_CHAR;
  engineDir += "engine-rocksdb";

  return engineDir;

} // RocksDBHotBackup::getRocksDBPath()


bool RocksDBHotBackup::holdRocksDBTransactions() {

  return TransactionManagerFeature::manager()->holdTransactions(_timeoutSeconds * 1000000);

} // RocksDBHotBackup::holdRocksDBTransactions()


/// WARNING:  this wrapper is NOT used in LockCleaner class
void RocksDBHotBackup::releaseRocksDBTransactions() {

  TransactionManagerFeature::manager()->releaseTransactions();

} // RocksDBHotBackup::releaseRocksDBTransactions()


void RocksDBHotBackup::startGlobalShutdown() {

  rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestPriority::LOW, [](bool) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      LOG_TOPIC(INFO, arangodb::Logger::ENGINES)
        << "RocksDBHotBackupRestore:  restarting server with restored data";
      application_features::ApplicationServer::server->beginShutdown();
    });

} // RocksDBHotBackup::startGlobalShutdown


////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupCreate
///        POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupCreate::RocksDBHotBackupCreate(VPackSlice body)
  : RocksDBHotBackup(body), _isCreate(true), _forceBackup(false) {}


void RocksDBHotBackupCreate::parseParameters(rest::RequestType type) {

  _isCreate = (rest::RequestType::POST == type);
  _valid = _isCreate || (rest::RequestType::DELETE_REQ == type);

  if (!_valid) {
    _result.add(VPackValue(VPackValueType::Object));
    _result.add("httpMethod", VPackValue("only POST and DELETE allowed"));
  } // if

  // single server create, we generate the timestamp
  if (_isSingle && _isCreate) {
    _timestamp = timepointToString(std::chrono::system_clock::now());
  } else if (_isCreate) {
    getParamValue("timestamp", _timestamp, true);
  } else {
    getParamValue("directory", _directory, true);
  } // else

  // remaining params are optional
  getParamValue("timeout", _timeoutSeconds, false);
  getParamValue("userString", _userString, false);
  getParamValue("forceBackup", _forceBackup, false);

  //
  // extra validation
  //
  if (_valid) {
    // is timestamp exactly 20 characters?
    // TODO: will this validation be added?
  } // if


  if (!_valid) {
    _result.close();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
  } // if

} // RocksDBHotBackupCreate::parseParameters


// @brief route to independent functions for "create" and "delete"
void RocksDBHotBackupCreate::execute() {

  if (_isCreate) {
    executeCreate();
  } else {
    executeDelete();
  } // else

} // RocksDBHotBackupCreate::execute


/// @brief local function to identify SHA files that need hard link to backup directory
static basics::FileUtils::TRI_copy_recursive_e linkShaFiles(std::string const & name) {
  basics::FileUtils::TRI_copy_recursive_e ret_code(basics::FileUtils::TRI_COPY_IGNORE);

  if (name.length() > 64 && std::string::npos != name.find(".sha.")) {
    ret_code = basics::FileUtils::TRI_COPY_LINK;
  } // if

  return ret_code;

} // linkShaFiles


/// @brief /_admin/hotbackup/create with POST method comes here, initiates
///       a rocksdb checkpoint
void RocksDBHotBackupCreate::executeCreate() {

  // note current time
  // attempt lock on write transactions
  // attempt iResearch flush
  // time remaining, or flag to continue anyway

  std::string dirPathFinal = buildDirectoryPath(_timestamp, _userString);
  std::string dirPathTemp = rebuildPath(dirCreatingString);
  bool flag = clearPath(dirCreatingString);

  rocksdb::Checkpoint * temp_ptr = nullptr;
  rocksdb::Status stat = rocksdb::Checkpoint::Create(rocksutils::globalRocksDB(), &temp_ptr);
  std::unique_ptr<rocksdb::Checkpoint> ptr(temp_ptr);

  bool gotLock = false;

  if (stat.ok() && flag) {

    // make sure the transaction hold is released
    {
      auto guardHold = scopeGuard([&gotLock,this]()
                                  { if (gotLock && _isSingle) releaseRocksDBTransactions(); });

      gotLock = (_isSingle ? holdRocksDBTransactions() : 0 != lockingSerialNumber);

      if (gotLock || _forceBackup) {
        Result res = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->settingsManager()->sync(true);
        EngineSelectorFeature::ENGINE->flushWal(true, true);
        stat = ptr->CreateCheckpoint(dirPathTemp);
        _success = stat.ok();
      } // if
    } // guardHold released

    if (_success) {
      std::string errors;
      long systemError;
      int retVal;

      std::function<basics::FileUtils::TRI_copy_recursive_e(std::string const&)>  filter = linkShaFiles;
      /*_success =*/ basics::FileUtils::copyRecursive(getRocksDBPath(), dirPathTemp,
                                                  filter, errors);

      // now rename
      retVal = TRI_RenameFile(dirPathTemp.c_str(), dirPathFinal.c_str(), &systemError, &errors);
      _success = (TRI_ERROR_NO_ERROR == retVal);
    } // if
  } // if

  // set response codes
  if (_success) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;

    // velocypack loves to throw. wrap it.
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("directory", VPackValue(TRI_Basename(dirPathFinal.c_str())));
      _result.add("forced", VPackValue(!gotLock));
      _result.close();
    } catch (...) {
      _result.clear();
      _success = false;
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HTTP_SERVER_ERROR;
      _errorMessage = "RocksDBHotBackupCreate caught exception.";
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBHotBackupCreate caught exception.";
    } // catch
  } else {
    // stat.ok() means CreateCheckpoint() never called ... so lock issue
    _respCode = stat.ok() ? rest::ResponseCode::REQUEST_TIMEOUT : rest::ResponseCode::EXPECTATION_FAILED;
    _respError = stat.ok() ? TRI_ERROR_LOCK_TIMEOUT : TRI_ERROR_FAILED;
  } //else

} // RocksDBHotBackupCreate::executeCreate


/// @brief /_admin/hotbackup/create with DELETE method comes here, deletes
///        a directory if it exists
///        NOTE: returns success if the requested directory does not exist
///              (was previously deleted)
void RocksDBHotBackupCreate::executeDelete() {
  std::string dirToDelete;

  dirToDelete = rebuildPath(_directory);
  _success = clearPath(dirToDelete);

  // set response codes
  if (_success) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;
  } else {
    _respCode = rest::ResponseCode::NOT_FOUND;
    _respError = TRI_ERROR_FILE_NOT_FOUND;
  } //else

  return;

} // RocksDBHotBackupCreate::executeDelete



////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupRestore
///        POST:  Initiate restore of rocksdb snapshot in place of working directory
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupRestore::RocksDBHotBackupRestore(VPackSlice body)
  : RocksDBHotBackup(body), _saveCurrent(false) {}


/// @brief convert the message payload into class variable options
void RocksDBHotBackupRestore::parseParameters(rest::RequestType type) {

  _valid = (rest::RequestType::POST == type);

  if (!_valid) {
    _result.add(VPackValue(VPackValueType::Object));
    _result.add("httpMethod", VPackValue("only POST allowed"));
  } // if

  // timestamp used for snapshot created of existing database
  //  (in case of rollback or _saveCurrent flag)
  _timestampCurrent = timepointToString(std::chrono::system_clock::now());

  // full directory name of database image to restore (required)
  getParamValue("directory", _directoryRestore, true);

  // remaining params are optional
  getParamValue("saveCurrent", _saveCurrent, false);

  //
  // extra validation
  //
  if (_valid) {
    // does directory exist?
    // TODO: will this validation be added here?
  } // if


  if (!_valid) {
    _result.close();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
  } // if

} // RocksDBHotBackupRestore::parseParameters


/// @brief local function to identify which files to hard link versus copy
static basics::FileUtils::TRI_copy_recursive_e copyVersusLink(std::string const & name) {
  basics::FileUtils::TRI_copy_recursive_e ret_code(basics::FileUtils::TRI_COPY_IGNORE);
  std::string basename(TRI_Basename(name.c_str()));

  if (name.length() > 4 && 0 == name.substr(name.length()-4, 4).compare(".sst")) {
    ret_code = basics::FileUtils::TRI_COPY_LINK;
  } else if (std::string::npos != name.find(".sha.")) {
    ret_code = basics::FileUtils::TRI_COPY_LINK;
  } else if (0 == basename.compare("CURRENT")) {
    ret_code = basics::FileUtils::TRI_COPY_COPY;
  } else if (0 == basename.substr(0,8).compare("MANIFEST")) {
    ret_code = basics::FileUtils::TRI_COPY_COPY;
  } else if (0 == basename.substr(0,7).compare("OPTIONS")) {
    ret_code = basics::FileUtils::TRI_COPY_COPY;
  } // else

  return ret_code;

} // copyVersusLink


/// @brief This external is buried in RestServer/arangod.cpp.
///        Used to perform one last action upon shutdown.
extern std::function<int()> * restartAction;

static std::string restoreExistingPath;  // path of 'engine-rocksdb'
static std::string restoreReplacingPath; // path of restored rocksdb files
static std::string restoreFailsafePath;  // temp location of engine-rocksdb in case of error

static Mutex restoreMutex;

/// @brief Routine called by RestServer/arangod.cpp after everything else
///        shutdown.
static int localRestoreAction() {
  std::string errorStr;
  long systemError;

  /// Step 3.  Save previous dataset just in case
  int retVal = TRI_RenameFile(restoreExistingPath.c_str(), restoreFailsafePath.c_str(), &systemError, &errorStr);
  bool failsafeSet = (TRI_ERROR_NO_ERROR == retVal);

  if (failsafeSet) {
    /// Step 4. shift copy of restoring directory to active database position
    retVal = TRI_RenameFile(restoreReplacingPath.c_str(), restoreExistingPath.c_str(), &systemError, &errorStr);

    if (TRI_ERROR_NO_ERROR != retVal) {
      // failed to move new data into place.  attempt to restore old
      std::cerr << "FATAL: HotBackup restore unable to rename "
                << restoreReplacingPath << " to " << restoreExistingPath
                << "(error code " << retVal << ", " << errorStr << ").";
      TRI_RenameFile(restoreFailsafePath.c_str(), restoreExistingPath.c_str(), &systemError, &errorStr);
    }
  } else {
      std::cerr << "FATAL: HotBackup restore unable to rename "
                << restoreExistingPath << " to " << restoreFailsafePath
                << "(error code " << retVal << ", " << errorStr << ").";
  } // else

  return retVal;

} // localRestoreAction


/// @brief step through the restore procedures
///        (due to redesign, majority of work happens in subroutine createRestoringDirectory())
void RocksDBHotBackupRestore::execute() {
  /// Step 0. Take a global mutex, prevent two restores
  MUTEX_LOCKER (mLock, restoreMutex);

  if (nullptr == restartAction) {
    /// Step 1. create copy of hotbackup to restore
    ///    (restoringDir populated by function)
    ///    (populates error fields if good ==false)
    bool good = createRestoringDirectory(restoreReplacingPath);

    if (good) {
      /// Step 2. initiate shutdown and restart with new data directory
      restoreExistingPath = getRocksDBPath();

      // do we keep the existing dataset forever, generating our standard
      //  directory name plus "before_restore"
      // or put existing dataset in FAILSAFE directory temporarily
      std::string failsafeName;

      if (_saveCurrent) {
        // keep standard named directory
        restoreFailsafePath = buildDirectoryPath(timepointToString(std::chrono::system_clock::now()),
                                                                   "before_restore");
        failsafeName = TRI_Basename(restoreFailsafePath.c_str());
      } else {
        // keep for now in FAILSAFE directory
        failsafeName = dirFailsafeString;
        if (0 == failsafeName.compare(_directoryRestore)) {
          failsafeName += ".1";
        } // if
        restoreFailsafePath = rebuildPath(failsafeName);
      }
      clearPath(restoreFailsafePath);

      restartAction = new std::function<int()>();
      *restartAction = localRestoreAction;
      startGlobalShutdown();
      _success = true;

      try {
        _result.add(VPackValue(VPackValueType::Object));
        _result.add("previous", VPackValue(failsafeName));
        _result.close();
      } catch (...) {
        _result.clear();
        _success = false;
        _respCode = rest::ResponseCode::BAD;
        _respError = TRI_ERROR_HTTP_SERVER_ERROR;
        _errorMessage = "RocksDBHotBackupRestore caught exception.";
        LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
          << "RocksDBHotBackupRestore caught exception.";
      } // catch
    } // if
  } else {
    // restartAction already populated, nothing we can do
    _respCode = rest::ResponseCode::BAD;
    _errorMessage = "restartAction already set.  More than one restore occurring in parallel?";
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "RocksDBHotBackupRestore: " << _errorMessage;
  } // else

} // RocksDBHotBackupRestore::execute


/// @brief clear previous restoring directory and populate new
///        with files from desired hotbackup.
///        restoreDirOutput is created and passed to caller
bool RocksDBHotBackupRestore::createRestoringDirectory(std::string& restoreDirOutput) {
  bool retFlag = true;
  std::string errors, fullDirectoryRestore = rebuildPath(_directoryRestore);

  try {
    // create path name (used here and returned)
    restoreDirOutput = rebuildPath(dirRestoringString);

    // git rid of an old restoring directory/file if it exists
    retFlag = clearPath(restoreDirOutput);

    // now create a new restoring directory
    if (retFlag) {
      retFlag = basics::FileUtils::createDirectory(restoreDirOutput, nullptr);
    } // if

    //  copy contents of selected hotbackup to new "restoring" directory
    //  (both directories must exists)
    if (retFlag) {
      std::function<basics::FileUtils::TRI_copy_recursive_e(std::string const&)>  filter = copyVersusLink;
      retFlag = basics::FileUtils::copyRecursive(fullDirectoryRestore, restoreDirOutput,
                                                 filter, errors);
    } // if
  } catch (...) {
    retFlag = false;
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "createRestoringDirectory caught exception.";
  } // catch

  // set error values
  if (!retFlag) {
    _respError = TRI_ERROR_CANNOT_CREATE_DIRECTORY;
    _respCode = rest::ResponseCode::BAD;
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("failedDirectory", VPackValue(restoreDirOutput.c_str()));
      _result.close();
    } catch (...) {
      _result.clear();
    } // catch
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "RocksDBHotBackupRestore unable to create/populate " << restoreDirOutput
      << " from " << fullDirectoryRestore << " (errors: " << errors << ")";
  } // if

  return retFlag;

} // RocksDBHotBackupRestore::createRestoringDirectory


////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupList
///        POST:  Returns array of Hotbackup directory names
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupList::RocksDBHotBackupList(VPackSlice body)
  : RocksDBHotBackup(body) {}

void RocksDBHotBackupList::parseParameters(rest::RequestType type) {

  _valid = (rest::RequestType::POST == type);

  if (!_valid) {
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("httpMethod", VPackValue("only POST allowed"));
      _result.close();
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
    } catch (...) {
      _result.clear();
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HTTP_SERVER_ERROR;
      _errorMessage = "RocksDBHotBackupList::parseParameters caught exception.";
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBHotBackupList::parseParameters caught exception.";
    } // catch
  } // if

} // RocksDBHotBackupList::parseParameters


// @brief route to independent functions for "create" and "delete"
void RocksDBHotBackupList::execute() {
  std::vector<std::string> hotbackups = TRI_FilesDirectory(rebuildPathPrefix().c_str());

  // remove working directories from list
  std::vector<std::string>::iterator found;
  found = std::find(hotbackups.begin(), hotbackups.end(), dirCreatingString);
  if (hotbackups.end() != found) {
    hotbackups.erase(found);
  }

  found = std::find(hotbackups.begin(), hotbackups.end(), dirRestoringString);
  if (hotbackups.end() != found) {
    hotbackups.erase(found);
  }

  found = std::find(hotbackups.begin(), hotbackups.end(), dirDownloadingString);
  if (hotbackups.end() != found) {
    hotbackups.erase(found);
  }

  // add two failsafe directory name string variables (from different branch)
  found = std::find(hotbackups.begin(), hotbackups.end(), "FAILSAFE");
  if (hotbackups.end() != found) {
    hotbackups.erase(found);
  }

  found = std::find(hotbackups.begin(), hotbackups.end(), "FAILSAFE.1");
  if (hotbackups.end() != found) {
    hotbackups.erase(found);
  }


  try {
    _result.add(VPackValue(VPackValueType::Object));
    _result.add("server", VPackValue(getPersistedId()));
    _result.add("hotbackups", VPackValue(VPackValueType::Array));  // open
    for (auto const& dir : hotbackups) {
      _result.add(VPackValue(dir.c_str()));
    } // for
    _result.close();
    _result.close();
    _success = true;
  } catch (...) {
    _result.clear();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_SERVER_ERROR;
    _errorMessage = "RocksDBHotBackupList::execute caught exception.";
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "RocksDBHotBackupList::execute caught exception.";
  } // catch

} // RocksDBHotBackupList::execute



////////////////////////////////////////////////////////////////////////////////
/// @brief LockCleaner is a helper class to RocksDBHotBackupLock.  It insures
///   that the rocksdb transaction lock is removed if DELETE lock is never called
////////////////////////////////////////////////////////////////////////////////

struct LockCleaner {
  LockCleaner() = delete;
  LockCleaner(uint64_t lockSerialNumber, unsigned timeoutSeconds)
    : _lockSerialNumber(lockSerialNumber)
  {
    _timer.reset(SchedulerFeature::SCHEDULER->newSteadyTimer());
    _timer->expires_after(std::chrono::seconds(timeoutSeconds));
//    std::function<void(const asio_ns::error_code&)> func(this);
    _timer->async_wait(*this);
  };

  void operator()(const asio_ns::error_code& ec) {
    MUTEX_LOCKER (mLock, serialNumberMutex);
    // only unlock if creation of this object instance was due to
    //  the taking of current transaction lock
    if (lockingSerialNumber == _lockSerialNumber) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBHotBackup LockCleaner removing lost transaction lock.";
      // would prefer virtual releaseRocksDBTransactions() ... but would
      //   require copy of RocksDBHotBackupLock object used from RestHandler or unit test.
      TransactionManagerFeature::manager()->releaseTransactions();
      lockingSerialNumber = 0;
    } // if
  } // operator()

  std::shared_ptr<asio_ns::steady_timer> _timer;
  uint64_t _lockSerialNumber;
};



////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupLock
///        POST:  Initiate lock on transactions within rocksdb
///      DELETE:  Remove lock on transactions
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupLock::RocksDBHotBackupLock(const VPackSlice body)
  : RocksDBHotBackup(body), _isLock(false), _unlockTimeoutSeconds(5)
{
}


RocksDBHotBackupLock::~RocksDBHotBackupLock() {
}


void RocksDBHotBackupLock::parseParameters(rest::RequestType const type) {

  _isLock = (rest::RequestType::POST == type);
  _valid = _isLock || (rest::RequestType::DELETE_REQ == type);

  getParamValue("timeout", _timeoutSeconds, false);
  getParamValue("unlockTimeout", _unlockTimeoutSeconds, false);

  if (!_valid) {
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("httpMethod", VPackValue("only POST or DELETE allowed"));
      _result.close();
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
    } catch (...) {
      _result.clear();
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HTTP_SERVER_ERROR;
      _errorMessage = "RocksDBHotBackupLock::parseParameters caught exception.";
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBHotBackupLock::parseParameters caught exception.";
    } // catch
  } // if

  return;

} // RocksDBHotBackupLock::parseParameters


void RocksDBHotBackupLock::execute() {
  MUTEX_LOCKER (mLock, serialNumberMutex);

  if (!_isSingle) {
    if (_isLock) {
      // make sure no one already locked for restore
      if ( 0 == lockingSerialNumber ) {
        _success = holdRocksDBTransactions();

        // prepare emergency lock release in case of coordinator failure
        if (_success) {
          lockingSerialNumber = getSerialNumber();
          // LockCleaner gets copied by async_wait during constructor
          LockCleaner cleaner(lockingSerialNumber, _unlockTimeoutSeconds);
        } else {
          _respCode = rest::ResponseCode::REQUEST_TIMEOUT;
          _respError = TRI_ERROR_LOCK_TIMEOUT;
        } // else
      } else {
        _respCode = rest::ResponseCode::BAD;
        _respError = TRI_ERROR_HTTP_SERVER_ERROR;
        _errorMessage = "RocksDBHotBackupLock: another restore in progress";
      } // else
    } else {
      releaseRocksDBTransactions();
      lockingSerialNumber = 0;
      _success = true;
    }  // else
  } else {
    // single server locks during executeCreate call
    _success = true;
  } // else

  /// return codes
  if (_success) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;
  } // if

  return;

} // RocksDBHotBackupLock::execute


} // namespace arangodb
