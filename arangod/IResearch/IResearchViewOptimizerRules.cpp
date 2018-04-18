////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewOptimizerRules.h"
#include "IResearchViewNode.h"
#include "IResearchFilterFactory.h"
#include "IResearchOrderFactory.h"
#include "AqlHelper.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortNode.h"
#include "Aql/Optimizer.h"
#include "Aql/Condition.h"
#include "Aql/WalkerWorker.h"
#include "Aql/ExecutionNode.h"

using namespace arangodb::iresearch;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

NS_LOCAL

std::vector<arangodb::iresearch::IResearchSort> buildSort(
    ExecutionPlan const& plan,
    arangodb::aql::Variable const& ref,
    std::vector<std::pair<Variable const*, bool>> const& sorts,
    std::unordered_map<VariableId, AstNode const*> const& vars,
    bool scorersOnly
) {
  std::vector<IResearchSort> entries;

  QueryContext const ctx { nullptr, nullptr, nullptr, nullptr, &ref };

  for (auto& sort : sorts) {
    auto const* var = sort.first;
    auto varId = var->id;

    AstNode const* rootNode = nullptr;
    auto it = vars.find(varId);

    if (it != vars.end()) {
      auto const* node = rootNode = it->second;

      while (node && NODE_TYPE_ATTRIBUTE_ACCESS == node->type) {
        node = node->getMember(0);
      }

      if (node && NODE_TYPE_REFERENCE == node->type) {
        var = reinterpret_cast<Variable const*>(node->getData());
      }
    } else {
      auto const* setter = plan.getVarSetBy(varId);
      if (setter && EN::CALCULATION == setter->getType()) {
        auto const* expr = static_cast<CalculationNode const*>(setter)->expression();

        if (expr) {
          rootNode = expr->node();
        }
      }
    }

    if (var && rootNode && (!scorersOnly || OrderFactory::scorer(nullptr, *rootNode, ctx))) {
      entries.emplace_back(var, rootNode, sort.second);
    }
  }

  return entries;
}

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewConditionFinder
///////////////////////////////////////////////////////////////////////////////
class IResearchViewConditionFinder final
    : public arangodb::aql::WalkerWorker<ExecutionNode> {
 public:
  IResearchViewConditionFinder(
      ExecutionPlan* plan,
      std::unordered_map<size_t, ExecutionNode*>* changes,
      bool* hasEmptyResult) noexcept
   : _plan(plan),
     _changes(changes),
     _hasEmptyResult(hasEmptyResult) {
  }

  virtual bool before(ExecutionNode*) override;

  virtual bool enterSubquery(ExecutionNode*, ExecutionNode*) override {
    return false;
  }

 private:
  bool handleFilterCondition(
    ExecutionNode* en,
    Condition& condition
  );

  ExecutionPlan* _plan;
  std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
  std::unordered_set<VariableId> _filters;
  std::vector<std::pair<Variable const*, bool>> _sorts;
  // note: this class will never free the contents of this map
  std::unordered_map<size_t, ExecutionNode*>* _changes;
  bool* _hasEmptyResult;
}; // IResearchViewConditionFinder

