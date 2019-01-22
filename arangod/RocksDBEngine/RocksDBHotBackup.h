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
//#include "Basics/Result.h"
//#include "RocksDBColumnFamily.h"
//#include "RocksDBCommon.h"
#include "Rest/GeneralResponse.h"


namespace rocksdb {
class Transaction;
class Slice;
class Iterator;
class TransactionDB;
class WriteBatch;
class WriteBatchWithIndex;
class Comparator;
struct ReadOptions;
}  // namespace rocksdb

namespace arangodb {
namespace transaction {
class Methods;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Base class for various RocksDBHotBackup operations
////////////////////////////////////////////////////////////////////////////////

class RocksDBHotBackup {
public:
  static std::shared_ptr<RocksDBHotBackup> operationFactory(arangodb::rest::RequestType const type,
                                                            std::vector<std::string> const & suffixes,
                                                            VPackSlice & body)
  { return std::shared_ptr<RocksDBHotBackup>();}

  virtual void execute() = 0;

  virtual ~RocksDBHotBackup() {};
  bool valid() const {return _valid;};
  bool success() const {return _success;};

  rest::ResponseCode restResponseCode() const {return _respCode;};
  int restResponseError() const {return _respError;};

protected:

  bool _valid;          // are parameters valid
  bool _success;        // did operation finish successfully

  rest::ResponseCode _respCode;
  int _respError;

};// class RocksDBHotBackup


class RocksDBHotBackupCreate : public RocksDBHotBackup {
public:

  virtual void execute() {};

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
