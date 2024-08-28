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
/// @author Jan Steemann
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AstHelper.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Containers/ImmutableMap.h"
#include "Containers/SmallVector.h"
#include "Logger/LogMacros.h"

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

struct Replacement {
  Variable const* var;
};

struct EquivalenceSet {
  containers::ImmutableMap<Replacement const*, Replacement const*> unionFind;
  containers::ImmutableMap<VarAttribKey, std::shared_ptr<Replacement>,
                           boost::hash<VarAttribKey>>
      variableToClass;
};

bool processQuery(ExecutionPlan& plan, ExecutionNode* root,
                  EquivalenceSet equivalences, std::size_t currentLimitCount) {
  bool modified = false;

  auto const findRoot = [&](Replacement const* replacement) {
    while (replacement != nullptr) {
      auto other = equivalences.unionFind.find(replacement);
      if (other == nullptr) {
        break;
      }
      replacement = *other;
    }
    return replacement;
  };

  for (auto* node = root->getSingleton(); node != nullptr;
       node = node->getFirstParent()) {
    LOG_RULE << "[" << node->id() << "] " << node->getTypeString();
    // replace all known equivalence classes
    for (auto& [var, cls] : equivalences.variableToClass) {
      auto* clsRoot = findRoot(cls.get());
      TRI_ASSERT(clsRoot->var != nullptr);

      if (var.path.empty()) {
        if (var.var->id == clsRoot->var->id) {
          continue;
        }
        LOG_RULE << "REPLACEING " << var.var->name << " -> "
                 << clsRoot->var->name;
        node->replaceVariables({{var.var->id, clsRoot->var}});
      } else {
        node->replaceAttributeAccess(
            node, var.var,
            {const_cast<std::string_view*>(var.path.data()), var.path.size()},
            clsRoot->var, 0);
      }
      modified = true;
    }

    if (node->getType() == EN::SUBQUERY) {
      auto* sn = ExecutionNode::castTo<SubqueryNode*>(node);
      LOG_RULE << "ENTER SUBQUERY";
      modified |= processQuery(plan, sn->getSubquery(), equivalences,
                               currentLimitCount);
      LOG_RULE << "LEAVE SUBQUERY";
      continue;
    }

    // check if this is a filter node
    if (node->getType() != EN::FILTER) {
      continue;
    }

    auto* fn = ExecutionNode::castTo<FilterNode*>(node);
    auto* n = plan.getVarSetBy(fn->inVariable()->id);
    TRI_ASSERT(n->getType() == EN::CALCULATION);
    auto* cn = ExecutionNode::castTo<CalculationNode*>(n);

    containers::SmallVector<EqualCondition, 8> conditions;
    extractEqualConditions(cn->expression(), conditions);

    for (auto& pair : conditions) {
      // detect which variable is declared first
      auto [lhs, rhs] = pair;
      LOG_RULE << lhs.var->name << " is declared before " << rhs.var->name;

      // check if one of the two is already identified
      auto lhsIter = equivalences.variableToClass.find(lhs);
      auto rhsIter = equivalences.variableToClass.find(rhs);

      bool const lhsKnown = lhsIter != nullptr;
      bool const rhsKnown = rhsIter != nullptr;

      if (lhsKnown && !rhsKnown) {
        // simply identify rhs as well
        LOG_RULE << "IDENTIFY " << lhs << " == " << rhs;
        equivalences.variableToClass =
            std::move(equivalences.variableToClass).set(rhs, *lhsIter);

      } else if (!lhsKnown && rhsKnown) {
        // simply identify lhs as well
        LOG_RULE << "IDENTIFY " << lhs << " == " << rhs;
        equivalences.variableToClass =
            std::move(equivalences.variableToClass).set(lhs, *rhsIter);
      } else if (lhsKnown && rhsKnown) {
        // merge the two equivalence classes
        LOG_RULE << "MERGE " << lhs << " == " << rhs;
        auto lhsRoot = findRoot(lhsIter->get());
        auto rhsRoot = findRoot(rhsIter->get());
        equivalences.unionFind =
            std::move(equivalences.unionFind).set(lhsRoot, rhsRoot);
      } else {
        auto [lhs, rhs] = findDominatingVariable(plan, pair);

        LOG_RULE << "CREATE " << lhs << " == " << rhs;
        // create a new equivalence class
        // introduce a new variable if necessary
        auto variable = [&, &lhs = lhs]() -> Variable const* {
          if (lhs.path.empty()) {
            return lhs.var;
          } else {
            auto variable =
                plan.getAst()->variables()->createTemporaryVariable();
            // substitute
            auto subst = plan.createNode<CalculationNode>(
                &plan, plan.nextId(),
                std::make_unique<Expression>(plan.getAst(),
                                             lhs.expr->clone(plan.getAst())),
                variable);
            plan.insertAfter(cn, subst);
            return variable;
          }
        }();
        LOG_RULE << "WITNESS VAR " << variable->name;

        modified = true;

        auto cls = std::make_shared<Replacement>(Replacement{variable});
        equivalences.variableToClass =
            std::move(equivalences.variableToClass).set(lhs, cls);
        equivalences.variableToClass =
            std::move(equivalences.variableToClass).set(rhs, cls);
        equivalences.variableToClass =
            std::move(equivalences.variableToClass)
                .set(VarAttribKey{variable, {}}, cls);
      }
    }
  }

  return modified;
}

}  // namespace

void arangodb::aql::replaceEqualAttributeAccesses(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = processQuery(*plan, plan->root(), {}, 0);
  opt->addPlan(std::move(plan), rule, modified);
}
