////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/QueryExpressionContext.h"
#include "Basics/Exceptions.h"

namespace arangodb {

namespace aql {
class AqlItemBlock;
struct AstNode;
class ExecutionEngine;
} // aql

namespace iresearch {

class IResearchViewNode;

///////////////////////////////////////////////////////////////////////////////
/// @struct ViewExpressionContextBase
/// @brief FIXME remove this struct once IResearchView will be able to evaluate
///        epxressions with loop variable in SEARCH expressions
///////////////////////////////////////////////////////////////////////////////
struct ViewExpressionContextBase : public aql::QueryExpressionContext {
  explicit ViewExpressionContextBase(arangodb::aql::Query* query)
    : aql::QueryExpressionContext(query) {
  }

  aql::AstNode const* _expr{}; // for troubleshooting
}; // ViewExpressionContextBase

///////////////////////////////////////////////////////////////////////////////
/// @struct ViewExpressionContext
///////////////////////////////////////////////////////////////////////////////
struct ViewExpressionContext final : public ViewExpressionContextBase {
  ViewExpressionContext(arangodb::aql::Query* query, IResearchViewNode const& node)
    : ViewExpressionContextBase(query),
      _node(&node) {
    TRI_ASSERT(_node);
  }

  virtual size_t numRegisters() const override;

  virtual aql::AqlValue const& getRegisterValue(size_t i) const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual aql::Variable const* getVariable(size_t i) const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  virtual aql::AqlValue getVariableValue(
    aql::Variable const* variable,
    bool doCopy,
    bool& mustDestroy
  ) const override;

  aql::AqlItemBlock const* _data{};
  IResearchViewNode const* _node;
  size_t _pos{};
}; // ViewExpressionContext

} // iresearch 
} // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT_H 1
