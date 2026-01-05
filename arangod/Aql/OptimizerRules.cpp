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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRules.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Aggregator.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstHelper.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/CollectOptions.h"
#include "Aql/Collection.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/MaterializeNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/MaterializeRocksDBNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ReplaceNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryEndExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/SubqueryStartExecutionNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/MaterializeNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Aql/ExecutionNode/WindowNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/IndexHint.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/SortElement.h"
#include "Aql/SortInformation.h"
#include "Aql/TraversalConditionFinder.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/NumberUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/SupervisedBuffer.h"
#include "Containers/FlatHashSet.h"
#include "Containers/SmallUnorderedMap.h"
#include "Containers/SmallVector.h"
#include "Geo/GeoParams.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Methods.h"
#include "VocBase/Methods/Collections.h"

#include <absl/strings/str_cat.h>

#include <initializer_list>
#include <span>
#include <tuple>

namespace {

bool willUseV8(arangodb::aql::ExecutionPlan const& plan) {
  struct V8Checker
      : arangodb::aql::WalkerWorkerBase<arangodb::aql::ExecutionNode> {
    bool before(arangodb::aql::ExecutionNode* n) override {
      if (n->getType() == arangodb::aql::ExecutionNode::CALCULATION &&
          static_cast<arangodb::aql::CalculationNode*>(n)
              ->expression()
              ->willUseV8()) {
        result = true;
        return true;
      }
      return false;
    }
    bool result{false};
  } walker{};
  plan.root()->walk(walker);
  return walker.result;
}

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

// static node types used by some optimizer rules
// having them statically available avoids having to build the lists over
// and over for each AQL query
static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    removeUnnecessaryCalculationsNodeTypes{
        arangodb::aql::ExecutionNode::CALCULATION,
        arangodb::aql::ExecutionNode::SUBQUERY};
static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    interchangeAdjacentEnumerationsNodeTypes{
        arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
        arangodb::aql::ExecutionNode::ENUMERATE_LIST};
static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    scatterInClusterNodeTypes{
        arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
        arangodb::aql::ExecutionNode::ENUMERATE_NEAR_VECTORS,
        arangodb::aql::ExecutionNode::INDEX,
        arangodb::aql::ExecutionNode::MATERIALIZE,
        arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW,
        arangodb::aql::ExecutionNode::INSERT,
        arangodb::aql::ExecutionNode::UPDATE,
        arangodb::aql::ExecutionNode::REPLACE,
        arangodb::aql::ExecutionNode::REMOVE,
        arangodb::aql::ExecutionNode::UPSERT};
static constexpr std::initializer_list<arangodb::aql::ExecutionNode::NodeType>
    removeDataModificationOutVariablesNodeTypes{
        arangodb::aql::ExecutionNode::REMOVE,
        arangodb::aql::ExecutionNode::INSERT,
        arangodb::aql::ExecutionNode::UPDATE,
        arangodb::aql::ExecutionNode::REPLACE,
        arangodb::aql::ExecutionNode::UPSERT};
static constexpr std::initializer_list<
    arangodb::aql::ExecutionNode::NodeType> const moveFilterIntoEnumerateTypes{
    arangodb::aql::ExecutionNode::ENUMERATE_COLLECTION,
    arangodb::aql::ExecutionNode::INDEX,
    arangodb::aql::ExecutionNode::ENUMERATE_LIST};

bool applyGraphProjections(arangodb::aql::TraversalNode* traversal) {
  auto* options =
      static_cast<arangodb::traverser::TraverserOptions*>(traversal->options());
  arangodb::containers::FlatHashSet<arangodb::aql::AttributeNamePath>
      attributes;
  bool modified = false;
  size_t maxProjections = options->getMaxProjections();
  auto pathOutVariable = traversal->pathOutVariable();

  // find projections for vertex output variable
  bool useVertexProjections = true;

  // if the path does not include vertices, we can restrict the vertex
  // gathering to only the required attributes
  if (traversal->vertexOutVariable() != nullptr) {
    useVertexProjections = arangodb::aql::utils::findProjections(
        traversal, traversal->vertexOutVariable(), /*expectedAttribute*/ "",
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useVertexProjections && options->producePathsVertices() &&
      pathOutVariable != nullptr) {
    useVertexProjections = arangodb::aql::utils::findProjections(
        traversal, pathOutVariable, arangodb::StaticStrings::GraphQueryVertices,
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useVertexProjections && !attributes.empty() &&
      attributes.size() <= maxProjections) {
    traversal->setVertexProjections(
        arangodb::aql::Projections(std::move(attributes)));
    modified = true;
  }

  // find projections for edge output variable
  attributes.clear();
  bool useEdgeProjections = true;

  if (traversal->edgeOutVariable() != nullptr) {
    useEdgeProjections = arangodb::aql::utils::findProjections(
        traversal, traversal->edgeOutVariable(), /*expectedAttribute*/ "",
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useEdgeProjections && options->producePathsEdges() &&
      pathOutVariable != nullptr) {
    useEdgeProjections = arangodb::aql::utils::findProjections(
        traversal, pathOutVariable, arangodb::StaticStrings::GraphQueryEdges,
        /*excludeStartNodeFilterCondition*/ false, attributes);
  }

  if (useEdgeProjections) {
    // if we found any projections, make sure that they include _from
    // and _to, as the traversal code will refer to these attributes later.
    if (arangodb::ServerState::instance()->isCoordinator() &&
        !traversal->isSmart() && !traversal->isLocalGraphNode() &&
        !traversal->isUsedAsSatellite()) {
      // On cluster community variant we will also need the ID value on the
      // coordinator to uniquely identify edges
      arangodb::aql::AttributeNamePath idElement = {
          arangodb::StaticStrings::IdString,
          traversal->plan()->getAst()->query().resourceMonitor()};
      attributes.emplace(std::move(idElement));
      // Also the community variant needs to transport weight, as the
      // coordinator will do the searching.
      if (traversal->options()->mode ==
          arangodb::traverser::TraverserOptions::Order::WEIGHTED) {
        arangodb::aql::AttributeNamePath weightElement = {
            traversal->options()->weightAttribute,
            traversal->plan()->getAst()->query().resourceMonitor()};
        attributes.emplace(std::move(weightElement));
      }
    }

    arangodb::aql::AttributeNamePath fromElement = {
        arangodb::StaticStrings::FromString,
        traversal->plan()->getAst()->query().resourceMonitor()};
    attributes.emplace(std::move(fromElement));

    arangodb::aql::AttributeNamePath toElement = {
        arangodb::StaticStrings::ToString,
        traversal->plan()->getAst()->query().resourceMonitor()};
    attributes.emplace(std::move(toElement));

    if (attributes.size() <= maxProjections) {
      traversal->setEdgeProjections(
          arangodb::aql::Projections(std::move(attributes)));
      modified = true;
    }
  }

  return modified;
}

}  // namespace

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using namespace arangodb::iresearch;
using EN = arangodb::aql::ExecutionNode;

namespace arangodb {
namespace aql {

// checks if the path variable (variable) can be optimized away, or restricted
// to some attributes (vertices, edges, weights)
bool optimizeTraversalPathVariable(
    Variable const* variable, TraversalNode* traversal,
    std::vector<Variable const*> const& pruneVars) {
  if (variable == nullptr) {
    return false;
  }

  auto* options =
      static_cast<arangodb::traverser::TraverserOptions*>(traversal->options());

  if (!traversal->isVarUsedLater(variable)) {
    // traversal path outVariable not used later
    if (std::find(pruneVars.begin(), pruneVars.end(), variable) ==
        pruneVars.end()) {
      options->setProducePaths(/*vertices*/ false, /*edges*/ false,
                               /*weights*/ false);
      traversal->setPathOutput(nullptr);
      return true; /*modified*/
    }

    // we still need to build the path because PRUNE relies on it
    // TODO: this can potentially be optimized in the future.
    options->setProducePaths(/*vertices*/ true, /*edges*/ true,
                             /*weights*/ true);
    return false; /*modified*/
  }

  // path is used later, but lets check which of its sub-attributes
  // "vertices" or "edges" are in use (or the complete path)
  containers::FlatHashSet<AttributeNamePath> attributes;
  VarSet vars;

  ExecutionNode* current = traversal->getFirstParent();
  while (current != nullptr) {
    switch (current->getType()) {
      case EN::CALCULATION: {
        vars.clear();
        current->getVariablesUsedHere(vars);
        if (vars.find(variable) != vars.end()) {
          // path variable used here
          Expression* exp =
              ExecutionNode::castTo<CalculationNode*>(current)->expression();
          AstNode const* node = exp->node();
          if (!Ast::getReferencedAttributesRecursive(
                  node, variable, /*expectedAttribute*/ "", attributes,
                  current->plan()->getAst()->query().resourceMonitor())) {
            // full path variable is used, or accessed in a way that we don't
            // understand, e.g. "p" or "p[0]" or "p[*]..."
            return false;
          }
        }
        break;
      }
      default: {
        // if the path is used by any other node type, we don't know what to
        // do and will not optimize parts of it away
        vars.clear();
        current->getVariablesUsedHere(vars);
        if (vars.find(variable) != vars.end()) {
          return false;
        }
        break;
      }
    }
    current = current->getFirstParent();
  }

  // check which attributes from the path are actually used
  bool producePathsVertices = false;
  bool producePathsEdges = false;
  bool producePathsWeights = false;

  for (auto const& it : attributes) {
    TRI_ASSERT(!it.empty());
    if (!producePathsVertices &&
        it[0] == std::string_view{StaticStrings::GraphQueryVertices}) {
      producePathsVertices = true;
    } else if (!producePathsEdges &&
               it[0] == std::string_view{StaticStrings::GraphQueryEdges}) {
      producePathsEdges = true;
    } else if (!producePathsWeights &&
               options->mode == traverser::TraverserOptions::Order::WEIGHTED &&
               it[0] == std::string_view{StaticStrings::GraphQueryWeights}) {
      producePathsWeights = true;
    }
  }

  if (!producePathsVertices && !producePathsEdges && !producePathsWeights &&
      !attributes.empty()) {
    // none of the existing path attributes is actually accessed - but a
    // different (non-existing) attribute is accessed, e.g. `p.whatever`. in
    // order to not optimize away our path variable, and then being unable
    // to access the non-existing attribute, we simply activate the
    // production of vertices. this prevents us from running into errors
    // trying to access an attribute of an optimzed-away variable later
    producePathsVertices = true;
  }

  if (!producePathsVertices || !producePathsEdges || !producePathsWeights) {
    // pass the info to the traversal
    options->setProducePaths(producePathsVertices, producePathsEdges,
                             producePathsWeights);
    return true; /*modified*/
  }

  return false; /*modified*/
}

Collection* addCollectionToQuery(QueryContext& query, std::string const& cname,
                                 char const* context) {
  aql::Collection* coll = nullptr;

  if (!cname.empty()) {
    coll = query.collections().add(cname, AccessMode::Type::READ,
                                   aql::Collection::Hint::Collection);
    // simon: code below is used for FULLTEXT(), WITHIN(), NEAR(), ..
    // could become unnecessary if the AST takes care of adding the collections
    if (!ServerState::instance()->isCoordinator()) {
      TRI_ASSERT(coll != nullptr);
      query.trxForOptimization()
          .addCollectionAtRuntime(cname, AccessMode::Type::READ)
          .waitAndGet();
    }
  }

  if (coll == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        std::string("collection '") + cname + "' used in " + context +
            " not found");
  }

  return coll;
}

}  // namespace aql
}  // namespace arangodb

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

/// @brief remove INTO of a COLLECT if not used
/// additionally remove all unused aggregate calculations from a COLLECT
void arangodb::aql::removeCollectVariablesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  bool modified = false;

  for (ExecutionNode* n : nodes) {
    auto collectNode = ExecutionNode::castTo<CollectNode*>(n);
    TRI_ASSERT(collectNode != nullptr);

    auto const& varsUsedLater = collectNode->getVarsUsedLater();
    auto outVariable = collectNode->outVariable();

    if (outVariable != nullptr &&
        varsUsedLater.find(outVariable) == varsUsedLater.end()) {
      // outVariable not used later
      collectNode->clearOutVariable();
      collectNode->clearKeepVariables();
      modified = true;
    } else if (outVariable != nullptr &&
               !collectNode->hasExpressionVariable()) {
      // outVariable used later, no INTO expression, no KEEP
      // e.g. COLLECT something INTO g
      // we will now check how many parts of "g" are used later

      auto keepAttributes = containers::HashSet<std::string>();

      bool doOptimize = true;
      auto planNode = collectNode->getFirstParent();
      while (planNode != nullptr && doOptimize) {
        if (planNode->getType() == EN::CALCULATION) {
          auto cc = ExecutionNode::castTo<CalculationNode const*>(planNode);
          Expression const* exp = cc->expression();
          if (exp->node() != nullptr) {
            bool isSafeForOptimization;
            auto usedThere = ast::getReferencedAttributesForKeep(
                exp->node(), outVariable, isSafeForOptimization);
            if (isSafeForOptimization) {
              for (auto const& it : usedThere) {
                keepAttributes.emplace(it);
              }
            } else {
              doOptimize = false;
              break;
            }

          }  // end - expression exists
        } else {
          auto here = planNode->getVariableIdsUsedHere();
          if (here.find(outVariable->id) != here.end()) {
            // the outVariable of the last collect should not be used by any
            // following node directly
            doOptimize = false;
            break;
          }
          if (planNode->getType() == EN::COLLECT) {
            break;
          }
        }

        planNode = planNode->getFirstParent();

      }  // end - inspection of nodes below the found collect node - while valid
         // planNode

      if (doOptimize) {
        auto keepVariables = containers::HashSet<Variable const*>();
        // we are allowed to do the optimization
        auto current = n->getFirstDependency();
        while (current != nullptr) {
          for (auto const& var : current->getVariablesSetHere()) {
            for (auto it = keepAttributes.begin(); it != keepAttributes.end();
                 /* no hoisting */) {
              if ((*it) == var->name) {
                keepVariables.emplace(var);
                it = keepAttributes.erase(it);
              } else {
                ++it;
              }
            }
          }
          if (keepAttributes.empty()) {
            // done
            break;
          }
          current = current->getFirstDependency();
        }  // while current

        if (keepAttributes.empty() && !keepVariables.empty()) {
          collectNode->restrictKeepVariables(keepVariables);
          modified = true;
        }
      }  // end - if doOptimize
    }    // end - if collectNode has outVariable

    size_t numGroupVariables = collectNode->groupVariables().size();
    size_t numAggregateVariables = collectNode->aggregateVariables().size();

    collectNode->clearAggregates(
        [&varsUsedLater, &numGroupVariables, &numAggregateVariables,
         &modified](AggregateVarInfo const& aggregate) -> bool {
          // it is ok to remove unused aggregations if we have at least one
          // aggregate variable remaining, or if we have a group variable left.
          // it is not ok to have 0 aggregate variables and 0 group variables
          // left, because the different COLLECT executors require some
          // variables to be present.
          if (varsUsedLater.find(aggregate.outVar) == varsUsedLater.end()) {
            // result of aggregate function not used later
            if (numGroupVariables > 0 || numAggregateVariables > 1) {
              --numAggregateVariables;
              modified = true;
              return true;
            }
          }
          return false;
        });

    TRI_ASSERT(!collectNode->groupVariables().empty() ||
               !collectNode->aggregateVariables().empty());

  }  // for node in nodes
  opt->addPlan(std::move(plan), rule, modified);
}

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

  SmallUnorderedMap<ExecutionNode*, ExecutionNode*>::allocator_type::arena_type
      subqueriesArena;
  SmallUnorderedMap<ExecutionNode*, ExecutionNode*> subqueries{subqueriesArena};
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

/// @brief determine the "right" type of CollectNode and
/// add a sort node for each COLLECT (note: the sort may be removed later)
/// this rule cannot be turned off (otherwise, the query result might be wrong!)
void arangodb::aql::specializeCollectRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
                                          OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::COLLECT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto collectNode = ExecutionNode::castTo<CollectNode*>(n);

    if (collectNode->isFixedMethod()) {
      // already determined the COLLECT variant of this node.
      // it doesn't need to set again.
      continue;
    }

    auto const& groupVariables = collectNode->groupVariables();

    // test if we can use an alternative version of COLLECT with a hash table
    bool const canUseHashAggregation =
        (!groupVariables.empty() && collectNode->getOptions().canUseMethod(
                                        CollectOptions::CollectMethod::kHash));

    if (canUseHashAggregation) {
      bool preferHashCollect = collectNode->getOptions().shouldUseMethod(
          CollectOptions::CollectMethod::kHash);

      if (ExecutionNode const* loop = collectNode->getLoop();
          loop != nullptr && loop->getLoop() == nullptr &&
          (loop->getType() == EN::ENUMERATE_LIST ||
           loop->getType() == EN::TRAVERSAL ||
           loop->getType() == EN::ENUMERATE_PATHS ||
           loop->getType() == EN::SHORTEST_PATH)) {
        // if the COLLECT is contained inside a single loop, and the loop is an
        // enumeration over an array (in contrast to an enumeration over a
        // collection/view, then prefer the hashed collect variant. this is
        // because the loop output is unlikely to be sorted in any way.
        preferHashCollect = true;
      }

      if (preferHashCollect) {
        // user has explicitly asked for hash method
        // specialize existing the CollectNode so it will become a
        // HashedCollectBlock later. additionally, add a SortNode BEHIND the
        // CollectNode (to sort the final result).
        // this is an in-place modification of the plan.
        // we don't need to create an additional plan for this.
        collectNode->aggregationMethod(CollectOptions::CollectMethod::kHash);

        // add the post-SORT
        SortElementVector sortElements;
        for (auto const& v : collectNode->groupVariables()) {
          sortElements.push_back(SortElement::create(v.outVar, true));
        }

        auto sortNode = plan->createNode<SortNode>(plan.get(), plan->nextId(),
                                                   sortElements, false);

        TRI_ASSERT(collectNode->hasParent());
        auto parent = collectNode->getFirstParent();
        TRI_ASSERT(parent != nullptr);

        sortNode->addDependency(collectNode);
        parent->replaceDependency(collectNode, sortNode);

        modified = true;
        continue;
      }

      // are we allowed to generate additional plans?
      if (!opt->runOnlyRequiredRules()) {
        // create an additional plan with the adjusted COLLECT node
        std::unique_ptr<ExecutionPlan> newPlan(plan->clone());

        // use the cloned COLLECT node
        auto newCollectNode = ExecutionNode::castTo<CollectNode*>(
            newPlan->getNodeById(collectNode->id()));
        TRI_ASSERT(newCollectNode != nullptr);

        // specialize the CollectNode so it will become a HashedCollectBlock
        // later. additionally, add a SortNode BEHIND the CollectNode (to sort
        // the final result).
        newCollectNode->aggregationMethod(CollectOptions::CollectMethod::kHash);

        // add the post-SORT
        SortElementVector sortElements;
        for (auto const& v : newCollectNode->groupVariables()) {
          sortElements.push_back(SortElement::create(v.outVar, true));
        }

        auto sortNode = newPlan->createNode<SortNode>(
            newPlan.get(), newPlan->nextId(), std::move(sortElements), false);

        TRI_ASSERT(newCollectNode->hasParent());
        auto parent = newCollectNode->getFirstParent();
        TRI_ASSERT(parent != nullptr);

        sortNode->addDependency(newCollectNode);
        parent->replaceDependency(newCollectNode, sortNode);

        if (nodes.size() > 1) {
          // this will tell the optimizer to optimize the cloned plan with this
          // specific rule again
          opt->addPlanAndRerun(std::move(newPlan), rule, true);
        } else {
          // no need to run this specific rule again on the cloned plan
          opt->addPlan(std::move(newPlan), rule, true);
        }
      }
    } else if (groupVariables.empty() &&
               collectNode->hasOutVariable() == false &&
               collectNode->aggregateVariables().size() == 1 &&
               collectNode->aggregateVariables()[0].type == "LENGTH") {
      // we have no groups and only a single aggregator of type LENGTH, so we
      // can use the specialized count executor
      collectNode->aggregationMethod(CollectOptions::CollectMethod::kCount);
      modified = true;
      continue;
    }

    // finally, adjust the original plan and create a sorted version of COLLECT.
    collectNode->aggregationMethod(CollectOptions::CollectMethod::kSorted);

