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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Containers/NodeHashMap.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "roaring.hh"

namespace rocksdb {
class ColumnFamilyHandle;
class Status;
}  // namespace rocksdb

namespace arangodb {
class RocksDBMethods;
class RocksDBMethodsMemoryTracker;

namespace bitmap_index {

/// Collects the bitmap index writes of a transaction or write batch per key,
/// so that a key many documents share gets one merge operand per flush
/// instead of one per document. Mirrors the savepoints of its owner: changes
/// made after the latest savepoint can be rolled back. Must be flushed before
/// its owner commits or reads its own writes from a bitmap index, at a time
/// without active savepoints. Memory is reported to the owner's tracker like
/// write batch growth: it is only given back when the tracker is reset.
class PendingWrites {
 public:
  explicit PendingWrites(RocksDBMethodsMemoryTracker& memoryTracker) noexcept;
  ~PendingWrites();

  /// add (insert) or remove a document id for the term with the given count
  /// key; bitmapKey is the count key followed by the shard of the id
  void add(rocksdb::ColumnFamilyHandle* cf, std::string_view countKey,
           std::string_view bitmapKey, std::uint32_t offset, bool insert);

  /// write one merge operand per changed key and forget everything
  rocksdb::Status flush(RocksDBMethods& methods);

  void clear() noexcept;
  bool empty() const noexcept { return _entries.empty(); }

  void setSavePoint();
  void rollbackToSavePoint();
  void popSavePoint() noexcept;

 private:
  struct Entry {
    rocksdb::ColumnFamilyHandle* cf = nullptr;
    roaring::Roaring adds;
    roaring::Roaring removes;
    std::int64_t count = 0;
  };
  struct Change {
    Entry* count;
    Entry* bitmap;
    std::uint32_t offset;
    bool insert;
    bool wasAdded;
    bool wasRemoved;
  };
  Entry& entry(rocksdb::ColumnFamilyHandle* cf, std::string_view key);

  RocksDBMethodsMemoryTracker& _memoryTracker;
  containers::NodeHashMap<std::string, Entry> _entries;
  /// changes since the oldest active savepoint, to undo them
  std::vector<Change> _changes;
  /// number of changes at each active savepoint
  std::vector<std::size_t> _savePoints;
};

}  // namespace bitmap_index
}  // namespace arangodb
