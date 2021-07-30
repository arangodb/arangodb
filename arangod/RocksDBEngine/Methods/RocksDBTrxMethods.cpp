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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTrxMethods.h"

#include "Aql/QueryCache.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Statistics/ServerStatistics.h"

#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBTrxBaseMethods(state, db) {
  TRI_ASSERT(!_state->isSingleOperation());
}

RocksDBTrxMethods::~RocksDBTrxMethods() {
  // cleanupTransaction is a virtual function, so we need to override the dtor and
  // call it ourselves here, because the call in the base class would only execute
  // the base class implementation, therefore leaking the iteratorReadSnapshot.
  cleanupTransaction();
}

Result RocksDBTrxMethods::beginTransaction() {
  Result result = RocksDBTrxBaseMethods::beginTransaction();
  
  TRI_ASSERT(_iteratorReadSnapshot == nullptr);
  if (result.ok() && hasIntermediateCommitsEnabled()) {
    TRI_ASSERT(_state->options().intermediateCommitCount != UINT64_MAX ||
               _state->options().intermediateCommitSize != UINT64_MAX);
    _iteratorReadSnapshot = _db->GetSnapshot();  // must call ReleaseSnapshot later
    TRI_ASSERT(_iteratorReadSnapshot != nullptr);
  }
  return result;
}

rocksdb::ReadOptions RocksDBTrxMethods::iteratorReadOptions() const {
  if (hasIntermediateCommitsEnabled()) {
    rocksdb::ReadOptions ro = _readOptions;
    TRI_ASSERT(_iteratorReadSnapshot);
    ro.snapshot = _iteratorReadSnapshot;
    return ro;
  }
  return _readOptions;
}

void RocksDBTrxMethods::prepareOperation(DataSourceId cid, RevisionId rid,
                                         TRI_voc_document_operation_e operationType) {
  TRI_ASSERT(_rocksTransaction != nullptr);
  if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    RocksDBLogValue logValue = RocksDBLogValue::DocumentRemoveV2(rid);
    _rocksTransaction->PutLogData(logValue.slice());
    ++_numLogdata;
  }
}

/// @brief undo the effects of the previous prepareOperation call
void RocksDBTrxMethods::rollbackOperation(TRI_voc_document_operation_e operationType) {
  ++_numRollbacks;
  if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    TRI_ASSERT(_numLogdata > 0);
    --_numLogdata;
  }
}

Result RocksDBTrxMethods::checkIntermediateCommit(bool& hasPerformedIntermediateCommit) {
  // perform an intermediate commit if necessary
  size_t currentSize = _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  return checkIntermediateCommit(currentSize, hasPerformedIntermediateCommit); 
}

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewIterator(
    rocksdb::ColumnFamilyHandle* cf, ReadOptionsCallback readOptionsCallback) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  
  rocksdb::ReadOptions opts = _readOptions;
  if (hasIntermediateCommitsEnabled()) {
    TRI_ASSERT(_iteratorReadSnapshot);
    opts.snapshot = _iteratorReadSnapshot;
  }
  if (readOptionsCallback) {
    readOptionsCallback(opts);
  }
  
  std::unique_ptr<rocksdb::Iterator> iterator(_rocksTransaction->GetIterator(opts, cf));
  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid iterator in RocksDBTrxMethods");
  }
  return iterator;
}

void RocksDBTrxMethods::cleanupTransaction() {
  RocksDBTrxBaseMethods::cleanupTransaction();
  if (_iteratorReadSnapshot != nullptr) {
    TRI_ASSERT(hasIntermediateCommitsEnabled());
    _db->ReleaseSnapshot(_iteratorReadSnapshot);
    _iteratorReadSnapshot = nullptr;
  }
}

void RocksDBTrxMethods::createTransaction() {
  RocksDBTrxBaseMethods::createTransaction();
  // add transaction begin marker
  auto header =
      RocksDBLogValue::BeginTransaction(_state->vocbase().id(), _state->id());

  _rocksTransaction->PutLogData(header.slice());
  TRI_ASSERT(_numLogdata == 0);
  ++_numLogdata;
}

Result RocksDBTrxMethods::triggerIntermediateCommit(bool& hasPerformedIntermediateCommit) {
  TRI_ASSERT(!hasPerformedIntermediateCommit);

  TRI_IF_FAILURE("FailBeforeIntermediateCommit") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SegfaultBeforeIntermediateCommit") {
    TRI_TerminateDebugging("SegfaultBeforeIntermediateCommit");
  }

  TRI_ASSERT(!_state->isSingleOperation());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC("0fe63", DEBUG, Logger::ENGINES) << "INTERMEDIATE COMMIT!";
#endif

  Result res = doCommit();
  if (res.fail()) {
    // FIXME: do we abort the transaction ?
    return res;
  }

  hasPerformedIntermediateCommit = true;
  ++_state->statistics()._intermediateCommits;
  
  // reset counters for DML operations, but intentionally don't reset
  // the commit counter, as we need to track if we had intermediate commits
  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
  _numLogdata = 0;

  TRI_IF_FAILURE("FailAfterIntermediateCommit") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SegfaultAfterIntermediateCommit") {
    TRI_TerminateDebugging("SegfaultAfterIntermediateCommit");
  }

  createTransaction();
  _readOptions.snapshot = _rocksTransaction->GetSnapshot();
  TRI_ASSERT(_iteratorReadSnapshot != nullptr);
  TRI_ASSERT(_readOptions.snapshot != nullptr);
  return TRI_ERROR_NO_ERROR;
}

Result RocksDBTrxMethods::checkIntermediateCommit(uint64_t newSize, bool& hasPerformedIntermediateCommit) {
  hasPerformedIntermediateCommit = false;
    
  TRI_IF_FAILURE("noIntermediateCommits") {
    return TRI_ERROR_NO_ERROR;
  }

  if (hasIntermediateCommitsEnabled()) {
    auto numOperations = this->numOperations();
    // perform an intermediate commit
    // this will be done if either the "number of operations" or the
    // "transaction size" counters have reached their limit
    if (_state->options().intermediateCommitCount <= numOperations ||
        _state->options().intermediateCommitSize <= newSize) {
      return triggerIntermediateCommit(hasPerformedIntermediateCommit);
    }
  }
  return TRI_ERROR_NO_ERROR;
}

bool RocksDBTrxMethods::hasIntermediateCommitsEnabled() const noexcept {
  return _state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS); 
}
  