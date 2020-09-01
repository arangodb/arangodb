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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AST_HELPER_H
#define ARANGOD_AQL_AST_HELPER_H 1

#include <string>
#include <unordered_set>

#include "Containers/SmallVector.h"

namespace arangodb {
namespace aql {
struct AstNode;
struct Variable;
namespace ast {

/// @brief determines the to-be-kept attribute of an INTO expression
std::unordered_set<std::string> getReferencedAttributesForKeep(
    AstNode const* node, ::arangodb::containers::SmallVector<Variable const*> searchVariables,
    bool& isSafeForOptimization);

}  // namespace ast
}  // namespace aql
}  // namespace arangodb

#endif
