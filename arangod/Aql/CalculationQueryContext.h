////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/QueryContext.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/vocbase.h"
#include "CalculationTransactionState.h"
#include "CalculationTransactionContext.h"

namespace arangodb::aql {

class CalculationQueryContext final : public arangodb::aql::QueryContext {
 public:
  explicit CalculationQueryContext(
      TRI_vocbase_t& vocbase, transaction::OperationOrigin operationOrigin);

  virtual ~CalculationQueryContext() = default;

  arangodb::aql::QueryOptions const& queryOptions() const override {
    return _queryOptions;
  }

  arangodb::aql::QueryOptions& queryOptions() noexcept override {
    return _queryOptions;
  }

  double getLockTimeout() const noexcept override {
    return _queryOptions.transactionOptions.lockTimeout;
  }

  void setLockTimeout(double timeout) noexcept override {
    _queryOptions.transactionOptions.lockTimeout = timeout;
  }

  /// @brief pass-thru a resolver object from the transaction context
  arangodb::CollectionNameResolver const& resolver() const override {
    return _resolver;
  }

  arangodb::velocypack::Options const& vpackOptions() const override {
    return arangodb::velocypack::Options::Defaults;
  }

  /// @brief create a transaction::Context
  std::shared_ptr<arangodb::transaction::Context> newTrxContext()
      const override {
    return std::shared_ptr<arangodb::transaction::Context>(
        std::shared_ptr<arangodb::transaction::Context>(),
        &_transactionContext);
  }

  arangodb::transaction::Methods& trxForOptimization() override {
    return *_trx;
  }

  bool killed() const override;

  void resetRuntimeClock();

  void debugKillQuery() override {}

  /// @brief whether or not a query is a modification query
  bool isModificationQuery() const noexcept override { return false; }

  bool isAsyncQuery() const noexcept override { return false; }

  void enterV8Executor() override;

 private:
  arangodb::aql::QueryOptions _queryOptions;
  arangodb::CollectionNameResolver _resolver;
  mutable CalculationTransactionContext _transactionContext;
  std::unique_ptr<arangodb::transaction::Methods> _trx;

  // Reset in resetRuntimeClock() and is used in killed() to check if the query
  // execution time has run past the QueryOptions::maxRuntime.
  double _startTime;
};

}  // namespace arangodb::aql