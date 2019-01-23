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

#ifndef ARANGOD_ROCKSDB_HOTBACKUP_H
#define ARANGOD_ROCKSDB_HOTBACKUP_H 1

#include <velocypack/velocypack-aliases.h>
#include "Rest/GeneralResponse.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief Base class for various RocksDBHotBackup operations
////////////////////////////////////////////////////////////////////////////////

class RocksDBHotBackup {
public:
  static std::shared_ptr<RocksDBHotBackup> operationFactory(arangodb::rest::RequestType const type,
                                                            std::vector<std::string> const & suffixes,
                                                            VPackSlice & body);

  RocksDBHotBackup();
  virtual ~RocksDBHotBackup();

  bool valid() const {return _valid;};
  bool success() const {return _success;};

  rest::ResponseCode restResponseCode() const {return _respCode;};
  int restResponseError() const {return _respError;};

  // @brief Validate and extract parameters appropriate to the operation type
  virtual void parseParameters(rest::RequestType const, VPackSlice &) {};

  // @brief Execute the operation
  virtual void execute() {};

  VPackSlice resultSlice() {return _result.slice();};

protected:
  virtual std::string buildDirectoryPath(const std::string & timestamp, const std::string & userString);

  bool _valid;          // are parameters valid
  bool _success;        // did operation finish successfully

  rest::ResponseCode _respCode;
  int _respError;
  VPackBuilder _result;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief The following wrapper routines simplify unit testing
  ////////////////////////////////////////////////////////////////////////////////
  virtual std::string getDatabasePath();

  virtual std::string getPersistedId();
};// class RocksDBHotBackup


////////////////////////////////////////////////////////////////////////////////
/// @brief POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupCreate : public RocksDBHotBackup {
public:

  RocksDBHotBackupCreate();
  ~RocksDBHotBackupCreate();

  // @brief Validate and extract parameters appropriate to the operation type
  virtual void parseParameters(rest::RequestType const, VPackSlice &);

  // @brief Execute an operation
  virtual void execute();

protected:
  // @brief Execute the create operation
  void executeCreate();

  // @brief Execute the delete operation
  void executeDelete() {};


  bool _isCreate;
  std::string _timestamp;
  int _timeoutMS;
  std::string _userString;

  /// option to continue after timeout fails

};// class RocksDBHotBackupCreate


class RocksDBHotBackupRestore : public RocksDBHotBackup {
public:

  virtual void execute() {};

};// class RocksDBHotBackupRestore


class RocksDBHotBackupList : public RocksDBHotBackup {
public:

  virtual void execute() {};

};// class RocksDBHotBackupList


class RocksDBHotBackupPolicy : public RocksDBHotBackup {
public:

  virtual void execute() {};

};// class RocksDBHotBackupPolicy


}  // namespace arangodb

#endif
