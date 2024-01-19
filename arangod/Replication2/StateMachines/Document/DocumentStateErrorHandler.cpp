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

#include "Replication2/StateMachines/Document/DocumentStateErrorHandler.h"

#include "Utils/OperationResult.h"

namespace arangodb::replication2::replicated_state::document {
auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::CreateShard const& op,
    Result const& res) const noexcept -> Result {
  if (res.is(TRI_ERROR_ARANGO_DUPLICATE_NAME)) {
    // During follower applyEntries or leader recovery, we might have already
    // created the shard.
    LOG_CTX("1577a", DEBUG, _loggerContext)
        << "Shard " << op.shard
        << " creation failed because it already exists, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::DropShard const& op, Result const& res) const noexcept
    -> Result {
  // This method is also used to prevent crashes on the leader while dropping a
  // shard locally. If the shard is not there, there's not reason to panic -
  // followers will probably notice the same thing.
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("ce21f", DEBUG, _loggerContext)
        << "Shard " << op.shard
        << " drop failed because it was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::ModifyShard const& op,
    Result const& res) const noexcept -> Result {
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("2fec0", DEBUG, _loggerContext)
        << "Shard " << op.shard
        << " modification failed because it was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::CreateIndex const& op,
    Result const& res) const noexcept -> Result {
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("19bd8", DEBUG, _loggerContext)
        << "Index creation " << op.properties.toJson() << " on shard "
        << op.shard
        << " failed because the shard was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  if (res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
    // During follower applyEntries or leader recovery, we might run into a
    // situation where replaying an index creation is impossible. For example,
    // the index is created, then dropped, then duplicate documents are
    // inserted.
    LOG_CTX("a7289", DEBUG, _loggerContext)
        << "Index creation " << op.properties.toJson() << " on shard "
        << op.shard
        << " failed because the collection no longer corresponds to its "
           "constraints, ignoring: "
        << res;
    return TRI_ERROR_NO_ERROR;
  }
  if (res.is(TRI_ERROR_BAD_PARAMETER)) {
    // If there is another TTL index already, the
    // `RocksDBCollection::createIndex` throws the bad parameter error code.
    LOG_CTX("b4f7a", DEBUG, _loggerContext)
        << "Index creation " << op.properties.toJson() << " on shard "
        << op.shard
        << " failed because a TTL index already exists, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::DropIndex const& op, Result const& res) const noexcept
    -> Result {
  // This method is also used to prevent crashes on the leader while
  // creating/dropping an index. While applying a DropIndex, there's no
  // guarantee that the index or the shard is still there. However, if this
  // happens, we can safely ignore the error - the index is already gone. Note
  // that the undo operation after a failed CreateIndex is a DropIndex, so the
  // same logic is applied there.

  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("a8971", DEBUG, _loggerContext)
        << "Index drop " << op.indexId << " on shard " << op.shard
        << " failed because the shard was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  if (res.is(TRI_ERROR_ARANGO_INDEX_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the index. Therefore, it's possible to try a "double-drop".
    LOG_CTX("50835", DEBUG, _loggerContext)
        << "Index drop " << op.indexId << " on shard " << op.shard
        << " failed because the index was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleDocumentTransactionResult(
    arangodb::OperationResult const& res, TransactionId tid) const noexcept
    -> Result {
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

  auto makeResultFromOperationResult =
      [tid](arangodb::OperationResult const& res) -> arangodb::Result {
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
  };

  if (res.fail() && !ignoreError(res.errorNumber())) {
    return makeResultFromOperationResult(res);
  } else {
    if (res.fail()) {
      LOG_CTX("f1be8", DEBUG, _loggerContext)
          << "Ignoring document error: " << Result{res.errorNumber()} << " "
          << res.errorMessage();
    }
  }

  for (auto const& [code, cnt] : res.countErrorCodes) {
    if (!ignoreError(code)) {
      return makeResultFromOperationResult(res);
    } else {
      LOG_CTX("90219", DEBUG, _loggerContext)
          << "Ignoring document error: " << Result{code} << " " << cnt;
    }
  }

  return {};
}
}  // namespace arangodb::replication2::replicated_state::document
