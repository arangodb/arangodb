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

// We want to test old behavior for some configuration
// Linux arm choosed because most developers use x86 or apple arm
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE) && defined(__linux__) && \
    defined(__aarch64__)
inline constexpr bool kEnableMultiGet = false;
#else
inline constexpr bool kEnableMultiGet = true;
#endif

struct MultiGetContext {
  RocksDBTransactionMethods* methods{};
  StorageSnapshot const* snapshot{};
  std::string keysBuffer;
  std::vector<rocksdb::Slice> keys;
  std::vector<rocksdb::Status> statuses;
  std::vector<rocksdb::PinnableSlice> values;

  template<typename Func>
  void multiGet(size_t expected, Func&& func);

  void serialize(transaction::Methods& trx, LogicalCollection const& logical,
                 LocalDocumentId id) {
    auto& physical = static_cast<RocksDBCollection&>(*logical.getPhysical());
    if (ADB_UNLIKELY(!methods)) {
      methods = RocksDBTransactionState::toMethods(&trx, logical.id());
    }

    auto const objectId = physical.objectId();
    auto const start = keysBuffer.size();
    rocksutils::uint64ToPersistent(keysBuffer, objectId);
    rocksutils::uint64ToPersistent(keysBuffer, id.id());
    TRI_ASSERT(keysBuffer.size() - start == sizeof(uint64_t) * 2);

    keys.emplace_back(keysBuffer.data() + start, sizeof(uint64_t) * 2);
  }
};

enum class MultiGetState : uint8_t {
  kNext = 1U << 0U,
  kWork = 1U << 1U,
  kLast = 1U << 2U,
};

template<typename Func>
void MultiGetContext::multiGet(size_t expected, Func&& func) {
  keysBuffer.clear();
  keysBuffer.reserve(expected * sizeof(uint64_t) * 2);
  keys.clear();
  keys.reserve(expected);
  if (statuses.size() < expected) {
    statuses.resize(expected);
    values.resize(expected);
  }
  auto& family = *RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);
  size_t last = 0;
  size_t i = 0;
  bool running = true;
  while (running) {
    switch (func(i)) {
      case MultiGetState::kNext:
        break;
      case MultiGetState::kLast:
        running = false;
        [[fallthrough]];
      case MultiGetState::kWork:
        if (i != last) {
          TRI_ASSERT(snapshot != nullptr);
          auto& rocksdbSnapshot =
              static_cast<RocksDBEngine::RocksDBSnapshot const&>(*snapshot);
          methods->MultiGet(rocksdbSnapshot.getSnapshot(), family, i - last,
                            keys.data() + last, values.data() + last,
                            statuses.data() + last);
        }
        last = i;
        break;
    }
  }
}

}  // namespace arangodb::aql
