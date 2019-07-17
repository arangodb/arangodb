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
/// @author Matthew Van-Maszewski
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBHotBackup.h"

#include <thread>

#include "Agency/TimeString.h"
#include "ApplicationFeatures/RocksDBOptionFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabasePathFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEventListener.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/HotBackupCommon.h"
#include "Rest/Version.h"

#if USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBHotBackupEE.h"
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

#include <velocypack/Parser.h>
#include <rocksdb/utilities/checkpoint.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace {
arangodb::RocksDBHotBackup* toHotBackup(arangodb::RocksDBHotBackup* obj) {
  return static_cast<arangodb::RocksDBHotBackup*>(obj);
}
} //namespace

namespace arangodb {

/*
std::ostream& operator<<(std::ostream& os, const BackupMeta& bm) {
  os << "(" << bm._id << ", " << bm._version << ", " << bm._datetime << ")";
  return os;
}*/

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
  std::string const& command, VPackSlice body, VPackBuilder& report) {
  std::shared_ptr<RocksDBHotBackup> operation;

  if (0 == command.compare("create")) {
    operation.reset(toHotBackup(new RocksDBHotBackupCreate(body, report, true)));
  } else if (0 == command.compare("delete")) {
    operation.reset(toHotBackup(new RocksDBHotBackupCreate(body, report, false)));
  } else if (0 == command.compare("restore")) {
    operation.reset(toHotBackup(new RocksDBHotBackupRestore(body, report)));
  } else if (0 == command.compare("list")) {
    operation.reset(toHotBackup(new RocksDBHotBackupList(body, report)));
  } else if (0 == command.compare("lock")) {
    operation.reset(toHotBackup(new RocksDBHotBackupLock(body, report, true)));
  } else if (0 == command.compare("unlock")) {
    operation.reset(toHotBackup(new RocksDBHotBackupLock(body, report, false)));
  }
#if USE_ENTERPRISE
  else if (0 == command.compare("upload")) {
    operation.reset(toHotBackup(new RocksDBHotBackupUpload(body, report)));
  }
  else if (0 == command.compare("download")) {
    operation.reset(toHotBackup(new RocksDBHotBackupDownload(body, report)));
  }
#endif   // USE_ENTERPRISE

  // check the parameter once operation selected
  if (operation) {
    operation->parseParameters();
  } else {
    // if no operation selected, give base class which defaults to "bad"
    operation.reset(new RocksDBHotBackup(body, report));

  } // if

  return operation;

} // RocksDBHotBackup::operationFactory


//
// @brief Setup the base object, default is "bad parameters"
//
RocksDBHotBackup::RocksDBHotBackup(VPackSlice body, VPackBuilder& report)
  : _body(body), _valid(true), _success(false), _respCode(rest::ResponseCode::BAD),
    _respError(TRI_ERROR_HTTP_BAD_PARAMETER), _result(report), _timeoutSeconds(10) {
  _isSingle = ServerState::instance()->isSingleServer();
  return;
}


// @brief load the agency using the current encryption key
std::string RocksDBHotBackup::loadAgencyJson(std::string filename) {

#ifdef USE_ENTERPRISE
  std::string encryptionKey = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->getEncryptionKey();
  int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);
  if (fd != -1) {
    TRI_DEFER(TRI_CLOSE(fd));

    auto context = EncryptionFeature::beginDecryption(fd, encryptionKey);
    return EncryptionFeature::slurpData(*context.get());
  } else {
    return std::string{};
  }
#else
  return basics::FileUtils::slurp(filename);
#endif
}

ResultT<BackupMeta> RocksDBHotBackup::getMeta(std::string const& id) {
  try {
    std::string directory = rebuildPath(id);

    std::string metaString =
      basics::FileUtils::slurp(
        directory + TRI_DIR_SEPARATOR_CHAR + "META");

    std::shared_ptr<VPackBuilder> metaBuilder =
      arangodb::velocypack::Parser::fromJson(metaString);

    return BackupMeta::fromSlice(metaBuilder->slice());

  } catch (std::exception const& e) {
    return ResultT<BackupMeta>::error(TRI_ERROR_HOT_BACKUP_INTERNAL, e.what());
  }
}

