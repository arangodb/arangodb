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
    ReplicatedOperation::CreateShard const& op, Result const& res) const
    -> Result {
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
    ReplicatedOperation::DropShard const& op, Result const& res) const
    -> Result {
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("098b9", DEBUG, _loggerContext)
        << "Shard " << op.shard
        << " drop failed because it was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::ModifyShard const& op, Result const& res) const
    -> Result {
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("098b9", DEBUG, _loggerContext)
        << "Shard " << op.shard
        << " modification failed because it was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::CreateIndex const& op, Result const& res) const
    -> Result {
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("19bd8", DEBUG, _loggerContext)
        << "Index creation " << op.properties.toJson() << " on shard "
        << op.shard
        << " failed because the shard was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::handleOpResult(
    ReplicatedOperation::DropIndex const& op, Result const& res) const
    -> Result {
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the shard.
    LOG_CTX("a8971", DEBUG, _loggerContext)
        << "Index drop " << op.index.toJson() << " on shard " << op.shard
        << " failed because the shard was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  if (res.is(TRI_ERROR_ARANGO_INDEX_NOT_FOUND)) {
    // During follower applyEntries or leader recovery, we might have already
    // dropped the index. Therefore, it's possible to try a "double-drop".
    LOG_CTX("50835", DEBUG, _loggerContext)
        << "Index drop " << op.index.toJson() << " on shard " << op.shard
        << " failed because the index was not found, ignoring: " << res;
    return TRI_ERROR_NO_ERROR;
  }
  return res;
}

auto DocumentStateErrorHandler::shouldIgnoreDocumentError(
    arangodb::OperationResult const& res) noexcept -> bool {
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
  } else {
    LOG_CTX("f1be8", TRACE, _loggerContext)
        << "Ignoring document error: " << res.errorMessage();
  }

  for (auto const& [code, cnt] : res.countErrorCodes) {
    if (!ignoreError(code)) {
      return false;
    } else {
      LOG_CTX("90219", TRACE, _loggerContext)
          << "Ignoring document error: " << code << " " << cnt;
    }
  }
  return true;
}
}  // namespace arangodb::replication2::replicated_state::document
