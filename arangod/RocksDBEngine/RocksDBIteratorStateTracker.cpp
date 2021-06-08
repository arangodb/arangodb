////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBIteratorStateTracker.h"

#include "Basics/debugging.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"

#include <rocksdb/slice.h>

namespace arangodb {

RocksDBIteratorStateTracker::RocksDBIteratorStateTracker(transaction::Methods* trx)
    : _trx(trx),
      _intermediateCommitId(currentIntermediateCommitId()) {
  TRI_ASSERT(_trx != nullptr);
  TransactionState* state = _trx->state();
  TRI_ASSERT(state != nullptr);
  if (static_cast<RocksDBTransactionState*>(state)->isReadOnlyTransaction()) {
    // turn ourselves off for read-only transactions, for performance reasons
    deactivate();
  }
}

RocksDBIteratorStateTracker::~RocksDBIteratorStateTracker() = default;

bool RocksDBIteratorStateTracker::isActive() const noexcept {
  return _trx != nullptr;
}
  
void RocksDBIteratorStateTracker::trackKey(rocksdb::Slice key) {
  if (isActive()) {
    // track last intermediate commit id
    _intermediateCommitId = currentIntermediateCommitId();
  
    // track last iterator key
    _key.clear();
    _key.append(key.data(), key.size());
  }
}

void RocksDBIteratorStateTracker::reset() {
  if (isActive()) {
    _intermediateCommitId = currentIntermediateCommitId();
    _key.clear();
  }
}

bool RocksDBIteratorStateTracker::mustRebuildIterator() const {
  return isActive() && currentIntermediateCommitId() != _intermediateCommitId;
}

rocksdb::Slice RocksDBIteratorStateTracker::key() const {
  TRI_ASSERT(mustRebuildIterator());
  TRI_ASSERT(_key.size() > 0);
  return rocksdb::Slice(reinterpret_cast<char const*>(_key.data()), _key.size());
}

uint64_t RocksDBIteratorStateTracker::currentIntermediateCommitId() const {
  TRI_ASSERT(isActive());
  TransactionState* state = _trx->state();
  TRI_ASSERT(state != nullptr);
  return static_cast<RocksDBTransactionState*>(state)->intermediateCommitId();
}

void RocksDBIteratorStateTracker::deactivate() {
  // we simply unset _trx here and check it everywhere using isActive().
  // that way we don't have to store an extra boolean flag somewhere
  _trx = nullptr;
}

}  // namespace arangodb
