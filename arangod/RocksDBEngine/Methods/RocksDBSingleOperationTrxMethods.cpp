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

#include "RocksDBSingleOperationTrxMethods.h"

#include "Aql/QueryCache.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Statistics/ServerStatistics.h"

#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

RocksDBSingleOperationTrxMethods::RocksDBSingleOperationTrxMethods(
    RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBTrxBaseMethods(state, db) {
  TRI_ASSERT(_state->isSingleOperation());
  TRI_ASSERT(!_state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS));
}

rocksdb::ReadOptions RocksDBSingleOperationTrxMethods::iteratorReadOptions()
    const {
  // This should never be called for a single operation transaction.
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "should not call iteratorReadOptions for single operation methods");
}

void RocksDBSingleOperationTrxMethods::prepareOperation(
    DataSourceId cid, RevisionId rid,
    TRI_voc_document_operation_e operationType) {
  TRI_ASSERT(_rocksTransaction != nullptr);

  // singleOp => no modifications yet
  TRI_ASSERT(_rocksTransaction->GetNumPuts() == 0 &&
             _rocksTransaction->GetNumDeletes() == 0);
  switch (operationType) {
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
      break;

    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE: {
      auto logValue = RocksDBLogValue::SinglePut(_state->vocbase().id(), cid);

      _rocksTransaction->PutLogData(logValue.slice());
      TRI_ASSERT(_numLogdata == 0);
      _numLogdata++;
      break;
    }
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE: {
      TRI_ASSERT(rid.isSet());

      auto logValue =
          RocksDBLogValue::SingleRemoveV2(_state->vocbase().id(), cid, rid);

      _rocksTransaction->PutLogData(logValue.slice());
      TRI_ASSERT(_numLogdata == 0);
      _numLogdata++;
      break;
    }
  }
}

/// @brief undo the effects of the previous prepareOperation call
void RocksDBSingleOperationTrxMethods::rollbackOperation(
    TRI_voc_document_operation_e operationType) {
  ++_numRollbacks;
  switch (operationType) {
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE: {
      TRI_ASSERT(_numLogdata > 0);
      --_numLogdata;
      break;
    }
    default: {
      break;
    }
  }
}

std::unique_ptr<rocksdb::Iterator>
RocksDBSingleOperationTrxMethods::NewIterator(rocksdb::ColumnFamilyHandle*,
                                              ReadOptionsCallback) {
  // This should never be called for a single operation transaction.
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "should not call NewIterator for single operation methods");
}

bool RocksDBSingleOperationTrxMethods::iteratorMustCheckBounds(
    ReadOwnWrites) const {
  // This should never be called for a single operation transaction.
  TRI_ASSERT(false);
  return false;
}
