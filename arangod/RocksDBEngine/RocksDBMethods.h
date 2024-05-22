////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <cstddef>

namespace rocksdb {
class Slice;
}  // namespace rocksdb

namespace arangodb {
class RocksDBKey;

class RocksDBMethods {
 public:
  virtual ~RocksDBMethods() = default;

  virtual bool isIndexingDisabled() const noexcept { return false; }

  /// @brief returns true if indexing was disabled by this call
  /// the default implementation is to do nothing
  virtual bool DisableIndexing() { return false; }

  // the default implementation is to do nothing
  virtual bool EnableIndexing() { return false; }

  virtual rocksdb::Status Get(rocksdb::ColumnFamilyHandle*,
                              rocksdb::Slice const&, rocksdb::PinnableSlice*,
                              ReadOwnWrites) = 0;

  virtual rocksdb::Status SingleGet(rocksdb::Snapshot const* snapshot,
                                    rocksdb::ColumnFamilyHandle& family,
                                    rocksdb::Slice const& key,
                                    rocksdb::PinnableSlice& value) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "RocksDBMethods does not provide Get from snapshot");
  }

  // Read multiple keys from snapshot and return multiple values and statuses
  virtual void MultiGet(rocksdb::Snapshot const* snapshot,
                        rocksdb::ColumnFamilyHandle& family, size_t count,
                        rocksdb::Slice const* keys,
                        rocksdb::PinnableSlice* values,
                        rocksdb::Status* status) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "RocksDBMethods does not provide MultiGet from snapshot");
  }

  // Read multiple keys return multiple values and statuses
  virtual void MultiGet(rocksdb::ColumnFamilyHandle& family, size_t count,
                        rocksdb::Slice const* keys,
                        rocksdb::PinnableSlice* values, rocksdb::Status* status,
                        ReadOwnWrites) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "RocksDBMethods does not provide MultiGet");
  }

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

  /// @brief function to calculate overhead of a WriteBatchWithIndex entry,
  /// depending on keySize. will return 0 if indexing is disabled in the current
  /// transaction.
  static size_t indexingOverhead(bool indexingEnabled, size_t keySize) noexcept;
  static size_t indexingOverhead(size_t keySize) noexcept;

  /// @brief function to calculate overhead of a lock entry, depending on
  /// keySize. will return 0 if no locks are used by the current transaction
  /// (e.g. if the transaction is using an exclusive lock)
  static size_t lockOverhead(bool lockingEnabled, size_t keySize) noexcept;

  /// @brief assumed additional indexing overhead for each entry in a
  /// WriteBatchWithIndex. this is in addition to the actual WriteBuffer entry.
  /// the WriteBatchWithIndex keeps all entries (which are pointers) in a
  /// skiplist. it is unclear from the outside how much memory the skiplist
  /// will use per entry, so this value here is just a guess.
  static constexpr size_t fixedIndexingEntryOverhead = 32;

  /// @brief assumed additional overhead for each lock that is held by the
  /// transaction. the overhead is computed as follows:
  /// locks are stored in rocksdb in a hash table, which maps from the locked
  /// key to a LockInfo struct. The LockInfo struct is 120 bytes big.
  /// we also assume some more overhead for the hash table entries and some
  /// general overhead because the hash table is never assumed to be completely
  /// full (load factor < 1). thus we assume 80 bytes of overhead for each
  /// entry. this is an arbitrary value.
  static constexpr size_t fixedLockEntryOverhead = 120 + 80;

  /// @brief assumed additional overhead for making a dynamic memory allocation
  /// for an std::string value that exceeds the string's internal SSO buffer.
  static constexpr size_t memoryAllocationOverhead = 8;
};

}  // namespace arangodb
