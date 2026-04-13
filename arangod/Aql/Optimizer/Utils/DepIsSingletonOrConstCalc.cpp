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
/// @author Wilfried Goesgens
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "DepIsSingletonOrConstCalc.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/Variable.h"

namespace arangodb::aql {

bool depIsSingletonOrConstCalc(ExecutionNode const* node) {
  while (node) {
    node = node->getFirstDependency();
    if (node == nullptr) {
      return false;
    }

    if (node->getType() == ExecutionNode::SINGLETON) {
      return true;
    }

    if (node->getType() != ExecutionNode::CALCULATION) {
      return false;
    }

    VarSet used;
    // cppcheck-suppress nullPointerRedundantCheck
    node->getVariablesUsedHere(used);
    if (!used.empty()) {
      return false;
    }
  }
  return false;
}

}  // namespace arangodb::aql
