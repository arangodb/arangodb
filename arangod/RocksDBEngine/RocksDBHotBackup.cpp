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
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "RestServer/DatabasePathFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"

#include <rocksdb/utilities/checkpoint.h>

namespace arangodb {

//
// @brief static function to pick proper operation object and then have it
//        parse parameters
//
std::shared_ptr<RocksDBHotBackup> RocksDBHotBackup::operationFactory(
  rest::RequestType const type,
  std::vector<std::string> const & suffixes,
  VPackSlice & body)
{
  std::shared_ptr<RocksDBHotBackup> operation;
  bool isCoord(ServerState::instance()->isCoordinator());

  // initial implementation only has single suffix verbs
  if (1 == suffixes.size()) {
    if (0 == suffixes[0].compare("create"))
    {
      operation.reset((isCoord ? (RocksDBHotBackup *)new RocksDBHotBackupCreateCoord
                       : (RocksDBHotBackup *)new RocksDBHotBackupCreate));
    } // if
  }

  // check the parameter once operation selected
  if (operation) {
    operation->parseParameters(type, body);
  } else {
    // if no operation selected, give base class which defaults to "bad"
    operation.reset(new RocksDBHotBackup);
  } // if

  return operation;

} // RocksDBHotBackup::operationFactory


//
// @brief Setup the base object, default is "bad parameters"
//
RocksDBHotBackup::RocksDBHotBackup()
  : _valid(false), _success(false), _respCode(rest::ResponseCode::BAD),
    _respError(TRI_ERROR_HTTP_BAD_PARAMETER)
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

  ret_string = getDatabasePath();
  ret_string += TRI_DIR_SEPARATOR_CHAR;
  ret_string += suffix;

  return ret_string;

} // RocksDBHotBackup::buildDirectoryPath


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

////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupCreate
///        POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupCreate::RocksDBHotBackupCreate()
  : _isCreate(true), _timeoutMS(10000)
{
}


RocksDBHotBackupCreate::~RocksDBHotBackupCreate() {
}


void RocksDBHotBackupCreate::parseParameters(rest::RequestType const type, const VPackSlice & body) {
  bool isSingle(ServerState::instance()->isSingleServer());
  VPackSlice tempSlice;
  std::string param;

  _isCreate = (rest::RequestType::POST == type);
  _valid = _isCreate || (rest::RequestType::DELETE_REQ == type);

  try {
    // single server create, we generate the timestamp
    if (isSingle && _isCreate) {
      _timestamp = timepointToString(std::chrono::system_clock::now());
    } else {
      param = "timestamp";
      _valid = _valid && body.isObject() && body.hasKey(param);
      if (_valid) {
        tempSlice = body.get(param);
        _timestamp = tempSlice.copyString();
        _valid = (20 == _timestamp.length());
      } // if
    } // else

    // timeout is optional
    param = "timeoutMS";
    if (_valid && body.isObject() && body.hasKey(param)) {
      tempSlice = body.get(param);
      _timeoutMS = tempSlice.getInt();
    } // if

    // user string is optional
    param = "userString";
    if (_valid && body.isObject() && body.hasKey(param)) {
      tempSlice = body.get(param);
      _userString = tempSlice.copyString();
    } // if

/// param for don't wait, continue if timeout fails

  } catch(VPackException const &vexcept) {
    _valid = false;
    _result.add(VPackValue(VPackValueType::Object));
    _result.add(param, VPackValue(vexcept.what()));
    _result.close();
  };

  if (!_valid) {
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
  } // if

  return;

} // RocksDBHotBackupCreate::parseParameters


void RocksDBHotBackupCreate::execute() {

  if (_isCreate) {
    executeCreate();
  } else {
    executeDelete();
  } // else

  return;

} // RocksDBHotBackupCreate


void RocksDBHotBackupCreate::executeCreate() {

  // note current time
  // attempt lock on write transactions
  // attempt iResearch flush
  // time remaining, or flag to continue anyway

  rocksdb::Checkpoint * ptr;
  rocksdb::Status stat;
  std::string dirPath;

  stat = rocksdb::Checkpoint::Create(rocksutils::globalRocksDB(), &ptr);

  if (stat.ok()) {
    dirPath = buildDirectoryPath(_timestamp, _userString);
    stat = ptr->CreateCheckpoint(dirPath);
    delete ptr;
  }


  // set response codes
  if (stat.ok()) {
    _respCode = rest::ResponseCode::OK;
    _respError = TRI_ERROR_NO_ERROR;
    _success = true;

    // velocypack loves to throw. wrap it.
    try {
      _result.add(VPackValue(VPackValueType::Object));
      _result.add("directory", VPackValue(dirPath));
      _result.close();
    } catch (...) {
    }
  } else {
  } //else

  return;

} // RocksDBHotBackupCreate

}  // namespace arangodb
