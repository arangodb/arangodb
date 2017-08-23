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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "Cache/TransactionManager.h"
#include "Basics/cpu-relax.h"
#include "Cache/Transaction.h"

#include <stdint.h>
#include <atomic>

using namespace arangodb::cache;

TransactionManager::TransactionManager()
    : _openReads(0), _openSensitive(0), _openWrites(0), _term(0), _lock(false) {}

Transaction* TransactionManager::begin(bool readOnly) {
  Transaction* tx = new Transaction(readOnly);

  lock();

  if (readOnly) {
    _openReads++;
    if (_openWrites.load() > 0) {
      tx->sensitive = true;
      _openSensitive++;
    }
  } else {
    tx->sensitive = true;
    if (_openSensitive.load() == 0) {
      _term++;
    }
    if (_openWrites.load() == 0) {
      _openSensitive = _openReads.load() + _openWrites.load();
    }
    _openWrites++;
    _openSensitive++;
  }

  tx->term = _term.load();

  unlock();

  return tx;
}

void TransactionManager::end(Transaction* tx) {
  TRI_ASSERT(tx != nullptr);
  lock();

  // if currently in sensitive phase, and transaction term is old, it was
  // upgraded to sensitive status
  if (((_term & static_cast<uint64_t>(1)) > 0) && (_term > tx->term)) {
    tx->sensitive = true;
  }

  if (tx->readOnly) {
    _openReads--;
  } else {
    _openWrites--;
  }

  if (tx->sensitive && (--_openSensitive == 0)) {
    _term++;
  }

  unlock();

  delete tx;
}

uint64_t TransactionManager::term() { return _term.load(); }

void TransactionManager::lock() {
  while (true) {
    while (_lock.load(std::memory_order_relaxed)) {
      basics::cpu_relax();
    }
    bool expected = false;
    if (_lock.compare_exchange_weak(expected, true, std::memory_order_acq_rel,
                                    std::memory_order_relaxed)) {
      return;
    }
    basics::cpu_relax();
  }
}

void TransactionManager::unlock() {
  _lock.store(false, std::memory_order_release);
}
