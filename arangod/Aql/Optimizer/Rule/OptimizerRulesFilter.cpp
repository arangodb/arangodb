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
/// @author Julia Puget
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesFilter.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {
static constexpr std::initializer_list<
    arangodb::aql::ExecutionNode::NodeType> const moveFilterIntoEnumerateTypes{
      arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
      arangodb::aql::ExecutionNode::INDEX,
      arangodb::aql::ExecutionNode::ENUMERATE_LIST};
} // namespace

/// @brief remove all unnecessary filters
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
void arangodb::aql::removeUnnecessaryFiltersRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
    // now check who introduced our variable
    auto variable = ExecutionNode::castTo<FilterNode const*>(n)->inVariable();
    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      // filter variable was not introduced by a calculation.
      continue;
    }

    // filter variable was introduced a CalculationNode. now check the
    // expression
    auto s = ExecutionNode::castTo<CalculationNode*>(setter);
    auto root = s->expression()->node();

    if (!root->isDeterministic()) {
      // we better not tamper with this filter
      continue;
    }

    // filter expression is constant and thus cannot throw
    // we can now evaluate it safely

    if (root->isTrue()) {
      // filter is always true
      // remove filter node and merge with following node
      toUnlink.emplace(n);
      modified = true;
    }
    // before 3.6, if the filter is always false (i.e. root->isFalse()), at this
    // point a NoResultsNode was inserted.
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief move filters up in the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// filters are not pushed beyond limits
void arangodb::aql::moveFiltersUpRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  std::vector<ExecutionNode*> stack;
  bool modified = false;

  for (auto const& n : nodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(n);
    auto inVar = fn->inVariable();

    stack.clear();
    n->dependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::LIMIT || current->getType() == EN::WINDOW) {
        // cannot push a filter beyond a LIMIT or WINDOW node
        break;
      }

      if (!current->isDeterministic()) {
        // TODO: validate if this is actually necessary
        // must not move a filter beyond a node that is non-deterministic
        break;
      }

      if (current->isModificationNode()) {
        // must not move a filter beyond a modification node
        break;
      }

      if (current->getType() == EN::CALCULATION) {
        // must not move a filter beyond a node with a non-deterministic result
        auto calculation =
            ExecutionNode::castTo<CalculationNode const*>(current);
        if (!calculation->expression()->isDeterministic()) {
          break;
        }
      }

      bool found = false;

      for (auto const& v : current->getVariablesSetHere()) {
        if (inVar == v) {
          // shared variable, cannot move up any more
          found = true;
          break;
        }
      }

      if (found) {
        // done with optimizing this calculation node
        break;
      }

      if (!current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      current->dependencies(stack);

      // first, unlink the filter from the plan
      plan->unlinkNode(n);
      // and re-insert into plan in front of the current node
      plan->insertDependency(current, n);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief fuse filter conditions that follow each other
void arangodb::aql::fuseFiltersRule(Optimizer* opt,
                                    std::unique_ptr<ExecutionPlan> plan,
                                    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  if (nodes.size() < 2) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  ::arangodb::containers::HashSet<ExecutionNode*> seen;
  // candidates of CalculationNode, FilterNode
  std::vector<std::pair<ExecutionNode*, ExecutionNode*>> candidates;

  bool modified = false;

  for (auto const& n : nodes) {
    if (seen.find(n) != seen.end()) {
      // already processed
      continue;
    }

    Variable const* nextExpectedVariable = nullptr;
    ExecutionNode* lastFilter = nullptr;
    candidates.clear();

    ExecutionNode* current = n;
    while (current != nullptr) {
      if (current->getType() == EN::CALCULATION) {
        auto cn = ExecutionNode::castTo<CalculationNode*>(current);
        if (!cn->isDeterministic() ||
            cn->outVariable() != nextExpectedVariable) {
          break;
        }
        TRI_ASSERT(lastFilter != nullptr);
        candidates.emplace_back(current, lastFilter);
        nextExpectedVariable = nullptr;
      } else if (current->getType() == EN::FILTER) {
        seen.emplace(current);

        if (nextExpectedVariable != nullptr) {
          // an unexpected order of nodes
          break;
        }
        nextExpectedVariable =
            ExecutionNode::castTo<FilterNode const*>(current)->inVariable();
        TRI_ASSERT(nextExpectedVariable != nullptr);
        if (current->isVarUsedLater(nextExpectedVariable)) {
          // filter input variable is also used for other things. we must not
          // remove it or the corresponding calculation
          break;
        }
        lastFilter = current;
      } else {
        // all other types of nodes we cannot optimize
        break;
      }
      current = current->getFirstDependency();
    }

    if (candidates.size() >= 2) {
      modified = true;
      AstNode* root =
          ExecutionNode::castTo<CalculationNode*>(candidates[0].first)
              ->expression()
              ->nodeForModification();
      for (size_t i = 1; i < candidates.size(); ++i) {
        root = plan->getAst()->createNodeBinaryOperator(
            NODE_TYPE_OPERATOR_BINARY_AND,
            ExecutionNode::castTo<CalculationNode const*>(candidates[i].first)
                ->expression()
                ->node(),
            root);

        // throw away all now-unused filters and calculations
        plan->unlinkNode(candidates[i - 1].second);
        plan->unlinkNode(candidates[i - 1].first);
      }

      ExecutionNode* en = candidates.back().first;
      TRI_ASSERT(en->getType() == EN::CALCULATION);
      ExecutionNode::castTo<CalculationNode*>(en)->expression()->replaceNode(
          root);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief move filters into EnumerateCollection nodes
void arangodb::aql::moveFiltersIntoEnumerateRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ::moveFilterIntoEnumerateTypes, true);

  VarSet found;
  VarSet introduced;

  for (auto const& n : nodes) {
    if (n->getType() == EN::INDEX &&
        ExecutionNode::castTo<IndexNode const*>(n)->getIndexes().size() != 1) {
      // we can only handle exactly one index right now. otherwise some
      // IndexExecutor code may assert and fail
      continue;
    }

    std::vector<Variable const*> outVariable;
    outVariable.resize(1);

    if (n->getType() == EN::INDEX || n->getType() == EN::ENUMERATE_COLLECTION) {
      auto en = dynamic_cast<DocumentProducingNode*>(n);
      if (en == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "unable to cast node to DocumentProducingNode");
      }

      outVariable[0] = en->outVariable();
    } else {
      TRI_ASSERT(n->getType() == EN::ENUMERATE_LIST);
      outVariable = ExecutionNode::castTo<EnumerateListNode const*>(n)
                        ->getVariablesSetHere();
    }

    bool isUsedLater = n->isVarUsedLater(outVariable[0]);
    if (outVariable.size() > 1) {
      isUsedLater |= n->isVarUsedLater(outVariable[1]);
    }
    if (!isUsedLater) {
      // e.g. FOR doc IN collection RETURN 1
      continue;
    }

    containers::FlatHashMap<Variable const*, CalculationNode*> calculations;
    introduced.clear();

    ExecutionNode* current = n->getFirstParent();

    while (current != nullptr) {
      if (current->getType() != EN::FILTER &&
          current->getType() != EN::CALCULATION) {
        break;
      }

      if (current->getType() == EN::FILTER) {
        if (calculations.empty()) {
          break;
        }

        auto filterNode = ExecutionNode::castTo<FilterNode*>(current);
        Variable const* inVariable = filterNode->inVariable();

        auto it = calculations.find(inVariable);
        if (it == calculations.end()) {
          break;
        }

        CalculationNode* cn = (*it).second;
        Expression* expr = cn->expression();

        auto setFilter = [&](auto* en, Expression* expr) {
          Expression* existingFilter = en->filter();
          if (existingFilter != nullptr && existingFilter->node() != nullptr) {
            // node already has a filter, now AND-merge it with what we found!
            AstNode* merged = plan->getAst()->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_AND, existingFilter->node(),
                expr->node());

            en->setFilter(std::make_unique<Expression>(plan->getAst(), merged));
          } else {
            // node did not yet have a filter
            en->setFilter(expr->clone(plan->getAst()));
          }
        };

        if (n->getType() == EN::INDEX ||
            n->getType() == EN::ENUMERATE_COLLECTION) {
          auto en = dynamic_cast<DocumentProducingNode*>(n);
          TRI_ASSERT(en != nullptr);
          setFilter(en, expr);
        } else {
          TRI_ASSERT(n->getType() == EN::ENUMERATE_LIST);
          setFilter(ExecutionNode::castTo<EnumerateListNode*>(n), expr);
        }

        // remove the filter
        ExecutionNode* filterParent = current->getFirstParent();
        TRI_ASSERT(filterParent != nullptr);
        plan->unlinkNode(current);

        if (!current->isVarUsedLater(cn->outVariable())) {
          // also remove the calculation node
          plan->unlinkNode(cn);
        }

        current = filterParent;
        modified = true;
        continue;
      } else if (current->getType() == EN::CALCULATION) {
        // store all calculations we found
        TRI_vocbase_t& vocbase = plan->getAst()->query().vocbase();
        auto calculationNode = ExecutionNode::castTo<CalculationNode*>(current);
        auto expr = calculationNode->expression();
        if (!expr->isDeterministic() ||
            !expr->canRunOnDBServer(vocbase.isOneShard())) {
          break;
        }

        TRI_ASSERT(!expr->willUseV8());
        found.clear();
        Ast::getReferencedVariables(expr->node(), found);

        bool isFound = found.contains(outVariable[0]);
        if (outVariable.size() > 1) {
          isFound |= found.contains(outVariable[1]);
        }
        if (isFound) {
          // check if the introduced variable refers to another temporary
          // variable that is not valid yet in the EnumerateCollection/Index
          // node, which would prevent moving the calculation and filter
          // upwards, e.g.
          //   FOR doc IN collection
          //     LET a = RAND()
          //     FILTER doc.value == 2 && doc.value > a
          bool eligible = std::none_of(
              introduced.begin(), introduced.end(),
              [&](Variable const* temp) { return found.contains(temp); });

          if (eligible) {
            calculations.emplace(calculationNode->outVariable(),
                                 calculationNode);
          }
        }

        // track all newly introduced variables
        introduced.emplace(calculationNode->outVariable());
      }

      current = current->getFirstParent();
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}