    // insert a SortNode IN FRONT OF the CollectNode, if we don't already have
    // one that can be used instead
    if (!groupVariables.empty()) {
      // check if our input is already sorted
      SortNode* sortNode = nullptr;
      auto* d = n->getFirstDependency();
      while (d) {
        switch (d->getType()) {
          case EN::SORT:
            sortNode = ExecutionNode::castTo<SortNode*>(d);
            d = nullptr;  // stop the loop
            break;

          case EN::FILTER:
          case EN::LIMIT:
          case EN::CALCULATION:
          case EN::SUBQUERY:
          case EN::INSERT:
          case EN::REMOVE:
          case EN::REPLACE:
          case EN::UPDATE:
          case EN::UPSERT:
            // these nodes do not affect our sort order,
            // so we can safely ignore them
            d = d->getFirstDependency();  // continue search
            break;

          default:
            d = nullptr;  // stop the loop
            break;
        }
      }

      bool needNewSortNode = true;
      if (sortNode != nullptr) {
        needNewSortNode = false;
        // we found a SORT node, now let's check if it covers all our group
        // variables
        auto& elems = sortNode->elements();
        for (auto const& v : groupVariables) {
          needNewSortNode |= std::find_if(elems.begin(), elems.end(),
                                          [&v](SortElement const& e) {
                                            return e.var == v.inVar;
                                          }) == elems.end();
        }
      }

      if (needNewSortNode) {
        SortElementVector sortElements;
        for (auto const& v : groupVariables) {
          sortElements.push_back(SortElement::create(v.inVar, true));
        }

        sortNode = plan->createNode<SortNode>(plan.get(), plan->nextId(),
                                              std::move(sortElements), true);

        TRI_ASSERT(collectNode->hasDependency());
        auto* dep = collectNode->getFirstDependency();
        TRI_ASSERT(dep != nullptr);
        sortNode->addDependency(dep);
        collectNode->replaceDependency(dep, sortNode);

        modified = true;
      }
    }
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

/// @brief simplify conditions in CalculationNodes
void arangodb::aql::simplifyConditionsRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  auto p = plan.get();
  bool changed = false;

  auto visitor = [&changed, p](AstNode* node) {
    again:
      if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        auto const* accessed = node->getMemberUnchecked(0);

        if (accessed->type == NODE_TYPE_REFERENCE) {
          Variable const* v = ast::ReferenceNode(accessed).getVariable();
          TRI_ASSERT(v != nullptr);

          auto setter = p->getVarSetBy(v->id);

          if (setter == nullptr || setter->getType() != EN::CALCULATION) {
            return node;
          }

          accessed = ExecutionNode::castTo<CalculationNode*>(setter)
                         ->expression()
                         ->node();
          if (accessed == nullptr) {
            return node;
          }
        }

        TRI_ASSERT(accessed != nullptr);

        if (accessed->type == NODE_TYPE_OBJECT) {
          std::string_view attributeName(node->getStringView());
          bool isDynamic = false;
          size_t const n = accessed->numMembers();
          for (size_t i = 0; i < n; ++i) {
            auto member = accessed->getMemberUnchecked(i);

            if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
                member->getStringView() == attributeName) {
              // found the attribute!
              ast::ObjectElementNode objElem(member);
              AstNode* next = objElem.getValue();
              if (!next->isDeterministic()) {
                // do not descend into non-deterministic nodes
                return node;
              }
              // descend further
              node = next;
              // now try optimizing the simplified condition
              // time for a goto...!
              goto again;
            } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
              // dynamic attribute name
              isDynamic = true;
            }
          }

          // attribute not found
          if (!isDynamic) {
            changed = true;
            return p->getAst()->createNodeValueNull();
          }
        }
      } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
        ast::IndexedAccessNode indexAccess(node);
        auto const* accessed = indexAccess.getObject();

        if (accessed->type == NODE_TYPE_REFERENCE) {
          Variable const* v = ast::ReferenceNode(accessed).getVariable();
          TRI_ASSERT(v != nullptr);

          auto setter = p->getVarSetBy(v->id);

          if (setter == nullptr || setter->getType() != EN::CALCULATION) {
            return node;
          }

          accessed = ExecutionNode::castTo<CalculationNode*>(setter)
                         ->expression()
                         ->node();
          if (accessed == nullptr) {
            return node;
          }
        }

        auto indexValue = indexAccess.getIndex();

        if (!indexValue->isConstant() ||
            !(indexValue->isStringValue() || indexValue->isNumericValue())) {
          // cant handle this type of index statically
          return node;
        }

        if (accessed->type == NODE_TYPE_OBJECT) {
          std::string_view attributeName;
          std::string indexString;

          if (indexValue->isStringValue()) {
            // string index, e.g. ['123']
            attributeName = indexValue->getStringView();
          } else {
            // numeric index, e.g. [123]
            TRI_ASSERT(indexValue->isNumericValue());
            // convert the numeric index into a string
            indexString = std::to_string(indexValue->getIntValue());
            attributeName = std::string_view(indexString);
          }

          bool isDynamic = false;
          size_t const n = accessed->numMembers();
          for (size_t i = 0; i < n; ++i) {
            auto member = accessed->getMemberUnchecked(i);

            if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
                member->getStringView() == attributeName) {
              // found the attribute!
              ast::ObjectElementNode objElem2(member);
              AstNode* next = objElem2.getValue();
              if (!next->isDeterministic()) {
                // do not descend into non-deterministic nodes
                return node;
              }
              // descend further
              node = next;
              // now try optimizing the simplified condition
              // time for a goto...!
              goto again;
            } else if (member->type == NODE_TYPE_CALCULATED_OBJECT_ELEMENT) {
              // dynamic attribute name
              isDynamic = true;
            }
          }

          // attribute not found
          if (!isDynamic) {
            changed = true;
            return p->getAst()->createNodeValueNull();
          }
        } else if (accessed->type == NODE_TYPE_ARRAY) {
          int64_t position;
          if (indexValue->isStringValue()) {
            // string index, e.g. ['123'] -> convert to a numeric index
            bool valid;
            position = NumberUtils::atoi<int64_t>(
                indexValue->getStringValue(),
                indexValue->getStringValue() + indexValue->getStringLength(),
                valid);
            if (!valid) {
              // invalid index
              changed = true;
              return p->getAst()->createNodeValueNull();
            }
          } else {
            // numeric index, e.g. [123]
            TRI_ASSERT(indexValue->isNumericValue());
            position = indexValue->getIntValue();
          }
          int64_t const n = accessed->numMembers();
          if (position < 0) {
            // a negative position is allowed
            position = n + position;
          }
          if (position >= 0 && position < n) {
            ast::ArrayNode arr(accessed);
            AstNode* next = arr.getElements()[static_cast<size_t>(position)];
            if (!next->isDeterministic()) {
              // do not descend into non-deterministic nodes
              return node;
            }
            // descend further
            node = next;
            // now try optimizing the simplified condition
            // time for a goto...!
            goto again;
          }

          // index out of bounds
          changed = true;
          return p->getAst()->createNodeValueNull();
        }
      }

      return node;
  };

  bool modified = false;

  for (auto const& n : nodes) {
    auto nn = ExecutionNode::castTo<CalculationNode*>(n);

    if (!nn->expression()->isDeterministic() ||
        nn->outVariable()->type() == Variable::Type::Const) {
      // If this node is non-deterministic or has a constant expression, we must
      // not touch it!
      continue;
    }

    AstNode* root = nn->expression()->nodeForModification();

    if (root != nullptr) {
      // the changed variable is captured by reference by the lambda that
      // traverses the Ast and may modify it. if it performs a modification,
      // it will set changed=true
      changed = false;
      AstNode* simplified = plan->getAst()->traverseAndModify(root, visitor);
      if (simplified != root || changed) {
        nn->expression()->replaceNode(simplified);
        nn->expression()->invalidateAfterReplacements();
        modified = true;
      }
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
          replacements.try_emplace(outVariable->id,
                                   ast::ReferenceNode(rootNode).getVariable());

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
                ast::ReferenceNode(node).getVariable() ==
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

/// @brief useIndex, try to use an index for filtering
void arangodb::aql::useIndexesRule(Optimizer* opt,
                                   std::unique_ptr<ExecutionPlan> plan,
                                   OptimizerRule const& rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findEndNodes(nodes, true);

  std::unordered_map<ExecutionNodeId, ExecutionNode*> changes;

  auto cleanupChanges = scopeGuard([&changes]() noexcept {
    for (auto& v : changes) {
      delete v.second;
    }
  });

  bool hasEmptyResult = false;
  for (auto const& n : nodes) {
    ConditionFinder finder(plan.get(), changes);
    n->walk(finder);
    if (finder.producesEmptyResult()) {
      hasEmptyResult = true;
    }
  }

  if (!changes.empty()) {
    for (auto& it : changes) {
      plan->registerNode(it.second);
      plan->replaceNode(plan->getNodeById(it.first), it.second);

      // prevent double deletion by cleanupChanges()
      it.second = nullptr;
    }
    opt->addPlan(std::move(plan), rule, true);
  } else {
    opt->addPlan(std::move(plan), rule, hasEmptyResult);
  }
}

struct SortToIndexNode final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  ExecutionPlan* _plan;
  SortNode* _sortNode;
  std::vector<std::pair<Variable const*, bool>> _sorts;
  std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
  std::vector<std::vector<RegisterId>> _filters;
  bool _modified;

 public:
  explicit SortToIndexNode(ExecutionPlan* plan)
      : _plan(plan), _sortNode(nullptr), _modified(false) {
    _filters.emplace_back();
  }

  /// @brief gets the attributes from the filter conditions that will have a
  /// constant value (e.g. doc.value == 123) or than can be proven to be != null
  void getSpecialAttributes(
      AstNode const* node, Variable const* variable,
      std::vector<std::vector<arangodb::basics::AttributeName>>&
          constAttributes,
      ::arangodb::containers::HashSet<
          std::vector<arangodb::basics::AttributeName>>& nonNullAttributes)
      const {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      // recurse into both sides
      getSpecialAttributes(node->getMemberUnchecked(0), variable,
                           constAttributes, nonNullAttributes);
      getSpecialAttributes(node->getMemberUnchecked(1), variable,
                           constAttributes, nonNullAttributes);
      return;
    }

    if (!node->isComparisonOperator()) {
      return;
    }

    TRI_ASSERT(node->isComparisonOperator());

    AstNode const* lhs = node->getMemberUnchecked(0);
    AstNode const* rhs = node->getMemberUnchecked(1);
    AstNode const* check = nullptr;

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      if (lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // const value == doc.value
        check = rhs;
      } else if (rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // doc.value == const value
        check = lhs;
      }
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_NE) {
      if (lhs->isNullValue() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // null != doc.value
        check = rhs;
      } else if (rhs->isNullValue() &&
                 lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        // doc.value != null
        check = lhs;
      }
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_LT &&
               lhs->isConstant() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // const value < doc.value
      check = rhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_LE &&
               lhs->isConstant() && !lhs->isNullValue() &&
               rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // const value <= doc.value
      check = rhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_GT &&
               rhs->isConstant() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // doc.value > const value
      check = lhs;
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_GE &&
               rhs->isConstant() && !rhs->isNullValue() &&
               lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // doc.value >= const value
      check = lhs;
    }

    if (check == nullptr) {
      // condition is useless for us
      return;
    }

    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
        result;

    if (check->isAttributeAccessForVariable(result, false) &&
        result.first == variable) {
      if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        // found a constant value
        constAttributes.emplace_back(std::move(result.second));
      } else {
        // all other cases handle non-null attributes
        nonNullAttributes.emplace(std::move(result.second));
      }
    }
  }

  void processCollectionAttributes(
      Variable const* variable,
      std::vector<std::vector<arangodb::basics::AttributeName>>&
          constAttributes,
      ::arangodb::containers::HashSet<
          std::vector<arangodb::basics::AttributeName>>& nonNullAttributes)
      const {
    // resolve all FILTER variables into their appropriate filter conditions
    TRI_ASSERT(!_filters.empty());
    for (auto const& filter : _filters.back()) {
      TRI_ASSERT(filter.isRegularRegister());
      auto it = _variableDefinitions.find(filter.value());
      if (it != _variableDefinitions.end()) {
        // AND-combine all filter conditions we found, and fill constAttributes
        // and nonNullAttributes as we go along
        getSpecialAttributes((*it).second, variable, constAttributes,
                             nonNullAttributes);
      }
    }
  }

  SortElementVector makeIndexSortElements(SortCondition const& sortCondition,
                                          Variable const* outVariable) {
    TRI_ASSERT(sortCondition.isOnlyAttributeAccess());
    SortElementVector elements;
    elements.reserve(sortCondition.numAttributes());
    for (auto const& field : sortCondition.sortFields()) {
      std::vector<std::string> path;
      path.reserve(field.attributes.size());
      std::transform(field.attributes.begin(), field.attributes.end(),
                     std::back_inserter(path),
                     [](auto const& a) { return a.name; });
      elements.push_back(
          SortElement::createWithPath(outVariable, field.asc, path));
    }
    return elements;
  }

  bool handleEnumerateCollectionNode(
      EnumerateCollectionNode* enumerateCollectionNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (enumerateCollectionNode->isInInnerLoop()) {
      // index node contained in an outer loop. must not optimize away the sort!
      return true;
    }

    // figure out all attributes from the FILTER conditions that have a constant
    // value and/or that cannot be null
    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    ::arangodb::containers::HashSet<
        std::vector<arangodb::basics::AttributeName>>
        nonNullAttributes;
    processCollectionAttributes(enumerateCollectionNode->outVariable(),
                                constAttributes, nonNullAttributes);

    SortCondition sortCondition(_plan, _sorts, constAttributes,
                                nonNullAttributes, _variableDefinitions);

    if (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess()) {
      // we have found a sort condition
      // now check if any of the collection's indexes covers it

      Variable const* outVariable = enumerateCollectionNode->outVariable();
      std::vector<transaction::Methods::IndexHandle> usedIndexes;
      size_t coveredAttributes = 0;

      Collection const* coll = enumerateCollectionNode->collection();
      TRI_ASSERT(coll != nullptr);
      size_t numDocs =
          coll->count(&_plan->getAst()->query().trxForOptimization(),
                      transaction::CountType::kTryCache);

      bool canBeUsed = arangodb::aql::utils::getIndexForSortCondition(
          *coll, &sortCondition, outVariable, numDocs,
          enumerateCollectionNode->hint(), usedIndexes, coveredAttributes);
      if (canBeUsed) {
        // If this bit is set, then usedIndexes has length exactly one
        // and contains the best index found.
        auto condition = std::make_unique<Condition>(_plan->getAst());
        condition->normalize(_plan);
        TRI_ASSERT(usedIndexes.size() == 1);
        IndexIteratorOptions opts;
        TRI_ASSERT(!sortCondition.isEmpty());
        opts.ascending = sortCondition.sortFields()[0].asc;
        opts.useCache = false;
        auto indexNode = _plan->createNode<IndexNode>(
            _plan, _plan->nextId(), enumerateCollectionNode->collection(),
            outVariable, usedIndexes,
            false,  // here we could always assume false as there is no lookup
                    // condition here
            std::move(condition), opts);

        enumerateCollectionNode->CollectionAccessingNode::cloneInto(*indexNode);
        enumerateCollectionNode->DocumentProducingNode::cloneInto(_plan,
                                                                  *indexNode);

        _plan->replaceNode(enumerateCollectionNode, indexNode);
        _modified = true;

        if (coveredAttributes == sortCondition.numAttributes()) {
          deleteSortNode(indexNode, sortCondition);
        } else if (usedIndexes.size() == 1 &&
                   usedIndexes[0]->type() ==
                       Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX) {
          _sortNode->setGroupedElements(coveredAttributes);
        }
      }
    }

    return true;  // always abort further searching here
  }

  void deleteSortNode(IndexNode* indexNode, SortCondition& sortCondition) {
    // sort condition is fully covered by index... now we can remove the
    auto sortNode = _plan->getNodeById(_sortNode->id());
    _plan->unlinkNode(sortNode);
    // we need to have a sorted result later on, so we will need a sorted
    // GatherNode in the cluster
    indexNode->needsGatherNodeSort(
        makeIndexSortElements(sortCondition, indexNode->outVariable()));
    _modified = true;
  }

  bool handleIndexNode(IndexNode* indexNode) {
    if (_sortNode == nullptr) {
      return true;
    }

    if (indexNode->isInInnerLoop()) {
      // index node contained in an outer loop. must not optimize away the sort!
      return true;
    }

    auto const& indexes = indexNode->getIndexes();
    auto cond = indexNode->condition();
    TRI_ASSERT(cond != nullptr);

    Variable const* outVariable = indexNode->outVariable();
    TRI_ASSERT(outVariable != nullptr);

    auto index = indexes[0];
    bool isSorted = index->isSorted();
    bool isSparse = index->sparse();
    std::vector<std::vector<arangodb::basics::AttributeName>> fields =
        index->fields();

    if (indexes.size() != 1) {
      // can only use this index node if it uses exactly one index or multiple
      // indexes on exactly the same attributes

      if (!cond->isSorted()) {
        // index conditions do not guarantee sortedness
        return true;
      }

      if (isSparse) {
        return true;
      }

      for (auto& idx : indexes) {
        if (idx != index) {
          // Can only be sorted iff only one index is used.
          return true;
        }
      }

      // all indexes use the same attributes and index conditions guarantee
      // sorted output
    }

    TRI_ASSERT(indexes.size() == 1 || cond->isSorted());

    // if we get here, we either have one index or multiple indexes on the same
    // attributes

    if (indexes.size() == 1 && isSorted) {
      // if we have just a single index and we can use it for the filtering
      // condition, then we can use the index for sorting, too. regardless of if
      // the index is sparse or not. because the index would only return
      // non-null attributes anyway, so we do not need to care about null values
      // when sorting here
      isSparse = false;
    }

    SortCondition sortCondition(
        _plan, _sorts, cond->getConstAttributes(outVariable, !isSparse),
        cond->getNonNullAttributes(outVariable), _variableDefinitions);

    bool const isOnlyAttributeAccess =
        (!sortCondition.isEmpty() && sortCondition.isOnlyAttributeAccess());

    // FIXME: why not just call index->supportsSortCondition here always?
    bool indexFullyCoversSortCondition = false;
    if (index->type() == Index::IndexType::TRI_IDX_TYPE_INVERTED_INDEX) {
      indexFullyCoversSortCondition =
          index->supportsSortCondition(&sortCondition, outVariable, 1)
              .supportsCondition;
    } else {
      indexFullyCoversSortCondition =
          isOnlyAttributeAccess && isSorted && !isSparse &&
          sortCondition.isUnidirectional() &&
          sortCondition.isAscending() == indexNode->options().ascending &&
          sortCondition.coveredAttributes(outVariable, fields) >=
              sortCondition.numAttributes();
    }

    if (indexFullyCoversSortCondition) {
      deleteSortNode(indexNode, sortCondition);
    } else if (index->type() ==
               Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX) {
      auto [numberOfCoveredAttributes, sortIsAscending] =
          sortCondition.coveredUnidirectionalAttributesWithDirection(
              outVariable, fields);
      if (isOnlyAttributeAccess && isSorted && !isSparse &&
          numberOfCoveredAttributes > 0) {
        indexNode->setAscending(sortIsAscending);
        _sortNode->setGroupedElements(numberOfCoveredAttributes);
        _modified = true;
      }
    } else {
      if (isOnlyAttributeAccess && indexes.size() == 1) {
        // special case... the index cannot be used for sorting, but we only
        // compare with equality lookups.
        // now check if the equality lookup attributes are the same as
        // the index attributes
        auto root = cond->root();

        if (root != nullptr) {
          auto condNode = root->getMember(0);

          if (condNode->isOnlyEqualityMatch()) {
            // now check if the index fields are the same as the sort condition
            // fields e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1,
            // c.value2
            size_t const numCovered =
                sortCondition.coveredAttributes(outVariable, fields);

            if (numCovered == sortCondition.numAttributes() &&
                sortCondition.isUnidirectional() &&
                (isSorted || fields.size() >= sortCondition.numAttributes())) {
              deleteSortNode(indexNode, sortCondition);
              indexNode->setAscending(sortCondition.isAscending());
            }
          }
        }
      }
    }

    return true;  // always abort after we found an IndexNode
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::ENUMERATE_PATHS:
      case EN::SHORTEST_PATH:
      case EN::ENUMERATE_LIST:
      case EN::ENUMERATE_IRESEARCH_VIEW:
        // found some other FOR loop
        return true;

      case EN::SUBQUERY: {
        _filters.emplace_back();
        return false;  // skip. we don't care.
      }

      case EN::FILTER: {
        auto inVariable =
            ExecutionNode::castTo<FilterNode const*>(en)->inVariable()->id;
        _filters.back().emplace_back(inVariable);
        return false;
      }

      case EN::CALCULATION: {
        _variableDefinitions.try_emplace(
            ExecutionNode::castTo<CalculationNode const*>(en)
                ->outVariable()
                ->id,
            ExecutionNode::castTo<CalculationNode const*>(en)
                ->expression()
                ->node());
        return false;
      }

      case EN::SINGLETON:
      case EN::COLLECT:
      case EN::WINDOW:
      case EN::INSERT:
      case EN::REMOVE:
      case EN::REPLACE:
      case EN::UPDATE:
      case EN::UPSERT:
      case EN::RETURN:
      case EN::NORESULTS:
      case EN::SCATTER:
      case EN::DISTRIBUTE:
      case EN::GATHER:
      case EN::REMOTE:
      case EN::MATERIALIZE:
      case EN::LIMIT:  // LIMIT is criterion to stop
      case EN::ENUMERATE_NEAR_VECTORS:
        return true;  // abort.

      case EN::SORT:  // pulling two sorts together is done elsewhere.
        if (!_sorts.empty() || _sortNode != nullptr) {
          return true;  // a different SORT node. abort
        }
        _sortNode = ExecutionNode::castTo<SortNode*>(en);
        for (auto& it : _sortNode->elements()) {
          TRI_ASSERT(it.attributePath.empty());
          _sorts.emplace_back(it.var, it.ascending);
        }
        return false;

      case EN::INDEX:
        return handleIndexNode(ExecutionNode::castTo<IndexNode*>(en));
      case EN::ENUMERATE_COLLECTION:
        return handleEnumerateCollectionNode(
            ExecutionNode::castTo<EnumerateCollectionNode*>(en));

      default: {
        // should not reach this point
        TRI_ASSERT(false);
      }
    }
    return true;
  }

  void after(ExecutionNode* en) override final {
    if (en->getType() == EN::SUBQUERY) {
      TRI_ASSERT(!_filters.empty());
      _filters.pop_back();
    }
  }
};

