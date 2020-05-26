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
#include "Aql/AqlFunctionsInternalCache.h"
#include "IResearch/IResearchExpressionContext.h"
#include "Transaction/Methods.h"

#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {

struct Variable; // forward decl

} // aql
} // arangodb

struct ExpressionContextMock final : public arangodb::iresearch::ViewExpressionContextBase {

  static ExpressionContextMock EMPTY;

  ExpressionContextMock() : ViewExpressionContextBase(nullptr, nullptr, nullptr) {}

  virtual ~ExpressionContextMock();

  virtual bool isDataFromCollection(arangodb::aql::Variable const* variable) const override {
    return variable->isDataFromCollection;
  }

  virtual arangodb::aql::AqlValue getVariableValue(
    arangodb::aql::Variable const* variable,
    bool doCopy,
    bool& mustDestroy
  ) const override;

  void setTrx(arangodb::transaction::Methods* trx) {
    this->_trx = trx;
  }

  arangodb::aql::AqlFunctionsInternalCache _regexCache;
  std::unordered_map<std::string, arangodb::aql::AqlValue> vars;
}; // ExpressionContextMock

#endif // ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT
