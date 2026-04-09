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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CollectInCluster.h"

#include "Aql/Aggregator.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/SortElement.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"

namespace {

using EN = arangodb::aql::ExecutionNode;

void replaceGatherNodeVariables(
    arangodb::aql::ExecutionPlan* plan, arangodb::aql::GatherNode* gatherNode,
    std::unordered_map<arangodb::aql::Variable const*,
                       arangodb::aql::Variable const*> const& replacements) {
  std::string cmp;
  std::string buffer;

  arangodb::aql::SortElementVector& elements = gatherNode->elements();
  for (auto& it : elements) {
    auto it2 = replacements.find(it.var);

    if (it2 != replacements.end()) {
      it.resetTo((*it2).second);
    } else {
      cmp = it.toVarAccessString();
      for (auto const& it3 : replacements) {
        auto setter = plan->getVarSetBy(it3.first->id);
        if (setter == nullptr || setter->getType() != EN::CALCULATION) {
          continue;
        }
        auto* expr = EN::castTo<arangodb::aql::CalculationNode const*>(setter)
                         ->expression();
        buffer.clear();
        expr->stringify(buffer);
        if (cmp == buffer) {
          it.resetTo(it3.second);
          break;
        }
      }
    }
  }
}

}  // namespace

namespace arangodb::aql {

void collectInClusterRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                          OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  VarSet allUsed;
  VarSet used;

