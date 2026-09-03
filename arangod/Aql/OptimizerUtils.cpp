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
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerUtils.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpdateReplaceNode.h"
#include "Aql/ExecutionPlan.h"
#include <absl/strings/str_cat.h>

namespace arangodb::aql {
namespace utils {

Collection const* getCollection(ExecutionNode const* node) {
  using EN = ExecutionNode;

  switch (node->getType()) {
    case EN::ENUMERATE_COLLECTION:
      return ExecutionNode::castTo<EnumerateCollectionNode const*>(node)
          ->collection();
    case EN::INDEX:
      return ExecutionNode::castTo<IndexNode const*>(node)->collection();
    case EN::TRAVERSAL:
    case EN::ENUMERATE_PATHS:
    case EN::SHORTEST_PATH:
      return ExecutionNode::castTo<GraphNode const*>(node)->collection();

    default:
      // note: modification nodes are not covered here yet
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node type does not have a collection");
  }
}

}  // namespace utils
}  // namespace arangodb::aql
