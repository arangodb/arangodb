////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
  using ValueType = std::unique_ptr<uint8_t[]>;  // null mean was error
  std::vector<ValueType> values;

  template<typename Func>
  void multiGet(size_t expected, Func&& func);

  void serialize(transaction::Methods& trx, LogicalCollection const& logical,
                 LocalDocumentId id) {
    auto& physical = static_cast<RocksDBCollection&>(*logical.getPhysical());
    if (ADB_UNLIKELY(!methods)) {
      methods = RocksDBTransactionState::toMethods(&trx, logical.id());
    }
    serialize(physical.objectId(), id);
  }

 private:
  void serialize(uint64_t objectId, LocalDocumentId id) noexcept {
    rocksutils::uint64ToPersistentRaw(_buffer.data(), objectId);
    rocksutils::uint64ToPersistentRaw(_buffer.data(), id.id());
    _keys[_pos++] = {_buffer.data() + _bytes, kKeySize};
    _bytes += kKeySize;
  }

  rocksdb::Snapshot const* getSnapshot() const noexcept {
    TRI_ASSERT(snapshot != nullptr);
    auto& impl = static_cast<RocksDBEngine::RocksDBSnapshot const&>(*snapshot);
    auto const* rocksdbSnapshot = impl.getSnapshot();
    TRI_ASSERT(rocksdbSnapshot != nullptr);
    return rocksdbSnapshot;
  }

  // TODO(MBkkt) Maybe move on stack multiGet?
  size_t _pos;
  size_t _bytes;
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

  _pos = 0;
  _bytes = 0;

  auto& family = *RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);

  bool running = true;
  size_t i = 0;
  while (running) {
    auto const state = func(i);
    if (state == MultiGetState::kNext && _pos < kBatchSize) {
      continue;
    }
    running = state != MultiGetState::kLast;
    if (_pos == 0) {
      continue;
    }
    auto const* snapshot = getSnapshot();
    if (_pos <= kThreshold) {
      _statuses[0] = methods->Get(snapshot, family, _keys[0], _values[0]);
    } else {
      methods->MultiGet(snapshot, family, _pos, &_keys[0], &_values[0],
                        &_statuses[0]);
    }
    auto it = values.begin() + i, end = it + _pos;
    for (size_t c = 0; it != end; ++it, ++c) {
      TRI_ASSERT(!*it);
      if (!_statuses[c].ok()) {
        continue;
      }
      // TODO(MBkkt) Maybe we can avoid call byteSize() and rely on size()?
      auto const* data = _values[c].data();
      VPackSlice slice{reinterpret_cast<uint8_t const*>(data)};
      const auto size = slice.byteSize();
      *it = ValueType{new uint8_t[size]};
      memcpy(it->get(), data, size);
    }
    _pos = 0;
    _bytes = 0;
  }
}

}  // namespace arangodb::aql
