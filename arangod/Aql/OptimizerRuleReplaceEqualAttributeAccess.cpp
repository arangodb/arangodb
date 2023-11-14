////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstHelper.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ConditionFinder.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexNode.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/JoinNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/NumberUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Containers/FlatHashSet.h"
#include "Containers/HashSet.h"
#include "Containers/SmallUnorderedMap.h"
#include "Containers/SmallVector.h"
#include "Geo/GeoParams.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "OptimizerRules.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(true)

namespace {

/*
 * This function calls the callback for all binary equal operators found
 * in the given expression. It will descend into nary-ors only if there is a
 * single member. It descents into all nary-and operators.
 */
template<typename F>
void forEachEqualCondition(Expression* expr, F&& fn) {
  static_assert(std::is_invocable_v<F, AstNode const*>);
  auto* root = expr->node();
  // Remember that conditions are in disjunctive normal form
  // We can only deduce equality of expressions if there is no or statement.
  //    FILTER x == y || x == 5
  // does not allow to deduce that y is always equal to x.
  if (root == nullptr) {
    return;
  }

  if (root->type == NODE_TYPE_OPERATOR_NARY_OR) {
    if (root->numMembers() != 1) {
      return;
    } else {
      root = root->getMember(0);
    }
  }

  if (root == nullptr) {
    return;
  }
  // we can have multiple statements concatenated by an AND.

  if (root->type == NODE_TYPE_OPERATOR_NARY_AND) {
    for (size_t i = 0; i < root->numMembers(); i++) {
      auto* member = root->getMemberUnchecked(i);

      if (member != nullptr && member->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        fn(member);
      }
    }
  } else if (root->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
    fn(root);
  }
}

/*
 * This function searches for all equal compared attribute accesses.
 */
template<typename F>
void findEqualComparedAttributeAccesses(Expression* expr, F&& fn) {
  static_assert(std::is_invocable_v<F, AstNode const*, AstNode const*>);
  forEachEqualCondition(expr, [&](AstNode const* ast) {
    TRI_ASSERT(ast->type == NODE_TYPE_OPERATOR_BINARY_EQ);
    auto* lhs = ast->getMember(0);
    auto* rhs = ast->getMember(1);

    LOG_DEVEL << "EQUAL COMPARE 1 " << lhs->toString()
              << " == " << rhs->toString();

    if (lhs->type != NODE_TYPE_REFERENCE &&
        !lhs->isAttributeAccessForVariable()) {
      return;
    }
    if (rhs->type != NODE_TYPE_REFERENCE &&
        !rhs->isAttributeAccessForVariable()) {
      return;
    }

    fn(lhs, rhs);
  });
}

struct VariableAttributeAccess {
  Variable const* var;
  std::vector<std::string_view> path;
  AstNode const* expr;

  std::ostream& operator<<(std::ostream& os) const {
    os << var->name << " [";
    std::copy(path.begin(), path.end(),
              std::ostream_iterator<std::string_view>(os, ", "));
    return os << "]";
  }
};

std::optional<VariableAttributeAccess> extractVariableAttributeAccess(
    AstNode const* node) {
  if (node->type == NODE_TYPE_REFERENCE) {
    auto* var = static_cast<Variable const*>(node->getData());
    return VariableAttributeAccess{.var = var};
  } else if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    auto result = extractVariableAttributeAccess(node->getMember(0));
    result->path.emplace_back(node->getStringValue(), node->getStringLength());
    return result;
  }
  return std::nullopt;
}

using EqualCondition =
    std::pair<VariableAttributeAccess, VariableAttributeAccess>;

bool extractEqualConditions(
    Expression* expr, containers::SmallVector<EqualCondition, 8>& result) {
  findEqualComparedAttributeAccesses(
      expr, [&](AstNode const* lhs, AstNode const* rhs) {
        LOG_DEVEL << "COMPARE " << lhs->toString() << " == " << rhs->toString();
        auto lhsAccess = extractVariableAttributeAccess(lhs);
        auto rhsAccess = extractVariableAttributeAccess(rhs);
        if (!lhsAccess || !rhsAccess) {
          return;
        }

        lhsAccess->expr = lhs;
        rhsAccess->expr = rhs;
        result.emplace_back(std::move(*lhsAccess), std::move(*rhsAccess));
      });
  return false;
}

auto findDominatingVariable(ExecutionPlan& plan, EqualCondition& eq) {
  auto* node = plan.getVarSetBy(eq.first.var->id);
  if (node->getVarsValid().contains(eq.second.var)) {
    return std::forward_as_tuple(eq.second, eq.first);
  } else {
    return std::forward_as_tuple(eq.first, eq.second);
  }
}

}  // namespace

void arangodb::aql::replaceEqualAttributeAccesses(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;
  LOG_RULE << "Rule executed, Plan is:";

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  for (auto* f : nodes) {
    auto* fn = static_cast<FilterNode*>(f);
    auto* n = plan->getVarSetBy(fn->inVariable()->id);
    TRI_ASSERT(n->getType() == EN::CALCULATION);
    auto* cn = static_cast<CalculationNode*>(n);

    containers::SmallVector<EqualCondition, 8> conditions;
    extractEqualConditions(cn->expression(), conditions);

    for (auto& pair : conditions) {
      // detect which variable is declared first
      auto [lhs, rhs] = findDominatingVariable(*plan, pair);

      LOG_RULE << lhs.var->name << " is declared before " << rhs.var->name;

      // introduce a new variable
      auto variable = plan->getAst()->variables()->createTemporaryVariable();

      // substitute
      auto subst = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(),
          std::make_unique<Expression>(plan->getAst(),
                                       lhs.expr->clone(plan->getAst())),
          variable);
      plan->insertAfter(cn, subst);

      // now replace all usages of rhs and lhs

      for (ExecutionNode* node = fn; node != nullptr;
           node = node->getFirstParent()) {
        if (lhs.path.empty()) {
          node->replaceVariables({{lhs.var->id, variable}});
        } else {
          node->replaceAttributeAccess(node, lhs.var, lhs.path, variable, 0);
        }
        if (rhs.path.empty()) {
          node->replaceVariables({{rhs.var->id, variable}});
        } else {
          node->replaceAttributeAccess(node, rhs.var, rhs.path, variable, 0);
        }
      }
      modified = true;
    }
  }

  plan->show();
  opt->addPlan(std::move(plan), rule, modified);
}
