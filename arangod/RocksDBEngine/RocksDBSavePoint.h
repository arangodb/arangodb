////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_SAVEPOINT_H
#define ARANGOD_ROCKSDB_ROCKSDB_SAVEPOINT_H 1

#include "RocksDBEngine/RocksDBCommon.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace transaction {
class Methods;
}

class RocksDBSavePoint {
 public:
  RocksDBSavePoint(transaction::Methods* trx, TRI_voc_document_operation_e operationType);
  ~RocksDBSavePoint();

  /// @brief unconditionally cancel the savepoint
  void cancel();

  /// @brief acknowledges the current savepoint, so there
  /// will be no rollback when the destructor is called
  /// if an intermediate commit was performed, pass a value of
  /// true, false otherwise
  void finish(bool hasPerformedIntermediateCommit);

 private:
  void rollback();

 private:
  transaction::Methods* _trx;
  TRI_voc_document_operation_e const _operationType;
  bool _handled;
};

}  // namespace arangodb

#endif
