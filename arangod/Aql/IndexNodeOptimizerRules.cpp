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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////


#include "IndexNodeOptimizerRules.h"

#include "Aql/Ast.h"
#include "Aql/CalculationNodeVarFinder.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/Expression.h"
#include "Aql/IndexNode.h"
#include "Aql/LateMaterializedOptimizerRulesCommon.h"
#include "Aql/Optimizer.h"
#include "Basics/AttributeNameParser.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"

using namespace arangodb::aql;

namespace {

bool attributesMatch(arangodb::transaction::Methods::IndexHandle const& index,
                     latematerialized::NodeExpressionWithAttrs& node) {
  // check all node attributes to be in index
  for (auto& nodeAttr : node.attrs) {
    nodeAttr.afData.field = nullptr;
    size_t indexFieldNum = 0;
    for (auto const& field : index->fields()) {
      TRI_ASSERT(nodeAttr.afData.postfix.empty());
      if (latematerialized::isPrefix(field, nodeAttr.attr, false, nodeAttr.afData.postfix)) {
        nodeAttr.afData.fieldNumber = indexFieldNum;
        nodeAttr.afData.field = &field;
        break;
      }
      ++indexFieldNum;
    }
    // not found
    if (nodeAttr.afData.field == nullptr) {
      return false;
    }
  }
  return true;
}

bool processCalculationNode(
    Variable const* var, arangodb::transaction::Methods::IndexHandle const& index,
    CalculationNode* calculationNode, Expression* expression,
    std::vector<latematerialized::NodeExpressionWithAttrs>& nodesToChange) {
  latematerialized::NodeExpressionWithAttrs node;
  node.expression = expression;
  node.node = calculationNode;
  // find attributes referenced to index node out variable
  if (!latematerialized::getReferencedAttributes(expression->nodeForModification(), var, node)) {
    // is not safe for optimization
    return false;
  } else if (!node.attrs.empty()) {
    if (!attributesMatch(index, node)) {
      return false;
    } else {
      nodesToChange.emplace_back(std::move(node));
    }
  }
  return true;
}

}

