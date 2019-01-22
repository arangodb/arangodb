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
#include "RocksDBColumnFamily.h"
#include "RocksDBCommon.h"

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
  static RocksDBHotBackup * operationFactory(arangodb::rest::RequestType const type,
                                             std::vector<std::string> const & suffixes,
                                             arangodb::VPackSlice & body);

  virtual void execute() = 0;

  bool valid();
  bool success();

  int??? restResponseCode();
  int??? restResponseError();


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
