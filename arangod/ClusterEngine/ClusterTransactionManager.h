////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_TRANSACTION_MANAGER_H
#define ARANGOD_CLUSTER_CLUSTER_TRANSACTION_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "StorageEngine/TransactionManager.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class ClusterTransactionManager final : public TransactionManager {
 public:
  ClusterTransactionManager() : TransactionManager(), _nrRunning(0) {}
  ~ClusterTransactionManager() = default;

  // register a transaction
  void registerTransaction(TRI_voc_tid_t /*transactionId*/,
                           bool /*isReadOnlyTransaction*/) override {
    ++_nrRunning;
  }

  // unregister a transaction
  void unregisterTransaction(TRI_voc_tid_t transactionId, bool /*isReadOnlyTransaction*/) override {
    --_nrRunning;
  }

  uint64_t getActiveTransactionCount() override { return _nrRunning; }

 private:
  std::atomic<uint64_t> _nrRunning;
};
}  // namespace arangodb

#endif
