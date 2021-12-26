////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/NonConstExpression.h"
#include "Aql/types.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace aql {

class Ast;
struct NonConstExpression;

/**
 * @brief Container for multiple NonConstExpressions.
 *
 * A NonConst expression is part of any Aql Condition that is not
 * constant and needs to be evaluated before we can use it.
 * (e.g. for Indexes). This is a mechanism to do a partial evaluation
 * of the AqlCondition.
 *
 * This container associates the required variables and their registers,
 * and also retains if a V8 expression is used.
 */

struct NonConstExpressionContainer {
  NonConstExpressionContainer() = default;
  ~NonConstExpressionContainer() = default;

  // Do not copy, we are using a unique_ptr as member will not work anyways
  NonConstExpressionContainer(NonConstExpressionContainer const&) = delete;
  NonConstExpressionContainer& operator=(NonConstExpressionContainer const&) = delete;

  // Allow moving.
  NonConstExpressionContainer(NonConstExpressionContainer&&) = default;
  NonConstExpressionContainer& operator=(NonConstExpressionContainer&&) = default;

  std::vector<std::unique_ptr<NonConstExpression>> _expressions;

  // This is a 1 to 1 mapping of Variables to Registers.
  // This could be done by an unordered_map as well.
  // However in terms of reading, a linear scan on vectors does outperform
  // an unordered_map find, for a small number of elements.
  // On my tests below 20 elements vector was faster, otherwise map.
  // As it is very unlikely to have 20 active variables this variant shall
  // give a better overall read performance.
  std::vector<std::pair<VariableId, RegisterId>> _varToRegisterMapping;

  bool _hasV8Expression = false;

  // Serializes this container into a velocypack builder.
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  static NonConstExpressionContainer fromVelocyPack(Ast* ast, arangodb::velocypack::Slice slice);

  NonConstExpressionContainer clone(Ast* ast) const;
};

}  // namespace aql
}  // namespace arangodb
