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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Methods.h"

namespace arangodb {

RocksDBSavePoint::RocksDBSavePoint(transaction::Methods* trx,
                                   TRI_voc_document_operation_e operationType)
    : _trx(trx),
      _operationType(operationType),
      _handled(_trx->isSingleOperationTransaction()) {
  TRI_ASSERT(trx != nullptr);
  if (!_handled) {
    auto mthds = RocksDBTransactionState::toMethods(_trx);
    // only create a savepoint when necessary
    mthds->SetSavePoint();
  }
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

void RocksDBSavePoint::cancel() {
  // unconditionally cancel the savepoint
  if (!_handled) {
    auto mthds = RocksDBTransactionState::toMethods(_trx);
    mthds->PopSavePoint();
  
    // this will prevent the rollback call in the destructor
    _handled = true;
  }
}

void RocksDBSavePoint::finish(bool hasPerformedIntermediateCommit) {
  if (!_handled && !hasPerformedIntermediateCommit) {
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
}

void RocksDBSavePoint::rollback() {
  TRI_ASSERT(!_handled);
  auto mthds = RocksDBTransactionState::toMethods(_trx);
  mthds->RollbackToSavePoint();

  auto state = RocksDBTransactionState::toState(_trx);
  state->rollbackOperation(_operationType);

  _handled = true;  // in order to not roll back again by accident
}

} // namespace
