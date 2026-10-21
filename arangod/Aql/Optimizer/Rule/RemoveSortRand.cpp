////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "RemoveSortRand.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Containers/SmallVector.h"

namespace arangodb::aql {

/// @brief remove SORT RAND() if appropriate
void removeSortRandRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                        OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::SORT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto node = ExecutionNode::castTo<SortNode*>(n);
    auto const& elements = node->elements();
    if (elements.size() != 1) {
      // we're looking for "SORT RAND()", which has just one sort criterion
      continue;
    }

    auto const variable = elements[0].var;
    TRI_ASSERT(variable != nullptr);

    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != ExecutionNode::CALCULATION) {
      continue;
    }

    auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
    auto const expression = cn->expression();

    if (expression == nullptr || expression->node() == nullptr ||
        expression->node()->type != NODE_TYPE_FCALL) {
      // not the right type of node
      continue;
    }

    auto funcNode = expression->node();
    auto func = static_cast<Function const*>(funcNode->getData());

    // we're looking for "RAND()", which is a function call
    // with an empty parameters array
    if (func->name != "RAND" || funcNode->numMembers() != 1 ||
        funcNode->getMember(0)->numMembers() != 0) {
      continue;
    }

    // now we're sure we got SORT RAND() !

    // we found what we were looking for!
    // now check if the dependencies qualify
    if (!n->hasDependency()) {
      break;
    }

    auto current = n->getFirstDependency();
    ExecutionNode* collectionNode = nullptr;

    while (current != nullptr) {
      switch (current->getType()) {
        case ExecutionNode::SORT:
        case ExecutionNode::COLLECT:
        case ExecutionNode::FILTER:
        case ExecutionNode::SUBQUERY:
        case ExecutionNode::ENUMERATE_LIST:
        case ExecutionNode::TRAVERSAL:
        case ExecutionNode::SHORTEST_PATH:
        case ExecutionNode::INDEX:
        case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
          // if we found another SortNode, a CollectNode, FilterNode, a
          // SubqueryNode, an EnumerateListNode, a TraversalNode or an IndexNode
          // this means we cannot apply our optimization
          collectionNode = nullptr;
          current = nullptr;
          continue;  // this will exit the while loop
        }

        case ExecutionNode::ENUMERATE_COLLECTION: {
          if (collectionNode == nullptr) {
            // note this node
            collectionNode = current;
            break;
          } else {
            // we already found another collection node before. this means we
            // should not apply our optimization
            collectionNode = nullptr;
            current = nullptr;
            continue;  // this will exit the while loop
          }
          // cannot get here
          TRI_ASSERT(false);
        }

        default: {
          // ignore all other nodes
        }
      }

      if (!current->hasDependency()) {
        break;
      }

      current = current->getFirstDependency();
    }

    if (collectionNode == nullptr || !node->hasParent()) {
      continue;  // skip
    }

    current = node->getFirstParent();
    bool valid = false;
    if (current->getType() == ExecutionNode::LIMIT) {
      LimitNode* ln = ExecutionNode::castTo<LimitNode*>(current);
      if (ln->limit() == 1 && ln->offset() == 0) {
        valid = true;
      }
    }

    if (valid) {
      // we found a node to modify!
      TRI_ASSERT(collectionNode->getType() ==
                 ExecutionNode::ENUMERATE_COLLECTION);
      // set the random iteration flag for the EnumerateCollectionNode
      ExecutionNode::castTo<EnumerateCollectionNode*>(collectionNode)
          ->setRandom();

      // remove the SortNode and the CalculationNode
      plan->unlinkNode(n);
      plan->unlinkNode(setter);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
