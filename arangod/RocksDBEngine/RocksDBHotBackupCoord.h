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

#ifndef ARANGOD_ROCKSDB_HOTBACKUP_COORD_H
#define ARANGOD_ROCKSDB_HOTBACKUP_COORD_H 1

#include "RocksDBHotBackup.h"

namespace arangodb {

class RocksDBHotBackupCreateCoord : public RocksDBHotBackup {

 public:
  RocksDBHotBackupCreateCoord() = delete;
  RocksDBHotBackupCreateCoord(VPackSlice body)
    : RocksDBHotBackup(body) {}

  void execute() override {}

};// class RocksDBHotBackupCreate


class RocksDBHotBackupRestoreCoord : public RocksDBHotBackup {
 public:

  RocksDBHotBackupRestoreCoord() = delete;
  RocksDBHotBackupRestoreCoord(VPackSlice body)
    : RocksDBHotBackup(body) {}

  void execute() override {}

};// class RocksDBHotBackupRestore


class RocksDBHotBackupListCoord : public RocksDBHotBackup {
public:

  RocksDBHotBackupListCoord() = delete;
  RocksDBHotBackupListCoord(const VPackSlice body)
    : RocksDBHotBackup(body) {};

  void execute() override {};

};// class RocksDBHotBackupList


class RocksDBHotBackupLockCoord : public RocksDBHotBackup {
public:

  RocksDBHotBackupLockCoord() = delete;
  RocksDBHotBackupLockCoord(const VPackSlice body)
    : RocksDBHotBackup(body) {};

  void execute() override {};

};// class RocksDBHotBackupLock


class RocksDBHotBackupPolicyCoord : public RocksDBHotBackup {
 public:

  void execute() override {}

};// class RocksDBHotBackupPolicy

}  // namespace arangodb

#endif