void arangodb::aql::useIndexForSortRule(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::SORT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto sortNode = ExecutionNode::castTo<SortNode*>(n);

    SortToIndexNode finder(plan.get());
    sortNode->walk(finder);

    if (finder._modified) {
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief try to remove filters which are covered by indexes
void arangodb::aql::removeFiltersCoveredByIndexRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;
  bool modified = false;
  // this rule may modify the plan in place, but the new plan
  // may not yet be optimal. so we may pass it into this same
  // rule again. the default is to continue with the next rule
  // however
  bool rerun = false;

  for (auto const& node : nodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    // find the node with the filter expression
    auto setter = plan->getVarSetBy(fn->inVariable()->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::INDEX) {
        auto indexNode = ExecutionNode::castTo<IndexNode*>(current);

        // found an index node, now check if the expression is covered by the
        // index
        auto indexCondition = indexNode->condition();

        if (indexCondition != nullptr && !indexCondition->isEmpty()) {
          auto const& indexesUsed = indexNode->getIndexes();

          if (indexesUsed.size() == 1) {
            // single index. this is something that we can handle
            AstNode* newNode{nullptr};
            if (!indexNode->isAllCoveredByOneIndex()) {
              if (n != 1) {
                // either no condition or multiple ORed conditions
                // and index has not covered entire condition.
                break;
              }
              newNode = condition.removeIndexCondition(
                  plan.get(), indexNode->outVariable(), indexCondition->root(),
                  indexesUsed[0].get());
            }
            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(node);
              // note: we must leave the calculation node intact, in case it is
              // still used by other nodes in the plan
              modified = true;
              handled = true;
            } else if (newNode != condition.root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan->getAst(), newNode);
              CalculationNode* cn = plan->createNode<CalculationNode>(
                  plan.get(), plan->nextId(), std::move(expr),
                  calculationNode->outVariable());
              plan->replaceNode(setter, cn);
              modified = true;
              handled = true;
              // pass the new plan into this rule again, to optimize even
              // further
              rerun = true;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT) {
        break;
      }

      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  if (rerun) {
    TRI_ASSERT(modified);
    opt->addPlanAndRerun(std::move(plan), rule, modified);
  } else {
    opt->addPlan(std::move(plan), rule, modified);
  }
}

/// @brief helper to compute lots of permutation tuples
/// a permutation tuple is represented as a single vector together with
/// another vector describing the boundaries of the tuples.
/// Example:
/// data:   0,1,2, 3,4, 5,6
/// starts: 0,     3,   5,      (indices of starts of sections)
/// means a tuple of 3 permutations of 3, 2 and 2 points respectively
/// This function computes the next permutation tuple among the
/// lexicographically sorted list of all such tuples. It returns true
/// if it has successfully computed this and false if the tuple is already
/// the lexicographically largest one. If false is returned, the permutation
/// tuple is back to the beginning.
static bool NextPermutationTuple(std::vector<size_t>& data,
                                 std::vector<size_t>& starts) {
  auto begin = data.begin();  // a random access iterator

  for (size_t i = starts.size(); i-- != 0;) {
    std::vector<size_t>::iterator from = begin + starts[i];
    std::vector<size_t>::iterator to;
    if (i == starts.size() - 1) {
      to = data.end();
    } else {
      to = begin + starts[i + 1];
    }
    if (std::next_permutation(from, to)) {
      return true;
    }
  }

  return false;
}

/// @brief interchange adjacent EnumerateCollectionNodes in all possible ways
void arangodb::aql::interchangeAdjacentEnumerationsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;

  // note: we are looking here only for ENUMERATE_COLLECTION
  // and ENUMERATE_LIST node. this optimizer rule runs very
  // early, so nodes of type INDEX or JOIN are not yet present
  // in the plan, at least not the expected ones that come from
  // substituing a full collection scan with an index etc.
  // we may find indexes in the plan when this rule runs, but
  // only some geo/fulltext indexes which are inserted into the
  // plan by an optimizer rule that replaces old AQL functions
  // FULLTEXT/WITHIN with actual FOR loop-index lookups
  plan->findNodesOfType(nodes, ::interchangeAdjacentEnumerationsNodeTypes,
                        true);

  ::arangodb::containers::HashSet<ExecutionNode*> nodesSet;
  for (auto const& n : nodes) {
    TRI_ASSERT(!nodesSet.contains(n));
    nodesSet.emplace(n);
  }

  std::vector<ExecutionNode*> nodesToPermute;
  std::vector<size_t> permTuple;
  std::vector<size_t> starts;
  std::vector<ExecutionNode*> nn;

  containers::FlatHashMap<VariableId, CalculationNode const*> calculations;
  VarSet inputVars;
  VarSet filterVars;

  // We use that the order of the nodes is such that a node B that is among the
  // recursive dependencies of a node A is later in the vector.
  for (auto const& n : nodes) {
    if (!nodesSet.contains(n)) {
      continue;
    }
    nn.clear();
    nn.emplace_back(n);
    nodesSet.erase(n);

    inputVars.clear();

    // Now follow the dependencies as long as we see further such nodes:
    auto nwalker = n;

    while (true) {
      if (!nwalker->hasDependency()) {
        break;
      }

      auto dep = nwalker->getFirstDependency();

      if (dep->getType() != EN::ENUMERATE_COLLECTION &&
          dep->getType() != EN::ENUMERATE_LIST) {
        break;
      }

      if (n->getType() == EN::ENUMERATE_LIST &&
          dep->getType() == EN::ENUMERATE_LIST) {
        break;
      }

      bool foundDependency = false;

      if (dep->getType() == EN::ENUMERATE_LIST &&
          nwalker->getType() == EN::ENUMERATE_COLLECTION) {
        // now checking for the following case:
        //   FOR a IN ... (EnumerateList) (dep)
        //     FOR b IN collection (EnumerateCollection) (nwalker)
        //       LET #1 = b.something == a.whatever
        //       FILTER #1
        // in this case the two FOR loops don't depend on each other,
        // but can be executed as either `a -> b` or `b -> a`.
        // we can simply decide for one order here, so we can save
        // extra permutations
        calculations.clear();

        ExecutionNode* s = nwalker->getFirstParent();
        while (s != nullptr && !foundDependency) {
          if (s->getType() == EN::CALCULATION) {
            auto cn = ExecutionNode::castTo<CalculationNode const*>(s);
            calculations.emplace(cn->outVariable()->id, cn);
          } else if (s->getType() == EN::FILTER) {
            auto fn = ExecutionNode::castTo<FilterNode const*>(s);
            Variable const* inVariable = fn->inVariable();
            if (auto it = calculations.find(inVariable->id);
                it != calculations.end()) {
              filterVars.clear();
              Ast::getReferencedVariables(it->second->expression()->node(),
                                          filterVars);

              for (auto const& outVar : dep->getVariablesSetHere()) {
                if (filterVars.contains(outVar)) {
                  // this means we will not consider this permutation and
                  // save generating an extra plan, thus speeding up the
                  // optimization phase
                  foundDependency = true;
                  break;
                }
              }
            } else {
              // we did not pick up the CalculationNode for the
              // FilterNode we found. this is not necessarily a
              // problem, but we can as well give up now.
              // in the worst case, we create an extra permutation
              // that could have been avoided under some circumstances.
              break;
            }
          } else {
            // found a node type that we don't handle. we currently
            // only support CalculationNodes and FilterNodes
            break;
          }
          s = s->getFirstParent();
        }

        if (foundDependency) {
          break;
        }
      }

      if (!foundDependency) {
        // track variables that we rely on
        nwalker->getVariablesUsedHere(inputVars);

        // check if nodes depend on each other (i.e. node C consumes a variable
        // introduced by node B or A):
        // - FOR a IN A
        // -   FOR b IN a.values
        // -     FOR c IN b.values
        //   or
        // - FOR a IN A
        // -   FOR b IN ...
        // -     FOR c IN a.values
        for (auto const& outVar : dep->getVariablesSetHere()) {
          if (inputVars.contains(outVar)) {
            foundDependency = true;
            break;
          }
        }
      }

      if (foundDependency) {
        break;
      }

      nwalker = dep;
      nn.emplace_back(nwalker);
      nodesSet.erase(nwalker);
    }

    if (nn.size() > 1) {
      // Move it into the permutation tuple:
      starts.emplace_back(permTuple.size());

      for (auto const& nnn : nn) {
        nodesToPermute.emplace_back(nnn);
        permTuple.emplace_back(permTuple.size());
      }
    }
  }

  // Now we have collected all the runs of EnumerateCollectionNodes in the
  // plan, we need to compute all possible permutations of all of them,
  // independently. This is why we need to compute all permutation tuples.

  if (!starts.empty()) {
    NextPermutationTuple(permTuple, starts);  // will never return false

    do {
      // check if we already have enough plans (plus the one plan that we will
      // add at the end of this function)
      if (opt->runOnlyRequiredRules()) {
        // have enough plans. stop permutations
        break;
      }

      // Clone the plan:
      std::unique_ptr<ExecutionPlan> newPlan(plan->clone());

      // Find the nodes in the new plan corresponding to the ones in the
      // old plan that we want to permute:
      std::vector<ExecutionNode*> newNodes;
      newNodes.reserve(nodesToPermute.size());
      for (size_t j = 0; j < nodesToPermute.size(); j++) {
        newNodes.emplace_back(newPlan->getNodeById(nodesToPermute[j]->id()));
      }

      // Now get going with the permutations:
      for (size_t i = 0; i < starts.size(); i++) {
        size_t lowBound = starts[i];
        size_t highBound =
            (i < starts.size() - 1) ? starts[i + 1] : permTuple.size();
        // We need to remove the nodes
        // newNodes[lowBound..highBound-1] in newPlan and replace
        // them by the same ones in a different order, given by
        // permTuple[lowBound..highBound-1].
        auto parent = newNodes[lowBound]->getFirstParent();

        TRI_ASSERT(parent != nullptr);

        // Unlink all those nodes:
        for (size_t j = lowBound; j < highBound; j++) {
          newPlan->unlinkNode(newNodes[j]);
        }

        // And insert them in the new order:
        for (size_t j = highBound; j-- != lowBound;) {
          newPlan->insertDependency(parent, newNodes[permTuple[j]]);
        }
      }

      // OK, the new plan is ready, let's report it:
      opt->addPlan(std::move(newPlan), rule, true);
    } while (NextPermutationTuple(permTuple, starts));
  }

  opt->addPlan(std::move(plan), rule, false);
}

auto extractVocbaseFromNode(ExecutionNode* at) -> TRI_vocbase_t* {
  auto collectionAccessingNode =
      dynamic_cast<CollectionAccessingNode const*>(at);

  if (collectionAccessingNode != nullptr) {
    return collectionAccessingNode->vocbase();
  } else if (at->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW) {
    // Really? Yes, the & below is correct.
    return &ExecutionNode::castTo<IResearchViewNode const*>(at)->vocbase();
  }

  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "Cannot determine vocbase for execution node.");
}

// Sets up a Gather node for scatterInClusterRule.
//
// Each of EnumerateCollectionNode, IndexNode, IResearchViewNode, and
// ModificationNode needs slightly different treatment.
//
// In an ideal world the node itself would know how to compute these parameters
// for GatherNode (sortMode, parallelism, and elements), and we'd just ask it.
//
// firstDependency servers to indicate what was the first dependency to the
// execution node before it got unlinked and linked with remote nodes
auto insertGatherNode(
    ExecutionPlan& plan, ExecutionNode* node,
    SmallUnorderedMap<ExecutionNode*, ExecutionNode*> const& subqueries)
    -> GatherNode* {
  TRI_ASSERT(node);

  GatherNode* gatherNode{nullptr};

  auto nodeType = node->getType();
  switch (nodeType) {
    case ExecutionNode::ENUMERATE_COLLECTION: {
      auto collection =
          ExecutionNode::castTo<EnumerateCollectionNode const*>(node)
              ->collection();
      auto numberOfShards = collection->numberOfShards();

      auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
      auto parallelism = GatherNode::evaluateParallelism(*collection);

      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                               parallelism);
    } break;
    case ExecutionNode::INDEX: {
      auto idxNode = ExecutionNode::castTo<IndexNode const*>(node);
      auto collection = idxNode->collection();
      TRI_ASSERT(collection != nullptr);
      auto numberOfShards = collection->numberOfShards();

      auto elements = [&] {
        auto allIndexes = idxNode->getIndexes();
        TRI_ASSERT(!allIndexes.empty());

        // Using Index for sort only works if all indexes are equal.
        auto const& first = allIndexes[0];

        // also check if we actually need to bother about the sortedness of the
        // result, or if we use the index for filtering only
        if (first->isSorted() && idxNode->needsGatherNodeSort()) {
          for (auto const& it : allIndexes) {
            if (first != it) {
              return SortElementVector{};
            }
          }
        }

        return idxNode->getSortElements();
      }();

      auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
      auto parallelism = GatherNode::evaluateParallelism(*collection);

      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                               parallelism);

      if (!elements.empty() && numberOfShards != 1) {
        gatherNode->elements(elements);
      }
      return gatherNode;
    }
    case ExecutionNode::MATERIALIZE: {
      auto const* materializeNode =
          ExecutionNode::castTo<materialize::MaterializeNode const*>(node);
      auto const* maybeEnumerateNearVectorNode =
          plan.getVarSetBy(materializeNode->outVariable().id);

      if (maybeEnumerateNearVectorNode != nullptr &&
          maybeEnumerateNearVectorNode->getType() ==
              ExecutionNode::ENUMERATE_NEAR_VECTORS) {
        auto const* enumerateNearVectorNode =
            ExecutionNode::castTo<EnumerateNearVectorNode const*>(
                maybeEnumerateNearVectorNode);
        auto elements = SortElementVector{};
        auto const* collection = enumerateNearVectorNode->collection();
        TRI_ASSERT(collection != nullptr);
        auto numberOfShards = collection->numberOfShards();

        Variable const* sortVariable =
            enumerateNearVectorNode->distanceOutVariable();
        elements.push_back(SortElement::createWithPath(
            sortVariable, enumerateNearVectorNode->isAscending(), {}));

        auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
        auto parallelism = GatherNode::evaluateParallelism(*collection);

        gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                                 parallelism);

        if (!elements.empty() && numberOfShards != 1) {
          gatherNode->elements(elements);
        }
        return gatherNode;
      }
      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(),
                                               GatherNode::SortMode::Default);
      break;
    }
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::UPSERT: {
      auto collection =
          ExecutionNode::castTo<ModificationNode*>(node)->collection();

      if (nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::UPDATE) {
        // Note that in the REPLACE or UPSERT case we are not getting here,
        // since the distributeInClusterRule fires and a DistributionNode is
        // used.
        auto* modNode = ExecutionNode::castTo<ModificationNode*>(node);
        modNode->getOptions().ignoreDocumentNotFound = true;
      }

      auto numberOfShards = collection->numberOfShards();
      auto sortMode = GatherNode::evaluateSortMode(numberOfShards);
      auto parallelism = GatherNode::evaluateParallelism(*collection);

      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(), sortMode,
                                               parallelism);
      break;
    }
    default: {
      gatherNode = plan.createNode<GatherNode>(&plan, plan.nextId(),
                                               GatherNode::SortMode::Default);

      break;
    }
  }

  auto it = subqueries.find(node);
  if (it != subqueries.end()) {
    ExecutionNode::castTo<SubqueryNode*>((*it).second)
        ->setSubquery(gatherNode, true);
  }

  return gatherNode;
}

// replace
//
// A -> at -> B
//
// by
//
// A -> SCATTER -> REMOTE -> at -> REMOTE -> GATHER -> B
//
// in plan
//
// gatherNode is a parameter because it needs to be configured depending
// on they type of `at`, in particular at the moment this configuration
// uses a list of subqueries which are precomputed at the beginning
// of the optimizer rule; once that list is gone the configuration of the
// gather node can be moved into this function.
void arangodb::aql::insertScatterGatherSnippet(
    ExecutionPlan& plan, ExecutionNode* at,
    SmallUnorderedMap<ExecutionNode*, ExecutionNode*> const& subqueries) {
  // TODO: necessary?
  TRI_vocbase_t* vocbase = extractVocbaseFromNode(at);
  auto const isRootNode = plan.isRoot(at);
  auto const nodeDependencies = at->getDependencies();
  auto const nodeParents = at->getParents();

  // Unlink node from plan, note that we allow removing the root node
  plan.unlinkNode(at, true);

  auto* scatterNode = plan.createNode<ScatterNode>(
      &plan, plan.nextId(), ScatterNode::ScatterType::SHARD);

  TRI_ASSERT(at->getDependencies().empty());
  TRI_ASSERT(!nodeDependencies.empty());
  scatterNode->addDependency(nodeDependencies[0]);

  // insert REMOTE
  ExecutionNode* remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  remoteNode->addDependency(scatterNode);

  // Wire in `at`
  at->addDependency(remoteNode);

  // insert (another) REMOTE
  remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  TRI_ASSERT(at);
  remoteNode->addDependency(at);

  // GATHER needs some setup, so this happens in a separate function
  auto gatherNode = insertGatherNode(plan, at, subqueries);
  TRI_ASSERT(gatherNode);
  TRI_ASSERT(remoteNode);
  gatherNode->addDependency(remoteNode);

  // Link the gather node with the rest of the plan (if we have any)
  // TODO: what other cases can occur here?
  if (nodeParents.size() == 1) {
    nodeParents[0]->replaceDependency(nodeDependencies[0], gatherNode);
  }

  if (isRootNode) {
    // if we replaced the root node, set a new root node
    plan.root(gatherNode);
  }
}

// Moves a SCATTER/REMOTE from below `at` (where it was previously inserted by
// scatterInClusterRule), to just above `at`, because `at` was marked as
// excludeFromScatter by the smartJoinRule.
void moveScatterAbove(ExecutionPlan& plan, ExecutionNode* at) {
  TRI_vocbase_t* vocbase = extractVocbaseFromNode(at);

  ExecutionNode* remoteNode =
      plan.createNode<RemoteNode>(&plan, plan.nextId(), vocbase, "", "", "");
  plan.insertBefore(at, remoteNode);

  ExecutionNode* scatterNode = plan.createNode<ScatterNode>(
      &plan, plan.nextId(), ScatterNode::ScatterType::SHARD);
  plan.insertBefore(remoteNode, scatterNode);

  // There must be a SCATTER/REMOTE block south of us, which was inserted by
  // an earlier iteration in scatterInClusterRule.
  // We remove that block, effectively moving the SCATTER/REMOTE past the
  // current node
  // The effect is that in a SmartJoin we get joined up nodes that are
  // all executed on the DB-Server
  auto found = false;
  auto* current = at->getFirstParent();
  while (current != nullptr) {
    if (current->getType() == ExecutionNode::SCATTER) {
      auto* next = current->getFirstParent();
      if (next != nullptr && next->getType() == ExecutionNode::REMOTE) {
        plan.unlinkNode(current, true);
        plan.unlinkNode(next, true);
        found = true;
        break;
      } else {
        // If we have a SCATTER node, we also have to have a REMOTE node
        // otherwise the plan is inconsistent.
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Inconsistent plan.");
      }
    }
    current = current->getFirstParent();
  }
  if (!found) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    plan.show();
#endif
    // TODO: maybe we should *not* throw in maintainer mode, as the optimizer
    //       gives more useful error messages?
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Inconsistent plan.");
  }
}

// TODO: move into ExecutionPlan?
// TODO: Is this still needed after register planning is refactored?
// Find all Subquery Nodes
void arangodb::aql::findSubqueriesInPlan(
    ExecutionPlan& plan,
    SmallUnorderedMap<ExecutionNode*, ExecutionNode*>& subqueries) {
  containers::SmallVector<ExecutionNode*, 8> subs;
  plan.findNodesOfType(subs, ExecutionNode::SUBQUERY, true);

  for (auto& it : subs) {
    subqueries.emplace(
        ExecutionNode::castTo<SubqueryNode const*>(it)->getSubquery(), it);
  }
}

