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

#pragma once

#include "Aql/QueryContext.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "IResearch/IResearchExpressionContext.h"
#include "Transaction/Methods.h"

#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {

struct Variable;  // forward decl

}  // namespace aql
}  // namespace arangodb

struct ExpressionContextMock final
    : public arangodb::iresearch::ViewExpressionContextBase {
  static ExpressionContextMock EMPTY;

  ExpressionContextMock()
      : ViewExpressionContextBase(nullptr, nullptr, nullptr) {}

  ~ExpressionContextMock();

  arangodb::aql::AqlValue getVariableValue(
      arangodb::aql::Variable const* variable, bool doCopy,
      bool& mustDestroy) const override;

  void setVariable(arangodb::aql::Variable const* variable,
                   arangodb::velocypack::Slice value) override {
    // do nothing
  }

  void clearVariable(
      arangodb::aql::Variable const* variable) noexcept override {
    // do nothing
  }

  void setTrx(arangodb::transaction::Methods* trx) { this->_trx = trx; }

  arangodb::aql::AqlFunctionsInternalCache _regexCache;
  std::unordered_map<std::string, arangodb::aql::AqlValue> vars;
};  // ExpressionContextMock
