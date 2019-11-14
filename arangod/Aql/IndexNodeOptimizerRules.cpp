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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/Expression.h"
#include "Aql/IndexNode.h"
#include "Aql/LateMaterializedOptimizerRulesCommon.h"
#include "Aql/Optimizer.h"
#include "IndexNodeOptimizerRules.h"
#include "Basics/AttributeNameParser.h"
#include "Cluster/ServerState.h"

using namespace arangodb::aql;

namespace {
  bool attributesMatch(TRI_idx_iid_t& commonIndexId, IndexNode const* indexNode, latematerialized::NodeWithAttrs& node) {
    // check all node attributes to be in index
    for (auto& nodeAttr : node.attrs) {
      for (auto& index : indexNode->getIndexes()) {
        if (!index->hasCoveringIterator()) {
          continue;
        }
        auto indexId = index->id();
        // use one index only
        if (commonIndexId != 0 && commonIndexId != indexId) {
          continue;
        }
        size_t indexFieldNum = 0;
        for (auto const& field : index->fields()) {
          if (arangodb::basics::AttributeName::isIdentical(nodeAttr.attr, field, false)) {
            if (commonIndexId == 0) {
              commonIndexId = indexId;
            }
            nodeAttr.fieldNum = indexFieldNum;
            nodeAttr.field = &field;
            break;
          }
          ++indexFieldNum;
        }
        if (commonIndexId != 0 || nodeAttr.field != nullptr) {
          break;
        }
      }
      // not found
      if (nodeAttr.field == nullptr) {
        return false;
      }
    }
    return true;
  }
}

void arangodb::aql::lateDocumentMaterializationRule(Optimizer* opt,
                                                    std::unique_ptr<ExecutionPlan> plan,
                                                    OptimizerRule const& rule) {
  auto modified = false;
  auto addPlan = arangodb::scopeGuard([opt, &plan, &rule, &modified]() {
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
  for (auto limitNode : nodes) {
    auto loop = const_cast<ExecutionNode*>(limitNode->getLoop());
    if (ExecutionNode::INDEX == loop->getType()) {
      auto indexNode = ExecutionNode::castTo<IndexNode*>(loop);
      if (indexNode->isLateMaterialized()) {
        continue; // loop is already optimized
      }
      auto current = limitNode->getFirstDependency();
      ExecutionNode* sortNode = nullptr;
      // examining plan. We are looking for SortNode closest to lowest LimitNode
      // without document body usage before that node.
      // this node could be appended with materializer
      bool stopSearch = false;
      std::vector<latematerialized::NodeWithAttrs> nodesToChange;
      TRI_idx_iid_t commonIndexId = 0; // use one index only
      while (current != loop) {
        switch (current->getType()) {
          case ExecutionNode::SORT:
            if (sortNode == nullptr) { // we need nearest to limit sort node, so keep selected if any
              sortNode = current;
            }
            break;
          case ExecutionNode::CALCULATION: {
            auto calculationNode = ExecutionNode::castTo<CalculationNode*>(current);
            auto astNode = calculationNode->expression()->nodeForModification();
            latematerialized::NodeWithAttrs node;
            node.node = calculationNode;
            // find attributes referenced to index node out variable
            if (!latematerialized::getReferencedAttributes(astNode, indexNode->outVariable(), node)) {
              // is not safe for optimization
              stopSearch = true;
            } else if (!node.attrs.empty()) {
              if (!attributesMatch(commonIndexId, indexNode, node)) {
                // the node uses attributes which is not in index
                stopSearch = true;
              } else {
                nodesToChange.emplace_back(node);
              }
            }
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
        if (sortNode != nullptr && current->getType() != ExecutionNode::CALCULATION) {
          arangodb::containers::HashSet<Variable const*> currentUsedVars;
          current->getVariablesUsedHere(currentUsedVars);
          if (currentUsedVars.find(indexNode->outVariable()) != currentUsedVars.end()) {
            // this limit node affects only closest sort, if this sort is invalid
            // we need to check other limit node
            stopSearch = true;
          }
        }
        if (stopSearch) {
          // we have a doc body used before selected SortNode. Forget it, let`s look for better sort to use
          sortNode = nullptr;
          nodesToChange.clear();
          break;
        }
        current = current->getFirstDependency();  // inspect next node
      }
      if (sortNode && !nodesToChange.empty()) {
        auto ast = plan->getAst();
        IndexNode::IndexVarsInfo uniqueVariables;
        for (auto& node : nodesToChange) {
          std::transform(node.attrs.cbegin(), node.attrs.cend(), std::inserter(uniqueVariables, uniqueVariables.end()),
                         [ast](auto const& attrAndField) {
                           return std::make_pair(attrAndField.field, IndexNode::IndexVariable{attrAndField.fieldNum,
                             ast->variables()->createTemporaryVariable()});
                         });
        }
        auto localDocIdTmp = ast->variables()->createTemporaryVariable();
        for (auto& node : nodesToChange) {
          for (auto& attr : node.attrs) {
            auto it = uniqueVariables.find(attr.field);
            TRI_ASSERT(it != uniqueVariables.cend());
            auto newNode = ast->createNodeReference(it->second.var);
            if (attr.astNode != nullptr) {
              TEMPORARILY_UNLOCK_NODE(attr.astNode);
              attr.astNode->changeMember(attr.astNodeChildNum, newNode);
            } else {
              TRI_ASSERT(node.attrs.size() == 1);
              node.node->expression()->replaceNode(newNode);
            }
          }
        }

        // we could apply late materialization
        // 1. We need to notify index node - it should not materialize documents, but produce only localDocIds
        indexNode->setLateMaterialized(localDocIdTmp, commonIndexId, uniqueVariables);
        // 2. We need to add materializer after limit node to do materialization
        // insert a materialize node
        auto materializeNode =
          plan->registerNode(std::make_unique<materialize::MaterializeSingleNode>(
            plan.get(), plan->nextId(), indexNode->collection(),
            *localDocIdTmp, *indexNode->outVariable()));

        // on cluster we need to materialize node stay close to sort node on db server (to avoid network hop for materialization calls)
        // however on single server we move it to limit node to make materialization as lazy as possible
        auto materializeDependency = ServerState::instance()->isCoordinator() ? sortNode : limitNode;
        auto dependencyParent = materializeDependency->getFirstParent();
        TRI_ASSERT(dependencyParent != nullptr);
        dependencyParent->replaceDependency(materializeDependency, materializeNode);
        materializeDependency->addParent(materializeNode);
        modified = true;
      }
    }
  }
}
