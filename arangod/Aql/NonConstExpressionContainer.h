////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/types.h"
#include "Aql/NonConstExpression.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace aql {

class Ast;
struct NonConstExpression;

struct NonConstExpressionContainer {
  NonConstExpressionContainer() = default;
  ~NonConstExpressionContainer() = default;
  NonConstExpressionContainer(NonConstExpressionContainer const&) = delete;
  NonConstExpressionContainer(NonConstExpressionContainer&&) = default;
  NonConstExpressionContainer& operator=(NonConstExpressionContainer const&) = delete;

  std::vector<std::unique_ptr<NonConstExpression>> _expressions;
  std::unordered_map<VariableId, RegisterId> _varToRegisterMapping; 
  bool _hasV8Expression = false;

  // Serializes this container into a velocypack builder.
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  static NonConstExpressionContainer fromVelocyPack(Ast* ast, arangodb::velocypack::Slice slice);

  NonConstExpressionContainer clone(Ast* ast) const;
};


}
}
