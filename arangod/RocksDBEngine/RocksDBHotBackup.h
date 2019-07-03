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
#include "Cluster/ResultT.h"
#include "Rest/CommonDefines.h"
#include "VocBase/Methods/Version.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief Struct containing meta data for backups
////////////////////////////////////////////////////////////////////////////////

struct BackupMeta {
  std::string _id;
  std::string _version;
  std::string _datetime;

  static constexpr const char *ID = "id";
  static constexpr const char *VERSION = "version";
  static constexpr const char *DATETIME = "datetime";

  void toVelocyPack(VPackBuilder &builder) const {
    {
      VPackObjectBuilder ob(&builder);
      builder.add(ID, VPackValue(_id));
      builder.add(VERSION, VPackValue(_version));
      builder.add(DATETIME, VPackValue(_datetime));
    }
  }

  static ResultT<BackupMeta> fromSlice(VPackSlice const& slice) {
    try {
      BackupMeta meta;
      meta._id = slice.get(ID).copyString();
      meta._version  = slice.get(VERSION).copyString();
      meta._datetime = slice.get(DATETIME).copyString();
      return meta;
    } catch (std::exception const& e) {
      return ResultT<BackupMeta>::error(TRI_ERROR_BAD_PARAMETER, e.what());
    }
  }

  BackupMeta(std::string const& id, std::string const& version, std::string const& datetime) :
    _id(id), _version(version), _datetime(datetime) {}

private:
  BackupMeta() {}
};

//std::ostream& operator<<(std::ostream& os, const BackupMeta& bm);

////////////////////////////////////////////////////////////////////////////////
/// @brief Base class for various RocksDBHotBackup operations
////////////////////////////////////////////////////////////////////////////////

class RocksDBHotBackup : public std::enable_shared_from_this<RocksDBHotBackup> {
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

  /// @brief Find specific local backup with _listId
  void statId(std::string const& id, VPackBuilder& result, bool report = true);

  /// @brief Loads the meta data associated to the given backup id.
  ResultT<BackupMeta> getMeta(std::string const& id);
  Result writeMeta(std::string const& id, BackupMeta const& meta);

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

  /// @brief Returns true if the version can be restored
  static bool versionTestRestore(std::string const& ver) {
    using arangodb::methods::Version;
    using arangodb::methods::VersionResult;
    return Version::compare(Version::current(), Version::parseVersion(ver.c_str())) == VersionResult::VERSION_MATCH;
  }

protected:
  virtual std::string buildDirectoryPath(std::string const& timestamp, std::string const& userString);
  bool clearPath(std::string const& path);

  static std::string loadAgencyJson(std::string filename);

  // retrieve configuration values from request's body
  void getParamValue(char const* key, std::string& value, bool required);
  void getParamValue(char const* key, bool& value, bool required);
  void getParamValue(char const* key, unsigned& value, bool required);
  void getParamValue(char const* key, VPackSlice& value, bool required);
  void getParamValue(char const* key, double& value, bool required);
  void getParamValue(char const* key, VPackBuilder& value, bool required);

  VPackSlice const _body;   // request's configuration
  bool _valid;          // are parameters valid
  bool _success;        // did operation finish successfully

  rest::ResponseCode _respCode;
  int _respError;
  std::string _errorMessage;
  VPackBuilder& _result;

  bool _isSingle;       // is single db server (not cluster)

  double _timeoutSeconds; // used to stop transaction, used again to stop rocksdb
  std::string _listId;

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
  explicit RocksDBHotBackupCreate(VPackSlice body, VPackBuilder& report, bool isCreate);

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
  bool validateVersionString(std::string const& fullDirectoryRestore);

 protected:

  bool _saveCurrent;
  bool _ignoreVersion;
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

};// class RocksDBHotBackupList


////////////////////////////////////////////////////////////////////////////////
/// @brief POST:  Set lock on rocksdb transactions
///       DELETE:  Remove lock on rocksdb transactions
////////////////////////////////////////////////////////////////////////////////
class RocksDBHotBackupLock : public RocksDBHotBackup {
public:

  RocksDBHotBackupLock() = delete;
  RocksDBHotBackupLock(const VPackSlice body, VPackBuilder& report, bool isLock);
  ~RocksDBHotBackupLock();

  void parseParameters() override;

  void execute() override;

protected:
  bool _isLock;
  double _unlockTimeoutSeconds;

};// class RocksDBHotBackupLock


class RocksDBHotBackupPolicy : public RocksDBHotBackup {
public:

  void execute() override {}

};// class RocksDBHotBackupPolicy


}  // namespace arangodb

#endif