bool IResearchViewConditionFinder::before(ExecutionNode* en) {
  switch (en->getType()) {
    case EN::LIMIT:
      // LIMIT invalidates the sort expression we already found
      _sorts.clear();
      _filters.clear();
      break;

    case EN::SINGLETON:
    case EN::NORESULTS:
      // in all these cases we better abort
      return true;

    case EN::FILTER: {
      std::vector<Variable const*> invars(en->getVariablesUsedHere());
      TRI_ASSERT(invars.size() == 1);
      // register which variable is used in a FILTER
      _filters.emplace(invars[0]->id);
      break;
    }

    case EN::SORT: {
      // register which variables are used in a SORT
      if (_sorts.empty()) {
        for (auto& it : static_cast<SortNode const*>(en)->getElements()) {
          _sorts.emplace_back(it.var, it.ascending);
          TRI_IF_FAILURE("IResearchViewConditionFinder::sortNode") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      }
      break;
    }

    case EN::CALCULATION: {
      auto outvars = en->getVariablesSetHere();
      TRI_ASSERT(outvars.size() == 1);

      _variableDefinitions.emplace(
          outvars[0]->id,
          static_cast<CalculationNode const*>(en)->expression()->node());
      TRI_IF_FAILURE("IResearchViewConditionFinder::variableDefinition") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      break;
    }

    case EN::ENUMERATE_IRESEARCH_VIEW: {
      auto node = static_cast<IResearchViewNode const*>(en);
      if (_changes->find(node->id()) != _changes->end()) {
        // already optimized this node
        break;
      }

      Condition filterCondition(_plan->getAst());

      if (!handleFilterCondition(en, filterCondition)) {
        break;
      }

      auto sortCondition = buildSort(
        *_plan,
        *node->outVariable(),
        _sorts,
        _variableDefinitions,
        true  // node->isInInnerLoop() // build scorers only in case if we're inside a loop
      );

      if (filterCondition.isEmpty() && sortCondition.empty()) {
        // no conditions left
        break;
      }

      auto const canUseView = !filterCondition.root() || FilterFactory::filter(
        nullptr,
        { nullptr, nullptr, nullptr, nullptr, node->outVariable() },
        *filterCondition.root()
      );

      if (canUseView) {
        auto newNode = std::make_unique<arangodb::iresearch::IResearchViewNode>(
          _plan,
          _plan->nextId(),
          node->vocbase(),
          node->view(),
          node->outVariable(),
          filterCondition.root(),
          std::move(sortCondition)
        );

        TRI_IF_FAILURE("IResearchViewConditionFinder::insertViewNode") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        // We keep this node's change
        _changes->emplace(node->id(), newNode.get());
        newNode.release();
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "filter clause "
          "not yet supported with view");
      }

      break;
    }

    default:
      // in these cases we simply ignore the intermediate nodes, note
      // that we have taken care of nodes that could throw exceptions
      // above.
      break;
  }

  return false;
}

bool IResearchViewConditionFinder::handleFilterCondition(
    ExecutionNode* en,
    Condition& condition) {
  bool foundCondition = false;
  for (auto& it : _variableDefinitions) {
    if (_filters.find(it.first) != _filters.end()) {
      // a variable used in a FILTER
      AstNode* var = const_cast<AstNode*>(it.second);
      if (!var->canThrow() && var->isDeterministic() && var->isSimple()) {
        // replace all variables inside the FILTER condition with the
        // expressions represented by the variables
        var = it.second->clone(_plan->getAst());

        auto func = [this](AstNode* node) -> AstNode* {
          if (node->type == NODE_TYPE_REFERENCE) {
            auto variable = static_cast<Variable*>(node->getData());

            if (variable != nullptr) {
              auto setter = _plan->getVarSetBy(variable->id);

              if (setter != nullptr && setter->getType() == EN::CALCULATION) {
                auto s = static_cast<CalculationNode*>(setter);
                auto filterExpression = s->expression();
                AstNode* inNode = filterExpression->nodeForModification();
                if (!inNode->canThrow() && inNode->isDeterministic() &&
                    inNode->isSimple()) {
                  return inNode;
                }
              }
            }
          }
          return node;
        };

        var = Ast::traverseAndModify(var, func);
      }
      condition.andCombine(var);
      foundCondition = true;
    }
  }

  // normalize the condition
  condition.normalize(_plan);
  TRI_IF_FAILURE("ConditionFinder::normalizePlan") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  bool const conditionIsImpossible = (foundCondition && condition.isEmpty());

  if (conditionIsImpossible) {
    // condition is always false
    for (auto const& x : en->getParents()) {
      auto noRes = new NoResultsNode(_plan, _plan->nextId());
      _plan->registerNode(noRes);
      _plan->insertDependency(x, noRes);
      *_hasEmptyResult = true;
    }
    return false;
  }

  auto const& varsValid = en->getVarsValid();

  // remove all invalid variables from the condition
  if (condition.removeInvalidVariables(varsValid)) {
    // removing left a previously non-empty OR block empty...
    // this means we can't use the index to restrict the results
    return false;
  }

  return true;
}

NS_END // NS_LOCAL

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