Result RocksDBHotBackup::writeMeta(std::string const& id, BackupMeta const& meta) try {
  std::string directory = rebuildPath(id);
  std::string versionFileName = directory + TRI_DIR_SEPARATOR_CHAR + "META";
  VPackBuilder metaBuilder;
  meta.toVelocyPack(metaBuilder);
  basics::FileUtils::spit(versionFileName, metaBuilder.toJson(), true);
  return Result{};
} catch(std::exception const& e) {
  _errorMessage =
    std::string("RocksDBHotBackup::writeMeta caught exception: ") + e.what();
  return Result{TRI_ERROR_HOT_RESTORE_INTERNAL, _errorMessage};
}


// @brief returns specific information about the hotbackup with the given id
void RocksDBHotBackup::statId(std::string const& id, VPackBuilder& result, bool report) {
  std::string directory = rebuildPath(id);


  if (!basics::FileUtils::isDirectory(directory) ||
      basics::FileUtils::isRegularFile(directory+"/INPROGRESS")) {
    _success = false;
    _respError = TRI_ERROR_HTTP_NOT_FOUND;
    _errorMessage = "No such backup";
    return;
  }

  std::string version;

  ResultT<BackupMeta> meta = getMeta(id);
  if (meta.fail()) {
    _errorMessage = meta.errorMessage();
    _respError = meta.errorNumber();
    return;
  }

  if (_isSingle) {

    _success = true;
    _respError = TRI_ERROR_NO_ERROR;
    if (report) {
      { VPackObjectBuilder ob(&result, "list");
        result.add(VPackValue(id));
        meta.get().toVelocyPack(result);
      }
    }
    return;
  }

  if (ServerState::instance()->isDBServer()) {
    std::shared_ptr<VPackBuilder> agency;
    try {
      std::string agencyjson =
        RocksDBHotBackup::loadAgencyJson(
          directory + TRI_DIR_SEPARATOR_CHAR + "agency.json");

      if (ServerState::instance()->isDBServer()) {
        if (agencyjson.empty()) {
          _respCode = rest::ResponseCode::BAD;
          _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
          _success = false;
          _errorMessage = "Could not open agency.json";
          return ;
        }

        agency = arangodb::velocypack::Parser::fromJson(agencyjson);
      }


    } catch (std::exception const& e) {
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
      _success = false;
      _errorMessage = std::string("Could not open agency.json") + e.what();
      return ;
    }

    if (ServerState::instance()->isDBServer()) {
      result.add("server", VPackValue(getPersistedId()));
      result.add("agency-dump", agency->slice());
      { VPackObjectBuilder ob(&result, "list");
        result.add(VPackValue(id));
        meta.get().toVelocyPack(result);
      }
    }

    _success = true;
    return;
  }

  _success = false;
  _respError = TRI_ERROR_HOT_BACKUP_INTERNAL;
  _errorMessage = "hot backup API is only available on single and db servers.";
  return;

}

