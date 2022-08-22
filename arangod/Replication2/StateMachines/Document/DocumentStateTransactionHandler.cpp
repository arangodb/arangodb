////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

#include "Basics/voc-errors.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "Transaction/ReplicatedContext.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransactionHandler::DocumentStateTransactionHandler(
    GlobalLogIdentifier gid, DatabaseFeature& databaseFeature)
    : _gid(std::move(gid)), _db(databaseFeature, _gid.database) {}

auto DocumentStateTransactionHandler::getTrx(TransactionId tid)
    -> std::shared_ptr<DocumentStateTransaction> {
  auto it = _transactions.find(tid);
  if (it == _transactions.end()) {
    return nullptr;
  }
  return it->second;
}

auto DocumentStateTransactionHandler::applyEntry(DocumentLogEntry doc)
    -> Result {
  if (doc.operation == OperationType::kAbortAllOngoingTrx) {
    _transactions.clear();
    return Result{};
  }

  try {
    auto trx = ensureTransaction(doc);
    TRI_ASSERT(trx != nullptr);
    switch (doc.operation) {
      case OperationType::kInsert:
      case OperationType::kUpdate:
      case OperationType::kReplace:
      case OperationType::kRemove:
      case OperationType::kTruncate: {
        auto res = trx->apply(doc);
        if (res.fail() && res.ignoreDuringRecovery()) {
          LoggerContext logContext =
              LoggerContext(Logger::REPLICATED_STATE)
                  .with<logContextKeyDatabaseName>(_gid.database)
                  .with<logContextKeyLogId>(_gid.id);
          LOG_CTX("0da00", INFO, logContext)
              << "Result ignored while applying transaction " << doc.tid
              << " with operation " << to_string(doc.operation) << " on shard "
              << doc.shardId << ": " << res.result();
          return Result{};
        }
        return res.result();
      }
      case OperationType::kCommit: {
        auto res = trx->commit();
        removeTransaction(doc.tid);
        return res;
      }
      case OperationType::kAbort: {
        auto res = trx->abort();
        removeTransaction(doc.tid);
        return res;
      }
      case OperationType::kAbortAllOngoingTrx:
        TRI_ASSERT(false);  // should never happen as it should be handled above
      default:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
    }

  } catch (basics::Exception& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception& e) {
    return Result{TRI_ERROR_TRANSACTION_INTERNAL, e.what()};
  }
}

auto DocumentStateTransactionHandler::ensureTransaction(DocumentLogEntry doc)
    -> std::shared_ptr<IDocumentStateTransaction> {
  auto tid = doc.tid;
  auto trx = getTrx(tid);
  if (trx != nullptr) {
    return trx;
  }

  TRI_ASSERT(doc.operation != OperationType::kCommit &&
             doc.operation != OperationType::kAbort);

  auto options = transaction::Options();
  options.isFollowerTransaction = true;
  options.allowImplicitCollectionsForWrite = true;

  auto state = std::make_shared<SimpleRocksDBTransactionState>(_db.database(),
                                                               tid, options);

  auto ctx = std::make_shared<transaction::ReplicatedContext>(tid, state);

  auto methods = std::make_unique<transaction::Methods>(
      std::move(ctx), doc.shardId, AccessMode::Type::WRITE);

  // TODO Why is GLOBAL_MANAGED necessary?
  methods->addHint(transaction::Hints::Hint::GLOBAL_MANAGED);

  auto res = methods->begin();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  trx = std::make_shared<DocumentStateTransaction>(std::move(methods));
  _transactions.emplace(tid, trx);

  return trx;
}

void DocumentStateTransactionHandler::removeTransaction(TransactionId tid) {
  _transactions.erase(tid);
}

}  // namespace arangodb::replication2::replicated_state::document
