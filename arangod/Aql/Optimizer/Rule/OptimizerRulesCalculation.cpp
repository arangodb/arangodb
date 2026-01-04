////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesCalculation.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallUnorderedMap.h"
#include "Containers/SmallVector.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

struct VariableReplacer final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
public:
  explicit VariableReplacer(
      std::unordered_map<VariableId, Variable const*> const& replacements)
      : replacements(replacements) {}

  bool before(ExecutionNode* en) override final {
    en->replaceVariables(replacements);
    // always continue
    return false;
  }

private:
  std::unordered_map<VariableId, Variable const*> const& replacements;
};

namespace {
// static node types used by some optimizer rules
// having them statically available avoids having to build the lists over
// and over for each AQL query
static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    removeUnnecessaryCalculationsNodeTypes{
      arangodb::aql::ExecutionNode::CALCULATION,
      arangodb::aql::ExecutionNode::SUBQUERY};

bool accessesCollectionVariable(arangodb::aql::ExecutionPlan const* plan,
                                arangodb::aql::ExecutionNode const* node,
                                arangodb::aql::VarSet& vars) {
  using EN = arangodb::aql::ExecutionNode;

  if (node->getType() == EN::CALCULATION) {
    auto nn = EN::castTo<arangodb::aql::CalculationNode const*>(node);
    vars.clear();
    arangodb::aql::Ast::getReferencedVariables(nn->expression()->node(), vars);
  } else if (node->getType() == EN::SUBQUERY) {
    auto nn = EN::castTo<arangodb::aql::SubqueryNode const*>(node);
    vars.clear();
    nn->getVariablesUsedHere(vars);
  }

  for (auto const& it : vars) {
    auto setter = plan->getVarSetBy(it->id);
    if (setter == nullptr) {
      continue;
    }
    if (setter->getType() == EN::INDEX ||
        setter->getType() == EN::ENUMERATE_COLLECTION ||
        setter->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
        setter->getType() == EN::SUBQUERY ||
        setter->getType() == EN::TRAVERSAL ||
        setter->getType() == EN::ENUMERATE_PATHS ||
        setter->getType() == EN::SHORTEST_PATH) {
      return true;
        }
  }

  return false;
}
} // namespace

