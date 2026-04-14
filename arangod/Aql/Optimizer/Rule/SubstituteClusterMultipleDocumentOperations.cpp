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

#include "SubstituteClusterMultipleDocumentOperations.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/MultipleRemoteModificationNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/SingleRemoteOperationNode.h"
#include "Aql/ExecutionNode/UpdateReplaceNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Optimizer/Utils/SubstituteClusterDocumentHelpers.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/QueryContext.h"
#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "StorageEngine/TransactionState.h"

namespace arangodb::aql {

using EN = ExecutionNode;

namespace {

bool substituteClusterMultipleDocumentInsertOperations(
    Optimizer* opt, ExecutionPlan* plan, OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, {EN::INSERT}, false);

  if (plan->getAst()->query().trxForOptimization().state()->hasHint(
          transaction::Hints::Hint::GLOBAL_MANAGED)) {
    return false;
  }

  if (nodes.size() != 1) {
    return false;
  }

  auto* node = nodes[0];
  auto* dep = node->getFirstDependency();
  if (dep == nullptr || dep->getType() != EN::ENUMERATE_LIST) {
    return false;
  }

  if (!aql::depIsSingletonOrConstCalc(dep)) {
    return false;
  }

  bool modified = false;
  auto mod = ExecutionNode::castTo<InsertNode*>(node);

  // for now, not support smart graph
  if (mod->collection()->isSmart() &&
      mod->collection()->type() == TRI_COL_TYPE_EDGE) {
    return false;
  }

  Variable const* oldVariable = mod->getOutVariableOld();
  if (oldVariable != nullptr && mod->isVarUsedLater(oldVariable)) {
    // using RETURN OLD cannot use optimization
    return false;
  }

  Variable const* newVariable = mod->getOutVariableNew();
  if (newVariable != nullptr && mod->isVarUsedLater(newVariable)) {
    // using RETURN NEW. cannot use optimization
    return false;
  }

  auto* enumerateNode = ExecutionNode::castTo<EnumerateListNode const*>(dep);
  if (enumerateNode->getMode() == EnumerateListNode::kEnumerateObject) {
    // Cannot optimize object mode for EnumerateListNode
    return false;
  }
  if (enumerateNode->outVariable()[0] != mod->inVariable()) {
    return false;
  }

  if (enumerateNode->isInInnerLoop()) {
    // FOR ... INSERT is contained in inner loop. cannot use optimization
    return false;
  }

  // node cannot have any parent, because it either would have a RETURN or a
  // modification node, which is not supported for now
  if (node->getFirstParent() != nullptr) {
    return false;
  }

  auto setterNode = plan->getVarSetBy(enumerateNode->inVariable()->id);
  if (setterNode == nullptr || setterNode->getType() != EN::CALCULATION) {
    return false;
  }

  auto* calcSetterNode =
      ExecutionNode::castTo<CalculationNode const*>(setterNode);

  if (!calcSetterNode->expression()->isConstant() ||
      !calcSetterNode->expression()->isDeterministic()) {
    return false;
  }

  // deal with dependency of enumerate list needing to be singleton or const
  // calculation

  // TODO - need more checks?

  ExecutionNode* multiOperationNode =
      plan->createNode<MultipleRemoteModificationNode>(
          plan, plan->nextId(), mod->collection(), mod->getOptions(),
          enumerateNode->inVariable() /*in*/, nullptr, mod->getOutVariableOld(),
          mod->getOutVariableNew());

  aql::replaceNode(plan, mod, multiOperationNode);
  plan->unlinkNode(dep);

  modified = true;

  return modified;
}

}  // namespace

void substituteClusterMultipleDocumentOperationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified =
      substituteClusterMultipleDocumentInsertOperations(opt, plan.get(), rule);
  if (modified) {
    // turn off all other cluster optimization rules now as they are superfluous
    opt->disableRules(plan.get(), [](OptimizerRule const& rule) {
      return rule.isClusterOnly();
    });
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql