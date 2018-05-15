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

#ifndef ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_STATE_H
#define ARANGOD_ROCKSDB_ROCKSDB_TRANSACTION_STATE_H 1

#include "Basics/Common.h"
#include "Basics/SmallVector.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
namespace transaction {
class Methods;
struct Options;
}
  
class TransactionCollection;
/// @brief transaction type
class ClusterTransactionState final : public TransactionState {

 public:
  ClusterTransactionState(TRI_vocbase_t* vocbase, TRI_voc_tid_t,
                          transaction::Options const&);
  ~ClusterTransactionState();

  /// @brief begin a transaction
  Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  Result abortTransaction(transaction::Methods* trx) override;

  uint64_t numCommits() const { return _numCommits; }
  uint64_t numInserts() const { return _numInserts; }
  uint64_t numUpdates() const { return _numUpdates; }
  uint64_t numRemoves() const { return _numRemoves; }

  inline bool hasOperations() const {
    return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
  }

  bool hasFailedOperations() const override {
    return (_status == transaction::Status::ABORTED) && hasOperations();
  }
};

}  // namespace arangodb

#endif
