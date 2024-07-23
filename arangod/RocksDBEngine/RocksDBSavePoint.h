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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "RocksDBEngine/Methods/RocksDBTrxMethods.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class RocksDBTransactionState;

class RocksDBSavePoint {
  RocksDBSavePoint(RocksDBSavePoint const& other) = delete;
  RocksDBSavePoint& operator=(RocksDBSavePoint const& other) = delete;

 public:
  RocksDBSavePoint(DataSourceId collectionId, RocksDBTransactionState& state,
                   TRI_voc_document_operation_e operationType);

  ~RocksDBSavePoint();

  void prepareOperation(RevisionId rid);

  /// @brief acknowledges the current savepoint, so there
  /// will be no rollback when the destructor is called
  [[nodiscard]] Result finish(RevisionId rid);

  TRI_voc_document_operation_e operationType() const noexcept {
    return _operationType;
  }

  /// @brief this is going to be called if at least one Put or Delete
  /// has made it into the underyling WBWI. if so, on rollback we must
  /// perform a full rebuild
  void tainted() noexcept { _tainted = true; }

 private:
  void rollback();

  RocksDBTransactionState& _state;
  RocksDBTransactionMethods& _rocksMethods;
  DataSourceId _collectionId;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  uint64_t _numCommitsAtStart;
#endif
  TRI_voc_document_operation_e const _operationType;
  bool _handled;
  bool _tainted;
};

}  // namespace arangodb
