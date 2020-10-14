////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRules.h"

#include "Aql/ClusterNodes.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Optimizer.h"
#include "Basics/StaticStrings.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

ExecutionNode* hasSingleParent(ExecutionNode const* in,
                               std::vector<EN::NodeType> const& types) {
  auto* parent = in->getFirstParent();
  if (parent) {
    for (auto const& type : types) {
      if (parent->getType() == type) {
        return parent;
      }
    }
  }
  return nullptr;
}

ExecutionNode* hasSingleDep(ExecutionNode const* in, EN::NodeType const type) {
  auto* dep = in->getFirstDependency();
  if (dep && dep->getType() == type) {
    return dep;
  }
  return nullptr;
}

Index* hasSingleIndexHandle(ExecutionNode const* node) {
  TRI_ASSERT(node->getType() == EN::INDEX);
  IndexNode const* indexNode = ExecutionNode::castTo<IndexNode const*>(node);
  auto indexHandleVec = indexNode->getIndexes();
  if (indexHandleVec.size() == 1) {
    return indexHandleVec.front().get();
  }
  return nullptr;
}

Index* hasSingleIndexHandle(ExecutionNode const* node, Index::IndexType type) {
  auto* idx = ::hasSingleIndexHandle(node);
  if (idx && idx->type() == type) {
    return idx;
  }
  return nullptr;
}

std::vector<AstNode const*> hasBinaryCompare(ExecutionNode const* node) {
  // returns any AstNode in the expression that is
  // a binary comparison.
  TRI_ASSERT(node->getType() == EN::INDEX);
  IndexNode const* indexNode = ExecutionNode::castTo<IndexNode const*>(node);
  AstNode const* cond = indexNode->condition()->root();
  std::vector<AstNode const*> result;

  auto preVisitor = [&result](AstNode const* node) {
    if (node == nullptr) {
      return false;
    };

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      result.push_back(node);
      return false;
    }

    // skip over NARY AND/OR
    if (node->type == NODE_TYPE_OPERATOR_NARY_OR || node->type == NODE_TYPE_OPERATOR_NARY_AND) {
      return true;
    } else {
      return false;
    }
  };
  auto postVisitor = [](AstNode const*) {};
  Ast::traverseReadOnly(cond, preVisitor, postVisitor);

  return result;
}

std::string getFirstKey(std::vector<AstNode const*> const& compares) {
  for (auto const* node : compares) {
    AstNode const* keyNode = node->getMemberUnchecked(0);
    if (keyNode->type == AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS &&
        keyNode->stringEquals(StaticStrings::KeyString)) {
      keyNode = node->getMemberUnchecked(1);
    }
    if (keyNode->isStringValue()) {
      return keyNode->getString();
    }
  }
  return "";
}

bool depIsSingletonOrConstCalc(ExecutionNode const* node) {
  while (node) {
    node = node->getFirstDependency();
    if (node == nullptr) {
      return false;
    }

    if (node->getType() == EN::SINGLETON) {
      return true;
    }

    if (node->getType() != EN::CALCULATION) {
      return false;
    }

    VarSet used;
    node->getVariablesUsedHere(used);
    if (!used.empty()) {
      return false;
    }
  }
  return false;
}

bool parentIsReturnOrConstCalc(ExecutionNode const* node) {
  node = node->getFirstParent();
  while (node) {
    auto type = node->getType();
    // we do not need to check the order because
    // we expect a valid plan
    if (type != EN::CALCULATION && type != EN::RETURN) {
      return false;
    }
    node = node->getFirstParent();
  }
  return true;
}

void replaceNode(ExecutionPlan* plan, ExecutionNode* oldNode, ExecutionNode* newNode) {
  if (oldNode == plan->root()) {
    // intentional copy, the dependencies are changed in the loop
    std::vector<ExecutionNode*> deps = oldNode->getDependencies();
    for (auto* x : deps) {
      TRI_ASSERT(x != nullptr);
      newNode->addDependency(x);
      oldNode->removeDependency(x);
    }
    plan->root(newNode, true);
  } else {
    // replaceNode does not seem to work well with subqueries
    // if the subqueries root is replaced.
    // It looks like Subquery node will still point to
    // the old node.

    TRI_ASSERT(oldNode != plan->root());
    plan->replaceNode(oldNode, newNode);
    TRI_ASSERT(!oldNode->hasDependency());
    TRI_ASSERT(!oldNode->hasParent());
  }
}