std::string RocksDBHotBackup::buildDirectoryPath(std::string const& timestamp, std::string const& label) {

  std::string suffix = timestamp;

  if (0 != label.length()) {
    // limit directory name to 254 characters
    suffix += "_";
    suffix.append(label, 0, 254 - suffix.size());
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
  ret_string += "backups";

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
      LOG_TOPIC("81ad6", ERR, arangodb::Logger::ENGINES)
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

//
// @brief Common routine for retrieving parameter from request body.
//        Assumes caller has maintain state of _body and _valid
//
void RocksDBHotBackup::getParamValue(char const* key, double& value, bool required) {
  VPackSlice tempSlice;

  try {
    if (_body.isObject() && _body.hasKey(key)) {
      tempSlice = _body.get(key);
      value = tempSlice.getNumber<double>();
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

} // RocksDBHotBackup::getParamValue (double)



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



void RocksDBHotBackup::getParamValue(char const* key, VPackBuilder& value, bool required) {

  try {
    if (_body.isObject() && _body.hasKey(key)) {
      value = VPackBuilder{_body.get(key)};
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

  return transaction::ManagerFeature::manager()->holdTransactions(_timeoutSeconds * 1000000);

} // RocksDBHotBackup::holdRocksDBTransactions()


/// WARNING:  this wrapper is NOT used in LockCleaner class
void RocksDBHotBackup::releaseRocksDBTransactions() {

  transaction::ManagerFeature::manager()->releaseTransactions();

} // RocksDBHotBackup::releaseRocksDBTransactions()


void RocksDBHotBackup::startGlobalShutdown() {

  arangodb::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestLane::INTERNAL_LOW, []() {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      LOG_TOPIC("59a7d", INFO, arangodb::Logger::ENGINES)
        << "RocksDBHotBackupRestore:  restarting server with restored data";
      application_features::ApplicationServer::server->beginShutdown();
    });

} // RocksDBHotBackup::startGlobalShutdown


////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupCreate
///        POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupCreate::RocksDBHotBackupCreate(
  VPackSlice body, VPackBuilder& report, bool isCreate)
  : RocksDBHotBackup(body, report), _isCreate(isCreate), _forceBackup(false),
    _agencyDump(VPackSlice::noneSlice()) {}


void RocksDBHotBackupCreate::parseParameters() {

  // single server create, we generate the timestamp
  if (_isSingle && _isCreate) {
    _timestamp = timepointToString(std::chrono::system_clock::now());
  } else if (_isCreate) {
    getParamValue("timestamp", _timestamp, true);
    getParamValue("agency-dump", _agencyDump, false);
  } else {
    getParamValue("id", _id, true);
  } // else

  // remaining params are optional
  getParamValue("timeout", _timeoutSeconds, false);
  getParamValue("label", _label, false);
  getParamValue("forceBackup", _forceBackup, false);

  if (_label.empty()) {
    _label = to_string(boost::uuids::random_generator()());
  }

  if (!_valid) {
    _result.close();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
    _errorMessage = BAD_PARAMS_CREATE;
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


/// @brief /_admin/backup/create with POST method comes here, initiates
///       a rocksdb checkpoint
void RocksDBHotBackupCreate::executeCreate() {

  // note current time
  // attempt lock on write transactions
  // attempt iResearch flush
  // time remaining, or flag to continue anyway

  std::string randomDirCreatingString
    = std::string(dirCreatingString) + "_" +
      arangodb::basics::StringUtils::itoa(RandomGenerator::interval(1000000,
                                                                    2000000));
  std::string dirPathFinal = buildDirectoryPath(_timestamp, _label);
  std::string id = TRI_Basename(dirPathFinal.c_str());
  std::string dirPathTemp = rebuildPath(randomDirCreatingString);
  bool flag = clearPath(randomDirCreatingString);

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
        LOG_TOPIC("9ce0a", DEBUG, Logger::BACKUP) << "Creating checkpoint in RocksDB...";
        stat = ptr->CreateCheckpoint(dirPathTemp);
        _success = stat.ok();
        LOG_TOPIC("f3dbb", DEBUG, Logger::BACKUP) << "Done creating checkpoint in RocksDB, result:" << stat.ToString();
      } // if
    } // guardHold released

    if (_success) {
      std::string errors;
      long systemError;
      int retVal;

      std::function<basics::FileUtils::TRI_copy_recursive_e(std::string const&)>  filter = linkShaFiles;
      /*_success =*/ basics::FileUtils::copyRecursive(getRocksDBPath(), dirPathTemp,
                                                  filter, errors);

      // Check that all sst files are there:
      arangodb::RocksDBEventListenerThread::checkMissingShaFiles(dirPathTemp, 0);

      // now rename
      retVal = TRI_RenameFile(dirPathTemp.c_str(), dirPathFinal.c_str(), &systemError, &errors);
      _success = (TRI_ERROR_NO_ERROR == retVal);
    } // if

    // write encrypted agency dump if available
    if (_success && !_agencyDump.isNone()) {
      std::string agencyDumpFileName = dirPathFinal + TRI_DIR_SEPARATOR_CHAR + "agency.json";

      try {
        // toJson may throw
        std::string json = _agencyDump.toJson();

#ifdef USE_ENTERPRISE
        std::string encryptionKey = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->getEncryptionKey();
        int fd = TRI_CREATE(agencyDumpFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC | TRI_O_CLOEXEC,
                                     S_IRUSR | S_IWUSR | S_IRGRP);
        if (fd != -1) {
          TRI_DEFER(TRI_CLOSE(fd));
          auto context = EncryptionFeature::beginEncryption(fd, encryptionKey);
          _success = EncryptionFeature::writeData(*context.get(), json.c_str(), json.size());
        } else {
          _success = false;
        }
#else
        basics::FileUtils::spit(agencyDumpFileName, json, true);
#endif
      } catch(std::exception const& e) {
        _success = false;
        _respCode = rest::ResponseCode::BAD;
        _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
        _errorMessage =
          std::string("RocksDBHotBackupCreate caught exception: ") + e.what();
        LOG_TOPIC("cee0c", ERR, arangodb::Logger::ENGINES) << _errorMessage;
      }
    }

    if (_success) {
      Result res = writeMeta(id, BackupMeta(id, ARANGODB_VERSION, timepointToString(std::chrono::system_clock::now())));
      if (res.fail()) {
          _success = false;
          _respCode = rest::ResponseCode::BAD;
          _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
          _errorMessage = res.errorMessage();
          LOG_TOPIC("0412c", ERR, arangodb::Logger::ENGINES) << _errorMessage;
      }
    }
  } // if

  // set response codes
  if (_success) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;

    // velocypack loves to throw. wrap it.
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("id", VPackValue(id));
      _result.add("forced", VPackValue(!gotLock));
      _result.close();
    } catch (std::exception const& e) {
      _result.clear();
      _success = false;
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
      _errorMessage =
        std::string("RocksDBHotBackupCreate caught exception: ") + e.what();
      LOG_TOPIC("23cf3", ERR, arangodb::Logger::ENGINES) << _errorMessage;
    } // catch
  } else {
    // stat.ok() means CreateCheckpoint() never called ... so lock issue
    _respCode = stat.ok() ? rest::ResponseCode::REQUEST_TIMEOUT : rest::ResponseCode::EXPECTATION_FAILED;
    _respError = stat.ok() ? TRI_ERROR_LOCK_TIMEOUT : TRI_ERROR_FAILED;
    _errorMessage = stat.ok() ? std::string("Could not acquire lock before creating checkpoint.") : std::string("RocksDB error when creating checkpoint: ") + stat.ToString();
  } //else

} // RocksDBHotBackupCreate::executeCreate


/// @brief /_admin/backup/create with DELETE method comes here, deletes
///        a directory if it exists
///        NOTE: returns success if the requested directory does not exist
///              (was previously deleted)
void RocksDBHotBackupCreate::executeDelete() {
  std::string dirToDelete;
  dirToDelete = rebuildPath(_id);

  if (!basics::FileUtils::exists(dirToDelete)) {
    _respCode = rest::ResponseCode::NOT_FOUND;
    _respError = TRI_ERROR_FILE_NOT_FOUND;
    return ;
  }

  _success = clearPath(dirToDelete);
  // set response codes
  if (_success) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;
    VPackObjectBuilder guard(&_result);
  } else {
    _respCode = rest::ResponseCode::SERVER_ERROR;
    _respError = TRI_ERROR_HOT_BACKUP_INTERNAL;
  } //else

  return;

} // RocksDBHotBackupCreate::executeDelete

