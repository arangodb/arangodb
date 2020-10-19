////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGOD_IRESEARCH__IRESEARCH_CALCULATION_ANALYZER
#define ARANGOD_IRESEARCH__IRESEARCH_CALCULATION_ANALYZER 1

#include "analysis/token_attributes.hpp"
#include "Aql/Ast.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"

#include <string>

namespace arangodb {
namespace iresearch {

class CalculationAnalyzer final : public irs::analysis::analyzer{
 public:
  struct options_t {
    options_t() = default;

    options_t(std::string&& query, bool collapse, bool keep, uint32_t batch)
      : queryString(query), collapseArrayPositions(collapse),
      keepNull(keep), batchSize(batch) {}

    /// @brief Query string to be executed for each document.
    /// Field value is set with @param binded parameter.
    std::string queryString;

    /// @brief determines how processed members of array result:
    /// if set to true all members are considered to be at position 0
    /// if set to false each array members is set at positions serially
    bool collapseArrayPositions{ false };

    /// @brief do not emit empty token if query result is NULL
    /// this could be used fo index filtering.
    bool keepNull{ true };

    /// @brief  batch size for running query. Set to 1 as most of the cases
    /// we expect just 1 to 1 modification query.
    uint32_t batchSize{ 1 };
  };

  static bool parse_options(const irs::string_ref& args, options_t& options);

  static constexpr irs::string_ref type_name() noexcept {
    return "calculation";
  }

  static bool normalize(const irs::string_ref& args, std::string& out);
  static irs::analysis::analyzer::ptr make(irs::string_ref const& args);

  CalculationAnalyzer(options_t const& options);

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<irs::increment>::id()) {
      return &_inc;
    }

    return type == irs::type<irs::term_attribute>::id()
      ? &_term : nullptr;
  }

  virtual bool next() override;
  virtual bool reset(irs::string_ref const& field) noexcept override;

 private:

  class CalculationQueryContext : public arangodb::aql:: QueryContext{
   public:
    CalculationQueryContext(TRI_vocbase_t& vocbase);

    virtual arangodb::aql::QueryOptions const& queryOptions() const override {
      return _queryOptions;
    }

    /// @brief pass-thru a resolver object from the transaction context
    virtual arangodb::CollectionNameResolver const& resolver() const override {
      return _resolver;
    }

    virtual arangodb::velocypack::Options const& vpackOptions() const override {
      return arangodb::velocypack::Options::Defaults;
    }

    /// @brief create a transaction::Context
    virtual std::shared_ptr<arangodb::transaction::Context> newTrxContext() const override;

    virtual arangodb::transaction::Methods& trxForOptimization() override {
      return *_trx;
    }

    virtual bool killed() const override { return false; }

    /// @brief whether or not a query is a modification query
    virtual bool isModificationQuery() const noexcept override { return false; }

    virtual bool isAsyncQuery() const noexcept override { return false; }

    virtual void enterV8Context() override { TRI_ASSERT(FALSE); }

    arangodb::aql::AqlItemBlockManager& itemBlockManager() noexcept {
      return _itemBlockManager;
    }

   private:
    arangodb::aql::QueryOptions _queryOptions;
    arangodb::CollectionNameResolver _resolver;
    mutable arangodb::transaction::StandaloneContext _transactionContext;
    std::unique_ptr<arangodb::transaction::Methods> _trx;
    arangodb::aql::ResourceMonitor _resourceMonitor;
    arangodb::aql::AqlItemBlockManager _itemBlockManager;
  };

  irs::term_attribute _term;
  irs::increment _inc;
  std::string _str;
  options_t _options;
  CalculationQueryContext _query;
  arangodb::aql::ExecutionEngine _engine;
  std::unique_ptr<arangodb::aql::ExecutionPlan> _plan;
  arangodb::aql::SharedAqlItemBlockPtr _queryResults;
  size_t _resultRowIdx{ 0 };
  std::vector<arangodb::aql::AstNode*> _bindedNodes;
  arangodb::aql::ExecutionState _executionState{arangodb::aql::ExecutionState::DONE};
  uint32_t _next_inc_val{0};
}; // CalculationAnalyzer
}
}

#endif // ARANGOD_IRESEARCH__IRESEARCH_CALCULATION_ANALYZER