bool substituteClusterSingleDocumentOperationsIndex(Optimizer* opt, ExecutionPlan* plan,
                                                    OptimizerRule const& rule) {
  ::arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::INDEX, false);

  if (nodes.size() != 1) {
    return false;
  }

  bool modified = false;
  for (auto* node : nodes) {
    if (!::depIsSingletonOrConstCalc(node)) {
      continue;
    }

    Index* index = ::hasSingleIndexHandle(node, Index::TRI_IDX_TYPE_PRIMARY_INDEX);
    if (index) {
      IndexNode* indexNode = ExecutionNode::castTo<IndexNode*>(node);
      auto binaryCompares = ::hasBinaryCompare(node);
      std::string key = ::getFirstKey(binaryCompares);
      if (key.empty()) {
        continue;
      }

      TRI_ASSERT(node != nullptr);

      auto* parentModification =
          ::hasSingleParent(node, {EN::INSERT, EN::REMOVE, EN::UPDATE, EN::REPLACE});

      if (parentModification) {
        auto mod = ExecutionNode::castTo<ModificationNode*>(parentModification);
        
        if (!::parentIsReturnOrConstCalc(mod)) {
          continue;
        }

        if (mod->collection() != indexNode->collection()) {
          continue;
        }
        
        auto parentType = parentModification->getType();
        Variable const* update = nullptr;
        Variable const* keyVar = nullptr;

        if (parentType == EN::INSERT) {
          continue;
        } else if (parentType == EN::REMOVE) {
          keyVar = ExecutionNode::castTo<RemoveNode const*>(mod)->inVariable();
        } else {
          // update / replace
          auto updateReplaceNode = ExecutionNode::castTo<UpdateReplaceNode const*>(mod);
          update = updateReplaceNode->inDocVariable();
          if (updateReplaceNode->inKeyVariable() != nullptr) {
            keyVar = updateReplaceNode->inKeyVariable();
          }
        }

        if (keyVar && indexNode->outVariable()->id != keyVar->id) {
          continue;
        }

        if (!keyVar) {
          if (update && indexNode->outVariable()->id == update->id) {
            // the update document is already described by the key provided
            // in the index condition
            update = nullptr;
          } else {
            continue;
          }
        }

        ExecutionNode* singleOperationNode = plan->createNode<SingleRemoteOperationNode>(
            plan, plan->nextId(), parentType, true, key, mod->collection(),
            mod->getOptions(), update, indexNode->outVariable(),
            mod->getOutVariableOld(), mod->getOutVariableNew());

        ::replaceNode(plan, mod, singleOperationNode);
        plan->unlinkNode(indexNode);
        modified = true;
      } else if (::parentIsReturnOrConstCalc(node)) {
        ExecutionNode* singleOperationNode = plan->registerNode(
            new SingleRemoteOperationNode(plan, plan->nextId(), EN::INDEX, true,
                                          key, indexNode->collection(),
                                          ModificationOptions{}, nullptr /*in*/,
                                          indexNode->outVariable() /*out*/,
                                          nullptr /*old*/, nullptr /*new*/));
        ::replaceNode(plan, indexNode, singleOperationNode);
        modified = true;
      }
    }
  }

  return modified;
}

bool substituteClusterSingleDocumentOperationsNoIndex(Optimizer* opt, ExecutionPlan* plan,
                                                      OptimizerRule const& rule) {
  ::arangodb::containers::SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  ::arangodb::containers::SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, {EN::INSERT, EN::REMOVE, EN::UPDATE, EN::REPLACE}, false);

  if (nodes.size() != 1) {
    return false;
  }

  bool modified = false;
  for (auto* node : nodes) {
    auto mod = ExecutionNode::castTo<ModificationNode*>(node);

    if (!::depIsSingletonOrConstCalc(node)) {
      continue;
    }

    if (!::parentIsReturnOrConstCalc(node)) {
      continue;
    }

    auto depType = mod->getType();
    Variable const* update = nullptr;
    Variable const* keyVar = nullptr;
    std::string key;
        
    if (depType == EN::REMOVE) {
      keyVar = ExecutionNode::castTo<RemoveNode const*>(mod)->inVariable();
    } else if (depType == EN::INSERT) {
      update = ExecutionNode::castTo<InsertNode const*>(mod)->inVariable();
    } else {
      auto updateReplaceNode = ExecutionNode::castTo<UpdateReplaceNode const*>(mod);
      update = updateReplaceNode->inDocVariable();
      if (updateReplaceNode->inKeyVariable() != nullptr) {
        keyVar = updateReplaceNode->inKeyVariable();
      }
    }

    ExecutionNode* cursor = node;
    CalculationNode* calc = nullptr;

    if (keyVar) {
      VarSet keySet;
      keySet.emplace(keyVar);

      while (cursor) {
        cursor = ::hasSingleDep(cursor, EN::CALCULATION);
        if (cursor) {
          CalculationNode* c = ExecutionNode::castTo<CalculationNode*>(cursor);
          if (c->setsVariable(keySet)) {
            calc = c;
            break;
          }
        }
      }

      if (!calc) {
        continue;
      }
      AstNode const* expr = calc->expression()->node();
      if (expr->isStringValue()) {
        key = expr->getString();
      } else if (expr->isObject()) {
        bool foundKey = false;
        for (std::size_t i = 0; i < expr->numMembers(); i++) {
          auto* anode = expr->getMemberUnchecked(i);
          if (anode->getStringRef() == StaticStrings::KeyString) {
            if (anode->getMember(0)->isStringValue()) {
              key = anode->getMember(0)->getString();
            }
            foundKey = true;
          } else if (anode->getStringRef() == StaticStrings::RevString) {
            foundKey = false;  // decline if _rev is in the game
            break;
          }
        }
        if (!foundKey) {
          continue;
        }
      } else {
        // write more code here if we
        // want to support thinks like:
        //
        // DOCUMENT("foo/bar")
      }

      if (key.empty()) {
        continue;
      }
    }

    if (!::depIsSingletonOrConstCalc(cursor)) {
      continue;
    }

    ExecutionNode* singleOperationNode = plan->registerNode(
        new SingleRemoteOperationNode(plan, plan->nextId(), depType, false, key,
                                      mod->collection(), mod->getOptions(), update /*in*/,
                                      nullptr, mod->getOutVariableOld(),
                                      mod->getOutVariableNew()));

    ::replaceNode(plan, mod, singleOperationNode);

    if (calc) {
      plan->unlinkNode(calc);
    }
    modified = true;
  }  // for node : nodes

  return modified;
}

}  // namespace

namespace arangodb {
namespace aql {

void substituteClusterSingleDocumentOperationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan, OptimizerRule const& rule) {
  bool modified = false;

  for (auto const& fun : {&::substituteClusterSingleDocumentOperationsIndex,
                          &::substituteClusterSingleDocumentOperationsNoIndex}) {
    modified = fun(opt, plan.get(), rule);
    if (modified) {
      break;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}
}
