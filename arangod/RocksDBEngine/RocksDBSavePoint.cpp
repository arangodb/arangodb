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

#include "RocksDBSavePoint.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Methods.h"

namespace arangodb {

RocksDBSavePoint::RocksDBSavePoint(RocksDBTransactionState* state,
                                   transaction::Methods* trx,
                                   TRI_voc_document_operation_e operationType)
    : _state(state),
      _trx(trx),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _numCommitsAtStart(0),
#endif
      _operationType(operationType),
      _handled(_trx->isSingleOperationTransaction()),
      _tainted(false) {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(trx != nullptr);

  if (!_handled) {
    auto mthds = RocksDBTransactionState::toMethods(_trx);
    // only create a savepoint when necessary
    mthds->SetSavePoint();
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _numCommitsAtStart = _state->numCommits();
#endif
}

RocksDBSavePoint::~RocksDBSavePoint() {
  if (!_handled) {
    try {
      // only roll back if we create a savepoint and have
      // not performed an intermediate commit in-between
      rollback();
    } catch (std::exception const& ex) {
      LOG_TOPIC("519ed", ERR, Logger::ENGINES)
          << "caught exception during rollback to savepoint: " << ex.what();
    } catch (...) {
      // whatever happens during rollback, no exceptions are allowed to escape
      // from here
    }
  }
}

void RocksDBSavePoint::prepareOperation(TRI_voc_cid_t cid, TRI_voc_rid_t rid) {
  TRI_ASSERT(!_tainted);

  _state->prepareOperation(cid, rid, _operationType);
}

/// @brief acknowledges the current savepoint, so there
/// will be no rollback when the destructor is called
Result RocksDBSavePoint::finish(TRI_voc_cid_t cid, TRI_voc_rid_t rid) {
  bool hasPerformedIntermediateCommit = false;
  Result res = basics::catchToResult([&]() -> Result {
    return _state->addOperation(cid, rid, _operationType, hasPerformedIntermediateCommit);
  });

  if (!_handled) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_numCommitsAtStart + (hasPerformedIntermediateCommit ? 1 : 0) == _state->numCommits());
#endif

    if (res.ok()) {
      if (!hasPerformedIntermediateCommit) {
        // pop the savepoint from the transaction in order to
        // save some memory for transactions with many operations
        // this is only safe to do when we have a created a savepoint
        // when creating the guard, and when there hasn't been an
        // intermediate commit in the transaction
        // when there has been an intermediate commit, we must
        // leave the savepoint alone, because it belonged to another
        // transaction, and the current transaction will not have any
        // savepoint
        auto mthds = RocksDBTransactionState::toMethods(_trx);
        mthds->PopSavePoint();
      }
    
      // this will prevent the rollback call in the destructor
      _handled = true;
    } else {
      TRI_ASSERT(res.fail());
      if (hasPerformedIntermediateCommit) {
        // very rare case reached only during testing
        _handled = true;
      }
    }
  }

  return res;
}

void RocksDBSavePoint::rollback() {
  TRI_ASSERT(!_handled);
  auto mthds = RocksDBTransactionState::toMethods(_trx);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_numCommitsAtStart == _state->numCommits());
#endif

  rocksdb::Status s;
  if (_tainted) {
    // we have written at least one Put or Delete operation after
    // we created the savepoint. because that has modified the
    // WBWI, we need to do a full rebuild
    s = mthds->RollbackToSavePoint();
  } else {
    // we have written only LogData values since we created the
    // savepoint. we can get away by rolling back the WBWI's
    // underlying WriteBatch only. this is a lot faster (simple
    // std::string::resize instead of a full rebuild of the WBWI
    // from the WriteBatch)
    s = mthds->RollbackToWriteBatchSavePoint();
  }  
  TRI_ASSERT(s.ok());

  _state->rollbackOperation(_operationType);

  _handled = true;  // in order to not roll back again by accident
}

} // namespace