/// @brief scatter operations in cluster
/// this rule inserts scatter, gather and remote nodes so operations on
/// sharded collections actually work
/// it will change plans in place
void arangodb::aql::scatterInClusterRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  bool wasModified = false;

  // We cache the subquery map to not compute it over and over again
  // It is needed to setup the gather node later on
  SmallUnorderedMap<ExecutionNode*, ExecutionNode*>::allocator_type::arena_type
      subqueriesArena;
  SmallUnorderedMap<ExecutionNode*, ExecutionNode*> subqueries{subqueriesArena};
  findSubqueriesInPlan(*plan, subqueries);

  // we are a coordinator. now look in the plan for nodes of type
  // EnumerateCollectionNode, IndexNode, IResearchViewNode, and modification
  // nodes
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ::scatterInClusterNodeTypes, true);

  TRI_ASSERT(plan->getAst());

  for (auto& node : nodes) {
    // found a node we need to replace in the plan

    auto const deps = node->getDependencies();
    TRI_ASSERT(deps.size() == 1);

    // don't do this if we are already distributing!
    if (deps[0]->getType() == ExecutionNode::REMOTE &&
        deps[0]->getFirstDependency()->getType() == ExecutionNode::DISTRIBUTE) {
      continue;
    }

    // TODO: sonderlocke for ENUMERATE_IRESEARCH_VIEW to skip views that are
    // empty Can this be done better?
    if (node->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW) {
      auto& viewNode = *ExecutionNode::castTo<IResearchViewNode*>(node);
      auto& options = viewNode.options();

      if (viewNode.empty() ||
          (options.restrictSources && options.sources.empty())) {
        // nothing to scatter, view has no associated collections
        // or node is restricted to empty collection list
        continue;
      }
    }

    if (plan->shouldExcludeFromScatterGather(node)) {
      // If the smart-joins rule marked this node as not requiring a full
      // scatter..gather setup, we move the scatter/remote from below above
      moveScatterAbove(*plan, node);
    } else {
      // insert a full SCATTER/GATHER
      insertScatterGatherSnippet(*plan, node, subqueries);
    }
    wasModified = true;
  }

  opt->addPlan(std::move(plan), rule, wasModified);
}

/// @brief auxilliary struct for finding common nodes in OR conditions
struct CommonNodeFinder {
  std::vector<AstNode const*> possibleNodes;

  bool find(AstNode const* node, AstNodeType condition,
            AstNode const*& commonNode, std::string& commonName) {
    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      ast::LogicalOperatorNode orOp(node);
      return (find(orOp.getLeft(), condition, commonNode, commonName) &&
              find(orOp.getRight(), condition, commonNode, commonName));
    }

    if (node->type == NODE_TYPE_VALUE) {
      possibleNodes.clear();
      return true;
    }

    if (node->type == condition ||
        (condition != NODE_TYPE_OPERATOR_BINARY_EQ &&
         (node->type == NODE_TYPE_OPERATOR_BINARY_LE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_LT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GE ||
          node->type == NODE_TYPE_OPERATOR_BINARY_GT ||
          node->type == NODE_TYPE_OPERATOR_BINARY_IN))) {
      ast::BinaryOperatorNode binOp(node);
      auto lhs = binOp.getLeft();
      auto rhs = binOp.getRight();

      bool const isIn =
          (node->type == NODE_TYPE_OPERATOR_BINARY_IN && rhs->isArray());

      if (node->type == NODE_TYPE_OPERATOR_BINARY_IN &&
          rhs->type == NODE_TYPE_EXPANSION) {
        // ooh, cannot optimize this (yet)
        possibleNodes.clear();
        return false;
      }

      if (!isIn && lhs->isConstant()) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->isConstant()) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (rhs->type == NODE_TYPE_FCALL || rhs->type == NODE_TYPE_FCALL_USER ||
          rhs->type == NODE_TYPE_REFERENCE) {
        commonNode = lhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn &&
          (lhs->type == NODE_TYPE_FCALL || lhs->type == NODE_TYPE_FCALL_USER ||
           lhs->type == NODE_TYPE_REFERENCE)) {
        commonNode = rhs;
        commonName = commonNode->toString();
        possibleNodes.clear();
        return true;
      }

      if (!isIn && (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
                    lhs->type == NODE_TYPE_INDEXED_ACCESS)) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (lhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          // don't return, must consider the other side of the condition
        } else {
          possibleNodes.emplace_back(lhs);
        }
      }
      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          rhs->type == NODE_TYPE_INDEXED_ACCESS) {
        if (possibleNodes.size() == 2) {
          for (size_t i = 0; i < 2; i++) {
            if (rhs->toString() == possibleNodes[i]->toString()) {
              commonNode = possibleNodes[i];
              commonName = commonNode->toString();
              possibleNodes.clear();
              return true;
            }
          }
          return false;
        } else {
          possibleNodes.emplace_back(rhs);
          return true;
        }
      }
    }
    possibleNodes.clear();
    return false;
  }
};

/// @brief auxilliary struct for the OR-to-IN conversion
struct OrSimplifier {
  Ast* ast;
  ExecutionPlan* plan;

  OrSimplifier(Ast* ast, ExecutionPlan* plan) : ast(ast), plan(plan) {}

  std::string stringifyNode(AstNode const* node) const {
    try {
      return node->toString();
    } catch (...) {
    }
    return std::string();
  }

  bool qualifies(AstNode const* node, std::string& attributeName) const {
    if (node->isConstant()) {
      return false;
    }

    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
        node->type == NODE_TYPE_INDEXED_ACCESS ||
        node->type == NODE_TYPE_REFERENCE) {
      attributeName = stringifyNode(node);
      return true;
    }

    return false;
  }

  bool detect(AstNode const* node, bool preferRight, std::string& attributeName,
              AstNode const*& attr, AstNode const*& value) const {
    attributeName.clear();

    if (node->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      ast::RelationalOperatorNode eqOp(node);
      auto lhs = eqOp.getLeft();
      auto rhs = eqOp.getRight();
      if (!preferRight && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }

      if (qualifies(rhs, attributeName)) {
        if (lhs->isDeterministic()) {
          attr = rhs;
          value = lhs;
          return true;
        }
      }
      // intentionally falls through
    } else if (node->type == NODE_TYPE_OPERATOR_BINARY_IN) {
      ast::RelationalOperatorNode inOp(node);
      auto lhs = inOp.getLeft();
      auto rhs = inOp.getRight();
      if (rhs->isArray() && qualifies(lhs, attributeName)) {
        if (rhs->isDeterministic()) {
          attr = lhs;
          value = rhs;
          return true;
        }
      }
      // intentionally falls through
    }

    return false;
  }

  AstNode* buildValues(AstNode const* attr, AstNode const* lhs,
                       bool leftIsArray, AstNode const* rhs,
                       bool rightIsArray) const {
    auto values = ast->createNodeArray();
    if (leftIsArray) {
      size_t const n = lhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(lhs->getMemberUnchecked(i));
      }
    } else {
      values->addMember(lhs);
    }

    if (rightIsArray) {
      size_t const n = rhs->numMembers();
      for (size_t i = 0; i < n; ++i) {
        values->addMember(rhs->getMemberUnchecked(i));
      }
    } else {
      values->addMember(rhs);
    }

    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, attr,
                                         values);
  }

  AstNode* simplify(AstNode const* node) const {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
      ast::LogicalOperatorNode orOp(node);
      auto lhs = orOp.getLeft();
      auto rhs = orOp.getRight();

      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);

      if (lhs != lhsNew || rhs != rhsNew) {
        // create a modified node
        node = ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      if ((lhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
           lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN) &&
          (rhsNew->type == NODE_TYPE_OPERATOR_BINARY_EQ ||
           rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN)) {
        std::string leftName;
        std::string rightName;
        AstNode const* leftAttr = nullptr;
        AstNode const* rightAttr = nullptr;
        AstNode const* leftValue = nullptr;
        AstNode const* rightValue = nullptr;

        for (size_t i = 0; i < 4; ++i) {
          if (detect(lhsNew, i >= 2, leftName, leftAttr, leftValue) &&
              detect(rhsNew, i % 2 == 0, rightName, rightAttr, rightValue) &&
              leftName == rightName) {
            std::pair<Variable const*,
                      std::vector<arangodb::basics::AttributeName>>
                tmp1;

            if (leftValue->isAttributeAccessForVariable(tmp1)) {
              bool qualifies = false;
              auto setter = plan->getVarSetBy(tmp1.first->id);
              if (setter != nullptr &&
                  setter->getType() == EN::ENUMERATE_COLLECTION) {
                qualifies = true;
              }

              std::pair<Variable const*,
                        std::vector<arangodb::basics::AttributeName>>
                  tmp2;

              if (qualifies && rightValue->isAttributeAccessForVariable(tmp2)) {
                auto setter = plan->getVarSetBy(tmp2.first->id);
                if (setter != nullptr &&
                    setter->getType() == EN::ENUMERATE_COLLECTION) {
                  if (tmp1.first != tmp2.first || tmp1.second != tmp2.second) {
                    continue;
                  }
                }
              }
            }

            return buildValues(leftAttr, leftValue,
                               lhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN,
                               rightValue,
                               rhsNew->type == NODE_TYPE_OPERATOR_BINARY_IN);
          }
        }
      }

      // return node as is
      return const_cast<AstNode*>(node);
    }

    if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
      ast::LogicalOperatorNode andOp(node);
      auto lhs = andOp.getLeft();
      auto rhs = andOp.getRight();

      auto lhsNew = simplify(lhs);
      auto rhsNew = simplify(rhs);

      if (lhs != lhsNew || rhs != rhsNew) {
        // return a modified node
        return ast->createNodeBinaryOperator(node->type, lhsNew, rhsNew);
      }

      // intentionally falls through
    }

    return const_cast<AstNode*>(node);
  }
};

/// @brief this rule replaces expressions of the type:
///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to
//  the same (single) attribute.
void arangodb::aql::replaceOrWithInRule(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = ExecutionNode::castTo<FilterNode const*>(n);
    auto cn = ExecutionNode::castTo<CalculationNode*>(dep);
    auto outVar = cn->outVariable();

    if (outVar != fn->inVariable()) {
      continue;
    }

    auto root = cn->expression()->node();

    OrSimplifier simplifier(plan->getAst(), plan.get());
    auto newRoot = simplifier.simplify(root);

    if (newRoot != root) {
      auto expr = std::make_unique<Expression>(plan->getAst(), newRoot);

      TRI_IF_FAILURE("OptimizerRules::replaceOrWithInRuleOom") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      ExecutionNode* newNode = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(), std::move(expr), outVar);

      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

struct RemoveRedundantOr {
  AstNode const* bestValue = nullptr;
  AstNodeType comparison;
  bool inclusive;
  bool isComparisonSet = false;
  CommonNodeFinder finder;
  AstNode const* commonNode = nullptr;
  std::string commonName;

  bool hasRedundantCondition(AstNode const* node) {
    try {
      if (finder.find(node, NODE_TYPE_OPERATOR_BINARY_LT, commonNode,
                      commonName)) {
        return hasRedundantConditionWalker(node);
      }
    } catch (...) {
      // ignore errors and simply return false
    }
    return false;
  }

  AstNode* createReplacementNode(Ast* ast) {
    TRI_ASSERT(commonNode != nullptr);
    TRI_ASSERT(bestValue != nullptr);
    TRI_ASSERT(isComparisonSet == true);
    return ast->createNodeBinaryOperator(comparison, commonNode->clone(ast),
                                         bestValue);
  }

 private:
  bool isInclusiveBound(AstNodeType type) {
    return (type == NODE_TYPE_OPERATOR_BINARY_GE ||
            type == NODE_TYPE_OPERATOR_BINARY_LE);
  }

  int isCompatibleBound(AstNodeType type, AstNode const*) {
    if ((comparison == NODE_TYPE_OPERATOR_BINARY_LE ||
         comparison == NODE_TYPE_OPERATOR_BINARY_LT) &&
        (type == NODE_TYPE_OPERATOR_BINARY_LE ||
         type == NODE_TYPE_OPERATOR_BINARY_LT)) {
      return -1;  // high bound
    } else if ((comparison == NODE_TYPE_OPERATOR_BINARY_GE ||
                comparison == NODE_TYPE_OPERATOR_BINARY_GT) &&
               (type == NODE_TYPE_OPERATOR_BINARY_GE ||
                type == NODE_TYPE_OPERATOR_BINARY_GT)) {
      return 1;  // low bound
    }
    return 0;  // incompatible bounds
  }

  // returns false if the existing value is better and true if the input value
  // is better
  bool compareBounds(AstNodeType type, AstNode const* value, int lowhigh) {
    int cmp = compareAstNodes(bestValue, value, true);

    if (cmp == 0 && (isInclusiveBound(comparison) != isInclusiveBound(type))) {
      return (isInclusiveBound(type) ? true : false);
    }
    return (cmp * lowhigh == 1);
  }

  bool hasRedundantConditionWalker(AstNode const* node) {
    AstNodeType type = node->type;

    if (type == NODE_TYPE_OPERATOR_BINARY_OR) {
      ast::LogicalOperatorNode orOp(node);
      return (hasRedundantConditionWalker(orOp.getLeft()) &&
              hasRedundantConditionWalker(orOp.getRight()));
    }

    if (type == NODE_TYPE_OPERATOR_BINARY_LE ||
        type == NODE_TYPE_OPERATOR_BINARY_LT ||
        type == NODE_TYPE_OPERATOR_BINARY_GE ||
        type == NODE_TYPE_OPERATOR_BINARY_GT) {
      ast::RelationalOperatorNode relOp(node);
      auto lhs = relOp.getLeft();
      auto rhs = relOp.getRight();

      if (hasRedundantConditionWalker(rhs) &&
          !hasRedundantConditionWalker(lhs) && lhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = Ast::reverseOperator(type);
          bestValue = lhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(Ast::reverseOperator(type), lhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, lhs, lowhigh)) {
          comparison = Ast::reverseOperator(type);
          bestValue = lhs;
        }
        return true;
      }
      if (hasRedundantConditionWalker(lhs) &&
          !hasRedundantConditionWalker(rhs) && rhs->isConstant()) {
        if (!isComparisonSet) {
          comparison = type;
          bestValue = rhs;
          isComparisonSet = true;
          return true;
        }

        int lowhigh = isCompatibleBound(type, rhs);
        if (lowhigh == 0) {
          return false;
        }

        if (compareBounds(type, rhs, lowhigh)) {
          comparison = type;
          bestValue = rhs;
        }
        return true;
      }
      // if hasRedundantConditionWalker(lhs) and
      // hasRedundantConditionWalker(rhs), then one of the conditions in the
      // OR statement is of the form x == x intentionally falls through if
    } else if (type == NODE_TYPE_REFERENCE ||
               type == NODE_TYPE_ATTRIBUTE_ACCESS ||
               type == NODE_TYPE_INDEXED_ACCESS) {
      // get a string representation of the node for comparisons
      return (node->toString() == commonName);
    }

    return false;
  }
};

void arangodb::aql::removeRedundantOrRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
                                          OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;
  for (auto const& n : nodes) {
    TRI_ASSERT(n->hasDependency());

    auto const dep = n->getFirstDependency();

    if (dep->getType() != EN::CALCULATION) {
      continue;
    }

    auto fn = ExecutionNode::castTo<FilterNode const*>(n);
    auto cn = ExecutionNode::castTo<CalculationNode*>(dep);
    auto outVar = cn->outVariable();

    if (outVar != fn->inVariable()) {
      continue;
    }
    if (cn->expression()->node()->type != NODE_TYPE_OPERATOR_BINARY_OR) {
      continue;
    }

    RemoveRedundantOr remover;
    if (remover.hasRedundantCondition(cn->expression()->node())) {
      auto astNode = remover.createReplacementNode(plan->getAst());

      auto expr = std::make_unique<Expression>(plan->getAst(), astNode);
      ExecutionNode* newNode = plan->createNode<CalculationNode>(
          plan.get(), plan->nextId(), std::move(expr), outVar);
      plan->replaceNode(cn, newNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove $OLD and $NEW variables from data-modification statements
/// if not required
void arangodb::aql::removeDataModificationOutVariablesRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ::removeDataModificationOutVariablesNodeTypes,
                        true);

  for (auto const& n : nodes) {
    auto node = ExecutionNode::castTo<ModificationNode*>(n);
    TRI_ASSERT(node != nullptr);

    Variable const* old = node->getOutVariableOld();
    if (!n->isVarUsedLater(old)) {
      // "$OLD" is not used later
      node->clearOutVariableOld();
      modified = true;
    } else {
      switch (n->getType()) {
        case EN::UPDATE:
        case EN::REPLACE: {
          Variable const* inVariable =
              ExecutionNode::castTo<UpdateReplaceNode const*>(n)
                  ->inKeyVariable();
          if (inVariable != nullptr) {
            auto setter = plan->getVarSetBy(inVariable->id);
            if (setter != nullptr &&
                (setter->getType() == EN::ENUMERATE_COLLECTION ||
                 setter->getType() == EN::INDEX)) {
              std::unordered_map<VariableId, Variable const*> replacements;
              replacements.try_emplace(old->id, inVariable);
              VariableReplacer finder(replacements);
              plan->root()->walk(finder);
              modified = true;
            }
          }
          break;
        }
        case EN::REMOVE: {
          Variable const* inVariable =
              ExecutionNode::castTo<RemoveNode const*>(n)->inVariable();
          TRI_ASSERT(inVariable != nullptr);
          auto setter = plan->getVarSetBy(inVariable->id);
          if (setter != nullptr &&
              (setter->getType() == EN::ENUMERATE_COLLECTION ||
               setter->getType() == EN::INDEX)) {
            std::unordered_map<VariableId, Variable const*> replacements;
            replacements.try_emplace(old->id, inVariable);
            VariableReplacer finder(replacements);
            plan->root()->walk(finder);
            modified = true;
          }
          break;
        }
        default: {
          // do nothing
        }
      }
    }

    if (!n->isVarUsedLater(node->getOutVariableNew())) {
      // "$NEW" is not used later
      node->clearOutVariableNew();
      modified = true;
    }

    if (!n->hasParent()) {
      node->producesResults(false);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief optimizes away unused traversal output variables and
/// merges filter nodes into graph traversal nodes
void arangodb::aql::optimizeTraversalsRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  std::unordered_set<arangodb::aql::AttributeNamePath> attributes;
  bool modified = false;

  // first make a pass over all traversal nodes and remove unused
  // variables from them
  // While on it, pick up possible projections on the vertex and edge documents
  for (auto const& n : tNodes) {
    auto* traversal = ExecutionNode::castTo<TraversalNode*>(n);
    auto* options = static_cast<arangodb::traverser::TraverserOptions*>(
        traversal->options());

    std::vector<Variable const*> pruneVars;
    traversal->getPruneVariables(pruneVars);

    // optimize path output variable
    auto pathOutVariable = traversal->pathOutVariable();
    modified |=
        optimizeTraversalPathVariable(pathOutVariable, traversal, pruneVars);

    // note that we can NOT optimize away the vertex output variable
    // yet, as many traversal internals depend on the number of vertices
    // found/built
    //
    // however, we can turn off looking up vertices and producing them in the
    // result set. we can do this if the traversal's vertex out variable is
    // never used later and also the traversal's path out variable is not used
    // later (note that the path out variable can contain the "vertices" sub
    // attribute)
    auto outVariable = traversal->vertexOutVariable();
    if (outVariable != nullptr) {
      if (!n->isVarUsedLater(outVariable) &&
          std::find(pruneVars.begin(), pruneVars.end(), outVariable) ==
              pruneVars.end()) {
        outVariable = traversal->pathOutVariable();
        if (outVariable == nullptr ||
            ((!n->isVarUsedLater(outVariable) ||
              !options->producePathsVertices()) &&
             std::find(pruneVars.begin(), pruneVars.end(), outVariable) ==
                 pruneVars.end())) {
          // both traversal vertex and path outVariables not used later
          options->setProduceVertices(false);
          modified = true;
        }
      }
    }

    outVariable = traversal->edgeOutVariable();
    if (outVariable != nullptr) {
      if (!n->isVarUsedLater(outVariable)) {
        // traversal edge outVariable not used later
        options->setProduceEdges(false);
        if (std::find(pruneVars.begin(), pruneVars.end(), outVariable) ==
            pruneVars.end()) {
          traversal->setEdgeOutput(nullptr);
        }
        modified = true;
      }
    }

    // handle projections (must be done after path variable optimization)
    bool appliedProjections = applyGraphProjections(traversal);
    if (appliedProjections) {
      modified = true;
    }

    // check if we can make use of the optimized neighbors enumerator
    if (!options->isDisjoint()) {
      // Use NeighborsEnumerator optimization only in case we have do not
      // have a (Hybrid)Disjoint SmartGraph
      if (!ServerState::instance()->isCoordinator()) {
        if (traversal->vertexOutVariable() != nullptr &&
            traversal->edgeOutVariable() == nullptr &&
            traversal->pathOutVariable() == nullptr &&
            options->isUseBreadthFirst() &&
            options->uniqueVertices ==
                arangodb::traverser::TraverserOptions::GLOBAL &&
            !options->usesPrune() && !options->hasDepthLookupInfo()) {
          // this is possible in case *only* vertices are produced (no edges, no
          // path), the traversal is breadth-first, the vertex uniqueness level
          // is set to "global", there is no pruning and there are no
          // depth-specific filters
          options->useNeighbors = true;
          modified = true;
        }
      }
    }
  }

  if (!tNodes.empty()) {
    // These are all the end nodes where we start
    containers::SmallVector<ExecutionNode*, 8> nodes;
    plan->findEndNodes(nodes, true);

    for (auto const& n : nodes) {
      TraversalConditionFinder finder(plan.get(), &modified);
      n->walk(finder);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief optimizes away unused K_PATHS things
void arangodb::aql::optimizePathsRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::ENUMERATE_PATHS, true);

  if (nodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;

  // first make a pass over all traversal nodes and remove unused
  // variables from them
  for (auto const& n : nodes) {
    EnumeratePathsNode* ksp = ExecutionNode::castTo<EnumeratePathsNode*>(n);
    if (!ksp->usesPathOutVariable()) {
      continue;
    }

    Variable const* variable = &(ksp->pathOutVariable());

    containers::FlatHashSet<AttributeNamePath> attributes;
    VarSet vars;
    bool canOptimize = true;

    ExecutionNode* current = ksp->getFirstParent();
    arangodb::ResourceMonitor& resMonitor =
        plan->getAst()->query().resourceMonitor();

    while (current != nullptr && canOptimize) {
      switch (current->getType()) {
        case EN::CALCULATION: {
          vars.clear();
          current->getVariablesUsedHere(vars);
          if (vars.find(variable) != vars.end()) {
            // path variable used here
            Expression* exp =
                ExecutionNode::castTo<CalculationNode*>(current)->expression();
            AstNode const* node = exp->node();
            if (!Ast::getReferencedAttributesRecursive(
                    node, variable, /*expectedAttributes*/ "", attributes,
                    resMonitor)) {
              // full path variable is used, or accessed in a way that we don't
              // understand, e.g. "p" or "p[0]" or "p[*]..."
              canOptimize = false;
            }
          }
          break;
        }
        default: {
          // if the path is used by any other node type, we don't know what to
          // do and will not optimize parts of it away
          vars.clear();
          current->getVariablesUsedHere(vars);
          if (vars.find(variable) != vars.end()) {
            canOptimize = false;
          }
          break;
        }
      }
      current = current->getFirstParent();
    }

    if (canOptimize) {
      bool produceVertices =
          attributes.contains({StaticStrings::GraphQueryVertices, resMonitor});

      if (!produceVertices) {
        auto* options = ksp->options();
        options->setProduceVertices(false);
        modified = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

// remove filter nodes already covered by a traversal
void arangodb::aql::removeFiltersCoveredByTraversal(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> fNodes;
  plan->findNodesOfType(fNodes, EN::FILTER, true);
  if (fNodes.empty()) {
    // no filters present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;
  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto const& node : fNodes) {
    auto fn = ExecutionNode::castTo<FilterNode const*>(node);
    // find the node with the filter expression
    auto setter = plan->getVarSetBy(fn->inVariable()->id);
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto calculationNode = ExecutionNode::castTo<CalculationNode*>(setter);
    auto conditionNode = calculationNode->expression()->node();

    // build the filter condition
    Condition condition(plan->getAst());
    condition.andCombine(conditionNode);
    condition.normalize(plan.get());

    if (condition.root() == nullptr) {
      continue;
    }

    size_t const n = condition.root()->numMembers();

    if (n != 1) {
      // either no condition or multiple ORed conditions...
      continue;
    }

    bool handled = false;
    auto current = node;
    while (current != nullptr) {
      if (current->getType() == EN::TRAVERSAL) {
        auto traversalNode =
            ExecutionNode::castTo<TraversalNode const*>(current);

        // found a traversal node, now check if the expression
        // is covered by the traversal
        auto traversalCondition = traversalNode->condition();

        if (traversalCondition != nullptr && !traversalCondition->isEmpty()) {
          VarSet varsUsedByCondition;
          Ast::getReferencedVariables(condition.root(), varsUsedByCondition);

          auto remover = [&](Variable const* outVariable,
                             bool isPathCondition) -> bool {
            if (outVariable == nullptr) {
              return false;
            }
            if (varsUsedByCondition.find(outVariable) ==
                varsUsedByCondition.end()) {
              return false;
            }

            auto newNode = condition.removeTraversalCondition(
                plan.get(), outVariable, traversalCondition->root(),
                isPathCondition);
            if (newNode == nullptr) {
              // no condition left...
              // FILTER node can be completely removed
              toUnlink.emplace(node);
              // note: we must leave the calculation node intact, in case it
              // is still used by other nodes in the plan
              return true;
            } else if (newNode != condition.root()) {
              // some condition is left, but it is a different one than
              // the one from the FILTER node
              auto expr = std::make_unique<Expression>(plan->getAst(), newNode);
              auto* cn = plan->createNode<CalculationNode>(
                  plan.get(), plan->nextId(), std::move(expr),
                  calculationNode->outVariable());
              plan->replaceNode(setter, cn);
              return true;
            }

            return false;
          };

          std::array<std::pair<Variable const*, bool>, 3> vars = {
              std::make_pair(traversalNode->pathOutVariable(),
                             /*isPathCondition*/ true),
              std::make_pair(traversalNode->vertexOutVariable(),
                             /*isPathCondition*/ false),
              std::make_pair(traversalNode->edgeOutVariable(),
                             /*isPathCondition*/ false)};

          for (auto [v, isPathCondition] : vars) {
            if (remover(v, isPathCondition)) {
              modified = true;
              handled = true;
              break;
            }
          }
        }

        if (handled) {
          break;
        }
      }

      if (handled || current->getType() == EN::LIMIT ||
          !current->hasDependency()) {
        break;
      }
      current = current->getFirstDependency();
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief removes redundant path variables, after applying
/// `removeFiltersCoveredByTraversal`. Should significantly reduce overhead
void arangodb::aql::removeTraversalPathVariable(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, EN::TRAVERSAL, true);

  bool modified = false;
  // first make a pass over all traversal nodes and remove unused
  // variables from them

  for (auto const& n : tNodes) {
    TraversalNode* traversal = ExecutionNode::castTo<TraversalNode*>(n);
    auto outVariable = traversal->pathOutVariable();
    if (outVariable != nullptr) {
      std::vector<Variable const*> pruneVars;
      traversal->getPruneVariables(pruneVars);
      modified |=
          optimizeTraversalPathVariable(outVariable, traversal, pruneVars);
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief pulls out simple subqueries and merges them with the level above
///
/// For example, if we have the input query
///
/// FOR x IN (
///     FOR y IN collection FILTER y.value >= 5 RETURN y.test
///   )
///   RETURN x.a
///
/// then this rule will transform it into:
///
/// FOR tmp IN collection
///   FILTER tmp.value >= 5
///   LET x = tmp.test
///   RETURN x.a
void arangodb::aql::inlineSubqueriesRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::SUBQUERY, true);

  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;
  std::vector<ExecutionNode*> subNodes;

  for (auto const& n : nodes) {
    auto subqueryNode = ExecutionNode::castTo<SubqueryNode*>(n);

    if (subqueryNode->isModificationNode()) {
      // can't modify modifying subqueries
      continue;
    }

    if (!subqueryNode->isDeterministic()) {
      // can't inline non-deterministic subqueries
      continue;
    }

    // check if subquery contains a COLLECT node with an INTO variable
    // or a WINDOW node in an inner loop
    bool eligible = true;
    bool containsLimitOrSort = false;
    auto current = subqueryNode->getSubquery();
    TRI_ASSERT(current != nullptr);

    while (current != nullptr) {
      if (current->getType() == EN::WINDOW && subqueryNode->isInInnerLoop()) {
        // WINDOW captures all existing rows in the scope, moving WINDOW
        // ends up with different rows captured
        eligible = false;
        break;
      } else if (current->getType() == EN::COLLECT) {
        if (subqueryNode->isInInnerLoop()) {
          eligible = false;
          break;
        }
        if (ExecutionNode::castTo<CollectNode const*>(current)
                ->hasOutVariable()) {
          // COLLECT ... INTO captures all existing variables in the scope.
          // if we move the subquery from one scope into another, we will end up
          // with different variables captured, so we must not apply the
          // optimization in this case.
          eligible = false;
          break;
        }
      } else if (current->getType() == EN::LIMIT ||
                 current->getType() == EN::SORT) {
        containsLimitOrSort = true;
      }
      current = current->getFirstDependency();
    }

    if (!eligible) {
      continue;
    }

    Variable const* out = subqueryNode->outVariable();
    TRI_ASSERT(out != nullptr);
    // the subquery outvariable and all its aliases
    VarSet subqueryVars;
    subqueryVars.emplace(out);

    // the potential calculation nodes that produce the aliases
    std::vector<ExecutionNode*> aliasNodesToRemoveLater;

    VarSet varsUsed;

    current = n->getFirstParent();
    // now check where the subquery is used
    while (current->hasParent()) {
      if (current->getType() == EN::ENUMERATE_LIST) {
        if (current->isInInnerLoop() && containsLimitOrSort) {
          // exit the loop
          current = nullptr;
          break;
        }

        // we're only interested in FOR loops...
        auto listNode = ExecutionNode::castTo<EnumerateListNode*>(current);
        if (listNode->getMode() == EnumerateListNode::kEnumerateObject) {
          // exit the loop
          current = nullptr;
          break;
        }
        // ...that use our subquery as its input
        if (subqueryVars.find(listNode->inVariable()) != subqueryVars.end()) {
          // bingo!

          // check if the subquery result variable or any of the aliases are
          // used after the FOR loop
          bool mustAbort = false;
          for (auto const& itSub : subqueryVars) {
            if (listNode->isVarUsedLater(itSub)) {
              // exit the loop
              current = nullptr;
              mustAbort = true;
              break;
            }
          }
          if (mustAbort) {
            break;
          }

          for (auto const& toRemove : aliasNodesToRemoveLater) {
            plan->unlinkNode(toRemove, false);
          }

          subNodes.clear();
          subNodes.reserve(4);
          subqueryNode->getSubquery()->getDependencyChain(subNodes, true);
          TRI_ASSERT(!subNodes.empty());
          auto returnNode = ExecutionNode::castTo<ReturnNode*>(subNodes[0]);
          TRI_ASSERT(returnNode->getType() == EN::RETURN);

          modified = true;
          auto queryVariables = plan->getAst()->variables();
          auto previous = n->getFirstDependency();
          auto insert = n->getFirstParent();
          TRI_ASSERT(insert != nullptr);

          // unlink the original SubqueryNode
          plan->unlinkNode(n, false);

          for (auto& it : subNodes) {
            // first unlink them all
            plan->unlinkNode(it, true);

            if (it->getType() == EN::SINGLETON) {
              // reached the singleton node already. that means we can stop
              break;
            }

            // and now insert them one level up
            if (it != returnNode) {
              // we skip over the subquery's return node. we don't need it
              // anymore
              insert->removeDependencies();
              TRI_ASSERT(it != nullptr);
              insert->addDependency(it);
              insert = it;

              // additionally rename the variables from the subquery so they
              // cannot conflict with the ones from the top query
              for (auto const& variable : it->getVariablesSetHere()) {
                queryVariables->renameVariable(variable->id);
              }
            }
          }

          // link the top node in the subquery with the original plan
          if (previous != nullptr) {
            insert->addDependency(previous);
          }

          // remove the list node from the plan
          plan->unlinkNode(listNode, false);

          queryVariables->renameVariable(returnNode->inVariable()->id,
                                         listNode->outVariable()[0]->name);

          // finally replace the variables
          std::unordered_map<VariableId, Variable const*> replacements;
          replacements.try_emplace(listNode->outVariable()[0]->id,
                                   returnNode->inVariable());
          VariableReplacer finder(replacements);
          plan->root()->walk(finder);

          plan->clearVarUsageComputed();
          plan->findVarUsage();

          // abort optimization
          current = nullptr;
        }
      } else if (current->getType() == EN::CALCULATION) {
        auto rootNode = ExecutionNode::castTo<CalculationNode*>(current)
                            ->expression()
                            ->node();
        if (rootNode->type == NODE_TYPE_REFERENCE) {
          if (subqueryVars.find(ast::ReferenceNode(rootNode).getVariable()) !=
              subqueryVars.end()) {
            // found an alias for the subquery variable
            subqueryVars.emplace(
                ExecutionNode::castTo<CalculationNode*>(current)
                    ->outVariable());
            aliasNodesToRemoveLater.emplace_back(current);
            current = current->getFirstParent();

            continue;
          }
        }
      }

      if (current == nullptr) {
        break;
      }

      varsUsed.clear();
      current->getVariablesUsedHere(varsUsed);

      bool mustAbort = false;
      for (auto const& itSub : subqueryVars) {
        if (varsUsed.find(itSub) != varsUsed.end()) {
          // we found another node that uses the subquery variable
          // we need to stop the optimization attempts here
          mustAbort = true;
          break;
        }
      }
      if (mustAbort) {
        break;
      }

      current = current->getFirstParent();
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// Essentially mirrors the geo::QueryParams struct, but with
/// abstracts AstNode value objects
struct GeoIndexInfo {
  operator bool() const {
    return collectionNodeToReplace != nullptr && collectionNodeOutVar &&
           collection && index && valid;
  }
  void invalidate() { valid = false; }

  /// node that will be replaced by (geo) IndexNode
  ExecutionNode* collectionNodeToReplace = nullptr;
  Variable const* collectionNodeOutVar = nullptr;

  /// accessed collection
  aql::Collection const* collection = nullptr;
  /// selected index
  std::shared_ptr<Index> index;

  /// Filter calculations to modify
  std::map<ExecutionNode*, Expression*> exesToModify;
  std::set<AstNode const*> nodesToRemove;

  // ============ Distance ============
  AstNode const* distCenterExpr = nullptr;
  AstNode const* distCenterLatExpr = nullptr;
  AstNode const* distCenterLngExpr = nullptr;
  // Expression representing minimum distance
  AstNode const* minDistanceExpr = nullptr;
  // Was operator < or <= used
  bool minInclusive = true;
  // Expression representing maximum distance
  AstNode const* maxDistanceExpr = nullptr;
  // Was operator > or >= used
  bool maxInclusive = true;

  // ============ Near Info ============
  bool sorted = false;
  /// Default order is from closest to farthest
  bool ascending = true;

  // ============ Filter Info ===========
  geo::FilterType filterMode = geo::FilterType::NONE;
  /// variable using the filter mask
  AstNode const* filterExpr = nullptr;

  // ============ Accessed Fields ============
  AstNode const* locationVar = nullptr;   // access to location field
  AstNode const* latitudeVar = nullptr;   // access path to latitude
  AstNode const* longitudeVar = nullptr;  // access path to longitude

  /// contains this node a valid condition
  bool valid = true;
};

// checks 2 parameters of distance function if they represent a valid access to
// latitude and longitude attribute of the geo index.
// distance(a,b,c,d) - possible pairs are (a,b) and (c,d)
static bool distanceFuncArgCheck(ExecutionPlan* plan, AstNode const* latArg,
                                 AstNode const* lngArg, bool supportLegacy,
                                 GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess1;
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess2;
  // first and second should be based on the same document - need to provide
  // the document in order to see which collection is bound to it and if that
  // collections supports geo-index
  if (!latArg->isAttributeAccessForVariable(attributeAccess1, true) ||
      !lngArg->isAttributeAccessForVariable(attributeAccess2, true)) {
    return false;
  }
  TRI_ASSERT(attributeAccess1.first != nullptr);
  TRI_ASSERT(attributeAccess2.first != nullptr);

  ExecutionNode* setter1 = plan->getVarSetBy(attributeAccess1.first->id);
  ExecutionNode* setter2 = plan->getVarSetBy(attributeAccess2.first->id);
  if (setter1 == nullptr || setter1 != setter2 ||
      setter1->getType() != EN::ENUMERATE_COLLECTION) {
    return false;  // expect access of doc.lat, doc.lng or doc.loc[0],
                   // doc.loc[1]
  }

  // get logical collection
  auto collNode = ExecutionNode::castTo<EnumerateCollectionNode*>(setter1);
  if (info.collectionNodeToReplace != nullptr &&
      info.collectionNodeToReplace != collNode) {
    return false;  // should probably never happen
  }

  // we should not access the LogicalCollection directly
  auto indexes = collNode->collection()->indexes();
  // check for suitiable indexes
  for (std::shared_ptr<Index> idx : indexes) {
    // check if current index is a geo-index
    std::size_t fieldNum = idx->fields().size();
    bool isGeo1 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX &&
                  supportLegacy;
    bool isGeo2 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX &&
                  supportLegacy;
    bool isGeo = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;

    if ((isGeo2 || isGeo) && fieldNum == 2) {  // individual fields
      // check access paths of attributes in ast and those in index match
      if (idx->fields()[0] == attributeAccess1.second &&
          idx->fields()[1] == attributeAccess2.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;
        }
        info.index = idx;
        info.latitudeVar = latArg;
        info.longitudeVar = lngArg;
        info.collectionNodeToReplace = collNode;
        info.collectionNodeOutVar = collNode->outVariable();
        info.collection = collNode->collection();
        return true;
      }
    } else if ((isGeo1 || isGeo) && fieldNum == 1) {
      std::vector<basics::AttributeName> fields1 = idx->fields()[0];
      std::vector<basics::AttributeName> fields2 = idx->fields()[0];

      velocypack::SupervisedBuffer sb(
          plan->getAst()->query().resourceMonitor());
      VPackBuilder builder(sb);
      idx->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
      bool geoJson = basics::VelocyPackHelper::getBooleanValue(
          builder.slice(), "geoJson", false);

      fields1.back().name += geoJson ? "[1]" : "[0]";
      fields2.back().name += geoJson ? "[0]" : "[1]";
      if (fields1 == attributeAccess1.second &&
          fields2 == attributeAccess2.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;
        }
        info.index = idx;
        info.latitudeVar = latArg;
        info.longitudeVar = lngArg;
        info.collectionNodeToReplace = collNode;
        info.collectionNodeOutVar = collNode->outVariable();
        info.collection = collNode->collection();
        return true;
      }
    }  // if isGeo 1 or 2
  }    // for index in collection
  return false;
}

// checks parameter of GEO_* function
static bool geoFuncArgCheck(ExecutionPlan* plan, AstNode const* args,
                            bool supportLegacy, GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess;
  // "arg" is either `[doc.lat, doc.lng]` or `doc.geometry`
  if (args->isArray() && args->numMembers() == 2) {
    return distanceFuncArgCheck(plan, /*lat*/ args->getMemberUnchecked(1),
                                /*lng*/ args->getMemberUnchecked(0),
                                supportLegacy, info);
  } else if (!args->isAttributeAccessForVariable(attributeAccess, true)) {
    return false;  // no attribute access, no index check
  }
  TRI_ASSERT(attributeAccess.first != nullptr);
  ExecutionNode* setter = plan->getVarSetBy(attributeAccess.first->id);
  if (setter == nullptr || setter->getType() != EN::ENUMERATE_COLLECTION) {
    return false;  // expected access of the for doc.attribute
  }

  // get logical collection
  auto collNode = ExecutionNode::castTo<EnumerateCollectionNode*>(setter);
  if (info.collectionNodeToReplace != nullptr &&
      info.collectionNodeToReplace != collNode) {
    return false;  // should probably never happen
  }

  // we should not access the LogicalCollection directly
  auto indexes = collNode->collection()->indexes();
  // check for suitiable indexes
  for (std::shared_ptr<arangodb::Index> idx : indexes) {
    // check if current index is a geo-index
    bool isGeo =
        idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;
    if (isGeo && idx->fields().size() == 1) {  // individual fields
      // check access paths of attributes in ast and those in index match
      if (idx->fields()[0] == attributeAccess.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;  // different index
        }
        info.index = idx;
        info.locationVar = args;
        info.collectionNodeToReplace = collNode;
        info.collectionNodeOutVar = collNode->outVariable();
        info.collection = collNode->collection();
        return true;
      }
    }
  }  // for index in collection
  return false;
}

/// returns true if left side is same as right or lhs is null
static bool isValidGeoArg(AstNode const* lhs, AstNode const* rhs) {
  if (lhs == nullptr) {  // lhs is from the GeoIndexInfo struct
    return true;         // if geoindex field is null everything is valid
  } else if (lhs->type != rhs->type) {
    return false;
  } else if (lhs->isArray()) {  // expect `[doc.lng, doc.lat]`
    if (lhs->numMembers() >= 2 && rhs->numMembers() >= 2) {
      return isValidGeoArg(lhs->getMemberUnchecked(0),
                           rhs->getMemberUnchecked(0)) &&
             isValidGeoArg(lhs->getMemberUnchecked(1),
                           rhs->getMemberUnchecked(1));
    }
    return false;
  } else if (lhs->type == NODE_TYPE_REFERENCE) {
    return ast::ReferenceNode(lhs).getVariable()->id ==
           ast::ReferenceNode(rhs).getVariable()->id;
  }
  // compareAstNodes does not handle non const attribute access
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> res1,
      res2;
  bool acc1 = lhs->isAttributeAccessForVariable(res1, true);
  bool acc2 = rhs->isAttributeAccessForVariable(res2, true);
  if (acc1 || acc2) {
    return acc1 && acc2 && res1 == res2;  // same variable same path
  }
  return aql::compareAstNodes(lhs, rhs, false) == 0;
}

static bool checkDistanceFunc(ExecutionPlan* plan, AstNode const* funcNode,
                              bool legacy, GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  if (funcNode->type == NODE_TYPE_REFERENCE) {
    // FOR x IN cc LET d = DISTANCE(...) FILTER d > 10 RETURN x
    Variable const* var = ast::ReferenceNode(funcNode).getVariable();
    TRI_ASSERT(var != nullptr);
    ExecutionNode* setter = plan->getVarSetBy(var->id);
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      return false;
    }
    funcNode =
        ExecutionNode::castTo<CalculationNode*>(setter)->expression()->node();
  }
  // get the ast node of the expression
  if (!funcNode || funcNode->type != NODE_TYPE_FCALL ||
      funcNode->numMembers() != 1) {
    return false;
  }
  AstNode* fargs = funcNode->getMemberUnchecked(0);
  auto func = ast::FunctionCallNode(funcNode).getFunction();
  if (fargs->numMembers() >= 4 &&
      func->name == "DISTANCE") {  // allow DISTANCE(a,b,c,d)
    if (info.distCenterExpr != nullptr) {
      return false;  // do not allow mixing of DISTANCE and GEO_DISTANCE
    }
    if (isValidGeoArg(info.distCenterLatExpr, fargs->getMemberUnchecked(2)) &&
        isValidGeoArg(info.distCenterLngExpr, fargs->getMemberUnchecked(3)) &&
        distanceFuncArgCheck(plan, fargs->getMemberUnchecked(0),
                             fargs->getMemberUnchecked(1), legacy, info)) {
      info.distCenterLatExpr = fargs->getMemberUnchecked(2);
      info.distCenterLngExpr = fargs->getMemberUnchecked(3);
      return true;
    } else if (isValidGeoArg(info.distCenterLatExpr,
                             fargs->getMemberUnchecked(0)) &&
               isValidGeoArg(info.distCenterLngExpr,
                             fargs->getMemberUnchecked(1)) &&
               distanceFuncArgCheck(plan, fargs->getMemberUnchecked(2),
                                    fargs->getMemberUnchecked(3), legacy,
                                    info)) {
      info.distCenterLatExpr = fargs->getMemberUnchecked(0);
      info.distCenterLngExpr = fargs->getMemberUnchecked(1);
      return true;
    }
  } else if (fargs->numMembers() == 2 && func->name == "GEO_DISTANCE") {
    if (info.distCenterLatExpr || info.distCenterLngExpr) {
      return false;  // do not allow mixing of DISTANCE and GEO_DISTANCE
    }
    if (isValidGeoArg(info.distCenterExpr, fargs->getMemberUnchecked(1)) &&
        geoFuncArgCheck(plan, fargs->getMemberUnchecked(0), legacy, info)) {
      info.distCenterExpr = fargs->getMemberUnchecked(1);
      return true;
    } else if (isValidGeoArg(info.distCenterExpr,
                             fargs->getMemberUnchecked(0)) &&
               geoFuncArgCheck(plan, fargs->getMemberUnchecked(1), legacy,
                               info)) {
      info.distCenterExpr = fargs->getMemberUnchecked(0);
      return true;
    }
  }
  return false;
}

// contains the AstNode* a supported function?
static bool checkGeoFilterFunction(ExecutionPlan* plan, AstNode const* funcNode,
                                   GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  // the expression must exist and it must be a function call
  if (funcNode->type != NODE_TYPE_FCALL || funcNode->numMembers() != 1 ||
      info.filterMode != geo::FilterType::NONE) {  // can't handle more than one
    return false;
  }

  auto func = ast::FunctionCallNode(funcNode).getFunction();
  AstNode* fargs = funcNode->getMemberUnchecked(0);
  bool contains = func->name == "GEO_CONTAINS";
  bool intersect = func->name == "GEO_INTERSECTS";
  if ((!contains && !intersect) || fargs->numMembers() != 2) {
    return false;
  }

  AstNode* arg = fargs->getMemberUnchecked(1);
  if (geoFuncArgCheck(plan, arg, /*legacy*/ true, info)) {
    TRI_ASSERT(contains || intersect);
    info.filterMode =
        contains ? geo::FilterType::CONTAINS : geo::FilterType::INTERSECTS;
    info.filterExpr = fargs->getMemberUnchecked(0);
    TRI_ASSERT(info.index);
    return true;
  }
  return false;
}

// checks if a node contanis a geo index function a valid operator
// to use within a filter condition
bool checkGeoFilterExpression(ExecutionPlan* plan, AstNode const* node,
                              GeoIndexInfo& info) {
  // checks @first `smaller` @second
  // note: this only modifies "info" if the function returns true
  auto eval = [&](AstNode const* first, AstNode const* second,
                  bool lessequal) -> bool {
    if (second->type == NODE_TYPE_VALUE &&  // only constants allowed
        info.maxDistanceExpr == nullptr &&  // max distance is not yet set
        checkDistanceFunc(plan, first, /*legacy*/ true, info)) {
      TRI_ASSERT(info.index);
      info.maxDistanceExpr = second;
      info.maxInclusive = info.maxInclusive && lessequal;
      info.nodesToRemove.insert(node);
      return true;
    } else if (first->type == NODE_TYPE_VALUE &&  // only constants allowed
               info.minDistanceExpr ==
                   nullptr &&  // min distance is not yet set
               checkDistanceFunc(plan, second, /*legacy*/ true, info)) {
      info.minDistanceExpr = first;
      info.minInclusive = info.minInclusive && lessequal;
      info.nodesToRemove.insert(node);
      return true;
    }
    return false;
  };

  switch (node->type) {
    case NODE_TYPE_FCALL:
      if (checkGeoFilterFunction(plan, node, info)) {
        info.nodesToRemove.insert(node);
        return true;
      }
      return false;
    // only DISTANCE is allowed with <=, <, >=, >
    case NODE_TYPE_OPERATOR_BINARY_LE: {
      ast::RelationalOperatorNode relOp(node);
      return eval(relOp.getLeft(), relOp.getRight(), true);
    }
    case NODE_TYPE_OPERATOR_BINARY_LT: {
      ast::RelationalOperatorNode relOp(node);
      return eval(relOp.getLeft(), relOp.getRight(), false);
    }
    case NODE_TYPE_OPERATOR_BINARY_GE: {
      ast::RelationalOperatorNode relOp(node);
      return eval(relOp.getRight(), relOp.getLeft(), true);
    }
    case NODE_TYPE_OPERATOR_BINARY_GT: {
      ast::RelationalOperatorNode relOp(node);
      return eval(relOp.getRight(), relOp.getLeft(), false);
    }
    default:
      return false;
  }
}

static bool optimizeSortNode(ExecutionPlan* plan, SortNode* sort,
                             GeoIndexInfo& info) {
  // note: info will only be modified if the function returns true
  TRI_ASSERT(sort->getType() == EN::SORT);
  // we're looking for "SORT DISTANCE(x,y,a,b)"
  SortElementVector const& elements = sort->elements();
  if (elements.size() != 1) {  // can't do it
    return false;
  }
  TRI_ASSERT(elements[0].var != nullptr);

  // find the expression that is bound to the variable
  // get the expression node that holds the calculation
  ExecutionNode* setter = plan->getVarSetBy(elements[0].var->id);
  if (setter == nullptr || setter->getType() != EN::CALCULATION) {
    return false;  // setter could be enumerate list node e.g.
  }
  CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(setter);
  Expression* expr = calc->expression();
  if (expr == nullptr || expr->node() == nullptr) {
    return false;  // the expression must exist and must have an astNode
  }

  // info will only be modified if the function returns true
  bool legacy = elements[0].ascending;  // DESC is only supported on S2 index
  if (!info.sorted && checkDistanceFunc(plan, expr->node(), legacy, info)) {
    info.sorted = true;  // do not parse another SORT
    info.ascending = elements[0].ascending;
    if (!ServerState::instance()->isCoordinator()) {
      // we must not remove a sort in the cluster... the results from each
      // shard will be sorted by using the index, however we still need to
      // establish a cross-shard sortedness by distance.
      info.exesToModify.emplace(sort, expr);
      info.nodesToRemove.emplace(expr->node());
    } else {
      // In the cluster case, we want to leave the SORT node in - for now!
      // This is to achieve that the GATHER node which is introduced to
      // distribute the query in the cluster remembers to sort things
      // using merge sort. However, later there will be a rule which
      // moves the sorting to the dbserver. When this rule is triggered,
      // we do not want to reinsert the SORT node on the dbserver, since
      // there, the items are already sorted by means of the geo index.
      // Therefore, we tell the sort node here, not to be reinserted
      // on the dbserver later on.
      // This is crucial to avoid that the SORT node remains and pulls
      // the whole collection out of the geo index, on the grounds that
      // the SORT node wants to sort the results, which is very bad for
      // performance.
      sort->dontReinsertInCluster();
    }
    return true;
  }
  return false;
}

// checks a single sort or filter node
static void optimizeFilterNode(ExecutionPlan* plan, FilterNode* fn,
                               GeoIndexInfo& info) {
  TRI_ASSERT(fn->getType() == EN::FILTER);

  // filter nodes always have one input variable
  auto variable = ExecutionNode::castTo<FilterNode const*>(fn)->inVariable();
  // now check who introduced our variable
  ExecutionNode* setter = plan->getVarSetBy(variable->id);
  if (setter == nullptr || setter->getType() != EN::CALCULATION) {
    return;
  }
  CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(setter);
  Expression* expr = calc->expression();
  if (expr->node() == nullptr) {
    return;  // the expression must exist and must have an AstNode
  }

  Ast::traverseReadOnly(
      expr->node(),
      [&](AstNode const* node) {  // pre
        if (node->isSimpleComparisonOperator() ||
            node->type == arangodb::aql::NODE_TYPE_FCALL ||
            node->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND ||
            node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND) {
          return true;
        }
        return false;
      },
      [&](AstNode const* node) {  // post
        if (!node->isSimpleComparisonOperator() &&
            node->type != arangodb::aql::NODE_TYPE_FCALL) {
          return;
        }
        if (checkGeoFilterExpression(plan, node, info)) {
          info.exesToModify.try_emplace(fn, expr);
        }
      });
}

// modify plan

// builds a condition that can be used with the index interface and
// contains all parameters required by the MMFilesGeoIndex
static std::unique_ptr<Condition> buildGeoCondition(ExecutionPlan* plan,
                                                    GeoIndexInfo const& info) {
  Ast* ast = plan->getAst();
  // shared code to add symbolic `doc.geometry` or `[doc.lng, doc.lat]`
  auto addLocationArg = [ast, &info](AstNode* args) {
    if (info.locationVar) {
      args->addMember(info.locationVar);
    } else if (info.latitudeVar && info.longitudeVar) {
      AstNode* array = ast->createNodeArray(2);
      array->addMember(info.longitudeVar);  // GeoJSON ordering
      array->addMember(info.latitudeVar);
      args->addMember(array);
    } else {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported geo type");
    }
  };

  TRI_ASSERT(info.index);
  auto cond = std::make_unique<Condition>(ast);
  bool hasCenter = info.distCenterLatExpr || info.distCenterExpr;
  bool hasDistLimit = info.maxDistanceExpr || info.minDistanceExpr;
  TRI_ASSERT(!hasCenter || hasDistLimit || info.sorted);
  if (hasCenter && (hasDistLimit || info.sorted)) {
    // create GEO_DISTANCE(...) [<|<=|>=|>] Var
    AstNode* args = ast->createNodeArray(2);
    if (info.distCenterLatExpr && info.distCenterLngExpr) {  // legacy
      TRI_ASSERT(!info.distCenterExpr);
      // info.sorted && info.ascending &&
      AstNode* array = ast->createNodeArray(2);
      array->addMember(info.distCenterLngExpr);  // GeoJSON ordering
      array->addMember(info.distCenterLatExpr);
      args->addMember(array);
    } else {
      TRI_ASSERT(info.distCenterExpr);
      TRI_ASSERT(!info.distCenterLatExpr && !info.distCenterLngExpr);
      args->addMember(info.distCenterExpr);  // center location
    }

    addLocationArg(args);
    AstNode* func = ast->createNodeFunctionCall("GEO_DISTANCE", args, true);

    TRI_ASSERT(info.maxDistanceExpr || info.minDistanceExpr || info.sorted);
    if (info.minDistanceExpr != nullptr) {
      AstNodeType t = info.minInclusive ? NODE_TYPE_OPERATOR_BINARY_GE
                                        : NODE_TYPE_OPERATOR_BINARY_GT;
      cond->andCombine(
          ast->createNodeBinaryOperator(t, func, info.minDistanceExpr));
    }
    if (info.maxDistanceExpr != nullptr) {
      AstNodeType t = info.maxInclusive ? NODE_TYPE_OPERATOR_BINARY_LE
                                        : NODE_TYPE_OPERATOR_BINARY_LT;
      cond->andCombine(
          ast->createNodeBinaryOperator(t, func, info.maxDistanceExpr));
    }
    if (info.minDistanceExpr == nullptr && info.maxDistanceExpr == nullptr &&
        info.sorted) {
      // hack to pass on the sort-to-point info
      AstNodeType t = NODE_TYPE_OPERATOR_BINARY_LT;
      std::string const& u = arangodb::StaticStrings::Unlimited;
      AstNode* cc = ast->createNodeValueString(u.c_str(), u.length());
      cond->andCombine(ast->createNodeBinaryOperator(t, func, cc));
    }
  }
  if (info.filterMode != geo::FilterType::NONE) {
    // create GEO_CONTAINS / GEO_INTERSECTS
    TRI_ASSERT(info.filterExpr);
    TRI_ASSERT(info.locationVar || (info.longitudeVar && info.latitudeVar));

    AstNode* args = ast->createNodeArray(2);
    args->addMember(info.filterExpr);
    addLocationArg(args);
    if (info.filterMode == geo::FilterType::CONTAINS) {
      cond->andCombine(ast->createNodeFunctionCall("GEO_CONTAINS", args, true));
    } else if (info.filterMode == geo::FilterType::INTERSECTS) {
      cond->andCombine(
          ast->createNodeFunctionCall("GEO_INTERSECTS", args, true));
    } else {
      TRI_ASSERT(false);
    }
  }

  cond->normalize(plan);
  return cond;
}

// applys the optimization for a candidate
static bool applyGeoOptimization(ExecutionPlan* plan, LimitNode* ln,
                                 GeoIndexInfo const& info) {
  TRI_ASSERT(info.collection != nullptr);
  TRI_ASSERT(info.collectionNodeToReplace != nullptr);
  TRI_ASSERT(info.index);

  // verify that all vars used in the index condition are valid
  auto const& valid = info.collectionNodeToReplace->getVarsValid();
  auto checkVars = [&valid](AstNode const* expr) {
    if (expr != nullptr) {
      VarSet varsUsed;
      Ast::getReferencedVariables(expr, varsUsed);
      for (Variable const* v : varsUsed) {
        if (valid.find(v) == valid.end()) {
          return false;  // invalid variable foud
        }
      }
    }
    return true;
  };
  if (!checkVars(info.distCenterExpr) || !checkVars(info.distCenterLatExpr) ||
      !checkVars(info.distCenterLngExpr) || !checkVars(info.filterExpr)) {
    return false;
  }

  size_t limit = 0;
  if (ln != nullptr) {
    limit = ln->offset() + ln->limit();
    TRI_ASSERT(limit != SIZE_MAX);
  }

  IndexIteratorOptions opts;
  opts.sorted = info.sorted;
  opts.ascending = info.ascending;
  opts.limit = limit;
  opts.evaluateFCalls = false;  // workaround to avoid evaluating "doc.geo"
  std::unique_ptr<Condition> condition(buildGeoCondition(plan, info));
  auto inode = plan->createNode<IndexNode>(
      plan, plan->nextId(), info.collection, info.collectionNodeOutVar,
      std::vector<transaction::Methods::IndexHandle>{
          transaction::Methods::IndexHandle{info.index}},
      false,  // here we are not using inverted index so
              // for sure no "whole" coverage
      std::move(condition), opts);
  plan->replaceNode(info.collectionNodeToReplace, inode);

  // remove expressions covered by our index
  Ast* ast = plan->getAst();
  for (std::pair<ExecutionNode*, Expression*> pair : info.exesToModify) {
    AstNode* root = pair.second->nodeForModification();
    auto pre = [&](AstNode const* node) -> bool {
      return node == root || Ast::isAndOperatorType(node->type);
    };
    auto visitor = [&](AstNode* node) -> AstNode* {
      if (Ast::isAndOperatorType(node->type)) {
        std::vector<AstNode*> keep;  // always shallow copy node
        for (std::size_t i = 0; i < node->numMembers(); i++) {
          AstNode* child = node->getMemberUnchecked(i);
          if (info.nodesToRemove.find(child) == info.nodesToRemove.end()) {
            keep.push_back(child);
          }
        }

        if (keep.size() > 2) {
          AstNode* n = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
          for (size_t i = 0; i < keep.size(); i++) {
            n->addMember(keep[i]);
          }
          return n;
        } else if (keep.size() == 2) {
          return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                               keep[0], keep[1]);
        } else if (keep.size() == 1) {
          return keep[0];
        }
        return node == root ? nullptr : ast->createNodeValueBool(true);
      } else if (info.nodesToRemove.find(node) != info.nodesToRemove.end()) {
        return node == root ? nullptr : ast->createNodeValueBool(true);
      }
      return node;
    };
    auto post = [](AstNode const*) {};
    AstNode* newNode = Ast::traverseAndModify(root, pre, visitor, post);
    if (newNode == nullptr) {  // if root was removed, unlink FILTER or SORT
      plan->unlinkNode(pair.first);
    } else if (newNode != root) {
      pair.second->replaceNode(newNode);
    }
  }

  // signal that plan has been changed
  return true;
}

void arangodb::aql::geoIndexRule(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  bool mod = false;

  plan->findNodesOfType(nodes, EN::ENUMERATE_COLLECTION, true);
  for (ExecutionNode* node : nodes) {
    GeoIndexInfo info;
    info.collectionNodeToReplace = node;

    ExecutionNode* current = node->getFirstParent();
    LimitNode* limit = nullptr;
    bool canUseSortLimit = true;
    bool mustRespectIdxHint = false;
    auto enumerateColNode =
        ExecutionNode::castTo<EnumerateCollectionNode const*>(node);
    auto const& colNodeHints = enumerateColNode->hint();
    if (colNodeHints.isForced() && colNodeHints.isSimple()) {
      auto indexes = enumerateColNode->collection()->indexes();
      auto& idxNames = colNodeHints.candidateIndexes();
      for (auto const& idxName : idxNames) {
        for (std::shared_ptr<Index> const& idx : indexes) {
          if (idx->name() == idxName) {
            auto idxType = idx->type();
            if ((idxType != Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX) &&
                (idxType != Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX) &&
                (idxType != Index::IndexType::TRI_IDX_TYPE_GEO_INDEX)) {
              mustRespectIdxHint = true;
            } else {
              info.index = idx;
            }
            break;
          }
        }
      }
    }

    while (current) {
      if (current->getType() == EN::FILTER) {
        // picking up filter conditions is always allowed
        optimizeFilterNode(plan.get(),
                           ExecutionNode::castTo<FilterNode*>(current), info);
      } else if (current->getType() == EN::SORT && canUseSortLimit) {
        // only pick up a sort clause if we haven't seen another loop yet
        if (!optimizeSortNode(
                plan.get(), ExecutionNode::castTo<SortNode*>(current), info)) {
          // 1. EnumerateCollectionNode x
          // 2. SortNode x.abc ASC
          // 3. LimitNode n,m  <-- cannot reuse LIMIT node here
          // limit = nullptr;
          break;  // stop parsing on non-optimizable SORT
        }
      } else if (current->getType() == EN::LIMIT && canUseSortLimit) {
        // only pick up a limit clause if we haven't seen another loop yet
        limit = ExecutionNode::castTo<LimitNode*>(current);
        break;  // stop parsing after first LIMIT
      } else if (current->getType() == EN::RETURN ||
                 current->getType() == EN::COLLECT) {
        break;  // stop parsing on return or collect
      } else if (current->getType() == EN::INDEX ||
                 current->getType() == EN::ENUMERATE_COLLECTION ||
                 current->getType() == EN::ENUMERATE_LIST ||
                 current->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
                 current->getType() == EN::TRAVERSAL ||
                 current->getType() == EN::ENUMERATE_PATHS ||
                 current->getType() == EN::SHORTEST_PATH) {
        // invalidate limit and sort. filters can still be used
        limit = nullptr;
        info.sorted = false;
        // don't allow picking up either sort or limit from here on
        canUseSortLimit = false;
      }
      current = current->getFirstParent();  // inspect next node
    }

    // if info is valid we try to optimize ENUMERATE_COLLECTION
    if (info && info.collectionNodeToReplace == node) {
      if (!mustRespectIdxHint &&
          applyGeoOptimization(plan.get(), limit, info)) {
        mod = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, mod);
}

void arangodb::aql::optimizeSubqueriesRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  // value type is {limit value, referenced by, used for counting}
  std::unordered_map<
      ExecutionNode*,
      std::tuple<int64_t, std::unordered_set<ExecutionNode const*>, bool>>
      subqueryAttributes;

  for (auto const& n : nodes) {
    auto cn = ExecutionNode::castTo<CalculationNode*>(n);
    AstNode const* root = cn->expression()->node();
    if (root == nullptr) {
      continue;
    }

    auto visitor = [&subqueryAttributes, &plan,
                    n](AstNode const* node) -> bool {
      std::pair<ExecutionNode*, int64_t> found{nullptr, 0};
      bool usedForCount = false;

      if (node->type == NODE_TYPE_REFERENCE) {
        Variable const* v = ast::ReferenceNode(node).getVariable();
        auto setter = plan->getVarSetBy(v->id);
        if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
          // we found a subquery result being used somehow in some
          // way that will make the optimization produce wrong results
          found.first = setter;
          found.second = -1;  // negative values will disable the optimization
        }
      } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
        auto sub = node->getMemberUnchecked(0);
        if (sub->type == NODE_TYPE_REFERENCE) {
          Variable const* v = ast::ReferenceNode(sub).getVariable();
          auto setter = plan->getVarSetBy(v->id);
          auto index = node->getMemberUnchecked(1);
          if (index->type == NODE_TYPE_VALUE && index->isNumericValue() &&
              setter != nullptr && setter->getType() == EN::SUBQUERY) {
            found.first = setter;
            found.second = index->getIntValue() + 1;  // x[0] => LIMIT 1
            if (found.second <= 0) {
              // turn optimization off
              found.second = -1;
            }
          }
        }
      } else if (node->type == NODE_TYPE_FCALL && node->numMembers() > 0) {
        ast::FunctionCallNode fcall(node);
        auto func = fcall.getFunction();
        auto args = fcall.getArguments();
        if (func->name == "FIRST" || func->name == "LENGTH" ||
            func->name == "COUNT") {
          if (args->numMembers() > 0 &&
              args->getMemberUnchecked(0)->type == NODE_TYPE_REFERENCE) {
            ast::ReferenceNode ref(args->getMemberUnchecked(0));
            Variable const* v = ref.getVariable();
            auto setter = plan->getVarSetBy(v->id);
            if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
              found.first = setter;
              if (func->name == "FIRST") {
                found.second = 1;  // FIRST(x) => LIMIT 1
              } else {
                found.second = -1;
                usedForCount = true;
              }
            }
          }
        }
      }

      if (found.first != nullptr) {
        auto it = subqueryAttributes.find(found.first);
        if (it == subqueryAttributes.end()) {
          subqueryAttributes.try_emplace(
              found.first,
              std::make_tuple(found.second,
                              std::unordered_set<ExecutionNode const*>{n},
                              usedForCount));
        } else {
          auto& sq = (*it).second;
          if (usedForCount) {
            // COUNT + LIMIT together will turn off the optimization
            std::get<2>(sq) = (std::get<0>(sq) <= 0);
            std::get<0>(sq) = -1;
            std::get<1>(sq).clear();
          } else {
            if (found.second <= 0 || std::get<0>(sq) < 0) {
              // negative value will turn off the optimization
              std::get<0>(sq) = -1;
              std::get<1>(sq).clear();
            } else {
              // otherwise, use the maximum of the limits needed, and insert
              // current node into our "safe" list
              std::get<0>(sq) = std::max(std::get<0>(sq), found.second);
              std::get<1>(sq).emplace(n);
            }
            std::get<2>(sq) = false;
          }
        }
        // don't descend further
        return false;
      }

      // descend further
      return true;
    };

    Ast::traverseReadOnly(root, visitor, [](AstNode const*) {});
  }

  for (auto const& it : subqueryAttributes) {
    ExecutionNode* node = it.first;
    TRI_ASSERT(node->getType() == EN::SUBQUERY);
    auto sn = ExecutionNode::castTo<SubqueryNode const*>(node);

    if (sn->isModificationNode()) {
      // cannot push a LIMIT into data-modification subqueries
      continue;
    }

    auto const& sq = it.second;
    int64_t limitValue = std::get<0>(sq);
    bool usedForCount = std::get<2>(sq);
    if (limitValue <= 0 && !usedForCount) {
      // optimization turned off
      continue;
    }

    // scan from the subquery node to the bottom of the ExecutionPlan to check
    // if any of the following nodes also use the subquery result
    auto out = sn->outVariable();
    VarSet used;
    bool invalid = false;

    auto current = node->getFirstParent();
    while (current != nullptr) {
      auto const& referencedBy = std::get<1>(sq);
      if (referencedBy.find(current) == referencedBy.end()) {
        // node not found in "safe" list
        // now check if it uses the subquery's out variable
        used.clear();
        current->getVariablesUsedHere(used);
        if (used.find(out) != used.end()) {
          invalid = true;
          break;
        }
      }
      // continue iteration
      current = current->getFirstParent();
    }

    if (invalid) {
      continue;
    }

    auto root = sn->getSubquery();
    if (root != nullptr && root->getType() == EN::RETURN) {
      // now inject a limit
      auto f = root->getFirstDependency();
      TRI_ASSERT(f != nullptr);

      if (std::get<2>(sq)) {
        Ast* ast = plan->getAst();
        // generate a calculation node that only produces "true"
        auto expr =
            std::make_unique<Expression>(ast, ast->createNodeValueBool(true));
        Variable* outVariable = ast->variables()->createTemporaryVariable();
        auto calcNode = plan->createNode<CalculationNode>(
            plan.get(), plan->nextId(), std::move(expr), outVariable);
        plan->insertAfter(f, calcNode);
        // change the result value of the existing Return node
        TRI_ASSERT(root->getType() == EN::RETURN);
        ExecutionNode::castTo<ReturnNode*>(root)->inVariable(outVariable);
        modified = true;
        continue;
      }

      if (f->getType() == EN::LIMIT) {
        // subquery already has a LIMIT node at its end
        // no need to do anything
        continue;
      }

      auto limitNode = plan->createNode<LimitNode>(plan.get(), plan->nextId(),
                                                   0, limitValue);
      plan->insertAfter(f, limitNode);
      modified = true;
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

/// @brief turn LENGTH(FOR doc IN ...) subqueries into an optimized count
/// operation
void arangodb::aql::optimizeCountRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  bool modified = false;

  if (plan->getAst()->query().queryOptions().fullCount) {
    // fullCount is unsupported yet
    opt->addPlan(std::move(plan), rule, modified);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  VarSet vars;
  std::unordered_map<ExecutionNode*,
                     std::pair<bool, std::unordered_set<AstNode const*>>>
      candidates;

  // find all calculation nodes in the plan
  for (auto const& n : nodes) {
    auto cn = ExecutionNode::castTo<CalculationNode*>(n);
    AstNode const* root = cn->expression()->node();
    if (root == nullptr) {
      continue;
    }

    std::unordered_map<ExecutionNode*,
                       std::pair<bool, std::unordered_set<AstNode const*>>>
        localCandidates;

    // look for all expressions that contain COUNT(subquery) or LENGTH(subquery)
    auto visitor = [&localCandidates, &plan](AstNode const* node) -> bool {
      if (node->type == NODE_TYPE_FCALL && node->numMembers() > 0) {
        ast::FunctionCallNode fcall(node);
        auto func = fcall.getFunction();
        auto args = fcall.getArguments();
        if (func->name == "LENGTH" || func->name == "COUNT") {
          if (args->numMembers() > 0 &&
              args->getMemberUnchecked(0)->type == NODE_TYPE_REFERENCE) {
            ast::ReferenceNode ref(args->getMemberUnchecked(0));
            Variable const* v = ref.getVariable();
            auto setter = plan->getVarSetBy(v->id);
            if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
              // COUNT(subquery) / LENGTH(subquery)
              auto sn = ExecutionNode::castTo<SubqueryNode*>(setter);
              if (sn->isModificationNode()) {
                // subquery modifies data
                // cannot apply optimization for data-modification queries
                return true;
              }
              if (!sn->isDeterministic()) {
                // subquery is non-deterministic. cannot apply the optimization
                return true;
              }

              auto current = sn->getSubquery();
              if (current == nullptr || current->getType() != EN::RETURN) {
                // subquery does not end with a RETURN instruction - we cannot
                // handle this
                return true;
              }

              auto it = localCandidates.find(setter);
              if (it == localCandidates.end()) {
                localCandidates.emplace(
                    setter,
                    std::make_pair(true,
                                   std::unordered_set<AstNode const*>({node})));
              } else {
                (*it).second.second.emplace(node);
              }
              return false;
            }
          }
        }
      } else if (node->type == NODE_TYPE_REFERENCE) {
        Variable const* v = ast::ReferenceNode(node).getVariable();
        auto setter = plan->getVarSetBy(v->id);
        if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
          // subquery used for something else inside the calculation,
          // e.g. FIRST(subquery).
          // we cannot continue with the optimization for this subquery, but for
          // others
          localCandidates[setter].first = false;
          return false;
        }
      }
      return true;
    };

    Ast::traverseReadOnly(root, visitor, [](AstNode const*) {});

    for (auto const& it : localCandidates) {
      // check if subquery result is used for something else than LENGTH/COUNT
      // in *this* calculation
      if (!it.second.first) {
        // subquery result is used for other calculations than COUNT(subquery)
        continue;
      }

      SubqueryNode const* sn =
          ExecutionNode::castTo<SubqueryNode const*>(it.first);
      if (n->isVarUsedLater(sn->outVariable())) {
        // subquery result is used elsewhere later - we cannot optimize
        continue;
      }

      bool valid = true;
      // check if subquery result is used somewhere else before the current
      // calculation we are looking at
      auto current = sn->getFirstParent();
      while (current != nullptr && current != n) {
        vars.clear();
        current->getVariablesUsedHere(vars);
        if (vars.find(sn->outVariable()) != vars.end()) {
          valid = false;
          break;
        }
        current = current->getFirstParent();
      }

      if (valid) {
        // subquery result is not used elsewhere - we can continue optimizing
        // transfer the candidate into the global result
        candidates.emplace(it.first, it.second);
      }
    }
  }

  for (auto const& it : candidates) {
    TRI_ASSERT(it.second.first);
    auto sn = ExecutionNode::castTo<SubqueryNode const*>(it.first);

    // scan from the subquery node to the bottom of the ExecutionPlan to check
    // if any of the following nodes also use the subquery result
    auto current = sn->getSubquery();
    TRI_ASSERT(current->getType() == EN::RETURN);
    auto returnNode = ExecutionNode::castTo<ReturnNode*>(current);
    auto returnSetter = plan->getVarSetBy(returnNode->inVariable()->id);
    if (returnSetter == nullptr) {
      continue;
    }
    if (returnSetter->getType() == EN::CALCULATION) {
      // check if we can understand this type of calculation
      auto cn = ExecutionNode::castTo<CalculationNode*>(returnSetter);
      auto expr = cn->expression();
      if (!expr->isConstant() && !expr->isAttributeAccess()) {
        continue;
      }
    }

    // find the head of the plan/subquery
    while (current->hasDependency()) {
      current = current->getFirstDependency();
    }

    TRI_ASSERT(current != nullptr);

    if (current->getType() != EN::SINGLETON) {
      continue;
    }

    // from here we need to find the first FOR loop.
    // if it is a full collection scan or an index scan, we note its out
    // variable. if we find a nested loop, we abort searching
    bool valid = true;
    ExecutionNode* found = nullptr;
    Variable const* outVariable = nullptr;
    current = current->getFirstParent();

    while (current != nullptr) {
      auto type = current->getType();
      switch (type) {
        case EN::ENUMERATE_COLLECTION:
        case EN::INDEX: {
          if (found != nullptr) {
            // found a nested collection/index scan
            found = nullptr;
            valid = false;
          } else {
            TRI_ASSERT(valid);
            if (dynamic_cast<DocumentProducingNode*>(current)->hasFilter()) {
              // node uses early pruning. this is not supported
              valid = false;
            } else {
              outVariable =
                  dynamic_cast<DocumentProducingNode*>(current)->outVariable();

              if (type == EN::INDEX &&
                  ExecutionNode::castTo<IndexNode const*>(current)
                          ->getIndexes()
                          .size() != 1) {
                // more than one index, so we would need to run uniqueness
                // checks on the results. this is currently unsupported, so
                // don't apply the optimization
                valid = false;
              } else {
                // a FOR loop without an early pruning filter. this is what we
                // are looking for!
                found = current;
              }
            }
          }
          break;
        }

        case EN::DISTRIBUTE:

        case EN::INSERT:
        case EN::UPDATE:
        case EN::REPLACE:
        case EN::REMOVE:
        case EN::UPSERT:
          // we don't handle data-modification queries

        case EN::LIMIT:
          // limit is not yet supported

        case EN::ENUMERATE_LIST:
        case EN::TRAVERSAL:
        case EN::SHORTEST_PATH:
        case EN::ENUMERATE_PATHS:
        case EN::ENUMERATE_IRESEARCH_VIEW: {
          // we don't handle nested FOR loops
          found = nullptr;
          valid = false;
          break;
        }

        case EN::RETURN: {
          // we reached the end
          break;
        }

        default: {
          if (outVariable != nullptr) {
            vars.clear();
            current->getVariablesUsedHere(vars);
            if (vars.find(outVariable) != vars.end()) {
              // result variable of FOR loop is used somewhere where we
              // can't handle it - don't apply the optimization
              found = nullptr;
              valid = false;
            }
          }
          break;
        }
      }

      if (!valid) {
        break;
      }

      current = current->getFirstParent();
    }

    if (valid && found != nullptr) {
      dynamic_cast<DocumentProducingNode*>(found)->setCountFlag();
      returnNode->inVariable(outVariable);

      // replace COUNT/LENGTH with SUM, as we are getting an array from the
      // subquery
      auto& server = plan->getAst()->query().vocbase().server();
      auto func = server.getFeature<AqlFunctionFeature>().byName("SUM");
      for (AstNode const* funcNode : it.second.second) {
        const_cast<AstNode*>(funcNode)->setData(static_cast<void const*>(func));
      }

      if (returnSetter->getType() == EN::CALCULATION) {
        plan->clearVarUsageComputed();
        plan->findVarUsage();

        auto cn = ExecutionNode::castTo<CalculationNode*>(returnSetter);
        if (cn->expression()->isConstant() &&
            !cn->isVarUsedLater(cn->outVariable())) {
          plan->unlinkNode(cn);
        }
      }
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

void arangodb::aql::asyncPrefetchRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  struct AsyncPrefetchChecker : WalkerWorkerBase<ExecutionNode> {
    bool before(ExecutionNode* n) override {
      auto eligibility = n->canUseAsyncPrefetching();
      if (eligibility == AsyncPrefetchEligibility::kDisableGlobally) {
        // found a node that we can't support -> abort
        eligible = false;
        return true;
      }
      return false;
    }

    bool eligible{true};
  };

  struct AsyncPrefetchEnabler : WalkerWorkerBase<ExecutionNode> {
    AsyncPrefetchEnabler() { stack.emplace_back(0); }

    bool before(ExecutionNode* n) override {
      auto eligibility = n->canUseAsyncPrefetching();
      if (eligibility == AsyncPrefetchEligibility::kDisableGlobally) {
        // found a node that we can't support -> abort
        TRI_ASSERT(!modified);
        return true;
      }
      if (eligibility ==
          AsyncPrefetchEligibility::kDisableForNodeAndDependencies) {
        TRI_ASSERT(!stack.empty());
        ++stack.back();
      }
      return false;
    }

    void after(ExecutionNode* n) override {
      TRI_ASSERT(!stack.empty());
      auto eligibility = n->canUseAsyncPrefetching();
      if (stack.back() == 0 &&
          eligibility == AsyncPrefetchEligibility::kEnableForNode) {
        // we are currently excluding any node inside a subquery.
        // TODO: lift this restriction.
        n->setIsAsyncPrefetchEnabled(true);
        modified = true;
      }
      if (eligibility ==
          AsyncPrefetchEligibility::kDisableForNodeAndDependencies) {
        TRI_ASSERT(stack.back() > 0);
        --stack.back();
      }
    }

    bool enterSubquery(ExecutionNode*, ExecutionNode*) override {
      // this will disable the optimization for subqueries right now
      stack.push_back(1);
      return true;
    }

    void leaveSubquery(ExecutionNode*, ExecutionNode*) override {
      TRI_ASSERT(!stack.empty());
      stack.pop_back();
    }

    // per query-level (main query, subqueries) stack of eligibilities
    containers::SmallVector<uint32_t, 4> stack;
    bool modified{false};
  };

  bool modified = false;
  // first check if the query satisfies all constraints we have for
  // async prefetching
  AsyncPrefetchChecker checker;
  plan->root()->walk(checker);

  if (checker.eligible) {
    // only if it does, start modifying nodes in the query
    AsyncPrefetchEnabler enabler;
    plan->root()->walk(enabler);
    modified = enabler.modified;
    if (modified) {
      plan->getAst()->setContainsAsyncPrefetch();
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}

void arangodb::aql::activateCallstackSplit(ExecutionPlan& plan) {
  if (willUseV8(plan)) {
    // V8 requires thread local context configuration, so we cannot
    // use our thread based split solution...
    return;
  }

  auto const& options = plan.getAst()->query().queryOptions();
  struct CallstackSplitter : WalkerWorkerBase<ExecutionNode> {
    explicit CallstackSplitter(size_t maxNodes)
        : maxNodesPerCallstack(maxNodes) {}
    bool before(ExecutionNode* n) override {
      // This rule must be executed after subquery splicing, so we must not see
      // any subqueries here!
      TRI_ASSERT(n->getType() != EN::SUBQUERY);

      if (n->getType() == EN::REMOTE) {
        // RemoteNodes provide a natural split in the callstack, so we can reset
        // the counter here!
        count = 0;
      } else if (++count >= maxNodesPerCallstack) {
        count = 0;
        n->enableCallstackSplit();
      }
      return false;
    }
    size_t maxNodesPerCallstack;
    size_t count = 0;
  };

  CallstackSplitter walker(options.maxNodesPerCallstack);
  plan.root()->walk(walker);
}

namespace {

void findSubqueriesSuitableForSplicing(
    ExecutionPlan const& plan,
    containers::SmallVector<SubqueryNode*, 8>& result) {
  TRI_ASSERT(result.empty());
  using ResultVector = decltype(result);

  using SuitableNodeSet =
      std::set<SubqueryNode*, std::less<>,
               containers::detail::short_alloc<SubqueryNode*, 128,
                                               alignof(SubqueryNode*)>>;

  // This finder adds all subquery nodes in pre-order to its `result` parameter,
  // and all nodes that are suitable for splicing to `suitableNodes`. Suitable
  // means that neither the containing subquery contains unsuitable nodes - at
  // least not in an ancestor of the subquery - nor the subquery contains
  // unsuitable nodes (directly, not recursively).
  //
  // It will be used in a fashion where the recursive walk on subqueries is done
  // *before* the recursive walk on dependencies.
  // It maintains a stack of bools for every subquery level. The topmost bool
  // holds whether we've encountered a skipping block so far.
  // When leaving a subquery, we decide whether it is suitable for splicing by
  // inspecting the two topmost bools in the stack - the one belonging to the
  // insides of the subquery, which we're going to pop right now, and the one
  // belonging to the containing subquery.
  //
  // *All* subquery nodes will be added to &result in pre-order, and all
  // *suitable* subquery nodes will be added to &suitableNodes. The latter can
  // be omitted later, as soon as support for spliced subqueries / shadow rows
  // is complete.

  class Finder final
      : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
   public:
    explicit Finder(ResultVector& result, SuitableNodeSet& suitableNodes)
        : _result{result}, _suitableNodes{suitableNodes} {}

    bool before(ExecutionNode* node) final {
      TRI_ASSERT(node->getType() != EN::MUTEX);  // should never appear here
      if (node->getType() == ExecutionNode::SUBQUERY) {
        _result.emplace_back(ExecutionNode::castTo<SubqueryNode*>(node));
      }
      return false;
    }

    bool enterSubquery(ExecutionNode*, ExecutionNode*) final {
      ++_isSuitableLevel;
      return true;
    }

    void leaveSubquery(ExecutionNode* subQuery, ExecutionNode*) final {
      TRI_ASSERT(_isSuitableLevel != 0);
      --_isSuitableLevel;
      _suitableNodes.emplace(ExecutionNode::castTo<SubqueryNode*>(subQuery));
    }

   private:
    // all subquery nodes will be added to _result in pre-order
    ResultVector& _result;
    // only suitable subquery nodes will be added to this set
    SuitableNodeSet& _suitableNodes;
    size_t _isSuitableLevel{1};  // push the top-level query
  };

  using SuitableNodeArena = SuitableNodeSet::allocator_type::arena_type;
  SuitableNodeArena suitableNodeArena{};
  SuitableNodeSet suitableNodes{suitableNodeArena};

  auto finder = Finder{result, suitableNodes};
  plan.root()->walkSubqueriesFirst(finder);

  {  // remove unsuitable nodes from result
    auto i = size_t{0};
    auto j = size_t{0};
    for (; j < result.size(); ++j) {
      TRI_ASSERT(i <= j);
      if (suitableNodes.count(result[j]) > 0) {
        if (i != j) {
          TRI_ASSERT(suitableNodes.count(result[i]) == 0);
          result[i] = result[j];
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          // To allow for the assert above
          result[j] = nullptr;
#endif
        }
        ++i;
      }
    }
    TRI_ASSERT(i <= result.size());
    result.resize(i);
  }
}
}  // namespace

// Splices in subqueries by replacing subquery nodes by
// a SubqueryStartNode and a SubqueryEndNode with the subquery's nodes
// in between.
void arangodb::aql::spliceSubqueriesRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<SubqueryNode*, 8> subqueryNodes;
  findSubqueriesSuitableForSplicing(*plan, subqueryNodes);

  // Note that we rely on the order `subqueryNodes` in the sense that, for
  // nested subqueries, the outer subquery must come before the inner, so we
  // don't iterate over spliced queries here.
  auto forAllDeps = [](ExecutionNode* node, auto cb) {
    for (; node != nullptr; node = node->getFirstDependency()) {
      TRI_ASSERT(node->getType() != ExecutionNode::SUBQUERY_START);
      TRI_ASSERT(node->getType() != ExecutionNode::SUBQUERY_END);
      cb(node);
    }
  };

  for (auto const& sq : subqueryNodes) {
    modified = true;
    auto evenNumberOfRemotes = true;

    forAllDeps(sq->getSubquery(), [&](auto node) {
      node->setIsInSplicedSubquery(true);
      if (node->getType() == ExecutionNode::REMOTE) {
        evenNumberOfRemotes = !evenNumberOfRemotes;
      }
    });

    auto const addClusterNodes = !evenNumberOfRemotes;

    {  // insert SubqueryStartNode

      // Create new start node
      auto start = plan->createNode<SubqueryStartNode>(
          plan.get(), plan->nextId(), sq->outVariable());

      // start and end inherit this property from the subquery node
      start->setIsInSplicedSubquery(sq->isInSplicedSubquery());

      // insert a SubqueryStartNode before the SubqueryNode
      plan->insertBefore(sq, start);
      // remove parent/dependency relation between sq and start
      TRI_ASSERT(start->getParents().size() == 1);
      sq->removeDependency(start);
      TRI_ASSERT(start->getParents().size() == 0);
      TRI_ASSERT(start->getDependencies().size() == 1);
      TRI_ASSERT(sq->getDependencies().size() == 0);
      TRI_ASSERT(sq->getParents().size() == 1);

      {  // remove singleton
        ExecutionNode* const singleton = sq->getSubquery()->getSingleton();
        std::vector<ExecutionNode*> const parents = singleton->getParents();
        TRI_ASSERT(parents.size() == 1);
        auto oldSingletonParent = parents[0];
        TRI_ASSERT(oldSingletonParent->getDependencies().size() == 1);
        // All parents of the Singleton of the subquery become parents of the
        // SubqueryStartNode. The singleton will be deleted after.
        for (auto* x : parents) {
          TRI_ASSERT(x != nullptr);
          x->replaceDependency(singleton, start);
        }
        TRI_ASSERT(oldSingletonParent->getDependencies().size() == 1);
        TRI_ASSERT(start->getParents().size() == 1);

        if (addClusterNodes) {
          auto const scatterNode = plan->createNode<ScatterNode>(
              plan.get(), plan->nextId(), ScatterNode::SHARD);
          auto const remoteNode = plan->createNode<RemoteNode>(
              plan.get(), plan->nextId(), &plan->getAst()->query().vocbase(),
              "", "", "");
          scatterNode->setIsInSplicedSubquery(true);
          remoteNode->setIsInSplicedSubquery(true);
          plan->insertAfter(start, scatterNode);
          plan->insertAfter(scatterNode, remoteNode);

          TRI_ASSERT(remoteNode->getDependencies().size() == 1);
          TRI_ASSERT(scatterNode->getDependencies().size() == 1);
          TRI_ASSERT(remoteNode->getParents().size() == 1);
          TRI_ASSERT(scatterNode->getParents().size() == 1);
          TRI_ASSERT(oldSingletonParent->getFirstDependency() == remoteNode);
          TRI_ASSERT(remoteNode->getFirstDependency() == scatterNode);
          TRI_ASSERT(scatterNode->getFirstDependency() == start);
          TRI_ASSERT(start->getFirstParent() == scatterNode);
          TRI_ASSERT(scatterNode->getFirstParent() == remoteNode);
          TRI_ASSERT(remoteNode->getFirstParent() == oldSingletonParent);
        } else {
          TRI_ASSERT(oldSingletonParent->getFirstDependency() == start);
          TRI_ASSERT(start->getFirstParent() == oldSingletonParent);
        }
      }
    }

    {  // insert SubqueryEndNode

      ExecutionNode* subqueryRoot = sq->getSubquery();
      Variable const* inVariable = nullptr;

      if (subqueryRoot->getType() == ExecutionNode::RETURN) {
        // The SubqueryEndExecutor can read the input from the return Node.
        auto subqueryReturn = ExecutionNode::castTo<ReturnNode*>(subqueryRoot);
        inVariable = subqueryReturn->inVariable();
        // Every return can only have a single dependency
        TRI_ASSERT(subqueryReturn->getDependencies().size() == 1);
        subqueryRoot = subqueryReturn->getFirstDependency();
        TRI_ASSERT(!plan->isRoot(subqueryReturn));
        plan->unlinkNode(subqueryReturn, true);
      }

      // Create new end node
      auto end = plan->createNode<SubqueryEndNode>(
          plan.get(), plan->nextId(), inVariable, sq->outVariable());
      // start and end inherit this property from the subquery node
      end->setIsInSplicedSubquery(sq->isInSplicedSubquery());
      // insert a SubqueryEndNode after the SubqueryNode sq
      plan->insertAfter(sq, end);

      end->replaceDependency(sq, subqueryRoot);

      TRI_ASSERT(end->getDependencies().size() == 1);
      TRI_ASSERT(end->getParents().size() == 1);
    }
    TRI_ASSERT(sq->getDependencies().empty());
    TRI_ASSERT(sq->getParents().empty());
  }

  if (modified) {
    plan->root()->invalidateCost();
  }
  opt->addPlan(std::move(plan), rule, modified);
}

enum class DistributeType { DOCUMENT, TRAVERSAL, PATH };

void arangodb::aql::insertDistributeInputCalculation(ExecutionPlan& plan) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan.findNodesOfType(nodes, ExecutionNode::DISTRIBUTE, true);

  for (auto const& n : nodes) {
    auto* distributeNode = ExecutionNode::castTo<DistributeNode*>(n);
    auto* targetNode =
        plan.getNodesById().at(distributeNode->getTargetNodeId());
    TRI_ASSERT(targetNode != nullptr);

    auto collection = static_cast<Collection const*>(nullptr);
    auto inputVariable = static_cast<Variable const*>(nullptr);
    auto alternativeVariable = static_cast<Variable const*>(nullptr);

    auto createKeys = bool{false};
    auto allowKeyConversionToObject = bool{false};
    auto allowSpecifiedKeys = bool{false};
    bool canProjectOnlyId{false};

    DistributeType fixupGraphInput = DistributeType::DOCUMENT;

    std::function<void(Variable * variable)> setInVariable;
    std::function<void(Variable * variable)> setTargetVariable;
    std::function<void(Variable * variable)> setDistributeVariable;
    bool ignoreErrors = false;

    // TODO: this seems a bit verbose, but is at least local & simple
    //       the modification nodes are all collectionaccessing, the graph nodes
    //       are currently assumed to be disjoint, and hence smart, so all
    //       collections are sharded the same way!
    switch (targetNode->getType()) {
      case ExecutionNode::INSERT: {
        auto* insertNode = ExecutionNode::castTo<InsertNode*>(targetNode);
        collection = insertNode->collection();
        inputVariable = insertNode->inVariable();
        createKeys = true;
        allowKeyConversionToObject = true;
        setInVariable = [insertNode](Variable* var) {
          insertNode->setInVariable(var);
        };

        alternativeVariable = insertNode->oldSmartGraphVariable();
        if (alternativeVariable != nullptr) {
          canProjectOnlyId = true;
        }

      } break;
      case ExecutionNode::REMOVE: {
        auto* removeNode = ExecutionNode::castTo<RemoveNode*>(targetNode);
        collection = removeNode->collection();
        inputVariable = removeNode->inVariable();
        createKeys = false;
        allowKeyConversionToObject = true;
        ignoreErrors = removeNode->getOptions().ignoreErrors;
        setInVariable = [removeNode](Variable* var) {
          removeNode->setInVariable(var);
        };
      } break;
      case ExecutionNode::UPDATE:
      case ExecutionNode::REPLACE: {
        auto* updateReplaceNode =
            ExecutionNode::castTo<UpdateReplaceNode*>(targetNode);
        collection = updateReplaceNode->collection();
        ignoreErrors = updateReplaceNode->getOptions().ignoreErrors;
        if (updateReplaceNode->inKeyVariable() != nullptr) {
          inputVariable = updateReplaceNode->inKeyVariable();
          // This is the _inKeyVariable! This works, since we use default
          // sharding!
          allowKeyConversionToObject = true;
          setInVariable = [updateReplaceNode](Variable* var) {
            updateReplaceNode->setInKeyVariable(var);
          };
        } else {
          inputVariable = updateReplaceNode->inDocVariable();
          allowKeyConversionToObject = false;
          setInVariable = [updateReplaceNode](Variable* var) {
            updateReplaceNode->setInDocVariable(var);
          };
        }
        createKeys = false;
      } break;
      case ExecutionNode::UPSERT: {
        // an UPSERT node has two input variables!
        auto* upsertNode = ExecutionNode::castTo<UpsertNode*>(targetNode);
        collection = upsertNode->collection();
        inputVariable = upsertNode->inDocVariable();
        alternativeVariable = upsertNode->insertVariable();
        ignoreErrors = upsertNode->getOptions().ignoreErrors;
        allowKeyConversionToObject = true;
        createKeys = true;
        allowSpecifiedKeys = true;
        setInVariable = [upsertNode](Variable* var) {
          upsertNode->setInsertVariable(var);
        };
      } break;
      case ExecutionNode::TRAVERSAL: {
        auto* traversalNode = ExecutionNode::castTo<TraversalNode*>(targetNode);
        TRI_ASSERT(traversalNode->isDisjoint());
        collection = traversalNode->collection();
        inputVariable = traversalNode->inVariable();
        allowKeyConversionToObject = true;
        createKeys = false;
        fixupGraphInput = DistributeType::TRAVERSAL;
        setInVariable = [traversalNode](Variable* var) {
          traversalNode->setInVariable(var);
        };
      } break;
      case ExecutionNode::ENUMERATE_PATHS: {
        auto* pathsNode =
            ExecutionNode::castTo<EnumeratePathsNode*>(targetNode);
        TRI_ASSERT(pathsNode->isDisjoint());
        collection = pathsNode->collection();
        // Subtle: EnumeratePathsNode uses a reference when returning
        // startInVariable
        TRI_ASSERT(pathsNode->usesStartInVariable());
        inputVariable = &pathsNode->startInVariable();
        TRI_ASSERT(pathsNode->usesTargetInVariable());
        alternativeVariable = &pathsNode->targetInVariable();
        allowKeyConversionToObject = true;
        createKeys = false;
        fixupGraphInput = DistributeType::PATH;
        setInVariable = [pathsNode](Variable* var) {
          pathsNode->setStartInVariable(var);
        };
        setTargetVariable = [pathsNode](Variable* var) {
          pathsNode->setTargetInVariable(var);
        };
        setDistributeVariable = [pathsNode](Variable* var) {
          pathsNode->setDistributeVariable(var);
        };
      } break;
      case ExecutionNode::SHORTEST_PATH: {
        auto* shortestPathNode =
            ExecutionNode::castTo<ShortestPathNode*>(targetNode);
        TRI_ASSERT(shortestPathNode->isDisjoint());
        collection = shortestPathNode->collection();
        TRI_ASSERT(shortestPathNode->usesStartInVariable());
        inputVariable = shortestPathNode->startInVariable();
        TRI_ASSERT(shortestPathNode->usesTargetInVariable());
        alternativeVariable = shortestPathNode->targetInVariable();
        allowKeyConversionToObject = true;
        createKeys = false;
        fixupGraphInput = DistributeType::PATH;
        setInVariable = [shortestPathNode](Variable* var) {
          shortestPathNode->setStartInVariable(var);
        };
        setTargetVariable = [shortestPathNode](Variable* var) {
          shortestPathNode->setTargetInVariable(var);
        };
        setDistributeVariable = [shortestPathNode](Variable* var) {
          shortestPathNode->setDistributeVariable(var);
        };
        break;
      }
      default: {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, absl::StrCat("Cannot distribute ",
                                             targetNode->getTypeString(), "."));
      }
    }
    TRI_ASSERT(inputVariable != nullptr);
    TRI_ASSERT(collection != nullptr);
    // allowSpecifiedKeys can only be true for UPSERT
    TRI_ASSERT(targetNode->getType() == ExecutionNode::UPSERT ||
               !allowSpecifiedKeys);
    // createKeys can only be true for INSERT/UPSERT
    TRI_ASSERT((targetNode->getType() == ExecutionNode::INSERT ||
                targetNode->getType() == ExecutionNode::UPSERT) ||
               !createKeys);

    CalculationNode* calcNode = nullptr;
    auto setter = plan.getVarSetBy(inputVariable->id);
    if (setter == nullptr ||  // this can happen for $smartHandOver
        setter->getType() == EN::ENUMERATE_COLLECTION ||
        setter->getType() == EN::INDEX) {
      // If our input variable is set by a collection/index enumeration, it is
      // guaranteed to be an object with a _key attribute, so we don't need to
      // do anything.
      if (!createKeys || collection->usesDefaultSharding()) {
        // no need to insert an extra calculation node in this case.
        return;
      }
      // in case we have a collection that is not sharded by _key,
      // the keys need to be created/validated by the coordinator.
    }

    auto* ast = plan.getAst();
    auto args = ast->createNodeArray();
    char const* function;
    args->addMember(ast->createNodeReference(inputVariable));
    switch (fixupGraphInput) {
      case DistributeType::TRAVERSAL:
      case DistributeType::PATH: {
        function = "MAKE_DISTRIBUTE_GRAPH_INPUT";
        break;
      }
      case DistributeType::DOCUMENT: {
        if (createKeys) {
          function = "MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION";
          if (alternativeVariable) {
            args->addMember(ast->createNodeReference(alternativeVariable));
          } else {
            args->addMember(ast->createNodeValueNull());
          }
          auto flags = ast->createNodeObject();
          flags->addMember(ast->createNodeObjectElement(
              "allowSpecifiedKeys",
              ast->createNodeValueBool(allowSpecifiedKeys)));
          flags->addMember(ast->createNodeObjectElement(
              "ignoreErrors", ast->createNodeValueBool(ignoreErrors)));
          flags->addMember(ast->createNodeObjectElement(
              "projectOnlyId", ast->createNodeValueBool(canProjectOnlyId)));
          auto const& collectionName = collection->name();
          flags->addMember(ast->createNodeObjectElement(
              "collection",
              ast->createNodeValueString(collectionName.c_str(),
                                         collectionName.length())));

          args->addMember(flags);
        } else {
          function = "MAKE_DISTRIBUTE_INPUT";
          auto flags = ast->createNodeObject();
          flags->addMember(ast->createNodeObjectElement(
              "allowKeyConversionToObject",
              ast->createNodeValueBool(allowKeyConversionToObject)));
          flags->addMember(ast->createNodeObjectElement(
              "ignoreErrors", ast->createNodeValueBool(ignoreErrors)));
          bool canUseCustomKey =
              collection->getCollection()->usesDefaultShardKeys() ||
              allowSpecifiedKeys;
          flags->addMember(ast->createNodeObjectElement(
              "canUseCustomKey", ast->createNodeValueBool(canUseCustomKey)));

          args->addMember(flags);
        }
      }
    }

    if (fixupGraphInput == DistributeType::PATH) {
      // We need to insert two additional calculation nodes
      // on for source, one for target.
      // Both nodes are then piped into the SelectSmartDistributeGraphInput
      // which selects the smart input side.

      Variable* sourceVariable =
          plan.getAst()->variables()->createTemporaryVariable();
      auto sourceExpr = std::make_unique<Expression>(
          ast, ast->createNodeFunctionCall(function, args, true));
      auto sourceCalcNode = plan.createNode<CalculationNode>(
          &plan, plan.nextId(), std::move(sourceExpr), sourceVariable);

      Variable* targetVariable =
          plan.getAst()->variables()->createTemporaryVariable();
      auto targetArgs = ast->createNodeArray();
      TRI_ASSERT(alternativeVariable != nullptr);
      targetArgs->addMember(ast->createNodeReference(alternativeVariable));
      TRI_ASSERT(args->numMembers() == targetArgs->numMembers());
      auto targetExpr = std::make_unique<Expression>(
          ast, ast->createNodeFunctionCall(function, targetArgs, true));
      auto targetCalcNode = plan.createNode<CalculationNode>(
          &plan, plan.nextId(), std::move(targetExpr), targetVariable);

      // update the target node with in and out variables
      setInVariable(sourceVariable);
      setTargetVariable(targetVariable);

      auto selectInputArgs = ast->createNodeArray();
      selectInputArgs->addMember(ast->createNodeReference(sourceVariable));
      selectInputArgs->addMember(ast->createNodeReference(targetVariable));

      Variable* variable =
          plan.getAst()->variables()->createTemporaryVariable();
      auto expr = std::make_unique<Expression>(
          ast,
          ast->createNodeFunctionCall("SELECT_SMART_DISTRIBUTE_GRAPH_INPUT",
                                      selectInputArgs, true));
      calcNode = plan.createNode<CalculationNode>(&plan, plan.nextId(),
                                                  std::move(expr), variable);
      distributeNode->setVariable(variable);
      setDistributeVariable(variable);
      // Inject the calculations before the distributeNode
      plan.insertBefore(distributeNode, sourceCalcNode);
      plan.insertBefore(distributeNode, targetCalcNode);
    } else {
      // We insert an additional calculation node to create the input for our
      // distribute node.
      Variable* variable =
          plan.getAst()->variables()->createTemporaryVariable();

      // update the targetNode so that it uses the same input variable as our
      // distribute node
      setInVariable(variable);

      auto expr = std::make_unique<Expression>(
          ast, ast->createNodeFunctionCall(function, args, true));
      calcNode = plan.createNode<CalculationNode>(&plan, plan.nextId(),
                                                  std::move(expr), variable);
      distributeNode->setVariable(variable);
    }

    plan.insertBefore(distributeNode, calcNode);
    plan.clearVarUsageComputed();
    plan.findVarUsage();
  }
}

class AttributeAccessReplacer final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  AttributeAccessReplacer(ExecutionNode const* self,
                          Variable const* searchVariable,
                          std::span<std::string_view> attribute,
                          Variable const* replaceVariable, size_t index)
      : _self(self),
        _searchVariable(searchVariable),
        _attribute(attribute),
        _replaceVariable(replaceVariable),
        _index(index) {
    TRI_ASSERT(_searchVariable != nullptr);
    TRI_ASSERT(!_attribute.empty());
    TRI_ASSERT(_replaceVariable != nullptr);
  }

  bool before(ExecutionNode* en) override final {
    en->replaceAttributeAccess(_self, _searchVariable, _attribute,
                               _replaceVariable, _index);

    // always continue
    return false;
  }

 private:
  ExecutionNode const* _self;
  Variable const* _searchVariable;
  std::span<std::string_view> _attribute;
  Variable const* _replaceVariable;
  size_t _index;
};

void arangodb::aql::optimizeProjections(Optimizer* opt,
                                        std::unique_ptr<ExecutionPlan> plan,
                                        OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(
      nodes, {EN::INDEX, EN::ENUMERATE_COLLECTION, EN::JOIN, EN::MATERIALIZE},
      true);

  auto replace = [&plan](ExecutionNode* self, Projections& p,
                         Variable const* searchVariable, size_t index) {
    bool modified = false;
    std::vector<std::string_view> path;
    for (size_t i = 0; i < p.size(); ++i) {
      TRI_ASSERT(p[i].variable == nullptr);
      p[i].variable = plan->getAst()->variables()->createTemporaryVariable();
      path.clear();
      for (auto const& it : p[i].path.get()) {
        path.emplace_back(it);
      }

      AttributeAccessReplacer replacer(self, searchVariable, std::span(path),
                                       p[i].variable, index);
      plan->root()->walk(replacer);
      modified = true;
    }
    return modified;
  };

  bool modified = false;
  for (auto* n : nodes) {
    if (n->getType() == EN::JOIN) {
      // JoinNode. optimize projections in all parts
      auto* joinNode = ExecutionNode::castTo<JoinNode*>(n);
      size_t index = 0;
      for (auto& it : joinNode->getIndexInfos()) {
        modified |= replace(n, it.projections, it.outVariable, index++);
      }
    } else if (n->getType() == EN::MATERIALIZE) {
      auto* matNode = dynamic_cast<materialize::MaterializeRocksDBNode*>(n);
      if (matNode == nullptr) {
        continue;
      }

      containers::FlatHashSet<AttributeNamePath> attributes;
      if (utils::findProjections(matNode, &matNode->outVariable(),
                                 /*expectedAttribute*/ "",
                                 /*excludeStartNodeFilterCondition*/ true,
                                 attributes)) {
        if (attributes.size() <= matNode->maxProjections()) {
          matNode->projections() = Projections(std::move(attributes));
        }
      }

      modified |= replace(n, matNode->projections(), &matNode->outVariable(),
                          /*index*/ 0);
    } else {
      // IndexNode or EnumerateCollectionNode.
      TRI_ASSERT(n->getType() == EN::ENUMERATE_COLLECTION ||
                 n->getType() == EN::INDEX);

      auto* documentNode = ExecutionNode::castTo<DocumentProducingNode*>(n);
      if (documentNode->projections().hasOutputRegisters()) {
        // Some late materialize rule sets output registers
        continue;
      }
      modified |= documentNode->recalculateProjections(plan.get());
      modified |= replace(n, documentNode->projections(),
                          documentNode->outVariable(), /*index*/ 0);
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}
