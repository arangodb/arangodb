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
#include "Aql/JoinNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Containers/NodeHashMap.h"
#include "Containers/HashSet.h"
#include "Containers/SmallVector.h"
#include "Logger/LogMacros.h"
#include "OptimizerRules.h"

#include <boost/container_hash/hash.hpp>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using EN = arangodb::aql::ExecutionNode;

#define LOG_RULE LOG_DEVEL_IF(false)

namespace {

/*
 * This function calls the callback for all binary equal operators found
 * in the given expression. It will descend into nary-ors only if there is a
 * single member. It descends into all nary-and operators.
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

struct VarAttribKey {
  Variable const* var;
  containers::SmallVector<std::string_view, 8> path;

  friend bool operator==(VarAttribKey const&,
                         VarAttribKey const&) noexcept = default;

  friend std::size_t hash_value(VarAttribKey const& key) {
    std::size_t seed = 0;
    boost::hash_combine(seed, key.var);
    boost::hash_range(seed, key.path.begin(), key.path.end());
    return seed;
  }

  std::ostream& operator<<(std::ostream& os) const {
    os << var->name << " [";
    std::copy(path.begin(), path.end(),
              std::ostream_iterator<std::string_view>(os, ", "));
    return os << "]";
  }
};

struct VariableAttributeAccess : VarAttribKey {
  AstNode const* expr;
};

std::ostream& operator<<(std::ostream& os, VarAttribKey const& attr) noexcept {
  os << attr.var->name << " [";
  std::copy(attr.path.begin(), attr.path.end(),
            std::ostream_iterator<std::string_view>(os, ", "));
  return os << "]";
}

std::optional<VarAttribKey> extractVariableAttributeAccess(
    AstNode const* node) {
  if (node->type == NODE_TYPE_REFERENCE) {
    auto* var = static_cast<Variable const*>(node->getData());
    return VarAttribKey{.var = var};
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
  findEqualComparedAttributeAccesses(expr, [&](AstNode const* lhs,
                                               AstNode const* rhs) {
    auto lhsAccess = extractVariableAttributeAccess(lhs);
    auto rhsAccess = extractVariableAttributeAccess(rhs);
    if (!lhsAccess || !rhsAccess) {
      return;
    }

    result.emplace_back(VariableAttributeAccess{std::move(*lhsAccess), lhs},
                        VariableAttributeAccess{std::move(*rhsAccess), rhs});
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

struct EquivalenceClass {
  Variable const* var;
};

}  // namespace

void arangodb::aql::replaceEqualAttributeAccesses(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  containers::NodeHashMap<VarAttribKey, std::shared_ptr<EquivalenceClass>,
                          boost::hash<VarAttribKey>>
      equivalenceClasses;

  for (auto* node = plan->root()->getSingleton(); node != nullptr;
       node = node->getFirstParent()) {
    LOG_RULE << "[" << node->id() << "] " << node->getTypeString();
    // replace all known equivalence classes
    for (auto& [var, cls] : equivalenceClasses) {
      if (var.path.empty()) {
        if (var.var->id == cls->var->id) {
          continue;
        }
        LOG_RULE << "REPLACEING " << var.var->name << " -> " << cls->var->name;
        node->replaceVariables({{var.var->id, cls->var}});
      } else {
        node->replaceAttributeAccess(
            node, var.var,
            {const_cast<std::string_view*>(var.path.data()), var.path.size()},
            cls->var, 0);
      }
      modified = true;
    }

    // check if this is a filter node
    if (node->getType() != EN::FILTER) {
      continue;
    }

    auto* fn = ExecutionNode::castTo<FilterNode*>(node);
    auto* n = plan->getVarSetBy(fn->inVariable()->id);
    TRI_ASSERT(n->getType() == EN::CALCULATION);
    auto* cn = ExecutionNode::castTo<CalculationNode*>(n);

    containers::SmallVector<EqualCondition, 8> conditions;
    extractEqualConditions(cn->expression(), conditions);

    for (auto& pair : conditions) {
      // detect which variable is declared first
      auto [lhs, rhs] = pair;
      LOG_RULE << lhs.var->name << " is declared before " << rhs.var->name;

      // check if one of the two is already identified
      auto lhsIter = equivalenceClasses.find(lhs);
      auto rhsIter = equivalenceClasses.find(rhs);

      bool const lhsKnown = lhsIter != equivalenceClasses.end();
      bool const rhsKnown = rhsIter != equivalenceClasses.end();

      if (lhsKnown && !rhsKnown) {
        // simply identify rhs as well
        LOG_RULE << "IDENTIFY " << lhs << " == " << rhs;
        equivalenceClasses.emplace(rhs, lhsIter->second);

      } else if (!lhsKnown && rhsKnown) {
        // simply identify lhs as well
        LOG_RULE << "IDENTIFY " << lhs << " == " << rhs;
        equivalenceClasses.emplace(lhs, rhsIter->second);
      } else if (lhsKnown && rhsKnown) {
        // merge the two equivalence classes
        LOG_RULE << "MERGE " << lhs << " == " << rhs;
        rhsIter->second->var = lhsIter->second->var;
      } else {
        auto [lhs, rhs] = findDominatingVariable(*plan, pair);

        LOG_RULE << "CREATE " << lhs << " == " << rhs;
        // create a new equivalence class
        // introduce a new variable
        auto variable = plan->getAst()->variables()->createTemporaryVariable();
        LOG_RULE << "WITNESS VAR " << variable->name;
        // substitute
        auto subst = plan->createNode<CalculationNode>(
            plan.get(), plan->nextId(),
            std::make_unique<Expression>(plan->getAst(),
                                         lhs.expr->clone(plan->getAst())),
            variable);
        plan->insertAfter(cn, subst);
        modified = true;

        auto cls =
            std::make_shared<EquivalenceClass>(EquivalenceClass{variable});
        equivalenceClasses.emplace(lhs, cls);
        equivalenceClasses.emplace(rhs, cls);
        equivalenceClasses.emplace(VarAttribKey{variable, {}}, cls);
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
