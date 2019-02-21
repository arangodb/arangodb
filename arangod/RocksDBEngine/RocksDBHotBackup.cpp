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

#include "Agency/TimeString.h"
#include "ApplicationFeatures/RocksDBOptionFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/TransactionManagerFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionManager.h"

#if USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBHotBackupEE.h"
#endif

#include <rocksdb/utilities/checkpoint.h>

namespace arangodb {

//
// @brief static function to pick proper operation object and then have it
//        parse parameters
//
std::shared_ptr<RocksDBHotBackup> RocksDBHotBackup::operationFactory(
  rest::RequestType const type,
  std::vector<std::string> const & suffixes,
  const VPackSlice body)
{
  std::shared_ptr<RocksDBHotBackup> operation;
  bool isCoord(ServerState::instance()->isCoordinator());

  // initial implementation only has single suffix verbs
  if (1 == suffixes.size()) {
    if (0 == suffixes[0].compare("create")) {
      operation.reset((isCoord ? (RocksDBHotBackup *)new RocksDBHotBackupCreateCoord(body)
                       : (RocksDBHotBackup *)new RocksDBHotBackupCreate(body)));
    } else if (0 == suffixes[0].compare("restore")) {
      operation.reset((isCoord ? (RocksDBHotBackup *)new RocksDBHotBackupRestoreCoord(body)
                       : (RocksDBHotBackup *)new RocksDBHotBackupRestore(body)));
    }
#if USE_ENTERPRISE
    else if (0 == suffixes[0].compare("upload")) {
      operation.reset((isCoord ? (RocksDBHotBackup *)new RocksDBHotBackupUploadCoord(body)
                       : (RocksDBHotBackup *)new RocksDBHotBackupUpload(body)));
    }
    else if (0 == suffixes[0].compare("download")) {
      operation.reset((isCoord ? (RocksDBHotBackup *)new RocksDBHotBackupDownloadCoord(body)
                       : (RocksDBHotBackup *)new RocksDBHotBackupDownload(body)));
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
RocksDBHotBackup::RocksDBHotBackup(const VPackSlice body)
  : _body(body), _valid(false), _success(false), _respCode(rest::ResponseCode::BAD),
    _respError(TRI_ERROR_HTTP_BAD_PARAMETER), _timeoutSeconds(10)
{
  return;
}

//
// @brief
//
RocksDBHotBackup::~RocksDBHotBackup() {};

std::string RocksDBHotBackup::buildDirectoryPath(const std::string & timestamp, const std::string & userString) {
  std::string ret_string, suffix;

  suffix = getPersistedId();
  suffix += "_";
  suffix += timestamp;

  if (0 != userString.length()) {
    // limit directory name to 254 characters
    suffix += "_";
    suffix.append(userString, 0, 254-suffix.size());
  }

  // clean up directory name
  for (auto it=suffix.begin(); suffix.end()!=it; ) {
    if (isalnum(*it)) {
      ++it;
    } else if (isspace(*it)) {
      *it = '_';
      ++it;
    } else if ('-'==*it || '_'==*it || '.'==*it) {
      ++it;
    } else if (ispunct(*it)) {
      *it='.';
    } else {
      suffix.erase(it,it+1);
    } // else
  } // for

  ret_string = rebuildPath(suffix);

  return ret_string;

} // RocksDBHotBackup::buildDirectoryPath


std::string RocksDBHotBackup::rebuildPath(const std::string & suffix) {
  std::string ret_string;

  ret_string = getDatabasePath();
  ret_string += TRI_DIR_SEPARATOR_CHAR;
  ret_string += "hotbackups";

  // This prefix path must exist, ignore errors
  long sysError;
  std::string errorStr;
  TRI_CreateRecursiveDirectory(ret_string.c_str(), sysError, errorStr);

  ret_string += TRI_DIR_SEPARATOR_CHAR;
  ret_string += suffix;

  return ret_string;

} // RocksDBHotBackup::rebuildPath

//
// @brief Common routine for retrieving parameter from request body.
//        Assumes caller has maintain state of _body and _valid
//
void RocksDBHotBackup::getParamValue(const char * key, std::string & value, bool required) {
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
  } catch(VPackException const &vexcept) {
    if (_valid) {
      _result.add(VPackValue(VPackValueType::Object));
      _valid = false;
    }
    _result.add(key, VPackValue(vexcept.what()));
  };

} // RocksDBHotBackup::getParamValue (std::string)


void RocksDBHotBackup::getParamValue(const char * key, bool & value, bool required) {
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
  } catch(VPackException const &vexcept) {
    if (_valid) {
      _result.add(VPackValue(VPackValueType::Object));
      _valid = false;
    }
    _result.add(key, VPackValue(vexcept.what()));
  };

} // RocksDBHotBackup::getParamValue (bool)


void RocksDBHotBackup::getParamValue(const char * key, unsigned & value, bool required) {
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
  } catch(VPackException const &vexcept) {
    if (_valid) {
      _result.add(VPackValue(VPackValueType::Object));
      _valid = false;
    }
    _result.add(key, VPackValue(vexcept.what()));
  };

} // RocksDBHotBackup::getParamValue (unsigned)

void RocksDBHotBackup::getParamValue(const char * key, VPackSlice & value, bool required) {
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
  } catch(VPackException const &vexcept) {
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
  std::string engineDir;

  engineDir = getDatabasePath();
  engineDir += TRI_DIR_SEPARATOR_CHAR;
  engineDir += "engine-rocksdb";

  return engineDir;

} // RocksDBHotBackup::getRocksDBPath()


/// @brief wrapper for easy unit testing
bool RocksDBHotBackup::pauseRocksDB() {

  return rocksutils::globalRocksDB()->pauseRocksDB(std::chrono::seconds(_timeoutSeconds));

} //  RocksDBHotBackup::pauseRocksDB


/// @brief wrapper for easy unit testing
bool RocksDBHotBackup::restartRocksDB() {

  return rocksutils::globalRocksDB()->restartRocksDB();

} //  RocksDBHotBackup::restartRocksDB


bool RocksDBHotBackup::holdRocksDBTransactions() {

  return TransactionManagerFeature::manager()->holdTransactions(_timeoutSeconds * 1000000);

} // RocksDBHotBackup::holdRocksDBTransactions()


void RocksDBHotBackup::releaseRocksDBTransactions() {

  TransactionManagerFeature::manager()->releaseTransactions();

} // RocksDBHotBackup::releaseRocksDBTransactions()


////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupCreate
///        POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupCreate::RocksDBHotBackupCreate(const VPackSlice body)
  : RocksDBHotBackup(body), _isCreate(true), _forceBackup(false)
{
}


RocksDBHotBackupCreate::~RocksDBHotBackupCreate() {
}


void RocksDBHotBackupCreate::parseParameters(rest::RequestType const type) {
  bool isSingle(ServerState::instance()->isSingleServer());

  _isCreate = (rest::RequestType::POST == type);
  _valid = _isCreate || (rest::RequestType::DELETE_REQ == type);

  if (!_valid) {
    _result.add(VPackValue(VPackValueType::Object));
    _result.add("httpMethod", VPackValue("only POST and DELETE allowed"));
  } // if

  // single server create, we generate the timestamp
  if (isSingle && _isCreate) {
    _timestamp = timepointToString(std::chrono::system_clock::now());
  } else {
    getParamValue("timestamp", _timestamp, true);
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
  } // if


  if (!_valid) {
    _result.close();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
  } // if

  return;

} // RocksDBHotBackupCreate::parseParameters


// @brief route to independent functions for "create" and "delete"
void RocksDBHotBackupCreate::execute() {

  if (_isCreate) {
    executeCreate();
  } else {
    executeDelete();
  } // else

  return;

} // RocksDBHotBackupCreate::execute


/// @brief local function to identify SHA files that need hard link to backup directory
static basics::FileUtils::TRI_copy_recursive_e linkShaFiles(std::string const & name) {
  basics::FileUtils::TRI_copy_recursive_e ret_code(basics::FileUtils::TRI_COPY_IGNORE);

  if (name.length() > 64 && std::string::npos != name.find(".sha."))
  {
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

  rocksdb::Checkpoint * ptr(nullptr);
  rocksdb::Status stat;
  std::string dirPath;
  bool gotLock(false);

  stat = rocksdb::Checkpoint::Create(rocksutils::globalRocksDB()->GetRootDB(), &ptr);

  if (stat.ok()) {
    dirPath = buildDirectoryPath(_timestamp, _userString);

    // make sure the transaction hold is released
    {
      auto guardHold = scopeGuard([&gotLock]()
                                  { if (gotLock) TransactionManagerFeature::manager()->releaseTransactions(); });
      // convert timeout from seconds to microseconds
      gotLock = TransactionManagerFeature::manager()->holdTransactions(_timeoutSeconds * 1000000);

      if (gotLock || _forceBackup) {
        EngineSelectorFeature::ENGINE->flushWal(true, true);
        stat = ptr->CreateCheckpoint(dirPath);
        _success = stat.ok();
      } // if
    } // guardHold released

    delete ptr;
    ptr = nullptr;

    if (_success) {
      std::string errors;

      std::function<basics::FileUtils::TRI_copy_recursive_e(std::string const&)>  filter=linkShaFiles;
      /*_success =*/ basics::FileUtils::copyRecursive(getRocksDBPath(), dirPath,
                                                  filter, errors);
    } // if
  } // if

  // set response codes
  if (_success) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;

    // velocypack loves to throw. wrap it.
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("directory", VPackValue(dirPath));
      _result.add("forced", VPackValue(!gotLock));
      _result.close();
    } catch (...) {
    }
  } else {
    // stat.ok() means CreateCheckpoint() never called ... so lock issue
    _respCode = stat.ok() ? rest::ResponseCode::REQUEST_TIMEOUT : rest::ResponseCode::EXPECTATION_FAILED;
    _respError = stat.ok() ? TRI_ERROR_LOCK_TIMEOUT : TRI_ERROR_FAILED;
  } //else

