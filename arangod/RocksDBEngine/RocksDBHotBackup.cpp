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

#include "Agency/TimeString.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"

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



////////////////////////////////////////////////////////////////////////////////
/// @brief RocksDBHotBackupCreate
///        POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
RocksDBHotBackupCreate::RocksDBHotBackupCreate() {
}


RocksDBHotBackupCreate::~RocksDBHotBackupCreate() {
}


void RocksDBHotBackupCreate::parseParameters(rest::RequestType const type, VPackSlice & body) {
  bool isSingle(ServerState::instance()->isSingleServer());
  VPackSlice tempSlice;
  arangodb::velocypack::ValueLength temp(0);

  _isCreate = (rest::RequestType::POST == type);
  _valid = true;

  // single server create, we generate the timestamp
  if (isSingle && _isCreate) {
    _timestamp = timepointToString(std::chrono::system_clock::now());
  } else {
    _valid = body.hasKey("timestamp");
    if (_valid) {
      tempSlice = body.get("timestamp");
      _timestamp = tempSlice.getString(temp);
      _valid = (20 == temp);
    } // if
  } // else

  // timeout is optional
  if (_valid && body.hasKey("timeoutMS")) {
    tempSlice = body.get("timeoutMS");
    _timeoutMS = tempSlice.getInt();
  } // if

  // user string is optional
  if (_valid && body.hasKey("userString")) {
    tempSlice = body.get("userString");
    _userString = tempSlice.getString(temp);
    // if temp > ??? truncate
    // do character set adjustment
  } // if


  if (!_valid) {
    _respCode = rest::ResponseCode::BAD;
    _respError = TRI_ERROR_HTTP_BAD_PARAMETER;
  } // if

  return;

} // RocksDBHotBackupCreate::parseParameters



}  // namespace arangodb