////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupRestore
///        POST:  Initiate restore of rocksdb snapshot in place of working directory
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupRestore::RocksDBHotBackupRestore(VPackSlice body, VPackBuilder& report)
  : RocksDBHotBackup(body, report), _saveCurrent(false), _ignoreVersion(false) {}


/// @brief convert the message payload into class variable options
void RocksDBHotBackupRestore::parseParameters() {

  _valid = true; //(rest::RequestType::POST == type);

  // timestamp used for snapshot created of existing database
  //  (in case of rollback or _saveCurrent flag)
  _timestampCurrent = timepointToString(std::chrono::system_clock::now());

  // full directory name of database image to restore (required)
  getParamValue("id", _idRestore, true);

  // remaining params are optional
  getParamValue("saveCurrent", _saveCurrent, false);

  // force restore on wrong version
  getParamValue("ignoreVersion", _ignoreVersion, false);

  if (!_valid) {
    _result.close();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
    _errorMessage = "backup's ID must be specified";
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
  }

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

  VPackObjectBuilder r(&_result);
  // Find the specific backup
  _success = true;
  statId(_idRestore, _result, false);
  if (_success == false) {
    _result.close();
    return;
  }

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
        if (0 == failsafeName.compare(_idRestore)) {
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
        _result.add("previous", VPackValue(failsafeName));
      } catch (std::exception const& e) {
        _result.clear();
        _success = false;
        _respCode = rest::ResponseCode::BAD;
        _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
        _errorMessage =
          std::string("RocksDBHotBackupRestore caught exception: ") + e.what();
        LOG_TOPIC("45ae8", ERR, arangodb::Logger::ENGINES) << _errorMessage;
      } // catch
    } // if

  } else {
    // restartAction already populated, nothing we can do
    _respCode = rest::ResponseCode::BAD;
    _errorMessage = "restartAction already set. More than one restore occurring in parallel?";
    LOG_TOPIC("09d1e", ERR, arangodb::Logger::ENGINES)
      << "RocksDBHotBackupRestore: " << _errorMessage;
  } // else

  _result.close();

} // RocksDBHotBackupRestore::execute

