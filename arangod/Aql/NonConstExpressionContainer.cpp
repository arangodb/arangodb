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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "NonConstExpressionContainer.h"

#include "Aql/Expression.h"
#include "Aql/NonConstExpression.h"
#include "Basics/StringUtils.h"

#include <velocypack/Iterator.h>

namespace arangodb::aql {

constexpr const char expressionKey[] = "expression";
constexpr const char indexPathKey[] = "indexPath";
constexpr const char expressionsKey[] = "expressions";
constexpr const char varMappingKey[] = "varMapping";

void NonConstExpressionContainer::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder containerObject(&builder);
  builder.add(VPackValue(expressionsKey));
  {
    VPackArrayBuilder expressionsArray(&builder);
    for (auto const& exp : _expressions) {
      VPackObjectBuilder expressionObject(&builder);
      builder.add(VPackValue(expressionKey));
      exp->expression->toVelocyPack(builder, true);
      builder.add(VPackValue(indexPathKey));
      {
        VPackArrayBuilder indexPathArray(&builder);
        for (auto const& it : exp->indexPath) {
          builder.add(VPackValue(it));
        }
      }
    }
  }
  builder.add(VPackValue(varMappingKey));
  {
    VPackObjectBuilder variableObject(&builder);
    for (auto const& [varId, regId] : _varToRegisterMapping) {
      builder.add(basics::StringUtils::itoa(varId), VPackValue(regId.value()));
    }
  }
}

NonConstExpressionContainer NonConstExpressionContainer::clone(Ast* ast) const {
  // We need the AST to clone expressions, which are captured in unique_ptrs
  // and hance do not have default copy operators.
  decltype(_expressions) expressions{};
  expressions.reserve(_expressions.size());
  for (auto const& e : _expressions) {
    expressions.emplace_back(std::make_unique<NonConstExpression>(
        e->expression->clone(ast), e->indexPath));
  }

  return NonConstExpressionContainer{std::move(expressions),
                                     _varToRegisterMapping};
}

NonConstExpressionContainer NonConstExpressionContainer::fromVelocyPack(
    Ast* ast, velocypack::Slice slice) {
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.hasKey(expressionsKey));
  TRI_ASSERT(slice.hasKey(varMappingKey));

  auto exprs = slice.get(expressionsKey);
  TRI_ASSERT(exprs.isArray());

  NonConstExpressionContainer result;

  for (auto const& exp : VPackArrayIterator(exprs)) {
    TRI_ASSERT(exp.isObject());
    TRI_ASSERT(exp.hasKey(expressionKey));
    TRI_ASSERT(exp.hasKey(indexPathKey));

    std::vector<size_t> indexPath{};

    auto indexPathSlice = exp.get(indexPathKey);
    TRI_ASSERT(indexPathSlice.isArray());
    for (auto const& p : VPackArrayIterator(indexPathSlice)) {
      TRI_ASSERT(p.isNumber<size_t>());
      indexPath.emplace_back(p.getNumber<size_t>());
    }
    result._expressions.emplace_back(std::make_unique<NonConstExpression>(
        std::make_unique<Expression>(ast, exp), std::move(indexPath)));
  }

  auto vars = slice.get(varMappingKey);
  TRI_ASSERT(vars.isObject());
  for (auto const& [varId, regId] : VPackObjectIterator(vars)) {
    VariableId variableId = 0;
    bool converted =
        basics::StringUtils::toNumber(varId.copyString(), variableId);
    TRI_ASSERT(converted);
    result._varToRegisterMapping.emplace_back(
        std::make_pair(variableId, regId.getNumber<RegisterId::value_t>()));
  }

  return result;
}

NonConstExpressionContainer::NonConstExpressionContainer(
    std::vector<std::unique_ptr<NonConstExpression>> expressions,
    std::vector<std::pair<VariableId, RegisterId>> varToRegisterMapping)
    : _expressions(std::move(expressions)),
      _varToRegisterMapping(std::move(varToRegisterMapping)) {}

}  // namespace arangodb::aql
