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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "ImmutableSearchCondition.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "IResearch/AqlHelper.h"

#include <utils/misc.hpp>

namespace arangodb::iresearch {

void immutableSearchCondition(aql::Optimizer* opt,
                              std::unique_ptr<aql::ExecutionPlan> plan,
                              aql::OptimizerRule const& rule) {
  TRI_ASSERT(plan && plan->getAst());

  // ensure 'Optimizer::addPlan' will be called
  bool modified = false;
  irs::Finally addPlan = [opt, &plan, &rule, &modified]() noexcept {
    opt->addPlan(std::move(plan), rule, modified);
  };

  if (!plan->contains(aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW)) {
    return;
  }

  containers::SmallVector<aql::ExecutionNode*, 8> viewNodes;
  plan->findNodesOfType(viewNodes,
                        aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
  aql::VarSet vars;
  aql::VarSet mutableVars;
  for (auto* node : viewNodes) {
    TRI_ASSERT(node);
    TRI_ASSERT(aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW ==
               node->getType());
    auto& view =
        *aql::ExecutionNode::castTo<IResearchViewNode*>(node);
    auto const* condition = &view.filterCondition();
    if (isFilterConditionEmpty(condition) || !view.scorers().empty() ||
        view.options().parallelism != 1 || view.hasOffsetInfo() ||
        !isInInnerLoopOrSubquery(view)) {
      continue;
    }
    hasDependencies(*plan, *condition, view.outVariable(), vars,
                    [&](aql::Variable const* var) {
                      mutableVars.emplace(var);
                      return false;
                    });
    if (mutableVars.empty()) {
      view.setImmutableParts(std::numeric_limits<uint32_t>::max());
      modified = true;
      continue;
    }
    uint32_t count = 0;
    while (true) {
      auto const type = condition->type;
      if (!aql::Ast::isOrOperatorType(type) &&
          !aql::Ast::isAndOperatorType(type)) {
        break;
      }
      auto const numMembers = condition->numMembers();
      if (numMembers <= 1) {
        TRI_ASSERT(numMembers == 1);
        condition = condition->getMemberUnchecked(0);
        continue;
      }
      const_cast<aql::AstNode*>(condition)->partitionMembers(
          [&](aql::AstNode* member) {
            bool used = aql::Ast::isVarsUsed(member, mutableVars);
            count += !used;
            return !used;
          });
      TRI_ASSERT(count != numMembers);
      break;
    }
    if (count > 0) {
      modified = true;
    }
    view.setImmutableParts(count);
  }
}

}  // namespace arangodb::iresearch
