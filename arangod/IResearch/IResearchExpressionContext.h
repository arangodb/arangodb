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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT_H
#define ARANGOD_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ExpressionContext.h"
#include "Aql/RegisterPlan.h"
#include "Basics/Exceptions.h"

namespace arangodb {

namespace aql {
class AqlItemBlock;
struct AstNode;
class ExecutionEngine;
class QueryContext;
class AqlFunctionsInternalCache;
}  // namespace aql

namespace iresearch {

class IResearchViewNode;

///////////////////////////////////////////////////////////////////////////////
/// @struct ViewExpressionContextBase
/// @brief FIXME remove this struct once IResearchView will be able to evaluate
///        epxressions with loop variable in SEARCH expressions.
///        simon: currently also used in tests
///////////////////////////////////////////////////////////////////////////////
struct ViewExpressionContextBase : public arangodb::aql::ExpressionContext {
  explicit ViewExpressionContextBase(arangodb::transaction::Methods* trx,
                                     aql::QueryContext* query,
                                     aql::AqlFunctionsInternalCache* cache)
  : ExpressionContext(), _trx(trx), _query(query), _aqlFunctionsInternalCache(cache)  {}
  
  void registerWarning(int errorCode, char const* msg) override final;
  void registerError(int errorCode, char const* msg) override final;
  
  icu::RegexMatcher* buildRegexMatcher(char const* ptr, size_t length,
                                       bool caseInsensitive) override final;
  icu::RegexMatcher* buildLikeMatcher(char const* ptr, size_t length,
                                      bool caseInsensitive) override final;
  icu::RegexMatcher* buildSplitMatcher(aql::AqlValue splitExpression, velocypack::Options const* opts,
                                       bool& isEmptyExpression) override final;

  arangodb::ValidatorBase* buildValidator(arangodb::velocypack::Slice const&) override final;

  TRI_vocbase_t& vocbase() const override final;
  /// may be inaccessible on some platforms
  transaction::Methods& trx() const override final;
  bool killed() const override final;

  aql::AstNode const* _expr{};  // for troubleshooting
  
protected:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::QueryContext* _query;
  arangodb::aql::AqlFunctionsInternalCache* _aqlFunctionsInternalCache;
};                              // ViewExpressionContextBase

///////////////////////////////////////////////////////////////////////////////
/// @struct ViewExpressionContext
///////////////////////////////////////////////////////////////////////////////
struct ViewExpressionContext final : public ViewExpressionContextBase {
  using VarInfoMap = std::unordered_map<aql::VariableId, aql::VarInfo>;

  ViewExpressionContext(arangodb::transaction::Methods& trx,
                        aql::QueryContext& query,
                        aql::AqlFunctionsInternalCache& cache, aql::Variable const& outVar,
                        VarInfoMap const& varInfoMap, int nodeDepth)
      : ViewExpressionContextBase(&trx, &query, &cache),
        _outVar(outVar),
        _varInfoMap(varInfoMap),
        _nodeDepth(nodeDepth) {}

  virtual bool isDataFromCollection(aql::Variable const* variable) const override {
    return variable->isDataFromCollection;
  }

  virtual aql::AqlValue getVariableValue(aql::Variable const* variable, bool doCopy,
                                         bool& mustDestroy) const override;

  inline aql::Variable const& outVariable() const noexcept { return _outVar; }
  inline VarInfoMap const& varInfoMap() const noexcept { return _varInfoMap; }
  inline int nodeDepth() const noexcept { return _nodeDepth; }

  aql::InputAqlItemRow _inputRow{aql::CreateInvalidInputRowHint{}};
  aql::Variable const& _outVar;
  VarInfoMap const& _varInfoMap;
  int const _nodeDepth;
};  // ViewExpressionContext

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT_H 1