  return;

} // RocksDBHotBackupCreate::executeCreate



////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupRestore
///        POST:  Initiate restore of rocksdb snapshot in place of working directory
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupRestore::RocksDBHotBackupRestore(const VPackSlice body)
  : RocksDBHotBackup(body), _saveCurrent(true), _forceRestore(true)
{
}


RocksDBHotBackupRestore::~RocksDBHotBackupRestore() {
}

/// @brief convert the message payload into class variable options
void RocksDBHotBackupRestore::parseParameters(rest::RequestType const type) {

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
  getParamValue("forceRestore", _forceRestore, false);
  getParamValue("timeout", _timeoutSeconds, false);

  //
  // extra validation
  //
  if (_valid) {
    // does directory exist?
  } // if


  if (!_valid) {
    _result.close();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
  } // if

  return;

} // RocksDBHotBackupRestore::parseParameters


/// @brief local function to identify which files to hard link versus copy
static basics::FileUtils::TRI_copy_recursive_e copyVersusLink(std::string const & name) {
  basics::FileUtils::TRI_copy_recursive_e ret_code(basics::FileUtils::TRI_COPY_IGNORE);
  std::string basename(TRI_Basename(name.c_str()));

  if (name.length() > 4 && 0 == name.substr(name.length()-4, 4).compare(".sst"))
  {
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


/// @brief step through the restore procedures
void RocksDBHotBackupRestore::execute() {
  std::string errors, restoringDir, rocksDBPath;
  bool good = {true}, restoringReady={false}, gotLock={false}, pauseWorked={false};

  rocksDBPath = getRocksDBPath();

  /// 1. create copy of hotbackup to restore
  ///    (restoringDir populated by function)
  restoringReady = createRestoringDirectory(restoringDir);
  good = restoringReady;

  // proceed only if copy completed
  if (good) {
    // make sure the transaction hold is released
    auto guardHold = scopeGuard([&gotLock, this]()
                                { if (gotLock) releaseRocksDBTransactions(); } );
    try {
      /// 2. attempt to stop transactions,
      // convert timeout from seconds to microseconds
      gotLock = holdRocksDBTransactions();

      if (gotLock || _forceRestore) {
        int retVal;
        std::string nowStamp, newDirectory, errorStr;
        long systemError;

        /// 3. stop rocksdb
        pauseWorked = pauseRocksDB();
        if (pauseWorked) {
          /// 4. shift active database to a hotbackup directory
          nowStamp = timepointToString(std::chrono::system_clock::now());
          newDirectory = buildDirectoryPath(nowStamp, "before_restore");
          retVal = TRI_RenameFile(rocksDBPath.c_str(), newDirectory.c_str(), &systemError, &errorStr);

          if (TRI_ERROR_NO_ERROR == retVal) {
            /// 5. shift copy of restoring directory to active database position
            retVal = TRI_RenameFile(restoringDir.c_str(), rocksDBPath.c_str(), &systemError, &errorStr);

            if (TRI_ERROR_NO_ERROR == retVal) {
              /// 6. restart rocksdb
              restartRocksDB();
              _success = true;
            } else {
              // rename to move restoring into position failed.
              good = false;
              _respCode = rest::ResponseCode::BAD;
              _errorMessage = "Unable to rename restore directory into production. (";
              _errorMessage += errorStr;
              _errorMessage += ")";
              LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
                << "RocksDBHotBackupRestore: " << _errorMessage;

              // ... and if this fails too? ...
              retVal = TRI_RenameFile(newDirectory.c_str(), rocksDBPath.c_str(), &systemError, &errorStr);
              if (TRI_ERROR_NO_ERROR != retVal) {
                TRI_ASSERT(false);
                LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
                  << "RocksDBHotBackupRestore: Unable to rename old production back after failure."
                  << errorStr;
              } // if
            } // else
          } else {
            // rename to move production away, failed
            good = false;
            _respCode = rest::ResponseCode::BAD;
            _errorMessage = "Unable to rename existing database directory. (";
            _errorMessage += errorStr;
            _errorMessage += ")";
          } // else
        } else {
          // unable to pause rocksdb, production db still alive and running
          good = false;
          _respCode = rest::ResponseCode::BAD;
          _errorMessage = "Unable to stop rocksdb within timeout.";
        } // else
      } else {
        // rocks db did not stop
        good = false;
        _respCode = rest::ResponseCode::BAD;
        _errorMessage = "Unable to stop rocksdb transactions within timeout.";
      } // else
    } catch(...) {
      good = false;
      _respCode = rest::ResponseCode::BAD;
      _errorMessage = "RocksDBHotBackupRestore::execute caught exception.";
      // _respError = 0;???
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBHotBackupRestore::execute caught exception.";
    } // catch
  }  // if

  /// clean up on error
  if (!good) {
    if (pauseWorked) {
      restartRocksDB();
    } // if

    if (restoringReady) {
      TRI_RemoveDirectory(restoringDir.c_str());
    } // if
  }  // if

  return;

} // RocksDBHotBackupRestore::execute


/// @brief clear previous restoring directory and populate new
///        with files from desired hotbackup
bool RocksDBHotBackupRestore::createRestoringDirectory(std::string & restoreDirOutput) {
  bool retFlag={true};
  std::string errors, fullDirectoryRestore = rebuildPath(_directoryRestore);

  try {
    // create path name (used here and returned)
    restoreDirOutput = rebuildPath("restoring");

    // git rid of an old restoring directory/file if it exists
    if (basics::FileUtils::exists(restoreDirOutput)) {
      if (basics::FileUtils::isDirectory(restoreDirOutput)) {
        TRI_RemoveDirectory(restoreDirOutput.c_str());
      } else {
        basics::FileUtils::remove(restoreDirOutput);
      } // else

      // test if still there and error out?
      if (basics::FileUtils::exists(restoreDirOutput)) {
        retFlag = false;
      } // if
    } // if

    // now create a new restoring directory
    if (retFlag) {
      retFlag = basics::FileUtils::createDirectory(restoreDirOutput, nullptr);
    } // if

    //  copy contents of selected hotbackup to new "restoring" directory
    //  (both directories must exists)
    if (retFlag) {
      std::function<basics::FileUtils::TRI_copy_recursive_e(std::string const&)>  filter=copyVersusLink;
      retFlag = basics::FileUtils::copyRecursive(fullDirectoryRestore, restoreDirOutput,
                                                 filter, errors);
    } // if
  } catch(...) {
    retFlag = false;
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "createRestoringDirectory caught exception.";
  } // catch

  // set error values
  if (!retFlag) {
    _respError = TRI_ERROR_CANNOT_CREATE_DIRECTORY;
    _respCode = rest::ResponseCode::BAD;
    _result.add(VPackValue(VPackValueType::Object));
    _result.add("failedDirectory", VPackValue(restoreDirOutput.c_str()));
    _result.close();
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "RocksDBHotBackupRestore unable to create/populate " << restoreDirOutput
      << " from " << fullDirectoryRestore << " (errors: " << errors << ")";
  } // if

  return retFlag;

} // RocksDBHotBackupRestore::createRestoringDirectory


} // namespace arangodb
