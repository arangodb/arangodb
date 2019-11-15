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
#include "Aql/Optimizer.h"
#include "IndexNodeOptimizerRules.h"
#include "Basics/AttributeNameParser.h"
#include "Cluster/ServerState.h"

using EN = arangodb::aql::ExecutionNode;

namespace {
  struct NodeWithAttrs {
    struct AttributeAndField {
      std::vector<arangodb::basics::AttributeName> attr;
      arangodb::aql::AstNode* astNode;
      size_t astNodeChildNum;
      size_t indexFieldNum;
      std::vector<arangodb::basics::AttributeName> const* indexField;
    };

    std::vector<AttributeAndField> attrs;
    arangodb::aql::CalculationNode* node;
  };

  bool attributesMatch(TRI_idx_iid_t& commonIndexId, arangodb::aql::IndexNode const* indexNode, NodeWithAttrs& node) {
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
            nodeAttr.indexFieldNum = indexFieldNum;
            nodeAttr.indexField = &field;
            break;
          }
          ++indexFieldNum;
        }
        if (nodeAttr.indexField != nullptr) {
          break;
        }
      }
      // not found
      if (nodeAttr.indexField == nullptr) {
        return false;
      }
    }
    return true;
  }

  // traverse the AST, using previsitor
  void traverseReadOnly(arangodb::aql::AstNode* node, arangodb::aql::AstNode* parentNode, size_t childNumber,
                        std::function<bool(arangodb::aql::AstNode const*, arangodb::aql::AstNode*, size_t)> const& preVisitor) {
    if (node == nullptr) {
      return;
    }

    if (!preVisitor(node, parentNode, childNumber)) {
      return;
    }

    size_t const n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = node->getMemberUnchecked(i);

      if (member != nullptr) {
        traverseReadOnly(member, node, i, preVisitor);
      }
    }
  }

  // traversal state
  struct TraversalState {
    arangodb::aql::Variable const* variable;
    NodeWithAttrs& nodeAttrs;
    bool optimize;
    bool wasAccess;
  };

  // determines attributes referenced in an expression for the specified out variable
  bool getReferencedAttributes(arangodb::aql::AstNode* node,
                               arangodb::aql::Variable const* variable,
                               NodeWithAttrs& nodeAttrs) {
    TraversalState state{variable, nodeAttrs, true, false};

    auto preVisitor = [&state](arangodb::aql::AstNode const* node,
        arangodb::aql::AstNode* parentNode, size_t childNumber) {
      if (node == nullptr) {
        return false;
      }

      switch (node->type) {
        case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS:
          if (!state.wasAccess) {
            state.nodeAttrs.attrs.emplace_back(
              NodeWithAttrs::AttributeAndField{std::vector<arangodb::basics::AttributeName>{
                {std::string(node->getStringValue(), node->getStringLength()), false}}, parentNode, childNumber, 0, nullptr});
            state.wasAccess = true;
          } else {
            state.nodeAttrs.attrs.back().attr.emplace_back(std::string(node->getStringValue(), node->getStringLength()), false);
          }
          return true;
        case arangodb::aql::NODE_TYPE_REFERENCE: {
          // reference to a variable
          auto v = static_cast<arangodb::aql::Variable const*>(node->getData());
          if (v == state.variable) {
            if (!state.wasAccess) {
              // we haven't seen an attribute access directly before
              state.optimize = false;

              return false;
            }
            std::reverse(state.nodeAttrs.attrs.back().attr.begin(), state.nodeAttrs.attrs.back().attr.end());
          } else {
            if (state.wasAccess) {
              state.nodeAttrs.attrs.pop_back();
            }
          }
          // finish an attribute path
          state.wasAccess = false;
          return true;
        }
        default:
          break;
      }

      if (state.wasAccess) {
        // not appropriate node type
        state.wasAccess = false;
        state.optimize = false;

        return false;
      }

      return true;
    };

    traverseReadOnly(node, nullptr, 0, preVisitor);

    return state.optimize;
  }
}

void arangodb::aql::lateDocumentMaterializationRule(arangodb::aql::Optimizer* opt,
                                                    std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
                                                    arangodb::aql::OptimizerRule const& rule) {
  auto modified = false;
  auto addPlan = arangodb::scopeGuard([opt, &plan, &rule, &modified]() {
    opt->addPlan(std::move(plan), rule, modified);
  });
  // index node supports late materialization
  if (!plan->contains(EN::INDEX) ||
      // we need sort node to be present (without sort it will be just skip, nothing to optimize)
      !plan->contains(EN::SORT) ||
      // limit node is needed as without limit all documents will be returned anyway, nothing to optimize
      !plan->contains(EN::LIMIT)) {
    return;
  }

  ::arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<ExecutionNode*> nodes{a};

  plan->findNodesOfType(nodes, EN::LIMIT, true);
  for (auto limitNode : nodes) {
    auto loop = const_cast<ExecutionNode*>(limitNode->getLoop());
    if (arangodb::aql::ExecutionNode::INDEX == loop->getType()) {
      auto indexNode = EN::castTo<IndexNode*>(loop);
      if (indexNode->isLateMaterialized()) {
        continue; // loop is aleady optimized
      }
      auto current = limitNode->getFirstDependency();
      ExecutionNode* sortNode = nullptr;
      // examining plan. We are looking for SortNode closest to lowest LimitNode
      // without document body usage before that node.
      // this node could be appended with materializer
      bool stopSearch = false;
      std::vector<NodeWithAttrs> nodesToChange;
      TRI_idx_iid_t commonIndexId = 0; // use one index only
      while (current != loop) {
        switch (current->getType()) {
          case arangodb::aql::ExecutionNode::SORT:
            if (sortNode == nullptr) { // we need nearest to limit sort node, so keep selected if any
              sortNode = current;
            }
            break;
          case arangodb::aql::ExecutionNode::CALCULATION: {
            auto calculationNode = EN::castTo<CalculationNode*>(current);
            auto astNode = calculationNode->expression()->nodeForModification();
            NodeWithAttrs node;
            node.node = calculationNode;
            // find attributes referenced to index node out variable
            if (!getReferencedAttributes(astNode, indexNode->outVariable(), node)) {
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
          case arangodb::aql::ExecutionNode::REMOTE:
            // REMOTE node is a blocker - we do not want to make materialization calls across cluster!
            if (sortNode != nullptr) {
              stopSearch = true;
            }
            break;
          default: // make clang happy
            break;
        }
        if (sortNode != nullptr && current->getType() != arangodb::aql::ExecutionNode::CALCULATION) {
          ::arangodb::containers::HashSet<Variable const*> currentUsedVars;
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
                         [&ast](auto const& attrAndField) {
                           return std::make_pair(attrAndField.indexField, IndexNode::IndexVariable{attrAndField.indexFieldNum,
                                                 ast->variables()->createTemporaryVariable()});
                         });
        }
        auto localDocIdTmp = ast->variables()->createTemporaryVariable();
        for (auto& node : nodesToChange) {
          for (auto& attr : node.attrs) {
            auto it = uniqueVariables.find(attr.indexField);
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