bool RocksDBHotBackupRestore::validateVersionString(std::string const& fullDirectoryRestore) {

  if (_ignoreVersion) {
    return true;
  }

  ResultT<BackupMeta> meta = getMeta(TRI_Basename(fullDirectoryRestore.c_str()));
  if (meta.ok()) {
    if (RocksDBHotBackup::versionTestRestore(meta.get()._version)) {
      return true;
    }
  }

  _respError = TRI_ERROR_FAILED;
  _respCode = rest::ResponseCode::BAD;
  _success = false;
  _errorMessage = "RocksDBHotBackupRestore unable to restore: version mismatch";

  LOG_TOPIC("16e06", ERR, arangodb::Logger::ENGINES) << _errorMessage;
  return false;
}


/// @brief clear previous restoring directory and populate new
///        with files from desired hotbackup.
///        restoreDirOutput is created and passed to caller
bool RocksDBHotBackupRestore::createRestoringDirectory(std::string& restoreDirOutput) {
  bool retFlag = true;
  std::string errors, fullDirectoryRestore = rebuildPath(_idRestore);

  if (!validateVersionString(fullDirectoryRestore)) {
    return false;
  }

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
  } catch (std::exception const& e) {
    retFlag = false;
    LOG_TOPIC("4d34f", ERR, arangodb::Logger::ENGINES)
      << std::string("createRestoringDirectory caught exception: ") + e.what();
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

    _errorMessage =
      std::string("RocksDBHotBackupRestore unable to create/populate ") +
      restoreDirOutput + " from " + fullDirectoryRestore + " (errors: " +
      errors + ")";

    LOG_TOPIC("d226a", ERR, arangodb::Logger::ENGINES) << _errorMessage;

  } // if

  return retFlag;

} // RocksDBHotBackupRestore::createRestoringDirectory


////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupList
///        POST:  Returns array of Hotbackup directory names
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupList::RocksDBHotBackupList(VPackSlice body, VPackBuilder& report)
  : RocksDBHotBackup(body, report) {}

void RocksDBHotBackupList::parseParameters() {

  _valid = true; //(rest::RequestType::POST == type);
  getParamValue("id", _listId, false);

  if (!_valid) {
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("httpMethod", VPackValue("only POST allowed"));
      _result.close();
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
    } catch (std::exception const& e) {
      _result.clear();
      _respCode = rest::ResponseCode::BAD;
      _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
      _errorMessage =
        std::string("RocksDBHotBackupList::parseParameters caught exception: ") + e.what();
      LOG_TOPIC("3b164", ERR, arangodb::Logger::ENGINES) << _errorMessage;
    } // catch
  } // if

} // RocksDBHotBackupList::parseParameters


// @brief route to independent functions for "create" and "delete"
void RocksDBHotBackupList::execute() {
  if (_listId.empty()) {
    listAll();
  } else {
    VPackObjectBuilder r(&_result);
    statId(_listId, _result);
  }
} // RocksDBHotBackupList::execute


// @brief list all available backups
void RocksDBHotBackupList::listAll() {
    std::vector<std::string> hotbackups = TRI_FilesDirectory(rebuildPathPrefix().c_str());

  // remove working directories from list
  // All directory names which start with dirCreatingString (note: the temporary
  // directory names have a random suffix after this string).
  std::string const tofind(dirCreatingString);
  hotbackups.erase(std::remove_if(hotbackups.begin(), hotbackups.end(),
        [&tofind](std::string const&s) {
          return s.compare(0, tofind.size(), tofind) == 0;
        }), hotbackups.end());

  std::vector<std::string>::iterator found;

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
    {
      VPackObjectBuilder resultOB(&_result);
      _result.add("server", VPackValue(getPersistedId()));
      {
        VPackObjectBuilder listOB(&_result, "list");
        for (auto const& id : hotbackups) {

          _result.add(VPackValue(id));
          auto metaResult = getMeta(id);
          if (metaResult.ok()) {
            metaResult.get().toVelocyPack(_result);
          } else {
            VPackObjectBuilder errorOB(&_result);
            _result.add("errorMessage", VPackValue(metaResult.errorMessage()));
            _result.add("errorNumber", VPackValue(metaResult.errorNumber()));
          }
        }
      }
    }

    _success = true;
  } catch (std::exception const& e) {
    _result.clear();
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HOT_RESTORE_INTERNAL;
    _errorMessage = std::string("RocksDBHotBackupList::execute caught exception:") + e.what();
    LOG_TOPIC("be9e0", ERR, arangodb::Logger::ENGINES) << _errorMessage;
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
    SchedulerFeature::SCHEDULER->queueDelay(RequestLane::INTERNAL_LOW, std::chrono::seconds(timeoutSeconds), *this);
  };

  void operator()(bool cancelled) {
    MUTEX_LOCKER (mLock, serialNumberMutex);
    // only unlock if creation of this object instance was due to
    //  the taking of current transaction lock
    if (lockingSerialNumber == _lockSerialNumber) {
      LOG_TOPIC("a20be", ERR, arangodb::Logger::ENGINES)
        << "RocksDBHotBackup LockCleaner removing lost transaction lock.";
      // would prefer virtual releaseRocksDBTransactions() ... but would
      //   require copy of RocksDBHotBackupLock object used from RestHandler or unit test.
      transaction::ManagerFeature::manager()->releaseTransactions();
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
RocksDBHotBackupLock::RocksDBHotBackupLock(
  VPackSlice const body, VPackBuilder& report, bool isLock)
  : RocksDBHotBackup(body, report), _isLock(isLock), _unlockTimeoutSeconds(5)
{
}

RocksDBHotBackupLock::~RocksDBHotBackupLock() {
}

void RocksDBHotBackupLock::parseParameters() {

  getParamValue("timeout", _timeoutSeconds, false);
  getParamValue("unlockTimeout", _unlockTimeoutSeconds, false);

  return;

} // RocksDBHotBackupLock::parseParameters

void RocksDBHotBackupLock::execute() {
  MUTEX_LOCKER (mLock, serialNumberMutex);

  {
    VPackObjectBuilder o(&_result);

    if (!_isSingle) {
      if (_isLock) {
        // make sure no one already locked for restore
        if ( 0 == lockingSerialNumber ) {
          _success = holdRocksDBTransactions();

          // prepare emergency lock release in case of coordinator failure
          if (_success) {
            lockingSerialNumber = getSerialNumber();
            // LockCleaner gets copied by async_wait during constructor
            _result.add("lockId", VPackValue(lockingSerialNumber));
            LockCleaner cleaner(lockingSerialNumber, _unlockTimeoutSeconds);
          } else {
            _respCode = rest::ResponseCode::REQUEST_TIMEOUT;
            _respError = TRI_ERROR_LOCK_TIMEOUT;
            _errorMessage = "RocksDBHotBackupLock: locking timed out";
          } // else
        } else {
          _respCode = rest::ResponseCode::BAD;
          _respError = TRI_ERROR_HOT_BACKUP_CONFLICT;
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

  }

  /// return codes
  if (_success) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;
  } // if

  return;

} // RocksDBHotBackupLock::execute

} // namespace arangodb
