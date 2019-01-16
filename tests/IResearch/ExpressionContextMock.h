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

#include "Aql/FixedVarExpressionContext.h"
#include "IResearch/IResearchExpressionContext.h"

#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {

struct Variable; // forward decl

} // aql
} // arangodb

struct ExpressionContextMock final : arangodb::iresearch::ViewExpressionContextBase {
  static ExpressionContextMock EMPTY;

  ExpressionContextMock() noexcept
    : arangodb::iresearch::ViewExpressionContextBase(nullptr) {
  }

  virtual ~ExpressionContextMock();

  virtual size_t numRegisters() const override {
    TRI_ASSERT(false);
    return 0;
  }

  virtual arangodb::aql::AqlValue const& getRegisterValue(size_t) const override {
    TRI_ASSERT(false);
    static arangodb::aql::AqlValue EMPTY;
    return EMPTY;
  }

  virtual arangodb::aql::Variable const* getVariable(size_t i) const override {
    TRI_ASSERT(false);
    return nullptr;
  }

  virtual arangodb::aql::AqlValue getVariableValue(
    arangodb::aql::Variable const* variable,
    bool doCopy,
    bool& mustDestroy
  ) const override;
  
  void registerWarning(int, char const*) override {
    TRI_ASSERT(false);
  }

  void registerError(int, char const*) override {
    TRI_ASSERT(false);
  }
  
  icu::RegexMatcher* buildRegexMatcher(char const* ptr, size_t length, bool caseInsensitive) override {
    TRI_ASSERT(false);
    return nullptr;
  }

  icu::RegexMatcher* buildLikeMatcher(char const* ptr, size_t length, bool caseInsensitive) override {
    TRI_ASSERT(false);
    return nullptr;
  }

  icu::RegexMatcher* buildSplitMatcher(arangodb::aql::AqlValue splitExpression, arangodb::transaction::Methods*, bool& isEmptyExpression) override {
    TRI_ASSERT(false);
    return nullptr;
  }

//  bool killed() const override {
//    TRI_ASSERT(false);
//    return false;
//  }
//
//  TRI_vocbase_t& vocbase() const override;
//
//  arangodb::aql::Query* query() const override;

  std::unordered_map<std::string, arangodb::aql::AqlValue> vars;
}; // ExpressionContextMock

#endif // ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_CONTEXT
