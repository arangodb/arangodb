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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <cstdint>
#include <memory>

#include "Cache/TransactionManager.h"

#include "Basics/debugging.h"
#include "Cache/Manager.h"
#include "Cache/Transaction.h"

namespace arangodb::cache {

// note: manager can be a null pointer in unit tests
TransactionManager::TransactionManager(Manager* manager)
    : _state({{0, 0, 0}, 0}), _manager(manager) {}

void TransactionManager::begin(Transaction& tx, bool readOnly) {
  tx = Transaction{readOnly};

  State newState;
  if (readOnly) {
    State state = _state.load(std::memory_order_relaxed);
    do {
      tx.sensitive = false;
      newState = state;
      newState.counters.openReads++;
      if (newState.counters.openWrites > 0) {
        tx.sensitive = true;
        newState.counters.openSensitive++;
      }
    } while (!_state.compare_exchange_strong(
        state, newState, std::memory_order_acq_rel, std::memory_order_relaxed));
  } else {
    tx.sensitive = true;
    State state = _state.load(std::memory_order_relaxed);
    do {
      newState = state;
      if (newState.counters.openSensitive == 0) {
        newState.term++;
      }
      if (newState.counters.openWrites++ == 0) {
        newState.counters.openSensitive = state.counters.openReads + 1;
      } else {
        newState.counters.openSensitive++;
      }
    } while (!_state.compare_exchange_strong(
        state, newState, std::memory_order_acq_rel, std::memory_order_relaxed));
  }
  tx.term = newState.term;
  TRI_ASSERT(tx.term != Transaction::kInvalidTerm);
}

void TransactionManager::end(Transaction& tx) noexcept {
  TRI_ASSERT(tx.term != Transaction::kInvalidTerm);

  State state = _state.load(std::memory_order_relaxed);
  State newState;
  do {
    if (((state.term & static_cast<uint64_t>(1)) > 0) &&
        (state.term > tx.term)) {
      tx.sensitive = true;
    }

    newState = state;
    if (tx.readOnly) {
      newState.counters.openReads--;
    } else {
      newState.counters.openWrites--;
    }

    if (tx.sensitive && (--newState.counters.openSensitive == 0)) {
      newState.term++;
    }
  } while (!_state.compare_exchange_strong(
      state, newState, std::memory_order_acq_rel, std::memory_order_relaxed));

  tx = {};
}

uint64_t TransactionManager::term() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_manager != nullptr) {
    _manager->trackTermCall();
  }
#endif
  return _state.load(std::memory_order_acquire).term;
}

}  // namespace arangodb::cache
