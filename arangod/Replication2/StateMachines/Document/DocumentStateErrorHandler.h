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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"

namespace arangodb {

struct OperationResult;

namespace replication2::replicated_state::document {
/*
 * During the replication process, errors can occur.
 * This is mainly caused by the fact that the followers rely on a snapshot sent
 * by the leader. The snapshot can be more recent than the log entries that are
 * applied to the followers, which may lead to various conflicts, some of which
 * are safe to ignore. The leader can also encounter such errors during the
 * recovery process.
 * The purpose of this interface is to provide a documented way of handling
 * these errors, such that it is clear what is ignored and what is not.
 */
struct IDocumentStateErrorHandler {
  virtual ~IDocumentStateErrorHandler() = default;

  [[nodiscard]] virtual auto handleOpResult(
      ReplicatedOperation::OperationType const& op,
      Result const& res) const noexcept -> Result = 0;

  [[nodiscard]] virtual auto handleOpResult(ReplicatedOperation const& op,
                                            Result const& res) const noexcept
      -> Result = 0;

  [[nodiscard]] virtual auto handleDocumentTransactionResult(
      arangodb::OperationResult const& res, TransactionId tid) const noexcept
      -> Result = 0;
};

class DocumentStateErrorHandler : public IDocumentStateErrorHandler {
 public:
  explicit DocumentStateErrorHandler(LoggerContext loggerContext)
      : _loggerContext(std::move(loggerContext)) {}

  [[nodiscard]] auto handleOpResult(ReplicatedOperation::CreateShard const& op,
                                    Result const& res) const noexcept -> Result;

  [[nodiscard]] auto handleOpResult(ReplicatedOperation::ModifyShard const& op,
                                    Result const& res) const noexcept -> Result;

  [[nodiscard]] auto handleOpResult(ReplicatedOperation::DropShard const& op,
                                    Result const& res) const noexcept -> Result;

  [[nodiscard]] auto handleOpResult(ReplicatedOperation::CreateIndex const& op,
                                    Result const& res) const noexcept -> Result;

  [[nodiscard]] auto handleOpResult(ReplicatedOperation::DropIndex const& op,
                                    Result const& res) const noexcept -> Result;

  [[nodiscard]] auto handleOpResult(ModifiesUserTransaction auto const& op,
                                    Result const& res) const noexcept
      -> Result {
    if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
      // During follower applyEntries or leader recovery, we might have already
      // dropped the shard this transaction refers to.
      LOG_CTX("098b9", DEBUG, _loggerContext)
          << "Transaction " << op.tid
          << " operation failed because the corresponding shard " << op.shard
          << " was not found, ignoring: " << res;
      return TRI_ERROR_NO_ERROR;
    }
    return res;
  }

  [[nodiscard]] auto handleOpResult(
      FinishesUserTransactionOrIntermediate auto const& op,
      Result const& res) const -> Result {
    return res;
  }

  [[nodiscard]] auto handleOpResult(
      ReplicatedOperation::AbortAllOngoingTrx const& op,
      Result const& res) const -> Result {
    return res;
  }

  [[nodiscard]] auto handleOpResult(
      ReplicatedOperation::OperationType const& op,
      Result const& res) const noexcept -> Result override {
    return std::visit(
        [this, &res](auto const& op) { return handleOpResult(op, res); }, op);
  }

  [[nodiscard]] auto handleOpResult(ReplicatedOperation const& op,
                                    Result const& res) const noexcept
      -> Result override {
    return handleOpResult(op.operation, res);
  }

  [[nodiscard]] auto handleDocumentTransactionResult(
      arangodb::OperationResult const& res, TransactionId tid) const noexcept
      -> Result override;

 private:
  LoggerContext _loggerContext;
};
}  // namespace replication2::replicated_state::document
}  // namespace arangodb
