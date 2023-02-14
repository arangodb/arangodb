////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

#include "Basics/application-exit.h"
#include "Basics/voc-errors.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Transaction/ReplicatedContext.h"
#include "DocumentStateHandlersFactory.h"

namespace {

auto shouldIgnoreError(arangodb::OperationResult const& res) noexcept -> bool {
  auto ignoreError = [](ErrorCode code) {
    /*
     * These errors are ignored because the snapshot can be more recent than
     * the applied log entries.
     * TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED could happen during insert
     * operations.
     * TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND could happen during
     * remove operations.
     */
    return code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
           code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  };

  if (res.fail() && !ignoreError(res.errorNumber())) {
    return false;
  }

  for (auto const& [code, cnt] : res.countErrorCodes) {
    if (!ignoreError(code)) {
      return false;
    }
  }
  return true;
}

auto makeResultFromOperationResult(arangodb::OperationResult const& res,
                                   arangodb::TransactionId tid)
    -> arangodb::Result {
  ErrorCode e{res.result.errorNumber()};
  std::stringstream msg;
  if (!res.countErrorCodes.empty()) {
    if (e == TRI_ERROR_NO_ERROR) {
      e = TRI_ERROR_TRANSACTION_INTERNAL;
    }
    msg << "Transaction " << tid << " error codes: ";
    for (auto const& it : res.countErrorCodes) {
      msg << it.first << ' ';
    }
    if (res.hasSlice()) {
      msg << "; Full result: " << res.slice().toJson();
    }
  }
  return arangodb::Result{e, std::move(msg).str()};
}

}  // namespace

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransactionHandler::DocumentStateTransactionHandler(
    GlobalLogIdentifier gid, TRI_vocbase_t* vocbase,
    std::shared_ptr<IDocumentStateHandlersFactory> factory)
    : _gid(std::move(gid)), _vocbase(vocbase), _factory(std::move(factory)) {
#ifndef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_vocbase != nullptr);
#endif
}

auto DocumentStateTransactionHandler::getTrx(TransactionId tid)
    -> std::shared_ptr<IDocumentStateTransaction> {
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

  TRI_ASSERT(doc.tid.isFollowerTransactionId());

  try {
    auto trx = ensureTransaction(doc);
    TRI_ASSERT(trx != nullptr);
    switch (doc.operation) {
      case OperationType::kInsert:
      case OperationType::kUpdate:
      case OperationType::kReplace:
      case OperationType::kRemove:
      case OperationType::kTruncate: {
        auto opRes = trx->apply(doc);
        auto res = opRes.fail() ? opRes.result
                                : makeResultFromOperationResult(opRes, doc.tid);
        if (res.fail() && shouldIgnoreError(opRes)) {
          LoggerContext logContext =
              LoggerContext(Logger::REPLICATED_STATE)
                  .with<logContextKeyDatabaseName>(_gid.database)
                  .with<logContextKeyLogId>(_gid.id);

          LOG_CTX("0da00", INFO, logContext)
              << "Result ignored while applying transaction " << doc.tid
              << " with operation " << int(doc.operation) << " on shard "
              << doc.shardId << ": " << res;
          return Result{};
        }
        return res;
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
        FATAL_ERROR_EXIT();
      case OperationType::kIntermediateCommit:
        return trx->intermediateCommit();
      default:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
    }
  } catch (basics::Exception& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception& e) {
    return Result{TRI_ERROR_TRANSACTION_INTERNAL, e.what()};
  }
}

auto DocumentStateTransactionHandler::ensureTransaction(
    DocumentLogEntry const& doc) -> std::shared_ptr<IDocumentStateTransaction> {
  TRI_ASSERT(doc.operation != OperationType::kAbortAllOngoingTrx);

  auto tid = doc.tid;
  auto trx = getTrx(tid);
  if (trx != nullptr) {
    return trx;
  }

  trx = _factory->createTransaction(doc, *_vocbase);
  _transactions.emplace(tid, trx);
  return trx;
}

void DocumentStateTransactionHandler::removeTransaction(TransactionId tid) {
  _transactions.erase(tid);
}

auto DocumentStateTransactionHandler::getUnfinishedTransactions() const
    -> TransactionMap const& {
  return _transactions;
}

}  // namespace arangodb::replication2::replicated_state::document
