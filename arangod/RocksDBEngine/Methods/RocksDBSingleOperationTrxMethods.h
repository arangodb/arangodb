////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/Methods/RocksDBTrxBaseMethods.h"

namespace arangodb {

/// transaction wrapper, uses the current rocksdb transaction
class RocksDBSingleOperationTrxMethods : public RocksDBTrxBaseMethods {
 public:
  explicit RocksDBSingleOperationTrxMethods(RocksDBTransactionState*,
                                            rocksdb::TransactionDB* db);

  rocksdb::ReadOptions iteratorReadOptions() const override;

  void prepareOperation(DataSourceId cid, RevisionId rid,
                        TRI_voc_document_operation_e operationType) override;

  /// @brief undo the effects of the previous prepareOperation call
  void rollbackOperation(TRI_voc_document_operation_e operationType) override;

  std::unique_ptr<rocksdb::Iterator> NewIterator(
      rocksdb::ColumnFamilyHandle*,
      ReadOptionsCallback readOptionsCallback) override;

  bool iteratorMustCheckBounds(ReadOwnWrites) const override;
};

}  // namespace arangodb