/// @brief move calculations up in the plan
/// this rule modifies the plan in place
/// it aims to move up calculations as far up in the plan as possible, to
/// avoid redundant calculations in inner loops
void arangodb::aql::moveCalculationsUpRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);
  plan->findNodesOfType(nodes, EN::SUBQUERY, true);

  containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*>::allocator_type::arena_type
      subqueriesArena;
  containers::SmallUnorderedMap<ExecutionNode*, ExecutionNode*> subqueries{subqueriesArena};
  {
    containers::SmallVector<ExecutionNode*, 8> subs;
    plan->findNodesOfType(subs, ExecutionNode::SUBQUERY, true);

    // we build a map of the top-most nodes of each subquery to the outer
    // subquery node
    for (auto& it : subs) {
      auto sub = ExecutionNode::castTo<SubqueryNode const*>(it)->getSubquery();
      while (sub->hasDependency()) {
        sub = sub->getFirstDependency();
      }
      subqueries.emplace(sub, it);
    }
  }

  bool modified = false;
  VarSet neededVars;
  VarSet vars;

  for (auto const& n : nodes) {
    bool isAccessCollection = false;
    if (!n->isDeterministic()) {
      // we will only move expressions up that cannot throw and that are
      // deterministic
      continue;
    }
    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);
      if (::accessesCollectionVariable(plan.get(), nn, vars)) {
        isAccessCollection = true;
      }
    }
    // note: if it's a subquery node, it cannot move upwards if there's a
    // modification keyword in the subquery e.g.
    // INSERT would not be scope-limited by the outermost subqueries, so we
    // could end up inserting a smaller amount of documents than what's actually
    // proposed in the query.

    neededVars.clear();
    n->getVariablesUsedHere(neededVars);

    auto current = n->getFirstDependency();

    while (current != nullptr) {
      if (current->setsVariable(neededVars)) {
        // shared variable, cannot move up any more
        // done with optimizing this calculation node
        break;
      }

      auto dep = current->getFirstDependency();
      if (dep == nullptr) {
        auto it = subqueries.find(current);
        if (it != subqueries.end()) {
          // we reached the top of some subquery

          // first, unlink the calculation from the plan
          plan->unlinkNode(n);
          // and re-insert into before the subquery node
          plan->insertDependency(it->second, n);

          modified = true;
          current = n->getFirstDependency();
          continue;
        }

        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      if (current->getType() == EN::LIMIT) {
        if (!arangodb::ServerState::instance()->isCoordinator()) {
          // do not move calculations beyond a LIMIT on a single server,
          // as this would mean carrying out potentially unnecessary
          // calculations
          break;
        }

        // coordinator case
        // now check if the calculation uses data from any collection. if so,
        // we expect that it is cheaper to execute the calculation close to the
        // origin of data (e.g. IndexNode, EnumerateCollectionNode) on a DB
        // server than on a coordinator. though executing the calculation will
        // have the same costs on DB server and coordinator, the assumption is
        // that we can reduce the amount of data we need to transfer between the
        // two if we can execute the calculation on the DB server and only
        // transfer the calculation result to the coordinator instead of the
        // full documents

        if (!isAccessCollection) {
          // not accessing any collection data
          break;
        }
        // accessing collection data.
        // allow the calculation to be moved beyond the LIMIT,
        // in the hope that this reduces the amount of data we have
        // to transfer between the DB server and the coordinator
      }

      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into before the current node
      plan->insertDependency(current, n);

      modified = true;
      current = dep;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief move calculations down in the plan
/// this rule modifies the plan in place
/// it aims to move calculations as far down in the plan as possible, beyond
/// FILTER and LIMIT operations
void arangodb::aql::moveCalculationsDownRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, {EN::CALCULATION, EN::SUBQUERY}, true);

  std::vector<ExecutionNode*> stack;
  VarSet vars;
  VarSet usedHere;
  bool modified = false;

  size_t i = 0;
  for (auto const& n : nodes) {
    bool const isLastVariable = ++i == nodes.size();

    // this is the variable that the calculation will set
    Variable const* variable = nullptr;

    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);
      if (!nn->expression()->isDeterministic()) {
        // we will only move expressions down that cannot throw and that are
        // deterministic
        continue;
      }
      variable = nn->outVariable();
    } else {
      TRI_ASSERT(n->getType() == EN::SUBQUERY);
      auto nn = ExecutionNode::castTo<SubqueryNode*>(n);
      if (!nn->isDeterministic() || nn->isModificationNode()) {
        // we will only move subqueries down that are deterministic and are not
        // modification subqueries
        continue;
      }
      variable = nn->outVariable();
    }

    stack.clear();
    n->parents(stack);

    ExecutionNode* lastNode = nullptr;

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      auto const currentType = current->getType();

      usedHere.clear();
      current->getVariablesUsedHere(usedHere);

      bool varUsedHere = std::find(usedHere.begin(), usedHere.end(),
                                   variable) != usedHere.end();

      if (n->getType() == EN::CALCULATION && currentType == EN::SUBQUERY &&
          varUsedHere && !current->isVarUsedLater(variable)) {
        // move calculations into subqueries if they are required by the
        // subquery and not used later
        current = ExecutionNode::castTo<SubqueryNode*>(current)->getSubquery();
        while (current->hasDependency()) {
          current = current->getFirstDependency();
        }
        lastNode = current;
      } else {
        if (varUsedHere) {
          // the node we're looking at needs the variable we're setting.
          // can't push further!
          break;
        }

        if (currentType == EN::FILTER || currentType == EN::SORT ||
            currentType == EN::LIMIT || currentType == EN::SINGLETON ||
            // do not move a subquery past another unrelated subquery
            (currentType == EN::SUBQUERY && n->getType() != EN::SUBQUERY)) {
          // we found something interesting that justifies moving our node down
          if (currentType == EN::LIMIT &&
              arangodb::ServerState::instance()->isCoordinator()) {
            // in a cluster, we do not want to move the calculations as far down
            // as possible, because this will mean we may need to transfer a lot
            // more data between DB servers and the coordinator

            // assume first that we want to move the node past the LIMIT

            // however, if our calculation uses any data from a
            // collection/index/view, it probably makes sense to not move it,
            // because the result set may be huge
            if (::accessesCollectionVariable(plan.get(), n, vars)) {
              break;
            }
          }

          lastNode = current;
        } else if (currentType == EN::INDEX ||
                   currentType == EN::ENUMERATE_COLLECTION ||
                   currentType == EN::ENUMERATE_IRESEARCH_VIEW ||
                   currentType == EN::ENUMERATE_LIST ||
                   currentType == EN::TRAVERSAL ||
                   currentType == EN::SHORTEST_PATH ||
                   currentType == EN::ENUMERATE_PATHS ||
                   currentType == EN::COLLECT || currentType == EN::NORESULTS) {
          // we will not push further down than such nodes
          break;
        }
      }

      if (!current->hasParent()) {
        break;
      }

      current->parents(stack);
    }

    if (lastNode != nullptr && lastNode->getFirstParent() != nullptr) {
      // first, unlink the calculation from the plan
      plan->unlinkNode(n);

      // and re-insert into after the last "good" node
      plan->insertDependency(lastNode->getFirstParent(), n);
      modified = true;

      // any changes done here may affect the following iterations
      // of this optimizer rule, so we need to recalculate the
      // variable usage here.
      if (!isLastVariable) {
        plan->clearVarUsageComputed();
        plan->findVarUsage();
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove CalculationNode(s) that are repeatedly used in a query
/// (i.e. common expressions)
void arangodb::aql::removeRedundantCalculationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  if (nodes.size() < 2) {
    // quick exit
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  std::string buffer;
  std::unordered_map<VariableId, Variable const*> replacements;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic()) {
      // If this node is non-deterministic, we must not touch it!
      continue;
    }

    arangodb::aql::Variable const* outvar = nn->outVariable();

    try {
      buffer.clear();
      nn->expression()->stringifyIfNotTooLong(buffer);
    } catch (...) {
      // expression could not be stringified (maybe because not all node types
      // are supported). this is not an error, we just skip the optimization
      continue;
    }

    std::string const referenceExpression(std::move(buffer));

    std::vector<ExecutionNode*> stack;
    n->dependencies(stack);

    while (!stack.empty()) {
      auto current = stack.back();
      stack.pop_back();

      if (current->getType() == EN::CALCULATION) {
        try {
          buffer.clear();
          ExecutionNode::castTo<CalculationNode const*>(current)
              ->expression()
              ->stringifyIfNotTooLong(buffer);

          if (buffer == referenceExpression) {
            // expressions are identical
            // check if target variable is already registered as a replacement
            // this covers the following case:
            // - replacements is set to B => C
            // - we're now inserting a replacement A => B
            // the goal now is to enter a replacement A => C instead of A => B
            auto target = ExecutionNode::castTo<CalculationNode const*>(current)
                              ->outVariable();
            while (target != nullptr) {
              auto it = replacements.find(target->id);

              if (it != replacements.end()) {
                target = (*it).second;
              } else {
                break;
              }
            }
            replacements.emplace(outvar->id, target);

            // also check if the insertion enables further shortcuts
            // this covers the following case:
            // - replacements is set to A => B
            // - we have just inserted a replacement B => C
            // the goal now is to change the replacement A => B to A => C
            for (auto it = replacements.begin(); it != replacements.end();
                 ++it) {
              if ((*it).second == outvar) {
                (*it).second = target;
              }
            }
          }
        } catch (...) {
          // expression could not be stringified (maybe because not all node
          // types are supported). this is not an error, we just skip the
          // optimization
          continue;
        }
      }

      if (current->getType() == EN::COLLECT) {
        if (ExecutionNode::castTo<CollectNode*>(current)->hasOutVariable()) {
          // COLLECT ... INTO is evil (tm): it needs to keep all already defined
          // variables
          // we need to abort optimization here
          break;
        }
      }

      if (!current->hasDependency()) {
        // node either has no or more than one dependency. we don't know what to
        // do and must abort
        // note: this will also handle Singleton nodes
        break;
      }

      current->dependencies(stack);
    }
  }

  if (!replacements.empty()) {
    // finally replace the variables
    VariableReplacer finder(replacements);
    plan->root()->walk(finder);
  }

  opt->addPlan(std::move(plan), rule, !replacements.empty());
}

