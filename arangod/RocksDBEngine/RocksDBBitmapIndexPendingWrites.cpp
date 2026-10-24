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

#include "RocksDBBitmapIndexPendingWrites.h"

#include "Basics/debugging.h"
#include "RocksDBEngine/RocksDBBitmapIndexFormat.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBMethodsMemoryTracker.h"

#include <rocksdb/status.h>

namespace arangodb::bitmap_index {
namespace {
/// rough memory of a map entry beyond its key
constexpr std::uint64_t kEntryOverhead = 128;
/// rough memory of one id in a Roaring bitmap
constexpr std::uint64_t kOffsetOverhead = 4;
}  // namespace

PendingWrites::PendingWrites(
    RocksDBMethodsMemoryTracker& memoryTracker) noexcept
    : _memoryTracker(memoryTracker) {}

PendingWrites::~PendingWrites() = default;

PendingWrites::Entry& PendingWrites::entry(rocksdb::ColumnFamilyHandle* cf,
                                           std::string_view key) {
  auto [it, inserted] = _entries.try_emplace(key);
  if (inserted) {
    it->second.cf = cf;
    _memoryTracker.increaseMemoryUsage(key.size() + kEntryOverhead);
  }
  return it->second;
}

void PendingWrites::add(rocksdb::ColumnFamilyHandle* cf,
                        std::string_view countKey, std::string_view bitmapKey,
                        std::uint32_t offset, bool insert) {
  Entry& count = entry(cf, countKey);
  Entry& bitmap = entry(cf, bitmapKey);
  Change change{.count = &count,
                .bitmap = &bitmap,
                .offset = offset,
                .insert = insert,
                .wasAdded = bitmap.adds.contains(offset),
                .wasRemoved = bitmap.removes.contains(offset)};
  if (!_savePoints.empty()) {
    _changes.push_back(change);
  }
  _memoryTracker.increaseMemoryUsage(kOffsetOverhead);
  if (insert) {
    bitmap.adds.add(offset);
    bitmap.removes.remove(offset);
    ++count.count;
  } else {
    bitmap.removes.add(offset);
    bitmap.adds.remove(offset);
    --count.count;
  }
}

rocksdb::Status PendingWrites::flush(RocksDBMethods& methods) {
  TRI_ASSERT(_savePoints.empty());
  rocksdb::Status s;
  RocksDBKey key;
  for (auto& [k, e] : _entries) {
    std::string value;
    if (!e.adds.isEmpty() || !e.removes.isEmpty()) {
      value = encodeDelta(e.adds, e.removes);
    } else if (e.count != 0) {
      value = encodeCount(ValueTag::CountDelta, e.count);
    } else {
      // changes cancelled out
      continue;
    }
    key.constructFromBuffer(k);
    s = methods.MergeUntracked(e.cf, key, value);
    if (!s.ok()) {
      break;
    }
  }
  clear();
  return s;
}

void PendingWrites::clear() noexcept {
  _entries.clear();
  _changes.clear();
  _savePoints.clear();
}

void PendingWrites::setSavePoint() {
  _savePoints.push_back(_changes.size());
}

void PendingWrites::rollbackToSavePoint() {
  TRI_ASSERT(!_savePoints.empty());
  std::size_t const changes = _savePoints.back();
  _savePoints.pop_back();
  while (_changes.size() > changes) {
    Change const& c = _changes.back();
    if (c.wasAdded) {
      c.bitmap->adds.add(c.offset);
    } else {
      c.bitmap->adds.remove(c.offset);
    }
    if (c.wasRemoved) {
      c.bitmap->removes.add(c.offset);
    } else {
      c.bitmap->removes.remove(c.offset);
    }
    c.count->count += c.insert ? -1 : 1;
    _changes.pop_back();
  }
  // entries created since the savepoint stay; flush skips them when empty
  if (_savePoints.empty()) {
    _changes.clear();
  }
}

void PendingWrites::popSavePoint() noexcept {
  TRI_ASSERT(!_savePoints.empty());
  _savePoints.pop_back();
  if (_savePoints.empty()) {
    _changes.clear();
  }
}

}  // namespace arangodb::bitmap_index
