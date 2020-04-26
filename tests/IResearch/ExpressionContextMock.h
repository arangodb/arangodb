////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT
#define ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT 1

#include "Aql/QueryContext.h"
#include "Aql/RegexCache.h"
#include "IResearch/IResearchExpressionContext.h"
#include "Transaction/Methods.h"

#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {

struct Variable; // forward decl

} // aql
} // arangodb

struct ExprCtxMockQuery final : public arangodb::aql::QueryContext {
  
  virtual QueryOptions const& queryOptions() const = 0;
  
  /// @brief pass-thru a resolver object from the transaction context
  virtual CollectionNameResolver const& resolver() const = 0;
  
  virtual velocypack::Options const& vpackOptions() const = 0;
  
  /// @brief create a transaction::Context
  virtual std::shared_ptr<aralgndob::transaction::Context> newTrxContext() const {
    return nullptr;
  }
  
  virtual transaction::Methods& trxForOptimization() = 0;
    
  virtual bool killed() const { return false; }

  virtual void setKilled() {}
  
private:
  std::shared_ptr<arangodb::transaction::StandaloneContext> _ctx;
  arangodb::aql::QueryOptions _queryOptions;
};

struct ExpressionContextMock final : public arangodb::iresearch::ViewExpressionContextBase {

  static std::unique_ptr<arangodb::aql::QueryContext> EMPTY_QUERY;
  static ExpressionContextMock EMPTY;
  
  ExpressionContextMock(arangodb::aql::QueryContext& query)
  : ViewExpressionContextBase(_trx, query, _regexCache), _trx(query.newTrxContext()) {}

  virtual ~ExpressionContextMock();

  virtual bool isDataFromCollection(arangodb::aql::Variable const* variable) const override {
    return variable->isDataFromCollection;
  }

  virtual arangodb::aql::AqlValue getVariableValue(
    arangodb::aql::Variable const* variable,
    bool doCopy,
    bool& mustDestroy
  ) const override;

  arangodb::transaction::Methods _trx;
  arangodb::aql::RegexCache _regexCache;
  std::unordered_map<std::string, arangodb::aql::AqlValue> vars;
}; // ExpressionContextMock

#endif // ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT
