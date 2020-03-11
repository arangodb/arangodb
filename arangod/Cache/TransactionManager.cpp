////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <cstdint>

#include "Cache/TransactionManager.h"

#include "Basics/cpu-relax.h"
#include "Basics/debugging.h"
#include "Cache/Transaction.h"

namespace arangodb::cache {

TransactionManager::TransactionManager()
    : _data({{0,0,0}, 0}) {}

Transaction* TransactionManager::begin(bool readOnly) {
  Transaction* tx = new Transaction(readOnly);

  Data newData;
  if (readOnly) {
    Data data = _data.load(std::memory_order_relaxed);
    do {
      tx->sensitive = false;
      newData = data;
      newData.counters.openReads++;
      if (newData.counters.openWrites > 0) {
        tx->sensitive = true;
        newData.counters.openSensitive++;
      }
    } while (!_data.compare_exchange_strong(data, newData, std::memory_order_acq_rel, std::memory_order_relaxed));
  } else {
    tx->sensitive = true;
    Data data = _data.load(std::memory_order_relaxed);
    do {
      newData = data;
      newData.counters.openWrites++;
      if (newData.counters.openSensitive == 0) {
        newData.term++;
      }
      if (newData.counters.openWrites++ == 0) {
        newData.counters.openSensitive = data.counters.openReads + 1;
      } else {
        newData.counters.openSensitive++;
      }
    } while (!_data.compare_exchange_strong(data, newData, std::memory_order_acq_rel, std::memory_order_relaxed));
  }
  tx->term = newData.term;
  return tx;
}

void TransactionManager::end(Transaction* tx) noexcept {
  TRI_ASSERT(tx != nullptr);

  Data data = _data.load(std::memory_order_relaxed);
  Data newData;
  do {
    if (((data.term & static_cast<uint64_t>(1)) > 0) && (data.term > tx->term)) {
      tx->sensitive = true;
    }

    auto newData = data;
    if (tx->readOnly) {
      newData.counters.openReads--;
    } else {
      newData.counters.openWrites--;
    }

    if (tx->sensitive && (--newData.counters.openSensitive == 0)) {
      newData.term++;
    }
  } while (!_data.compare_exchange_strong(data, newData, std::memory_order_acq_rel, std::memory_order_relaxed));

  delete tx;
}

uint64_t TransactionManager::term() { return _data.load(std::memory_order_acquire).term; }

}  // namespace arangodb::cache