/// @brief remove CalculationNodes and SubqueryNodes that are never needed
/// this modifies an existing plan in place
void arangodb::aql::removeUnnecessaryCalculationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ::removeUnnecessaryCalculationsNodeTypes, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  bool modified = false;

  for (auto const& n : nodes) {
    Variable const* outVariable = nullptr;

    if (n->getType() == EN::CALCULATION) {
      auto nn = ExecutionNode::castTo<CalculationNode*>(n);

      if (!nn->isDeterministic()) {
        // If this node is non-deterministic, we must not optimize it away!
        continue;
      }

      outVariable = nn->outVariable();
      // will remove calculation when we get here
    } else if (n->getType() == EN::SUBQUERY) {
      auto nn = ExecutionNode::castTo<SubqueryNode*>(n);

      if (!nn->isDeterministic()) {
        // subqueries that are non-deterministic must not be optimized away
        continue;
      }

      if (nn->isModificationNode()) {
        // subqueries that modify data must not be optimized away
        continue;
      }
      // will remove subquery when we get here
      outVariable = nn->outVariable();
    } else {
      TRI_ASSERT(false);
      continue;
    }

    TRI_ASSERT(outVariable != nullptr);

    if (!n->isVarUsedLater(outVariable)) {
      // The variable whose value is calculated here is not used at
      // all further down the pipeline! We remove the whole
      // calculation node,
      toUnlink.emplace(n);
    } else if (n->getType() == EN::CALCULATION) {
      // variable is still used later, but...
      // ...if it's used exactly once later by another calculation,
      // it's a temporary variable that we can fuse with the other
      // calculation easily
      CalculationNode* calcNode = ExecutionNode::castTo<CalculationNode*>(n);

      if (!calcNode->expression()->isDeterministic()) {
        continue;
      }

      AstNode const* rootNode = calcNode->expression()->node();

      if (rootNode->type == NODE_TYPE_REFERENCE) {
        // if the LET is a simple reference to another variable, e.g. LET a = b
        // then replace all references to a with references to b
        bool hasCollectWithOutVariable = false;
        auto current = n->getFirstParent();

        // check first if we have a COLLECT with an INTO later in the query
        // in this case we must not perform the replacements
        while (current != nullptr) {
          if (current->getType() == EN::COLLECT) {
            CollectNode const* collectNode =
                ExecutionNode::castTo<CollectNode const*>(current);
            if (collectNode->hasOutVariable() &&
                !collectNode->hasExpressionVariable()) {
              hasCollectWithOutVariable = true;
              break;
            }
          }
          current = current->getFirstParent();
        }

        if (!hasCollectWithOutVariable) {
          // no COLLECT found, now replace
          std::unordered_map<VariableId, Variable const*> replacements;
          replacements.try_emplace(
              outVariable->id,
              static_cast<Variable const*>(rootNode->getData()));

          VariableReplacer finder(replacements);
          plan->root()->walk(finder);
          toUnlink.emplace(n);
          continue;
        }
      } else if (rootNode->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // if the LET is a simple attribute access, e.g. LET a = b.c
        // then replace all references to a with b.c in all following nodes.
        // note: we can only safely replace variables inside CalculationNodes,
        // but no other node types
        bool eligible = true;
        auto current = n->getFirstParent();

        VarSet vars;
        std::vector<CalculationNode*> found;

        // check first if we have a COLLECT with an INTO later in the query
        // in this case we must not perform the replacements
        while (current != nullptr) {
          vars.clear();
          current->getVariablesUsedHere(vars);
          if (current->getType() != EN::CALCULATION) {
            // variable used by other node type than CalculationNode.
            // we cannot proceed.
            if (vars.contains(outVariable)) {
              eligible = false;
              break;
            }
          } else {
            // variable used by CalculationNode.
            if (vars.contains(outVariable)) {
              // now remember which CalculationNodes contain references to
              // our variable.
              found.emplace_back(
                  ExecutionNode::castTo<CalculationNode*>(current));
            }
          }

          // check if we have a COLLECT with into
          if (current->getType() == EN::COLLECT) {
            CollectNode const* collectNode =
                ExecutionNode::castTo<CollectNode const*>(current);
            if (collectNode->hasOutVariable() &&
                !collectNode->hasExpressionVariable()) {
              eligible = false;
              break;
            }
          }
          current = current->getFirstParent();
        }

        if (eligible) {
          auto visitor = [&](AstNode* node) {
            if (node->type == NODE_TYPE_REFERENCE &&
                static_cast<Variable const*>(node->getData()) ==
                    calcNode->outVariable()) {
              return const_cast<AstNode*>(rootNode);
            }
            return node;
          };
          for (auto const& it : found) {
            AstNode* simplified = plan->getAst()->traverseAndModify(
                it->expression()->nodeForModification(), visitor);
            it->expression()->replaceNode(simplified);
          }
          toUnlink.emplace(n);
          continue;
        }
      }

      VarSet vars;

      size_t usageCount = 0;
      CalculationNode* other = nullptr;
      auto current = n->getFirstParent();

      while (current != nullptr) {
        current->getVariablesUsedHere(vars);
        if (vars.contains(outVariable)) {
          if (current->getType() == EN::COLLECT) {
            if (ExecutionNode::castTo<CollectNode const*>(current)
                    ->hasOutVariable()) {
              // COLLECT with an INTO variable will collect all variables from
              // the scope, so we shouldn't try to remove or change the meaning
              // of variables
              usageCount = 0;
              break;
            }
          }
          if (current->getType() != EN::CALCULATION) {
            // don't know how to replace the variable in a non-LET node
            // abort the search
            usageCount = 0;
            break;
          }

          // got a LET. we can replace the variable reference in it by
          // something else
          ++usageCount;
          other = ExecutionNode::castTo<CalculationNode*>(current);
        }

        if (usageCount > 1) {
          break;
        }

        current = current->getFirstParent();
        vars.clear();
      }

      if (usageCount == 1) {
        // our variable is used by exactly one other calculation
        // now we can replace the reference to our variable in the other
        // calculation with the variable's expression directly
        auto otherExpression = other->expression();

        if (rootNode->type != NODE_TYPE_ATTRIBUTE_ACCESS &&
            Ast::countReferences(otherExpression->node(), outVariable) > 1) {
          // used more than once... better give up
          continue;
        }

        if (rootNode->isSimple() != otherExpression->node()->isSimple()) {
          // expression types (V8 vs. non-V8) do not match. give up
          continue;
        }

        auto otherLoop = other->getLoop();

        if (otherLoop != nullptr && rootNode->callsFunction()) {
          auto nLoop = n->getLoop();

          if (nLoop != otherLoop) {
            // original expression calls a function and is not contained in a
            // loop. we're about to move this expression into a loop, but we
            // don't want to move (expensive) function calls into loops
            continue;
          }
          VarSet outer = nLoop->getVarsValid();
          VarSet used;
          Ast::getReferencedVariables(rootNode, used);
          bool doOptimize = true;
          for (auto& it : used) {
            if (outer.find(it) == outer.end()) {
              doOptimize = false;
              break;
            }
          }
          if (!doOptimize) {
            continue;
          }
        }

        TRI_ASSERT(other != nullptr);
        otherExpression->replaceVariableReference(outVariable, rootNode);

        toUnlink.emplace(n);
      }
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
    TRI_ASSERT(nodes.size() >= toUnlink.size());
    modified = true;
    if (nodes.size() - toUnlink.size() > 0) {
      // need to rerun the rule because removing calculations may unlock
      // removal of further calculations
      opt->addPlanAndRerun(std::move(plan), rule, modified);
    } else {
      // no need to rerun the rule
      opt->addPlan(std::move(plan), rule, modified);
    }
  } else {
    opt->addPlan(std::move(plan), rule, modified);
  }
}