void arangodb::aql::lateDocumentMaterializationRule(Optimizer* opt,
                                                    std::unique_ptr<ExecutionPlan> plan,
                                                    OptimizerRule const& rule) {
  auto modified = false;
  auto const addPlan = arangodb::scopeGuard([opt, &plan, &rule, &modified]() {
    opt->addPlan(std::move(plan), rule, modified);
  });
  // index node supports late materialization
  if (!plan->contains(ExecutionNode::INDEX) ||
      // we need sort node to be present (without sort it will be just skip, nothing to optimize)
      !plan->contains(ExecutionNode::SORT) ||
      // limit node is needed as without limit all documents will be returned anyway, nothing to optimize
      !plan->contains(ExecutionNode::LIMIT)) {
    return;
  }

  arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  arangodb::containers::SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ExecutionNode::LIMIT, true);
  for (auto* limitNode : nodes) {
    auto* loop = const_cast<ExecutionNode*>(limitNode->getLoop());
    if (loop != nullptr && ExecutionNode::INDEX == loop->getType()) {
      auto* indexNode = ExecutionNode::castTo<IndexNode*>(loop);
      TRI_ASSERT(indexNode);
      if (!indexNode->canApplyLateDocumentMaterializationRule() || indexNode->isLateMaterialized()) {
        continue; // loop is already optimized
      }
      auto& indexes = indexNode->getIndexes();
      TRI_ASSERT(!indexes.empty());
      if (indexes.size() != 1) {
        continue; // several indexes are not supported
      }
      auto& index = indexes.front();
      if (!index->hasCoveringIterator()) {
        continue; // index must be covering
      }
      auto const* var = indexNode->outVariable();
      std::vector<latematerialized::NodeExpressionWithAttrs> nodesToChange;
      if (indexNode->hasFilter()) {
        auto* filter = indexNode->filter();
        TRI_ASSERT(filter);
        VarSet currentUsedVars;
        Ast::getReferencedVariables(filter->node(), currentUsedVars);
        if (currentUsedVars.find(var) != currentUsedVars.end() &&
            !processCalculationNode(var, index, nullptr, filter, nodesToChange)) {
          // IndexNode has an early pruning filter which references variables not stored in index.
          // In this case we cannot perform the optimization
          continue;
        }
      }
      auto* current = limitNode->getFirstDependency();
      TRI_ASSERT(current);
      ExecutionNode* sortNode = nullptr;
      // examining plan. We are looking for SortNode closest to lowest LimitNode
      // without document body usage before that node.
      // this node could be appended with materializer
      auto stopSearch = false;
      auto stickToSortNode = false;
      while (current != loop) {
        auto valid = true;
        auto const type = current->getType();
        switch (type) {
          case ExecutionNode::SORT:
            if (sortNode == nullptr) { // we need nearest to limit sort node, so keep selected if any
              sortNode = current;
            }
            break;
          case ExecutionNode::CALCULATION: {
            auto* calculationNode = ExecutionNode::castTo<CalculationNode*>(current);
            TRI_ASSERT(calculationNode);
            valid = processCalculationNode(var, index, calculationNode,
                                           calculationNode->expression(),
                                           nodesToChange);
            break;
          }
          case ExecutionNode::REMOTE:
            // REMOTE node is a blocker - we do not want to make materialization calls across cluster!
            if (sortNode != nullptr) {
              stopSearch = true;
            }
            break;
          default: // make clang happy
            break;
        }
        // currently only calculation nodes expected to use a loop variable with attributes
        // we successfully replace all references to the loop variable
        if (!stopSearch && valid && type != ExecutionNode::CALCULATION) {
          VarSet currentUsedVars;
          current->getVariablesUsedHere(currentUsedVars);
          if (currentUsedVars.find(var) != currentUsedVars.end()) {
            valid = false;
            if (ExecutionNode::SUBQUERY == type) {
              auto& subqueryNode = *ExecutionNode::castTo<SubqueryNode*>(current);
              auto* subquery = subqueryNode.getSubquery();
              TRI_ASSERT(subquery);
              arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type sa;
              arangodb::containers::SmallVector<ExecutionNode*> subqueryCalcNodes{sa};
              // find calculation nodes in the plan of a subquery
              CalculationNodeVarFinder finder(var, subqueryCalcNodes);
              valid = !subquery->walk(finder);
              if (valid) { // if the finder did not stop
                for (auto* scn : subqueryCalcNodes) {
                  TRI_ASSERT(scn && scn->getType() == ExecutionNode::CALCULATION);
                  currentUsedVars.clear();
                  scn->getVariablesUsedHere(currentUsedVars);
                  if (currentUsedVars.find(var) != currentUsedVars.end()) {
                    auto* calculationNode = ExecutionNode::castTo<CalculationNode*>(scn);
                    TRI_ASSERT(calculationNode);
                    valid = processCalculationNode(var, index, calculationNode,
                                                   calculationNode->expression(),
                                                   nodesToChange);
                    if (!valid) {
                      break;
                    }
                  }
                }
              }
            }
          }
        }
        if (!valid) {
          TRI_ASSERT(!stopSearch);
          if (sortNode != nullptr) {
            // we have a doc body used before selected SortNode
            // forget it, let`s look for better sort to use
            stopSearch = true;
          } else {
            // we are between limit and sort nodes
            // late materialization could still be applied but we must insert MATERIALIZE node after sort not after limit
            stickToSortNode = true;
          }
        }
        if (stopSearch) {
          // this limit node affects only closest sort if this sort is invalid
          // we need to check other limit node
          sortNode = nullptr;
          nodesToChange.clear();
          break;
        }
        current = current->getFirstDependency(); // inspect next node
      }
      if (sortNode && !nodesToChange.empty()) {
        IndexNode::IndexVarsInfo uniqueVariables;
        arangodb::containers::HashSet<ExecutionNode*> toUnlink;
        // at first use variables from simple expressions
        for (auto const& node : nodesToChange) {
          TRI_ASSERT(!node.attrs.empty());
          auto const& afData = node.attrs[0].afData;
          if (afData.parentNode == nullptr && afData.postfix.empty() && node.node != nullptr) {
            TRI_ASSERT(node.attrs.size() == 1);
            // we could add one redundant variable for each field only
            if (uniqueVariables.try_emplace(afData.field, IndexNode::IndexVariable{afData.fieldNumber,
                                            node.node->outVariable()}).second) {
              toUnlink.emplace(node.node);
            }
          }
        }
        auto* ast = plan->getAst();
        TRI_ASSERT(ast);
        // create variables for complex expressions
        for (auto const& node : nodesToChange) {
          TRI_ASSERT(!node.attrs.empty());
          for (auto const& attrAndField : node.attrs) {
            // create a variable if necessary
            // nullptr == node.node for index node with applied early pruning
            if ((attrAndField.afData.parentNode != nullptr ||
                 !attrAndField.afData.postfix.empty() || nullptr == node.node) &&
                uniqueVariables.find(attrAndField.afData.field) == uniqueVariables.cend()) {
              uniqueVariables.emplace(attrAndField.afData.field, IndexNode::IndexVariable{attrAndField.afData.fieldNumber,
                ast->variables()->createTemporaryVariable()});
            }
          }
        }
        auto const* localDocIdTmp = ast->variables()->createTemporaryVariable();
        TRI_ASSERT(localDocIdTmp);
        for (auto& node : nodesToChange) {
          for (auto& attr : node.attrs) {
            auto const it = uniqueVariables.find(attr.afData.field);
            TRI_ASSERT(it != uniqueVariables.cend());
            auto* newNode = ast->createNodeReference(it->second.var);
            TRI_ASSERT(newNode);
            if (!attr.afData.postfix.empty()) {
              newNode = ast->createNodeAttributeAccess(newNode, attr.afData.postfix);
              TRI_ASSERT(newNode);
            }
            if (attr.afData.parentNode != nullptr) {
              TEMPORARILY_UNLOCK_NODE(attr.afData.parentNode);
              attr.afData.parentNode->changeMember(attr.afData.childNumber, newNode);
            } else {
              TRI_ASSERT(node.attrs.size() == 1);
              node.expression->replaceNode(newNode);
            }
          }
        }
        if (!toUnlink.empty()) {
          plan->unlinkNodes(toUnlink);
        }

        // we could apply late materialization
        // 1. We need to notify index node - it should not materialize documents, but produce only localDocIds
        indexNode->setLateMaterialized(localDocIdTmp, index->id(), uniqueVariables);
        // 2. We need to add materializer after limit node to do materialization
        // insert a materialize node
        auto* materializeNode =
          plan->registerNode(std::make_unique<materialize::MaterializeSingleNode>(
            plan.get(), plan->nextId(), indexNode->collection(), *localDocIdTmp, *var));
        TRI_ASSERT(materializeNode);

        // on cluster we need to materialize node stay close to sort node on db server (to avoid network hop for materialization calls)
        // however on single server we move it to limit node to make materialization as lazy as possible
        auto* materializeDependency = ServerState::instance()->isCoordinator() || stickToSortNode ? sortNode : limitNode;
        TRI_ASSERT(materializeDependency);
        auto* dependencyParent = materializeDependency->getFirstParent();
        TRI_ASSERT(dependencyParent);
        dependencyParent->replaceDependency(materializeDependency, materializeNode);
        materializeDependency->addParent(materializeNode);
        modified = true;
      }
    }
  }
}
