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
#include "Basics/Result.h"
#include "Rest/GeneralResponse.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief Base class for various RocksDBHotBackup operations
////////////////////////////////////////////////////////////////////////////////

class RocksDBHotBackup {
public:
  static std::shared_ptr<RocksDBHotBackup> operationFactory(arangodb::rest::RequestType const type,
                                                            std::vector<std::string> const & suffixes,
                                                            const VPackSlice body);

  RocksDBHotBackup() = delete;
  RocksDBHotBackup(const VPackSlice body);
  virtual ~RocksDBHotBackup();

  bool valid() const {return _valid;};
  bool success() const {return _success;};

  rest::ResponseCode restResponseCode() const {return _respCode;};
  int restResponseError() const {return _respError;};
  std::string const& errorMessage() const {return _errorMessage;};

  // @brief Validate and extract parameters appropriate to the operation type
  virtual void parseParameters(rest::RequestType const) {};

  // @brief Execute the operation
  virtual void execute() {};

  VPackSlice resultSlice() {return _result.slice();};
  VPackBuilder const& result() const { return _result;};

  std::string getRocksDBPath();
  unsigned getTimeout() const {return _timeoutSeconds;}
  std::string rebuildPath(const std::string & suffix);

protected:
  virtual std::string buildDirectoryPath(const std::string & timestamp, const std::string & userString);

  // retrieve configuration values from request's body
  void getParamValue(const char * key, std::string & value, bool required);
  void getParamValue(const char * key, bool &value, bool required);
  void getParamValue(const char * key, unsigned & value, bool required);
  void getParamValue(const char * key, VPackSlice & value, bool required);

  const VPackSlice _body;   // request's configuration
  bool _valid;          // are parameters valid
  bool _success;        // did operation finish successfully

  rest::ResponseCode _respCode;
  int _respError;
  std::string _errorMessage;
  VPackBuilder _result;

  unsigned _timeoutSeconds; // used to stop transaction, used again to stop rocksdb

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief The following wrapper routines simplify unit testing
  ////////////////////////////////////////////////////////////////////////////////
  virtual std::string getDatabasePath();
  virtual std::string getPersistedId();
  virtual bool pauseRocksDB();
  virtual bool restartRocksDB();
  virtual bool holdRocksDBTransactions();
  virtual void releaseRocksDBTransactions();

};// class RocksDBHotBackup


////////////////////////////////////////////////////////////////////////////////
/// @brief POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupCreate : public RocksDBHotBackup {
public:

  RocksDBHotBackupCreate() = delete;
  RocksDBHotBackupCreate(const VPackSlice body);
  ~RocksDBHotBackupCreate();

  // @brief Validate and extract parameters appropriate to the operation type
  void parseParameters(rest::RequestType const) override;

  // @brief Execute an operation
  void execute() override;

  // @brief accessors to the parameters
  bool isCreate() const {return _isCreate;}
  const std::string & getTimestamp() const {return _timestamp;}
  const std::string & getUserString() const {return _userString;}

protected:
  // @brief Execute the create operation
  void executeCreate();

  // @brief Execute the delete operation
  void executeDelete() {};

  bool _isCreate;
  bool _forceBackup;
  std::string _timestamp;
  std::string _userString;

};// class RocksDBHotBackupCreate


class RocksDBHotBackupRestore : public RocksDBHotBackup {
public:

  RocksDBHotBackupRestore() = delete;
  RocksDBHotBackupRestore(const VPackSlice body);
  ~RocksDBHotBackupRestore();

  // @brief Validate and extract parameters appropriate to the operation type
  void parseParameters(rest::RequestType const) override;

  // @brief Execute an operation
  void execute() override;

  // @brief accessors to the parameters
  const std::string & getTimestampCurrent() const {return _timestampCurrent;}
  const std::string & getDirectoryRestore() const {return _directoryRestore;}

  bool createRestoringDirectory(std::string & nameOutput);

protected:

  bool _saveCurrent;
  bool _forceRestore;  // relates to transaction pause only, not rocksdb pause
  std::string _timestampCurrent;
  std::string _directoryRestore;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief The following wrapper routines simplify unit testing
  ////////////////////////////////////////////////////////////////////////////////


};// class RocksDBHotBackupRestore


class RocksDBHotBackupList : public RocksDBHotBackup {
public:

  void execute() override {};

};// class RocksDBHotBackupList


class RocksDBHotBackupPolicy : public RocksDBHotBackup {
public:

  void execute() override {};

};// class RocksDBHotBackupPolicy


}  // namespace arangodb

#endif
