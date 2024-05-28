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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

#include <rocksdb/slice.h>
#include <rocksdb/status.h>

#include <string>
#include <vector>

namespace arangodb::aql {

class MultiGetContext {
 public:
  static constexpr size_t kKeySize = sizeof(uint64_t) * 2;
  static constexpr size_t kBatchSize = 32;  // copied from rocksdb
  // TODO(MBkkt) benchmark and choose best threshold
  static constexpr size_t kThreshold = 1;

  RocksDBTransactionMethods* methods{};
  StorageSnapshot const* snapshot{};
  using ValueType = aql::DocumentData;  // null mean was error
  std::vector<ValueType> values;

  template<typename Func>
  void multiGet(size_t expected, Func&& func);

  size_t serialize(transaction::Methods& trx, LogicalCollection const& logical,
                   LocalDocumentId id) {
    auto* base = logical.getPhysical();
    TRI_ASSERT(base != nullptr);
#ifdef ARANGODB_USE_GOOGLE_TESTS
    auto* physical = dynamic_cast<RocksDBMetaCollection*>(base);
    if (ADB_UNLIKELY(physical == nullptr)) {
      base->lookup(
          &trx, id,
          [&](LocalDocumentId, ValueType&&, VPackSlice doc) {
            values[_global] = std::make_unique<std::string>(
                std::string_view{doc.startAs<char>(), doc.byteSize()});
            return true;
          },
          {.readCache = false, .fillCache = false, .countBytes = true},
          snapshot);
      return _global++;
    }
#else
    auto* physical = static_cast<RocksDBMetaCollection*>(base);
#endif
    if (ADB_UNLIKELY(!methods)) {
      methods = RocksDBTransactionState::toMethods(&trx, logical.id());
    }
    return serialize(physical->objectId(), id);
  }

 private:
  size_t serialize(uint64_t objectId, LocalDocumentId id) noexcept {
    auto* start = _buffer.data() + _local * kKeySize;
    rocksutils::uint64ToPersistentRaw(start, objectId);
    rocksutils::uint64ToPersistentRaw(start + kKeySize / 2, id.id());
    _keys[_local++] = {start, kKeySize};
    return _global++;
  }

  rocksdb::Snapshot const* getSnapshot() const noexcept {
    TRI_ASSERT(snapshot != nullptr);
    auto& impl = static_cast<RocksDBEngine::RocksDBSnapshot const&>(*snapshot);
    auto const* rocksdbSnapshot = impl.getSnapshot();
    TRI_ASSERT(rocksdbSnapshot != nullptr);
    return rocksdbSnapshot;
  }

  // TODO(MBkkt) Maybe move on stack multiGet?
  size_t _global;
  size_t _local;
  std::array<char, kBatchSize * kKeySize> _buffer;
  std::array<rocksdb::Slice, kBatchSize> _keys;
  std::array<rocksdb::Status, kBatchSize> _statuses;
  std::array<rocksdb::PinnableSlice, kBatchSize> _values;
};

enum class MultiGetState : uint8_t {
  kNext = 1U << 0U,
  kWork = 1U << 1U,  // returned from func when we want separate batch
  kLast = 1U << 2U,  // returned from func when no more keys
};

template<typename Func>
void MultiGetContext::multiGet(size_t expected, Func&& func) {
  values.clear();
  values.resize(expected);

  _global = 0;
  _local = 0;

  auto* family = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);

  bool running = true;
  while (running) {
    auto const state = func();
    if (state == MultiGetState::kNext && _local < kBatchSize) {
      continue;
    }
    running = state != MultiGetState::kLast;
    if (_local == 0) {
      continue;
    }
    auto const* snapshot = getSnapshot();
    TRI_ASSERT(family);
    if (_local <= kThreshold) {
      _statuses[0] =
          methods->SingleGet(snapshot, *family, _keys[0], _values[0]);
    } else {
      methods->MultiGet(snapshot, *family, _local, &_keys[0], &_values[0],
                        &_statuses[0]);
    }
    auto end = values.begin() + _global;
    auto it = end - _local;
    for (size_t c = 0; it != end; ++it, ++c) {
      TRI_ASSERT(*it == nullptr);
      if (!_statuses[c].ok()) {
        continue;
      }
      if (_values[c].IsPinned()) {
        *it = std::make_unique<std::string>(_values[c].ToStringView());
      } else {
        *it = std::make_unique<std::string>(std::move(*_values[c].GetSelf()));
      }
    }
    _local = 0;
  }
}

}  // namespace arangodb::aql
