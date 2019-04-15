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

#include "Basics/Result.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief Base class for various RocksDBHotBackup operations
////////////////////////////////////////////////////////////////////////////////

class RocksDBHotBackup {
public:
  static std::shared_ptr<RocksDBHotBackup> operationFactory(
    std::string const& suffixes, VPackSlice const body, VPackBuilder& report);

  RocksDBHotBackup() = delete;
  explicit RocksDBHotBackup(VPackSlice body, VPackBuilder& result);
  virtual ~RocksDBHotBackup() = default;

  bool valid() const {return _valid;}
  bool success() const {return _success;}

  rest::ResponseCode restResponseCode() const {return _respCode;}
  int restResponseError() const {return _respError;}
  std::string const& errorMessage() const {return _errorMessage;}

  /// @brief Validate and extract parameters appropriate to the operation type
  virtual void parseParameters() {}

  /// @brief Execute the operation
  virtual void execute() {}

  VPackSlice resultSlice() {return _result.slice();}
  VPackBuilder const& result() const { return _result;}

  std::string getRocksDBPath();
  unsigned getTimeout() const {return _timeoutSeconds;}

  /// @brief Build "/user/database/path/backups"
  std::string rebuildPathPrefix();

  /// @brief Build rebuildPathPrefix() + "/" + suffix
  std::string rebuildPath(std::string const& suffix);

protected:
  virtual std::string buildDirectoryPath(std::string const& timestamp, std::string const& userString);
  bool clearPath(std::string const& path);

  // retrieve configuration values from request's body
  void getParamValue(char const* key, std::string& value, bool required);
  void getParamValue(char const* key, bool& value, bool required);
  void getParamValue(char const* key, unsigned& value, bool required);
  void getParamValue(char const* key, VPackSlice& value, bool required);

  VPackSlice const _body;   // request's configuration
  bool _valid;          // are parameters valid
  bool _success;        // did operation finish successfully

  rest::ResponseCode _respCode;
  int _respError;
  std::string _errorMessage;
  VPackBuilder& _result;
  bool _isSingle;       // is single db server (not cluster)

  unsigned _timeoutSeconds; // used to stop transaction, used again to stop rocksdb

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief The following wrapper routines simplify unit testing
  ////////////////////////////////////////////////////////////////////////////////
  virtual std::string getDatabasePath();
  virtual std::string getPersistedId();
  virtual bool holdRocksDBTransactions();
  virtual void releaseRocksDBTransactions();
  virtual void startGlobalShutdown();

};// class RocksDBHotBackup


////////////////////////////////////////////////////////////////////////////////
/// @brief POST:  Initiate rocksdb checkpoint on local server
///        DELETE:  Remove an existing rocksdb checkpoint from local server
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupCreate : public RocksDBHotBackup {
public:

  RocksDBHotBackupCreate() = delete;
  virtual ~RocksDBHotBackupCreate() = default;
  explicit RocksDBHotBackupCreate(VPackSlice body, VPackBuilder& report);

  // @brief Validate and extract parameters appropriate to the operation type
  void parseParameters() override;

  // @brief Execute an operation
  void execute() override;

  // @brief accessors to the parameters
  bool isCreate() const {return _isCreate;}
  const std::string & getTimestamp() const {return _timestamp;}
  const std::string & getUserString() const {return _label;}
  const std::string & getDirectory() const {return _id;}
protected:
  // @brief Execute the create operation
  void executeCreate();

  // @brief Execute the delete operation
  void executeDelete();

  bool _isCreate;
  bool _forceBackup;
  std::string _timestamp;   // required for Create from Coordinator
  std::string _label;
  std::string _id;   // required for Delete
  VPackSlice _agencyDump;   // required for Create from Coordinator

};// class RocksDBHotBackupCreate


////////////////////////////////////////////////////////////////////////////////
/// @brief POST:  Creates copy of given hotbackup, stops server, moves copy into
///               production position, restarts process
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupRestore : public RocksDBHotBackup {
public:

  RocksDBHotBackupRestore() = delete;
  explicit RocksDBHotBackupRestore(VPackSlice body, VPackBuilder& report);

  // @brief Validate and extract parameters appropriate to the operation type
  void parseParameters() override;

  // @brief Execute an operation
  void execute() override;

  // @brief accessors to the parameters
  std::string const& getTimestampCurrent() const {return _timestampCurrent;}
  std::string const& getDirectoryRestore() const {return _idRestore;}

  bool createRestoringDirectory(std::string& nameOutput);

 protected:

  bool _saveCurrent;
  std::string _timestampCurrent;
  std::string _idRestore;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief The following wrapper routines simplify unit testing
  ////////////////////////////////////////////////////////////////////////////////


};// class RocksDBHotBackupRestore


////////////////////////////////////////////////////////////////////////////////
/// @brief POST:  Returns array of Hotbackup directory names
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupList : public RocksDBHotBackup {
public:

  RocksDBHotBackupList() = delete;
  explicit RocksDBHotBackupList(VPackSlice body, VPackBuilder& report);

  void parseParameters() override;
  void execute() override;

  void listAll();
  void statId();

protected:
  static std::string loadAgencyJson(std::string filename);

  std::string _listId;
};// class RocksDBHotBackupList


////////////////////////////////////////////////////////////////////////////////
/// @brief POST:  Set lock on rocksdb transactions
///       DELETE:  Remove lock on rocksdb transactions
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupLock : public RocksDBHotBackup {
public:

  RocksDBHotBackupLock() = delete;
  RocksDBHotBackupLock(const VPackSlice body, VPackBuilder& report);
  ~RocksDBHotBackupLock();

  void parseParameters() override;
  void execute() override;

protected:
  bool _isLock;
  unsigned _unlockTimeoutSeconds;

};// class RocksDBHotBackupLock


class RocksDBHotBackupPolicy : public RocksDBHotBackup {
public:

  void execute() override {}

};// class RocksDBHotBackupPolicy


}  // namespace arangodb

#endif