/// @brief move filters and sort conditions into views
void handleViewsRule(
    arangodb::aql::Optimizer* opt,
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan,
    arangodb::aql::OptimizerRule const* rule
) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  std::unordered_map<size_t, ExecutionNode*> changes;

  auto cleanupChanges = [&changes](){
    for (auto& v : changes) {
      delete v.second;
    }
  };
  TRI_DEFER(cleanupChanges());

  // newly created view nodes (replacement)
  std::unordered_set<ExecutionNode const*> createdViewNodes;

  // try to find `EnumerateViewNode`s and push corresponding filters and sorts inside
  plan->findEndNodes(nodes, true);

  bool hasEmptyResult = false;
  for (auto const& n : nodes) {
    IResearchViewConditionFinder finder(plan.get(), &changes, &hasEmptyResult);
    n->walk(finder);
  }

  createdViewNodes.reserve(changes.size());

  for (auto& it : changes) {
    auto*& node = it.second;

    if (!node || ExecutionNode::ENUMERATE_IRESEARCH_VIEW != node->getType()) {
      // filter out invalid nodes
      continue;
    }

    plan->registerNode(node);
    plan->replaceNode(plan->getNodeById(it.first), node);

    createdViewNodes.insert(node);

    // prevent double deletion by cleanupChanges()
    node = nullptr;
  }

  if (!changes.empty()) {
    std::unordered_set<ExecutionNode*> toUnlink;

    // remove filters covered by a view
    nodes.clear(); // ensure array is empty
    plan->findNodesOfType(nodes, ExecutionNode::FILTER, true);

    // `createdViewNodes` will not change
    auto const noMatch = createdViewNodes.end();

    for (auto* node : nodes) {
      // find the node with the filter expression
      auto inVar = static_cast<FilterNode const*>(node)->getVariablesUsedHere();
      TRI_ASSERT(inVar.size() == 1);

      auto setter = plan->getVarSetBy(inVar[0]->id);

      if (!setter || setter->getType() != EN::CALCULATION) {
        continue;
      }

      auto const it = createdViewNodes.find(setter->getLoop());

      if (it != noMatch) {
        toUnlink.emplace(node);
        toUnlink.emplace(setter);
        static_cast<CalculationNode*>(setter)->canRemoveIfThrows(true);
      }
    }

    // FIXME remove all sorts in case if view doesn't located inside a loop,
    // otherwise remove setters for covered sorts

    // remove setters covered by a view internally
    for (auto* node : createdViewNodes) {
      auto& viewNode = static_cast<IResearchViewNode const&>(*node);

      for (auto const& sort : viewNode.sortCondition()) {
        auto const* var = sort.var;

        if (!var) {
          continue;
        }

        auto* setter = plan->getVarSetBy(var->id);

        if (!setter || EN::CALCULATION != setter->getType()) {
          continue;
        }

        toUnlink.emplace(setter);
        static_cast<CalculationNode*>(setter)->canRemoveIfThrows(true);
      }
    }

//    nodes.clear(); // ensure array is empty
//    plan->findNodesOfType(nodes, ExecutionNode::SORT, true);
//
//    for (auto* node : nodes) {
//      // find the node with the sort expression
//      auto inVar = static_cast<aql::SortNode const*>(node)->getVariablesUsedHere();
//      TRI_ASSERT(!inVar.empty());
//
//      for (auto& var : inVar) {
//        auto setter = plan->getVarSetBy(var->id);
//
//        if (!setter || setter->getType() != ExecutionNode::CALCULATION) {
//          continue;
//        }
//
//        auto const it = createdViewNodes.find(setter->getLoop());
//
//        if (it != noMatch) {
//          if (!(*it)->isInInnerLoop()) {
//            toUnlink.emplace(node);
//            toUnlink.emplace(setter);
//          }
////FIXME uncomment when EnumerateViewNode can create variables
////        toUnlink.emplace(setter);
////        if (!(*it)->isInInnerLoop()) {
////          toUnlink.emplace(node);
////        }
//          static_cast<CalculationNode*>(setter)->canRemoveIfThrows(true);
//        }
//      }
//    }
//
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !changes.empty());
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