  for (auto& node : nodes) {
    allUsed.clear();
    used.clear();
    node->getVariablesUsedHere(used);

    TRI_ASSERT(node->getDependencies().size() == 1);

    auto collectNode = ExecutionNode::castTo<CollectNode*>(node);
    GatherNode* gatherNode = nullptr;
    auto current = node->getFirstDependency();

    while (current != nullptr) {
      if (current->getType() == EN::LIMIT) {
        break;
      }

      if (current->getType() != EN::GATHER) {
        current->getVariablesUsedHere(allUsed);
      }

      bool eligible = true;
      for (auto const& it : current->getVariablesSetHere()) {
        if (used.contains(it)) {
          eligible = false;
          break;
        }
      }

      if (!eligible) {
        break;
      }

      if (current->getType() == ExecutionNode::GATHER) {
        gatherNode = ExecutionNode::castTo<GatherNode*>(current);
      } else if (current->getType() == ExecutionNode::REMOTE) {
        auto previous = current->getFirstDependency();

        {
          bool hasFoundMultipleShards = false;
          auto p = previous;
          while (p != nullptr) {
            switch (p->getType()) {
              case ExecutionNode::REMOTE: {
                hasFoundMultipleShards = true;
                break;
              }
              case ExecutionNode::ENUMERATE_COLLECTION:
              case ExecutionNode::INDEX: {
                auto col = utils::getCollection(p);
                if (col->numberOfShards() > 1 ||
                    (col->type() == TRI_COL_TYPE_EDGE && col->isSmart())) {
                  hasFoundMultipleShards = true;
                }
                break;
              }
              case ExecutionNode::TRAVERSAL: {
                hasFoundMultipleShards = true;
                break;
              }
              case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
                auto& viewNode =
                    *ExecutionNode::castTo<iresearch::IResearchViewNode*>(p);
                auto collections = viewNode.collections();
                auto const collCount = collections.size();
                TRI_ASSERT(collCount > 0);
                hasFoundMultipleShards = collCount > 0;
                if (collCount == 1) {
                  hasFoundMultipleShards =
                      collections.front().first.get().numberOfShards() > 1;
                }
                break;
              }
              default:
                break;
            }

            if (hasFoundMultipleShards) {
              break;
            }
            p = p->getFirstDependency();
          }
          if (!hasFoundMultipleShards) {
            break;
          }
        }

        ExecutionNode* target = current;
        while (previous != nullptr &&
               previous->getType() == ExecutionNode::COLLECT) {
          target = previous;
          previous = previous->getFirstDependency();
        }

        TRI_ASSERT(eligible);

        if (previous != nullptr) {
          for (auto const& otherVariable : allUsed) {
            auto const setHere = collectNode->getVariablesSetHere();
            if (std::find(setHere.begin(), setHere.end(), otherVariable) ==
                setHere.end()) {
              eligible = false;
              break;
            }
          }

          if (!eligible) {
            break;
          }

          bool removeGatherNodeSort = false;

          if (collectNode->aggregationMethod() ==
              CollectOptions::CollectMethod::kCount) {
            TRI_ASSERT(collectNode->aggregateVariables().size() == 1);
            TRI_ASSERT(collectNode->hasOutVariable() == false);

            auto outVariable =
                plan->getAst()->variables()->createTemporaryVariable();
            std::vector<AggregateVarInfo> aggregateVariables;
            aggregateVariables.emplace_back(AggregateVarInfo{
                outVariable, collectNode->aggregateVariables()[0].inVar,
                "LENGTH"});
            auto dbCollectNode = plan->createNode<CollectNode>(
                plan.get(), plan->nextId(), collectNode->getOptions(),
                collectNode->groupVariables(), aggregateVariables, nullptr,
                nullptr, std::vector<std::pair<Variable const*, std::string>>{},
                collectNode->variableMap());

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());

            collectNode->aggregateVariables()[0].type = "SUM";
            collectNode->aggregateVariables()[0].inVar = outVariable;
            collectNode->aggregationMethod(
                CollectOptions::CollectMethod::kSorted);

            removeGatherNodeSort = true;
          } else if (collectNode->aggregationMethod() ==
                     CollectOptions::CollectMethod::kDistinct) {
            auto const& groupVars = collectNode->groupVariables();
            TRI_ASSERT(!groupVars.empty());
            auto out = plan->getAst()->variables()->createTemporaryVariable();

            std::vector<GroupVarInfo> const groupVariables{
                GroupVarInfo{out, groupVars[0].inVar}};

            auto dbCollectNode = plan->createNode<CollectNode>(
                plan.get(), plan->nextId(), collectNode->getOptions(),
                groupVariables, collectNode->aggregateVariables(), nullptr,
                nullptr, std::vector<std::pair<Variable const*, std::string>>{},
                collectNode->variableMap());

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());

            auto copy = collectNode->groupVariables();
            TRI_ASSERT(!copy.empty());
            std::unordered_map<Variable const*, Variable const*> replacements;
            replacements.try_emplace(copy[0].inVar, out);
            copy[0].inVar = out;
            collectNode->groupVariables(copy);

            replaceGatherNodeVariables(plan.get(), gatherNode, replacements);
          } else if (!collectNode->hasOutVariable() ||
                     collectNode->getOptions()
                         .aggregateIntoExpressionOnDBServers) {
            std::vector<AggregateVarInfo> dbServerAggVars;
            for (auto const& it : collectNode->aggregateVariables()) {
              std::string_view func = Aggregator::pushToDBServerAs(it.type);
              if (func.empty()) {
                eligible = false;
                break;
              }
              auto outVariable =
                  plan->getAst()->variables()->createTemporaryVariable();
              dbServerAggVars.emplace_back(
                  AggregateVarInfo{outVariable, it.inVar, std::string(func)});
            }

            if (!eligible) {
              break;
            }

            auto const& groupVars = collectNode->groupVariables();
            std::vector<GroupVarInfo> outVars;
            outVars.reserve(groupVars.size());
            std::unordered_map<Variable const*, Variable const*> replacements;

            for (auto const& it : groupVars) {
              auto out = plan->getAst()->variables()->createTemporaryVariable();
              replacements.try_emplace(it.inVar, out);
              outVars.emplace_back(GroupVarInfo{out, it.inVar});
            }

            Variable const* expressionVariable = nullptr;
            Variable const* outVariable = nullptr;
            std::vector<std::pair<Variable const*, std::string>> keepVariables;

            bool const aggregateOutVariablesOnDBServers =
                collectNode->getOptions().aggregateIntoExpressionOnDBServers &&
                collectNode->hasOutVariable();

            if (aggregateOutVariablesOnDBServers) {
              outVariable =
                  plan->getAst()->variables()->createTemporaryVariable();
              expressionVariable = collectNode->expressionVariable();
              keepVariables = collectNode->keepVariables();
            }

            auto dbCollectNode = plan->createNode<CollectNode>(
                plan.get(), plan->nextId(), collectNode->getOptions(), outVars,
                dbServerAggVars, expressionVariable, outVariable,
                std::move(keepVariables), collectNode->variableMap());

            dbCollectNode->addDependency(previous);
            target->replaceDependency(previous, dbCollectNode);

            dbCollectNode->aggregationMethod(collectNode->aggregationMethod());

            std::vector<GroupVarInfo> copy;
            size_t i = 0;
            for (GroupVarInfo const& it : collectNode->groupVariables()) {
              copy.emplace_back(
                  GroupVarInfo{/*outVar*/ it.outVar,
                               /*inVar*/ outVars[i].outVar});
              ++i;
            }
            collectNode->groupVariables(copy);

            size_t j = 0;
            for (AggregateVarInfo& it : collectNode->aggregateVariables()) {
              it.inVar = dbServerAggVars[j].outVar;
              it.type = Aggregator::runOnCoordinatorAs(it.type);
              ++j;
            }

            if (aggregateOutVariablesOnDBServers) {
              TRI_ASSERT(outVariable != nullptr);
              collectNode->setMergeListsAggregation(outVariable);
            }

            removeGatherNodeSort = (dbCollectNode->aggregationMethod() !=
                                    CollectOptions::CollectMethod::kSorted);

            if (gatherNode != nullptr && !removeGatherNodeSort &&
                !replacements.empty() && !gatherNode->elements().empty()) {
              replaceGatherNodeVariables(plan.get(), gatherNode, replacements);
            }
          } else {
            break;
          }

          if (gatherNode != nullptr && removeGatherNodeSort) {
            gatherNode->elements().clear();
          }

          wasModified = true;
        }
        break;
      }

      current = current->getFirstDependency();
    }
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

}  // namespace arangodb::aql
