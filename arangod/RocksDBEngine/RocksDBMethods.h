////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBCommon.h"

#include <memory>

namespace rocksdb {
class Slice;
}  // namespace rocksdb

namespace arangodb {
class RocksDBKey;

class RocksDBMethods {
 public:
  virtual ~RocksDBMethods() = default;

  virtual bool isIndexingDisabled() const { return false; }

  /// @brief returns true if indexing was disabled by this call
  /// the default implementation is to do nothing
  virtual bool DisableIndexing() { return false; }

  // the default implementation is to do nothing
  virtual bool EnableIndexing() { return false; }

  virtual rocksdb::Status Get(rocksdb::ColumnFamilyHandle*,
                              rocksdb::Slice const&, rocksdb::PinnableSlice*,
                              ReadOwnWrites) = 0;
  virtual rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                                       rocksdb::Slice const&,
                                       rocksdb::PinnableSlice*) = 0;
  /// assume_tracked=true will assume you used GetForUpdate on this key earlier.
  /// it will still verify this, so it is slower than PutUntracked
  virtual rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const&,
                              rocksdb::Slice const&, bool assume_tracked) = 0;
  /// Like Put, but will not perform any write-write conflict checks
  virtual rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*,
                                       RocksDBKey const&,
                                       rocksdb::Slice const&) = 0;

  virtual rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*,
                                 RocksDBKey const&) = 0;
  /// contrary to Delete, a SingleDelete may only be used
  /// when keys are inserted exactly once (and never overwritten)
  virtual rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*,
                                       RocksDBKey const&) = 0;

  virtual void PutLogData(rocksdb::Slice const&) = 0;
};

// INDEXING MAY ONLY BE DISABLED IN TOPLEVEL AQL TRANSACTIONS
// THIS IS BECAUSE THESE TRANSACTIONS WILL EITHER READ FROM
// OR (XOR) WRITE TO A COLLECTION. IF THIS PRECONDITION IS
// VIOLATED THE DISABLED INDEXING WILL BREAK GET OPERATIONS.
struct IndexingDisabler {
  // will only be active if condition is true

  IndexingDisabler() = delete;
  IndexingDisabler(IndexingDisabler&&) = delete;
  IndexingDisabler(IndexingDisabler const&) = delete;
  IndexingDisabler& operator=(IndexingDisabler const&) = delete;
  IndexingDisabler& operator=(IndexingDisabler&&) = delete;

  IndexingDisabler(RocksDBMethods* meth, bool condition) : _meth(nullptr) {
    if (condition) {
      bool disabledHere = meth->DisableIndexing();
      if (disabledHere) {
        _meth = meth;
      }
    }
  }

  ~IndexingDisabler() {
    if (_meth) {
      _meth->EnableIndexing();
    }
  }

 private:
  RocksDBMethods* _meth;
};

// if only single indices should be enabled during operations
struct IndexingEnabler {
  // will only be active if condition is true

  IndexingEnabler() = delete;
  IndexingEnabler(IndexingEnabler&&) = delete;
  IndexingEnabler(IndexingEnabler const&) = delete;
  IndexingEnabler& operator=(IndexingEnabler const&) = delete;
  IndexingEnabler& operator=(IndexingEnabler&&) = delete;

  IndexingEnabler(RocksDBMethods* meth, bool condition) : _meth(nullptr) {
    if (condition) {
      bool enableHere = meth->EnableIndexing();
      if (enableHere) {
        _meth = meth;
      }
    }
  }

  ~IndexingEnabler() {
    if (_meth) {
      _meth->DisableIndexing();
    }
  }

 private:
  RocksDBMethods* _meth;
};

}  // namespace arangodb
