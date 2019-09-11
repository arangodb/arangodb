////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AST_HELPER_H
#define ARANGOD_AQL_AST_HELPER_H 1

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Basics/SmallVector.h"
#include "Logger/Logger.h"

#include <unordered_set>

namespace arangodb {
namespace aql {
namespace ast {

/// @brief determines the to-be-kept attribute of an INTO expression
std::unordered_set<std::string> getReferencedAttributesForKeep(
    AstNode const* node, SmallVector<Variable const*> searchVariables,
    bool& isSafeForOptimization);

}  // namespace ast
}  // namespace aql
}  // namespace arangodb

#endif
