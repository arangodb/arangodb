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

#include "OptimizerRulesSortAndGather.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/WindowNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Containers/SmallVector.h"
#include "Aql/TypedAstNodes.h"
#include "Graph/TraverserOptions.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

bool shouldApplyHeapOptimization(arangodb::aql::SortNode& sortNode,
                                 arangodb::aql::LimitNode& limitNode) {
  size_t input = sortNode.getCost().estimatedNrItems;
  size_t output = limitNode.limit() + limitNode.offset();

  // first check an easy case
  if (input < 100) {  // TODO fine-tune this cut-off
    // no reason to complicate things for such a small input
    return false;
  }

  // now check something a little more sophisticated, comparing best estimate of
  // cost of heap sort to cost of regular sort (ignoring some variables)
  double N = static_cast<double>(input);
  double M = static_cast<double>(output);
  double lgN = std::log2(N);
  double lgM = std::log2(M);

  // the 0.25 here comes from some experiments, may need to be tweaked;
  // should kick in if output is roughly at most 3/4 of input
  return (0.25 * N * lgM + M * lgM) < (N * lgN);
}

static bool isAllowedIntermediateSortLimitNode(ExecutionNode* node) {
  switch (node->getType()) {
    case ExecutionNode::CALCULATION:
    case ExecutionNode::OFFSET_INFO_MATERIALIZE:
    case ExecutionNode::SUBQUERY:
    case ExecutionNode::REMOTE:
    case ExecutionNode::ASYNC:
      return true;
    case ExecutionNode::GATHER:
      // sorting gather is allowed
      return ExecutionNode::castTo<GatherNode*>(node)->isSortingGather();
    case ExecutionNode::WINDOW:
      // if we do not look at following rows we can appyly limit to sort
      return !ExecutionNode::castTo<WindowNode*>(node)->needsFollowingRows();
    case ExecutionNode::SINGLETON:
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::ENUMERATE_LIST:
    case ExecutionNode::ENUMERATE_NEAR_VECTORS:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::SORT:
    case ExecutionNode::COLLECT:
    case ExecutionNode::INSERT:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPDATE:
    case ExecutionNode::NORESULTS:
    case ExecutionNode::UPSERT:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::INDEX:
    case ExecutionNode::INDEX_COLLECT:
    case ExecutionNode::JOIN:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::ENUMERATE_PATHS:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
    case ExecutionNode::RETURN:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::SCATTER:
    case ExecutionNode::REMOTE_SINGLE:
    case ExecutionNode::REMOTE_MULTIPLE:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::SUBQUERY_START:
    case ExecutionNode::SUBQUERY_END:
    // TODO: As soon as materialize does no longer have to filter out
    //  non-existent documents, move MATERIALIZE to the allowed nodes!
    case ExecutionNode::MATERIALIZE:
    case ExecutionNode::MUTEX:
      return false;
    case ExecutionNode::MAX_NODE_TYPE_VALUE:
      break;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL_AQL,
      absl::StrCat(
          "Unhandled node type '", node->getTypeString(),
          "' in sort-limit optimizer rule. Please report "
          "this error. Try turning off the sort-limit rule to get your query "
          "working."));
}

/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
void arangodb::aql::removeRedundantSortsRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::SORT, true);

  if (nodes.empty()) {
    // quick exit
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  ::arangodb::containers::HashSet<ExecutionNode*> toUnlink;

  for (auto const& n : nodes) {
    if (toUnlink.find(n) != toUnlink.end()) {
      // encountered a sort node that we already deleted
      continue;
    }

    auto const sortNode = ExecutionNode::castTo<SortNode*>(n);

    auto sortInfo = sortNode->getSortInformation();

    if (sortInfo.isValid && !sortInfo.criteria.empty()) {
      // we found a sort that we can understand
      std::vector<ExecutionNode*> stack;

      sortNode->dependencies(stack);

      int nodesRelyingOnSort = 0;

      while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->getType() == EN::SORT) {
          // we found another sort. now check if they are compatible!

          auto other =
              ExecutionNode::castTo<SortNode*>(current)->getSortInformation();

          bool canContinueSearch = true;
          switch (sortInfo.isCoveredBy(other)) {
            case SortInformation::unequal: {
              // different sort criteria
              if (nodesRelyingOnSort == 0) {
                // a sort directly followed by another sort: now remove one of
                // them

                if (!other.isDeterministic) {
                  // if the sort is non-deterministic, we must not remove it
                  canContinueSearch = false;
                  break;
                }

                if (sortNode->isStable()) {
                  // we should not optimize predecessors of a stable sort
                  // (used in a COLLECT node)
                  // the stable sort is for a reason, and removing any
                  // predecessors sorts might change the result.
                  // We're not allowed to continue our search for further
                  // redundant SORTS in this iteration.
                  canContinueSearch = false;
                  break;
                }

                // remove sort that is a direct predecessor of a sort
                toUnlink.emplace(current);
              } else {
                canContinueSearch = false;
              }
              break;
            }

            case SortInformation::otherLessAccurate: {
              toUnlink.emplace(current);
              break;
            }

            case SortInformation::ourselvesLessAccurate: {
              // the sort at the start of the pipeline makes the sort at the end
              // superfluous, so we'll remove it
              // Related to: BTS-937
              toUnlink.emplace(n);
              canContinueSearch = false;
              break;
            }

            case SortInformation::allEqual: {
              // the sort at the end of the pipeline makes the sort at the start
              // superfluous, so we'll remove it
              toUnlink.emplace(current);
              break;
            }
          }
          if (!canContinueSearch) {
            break;
          }
        } else if (current->getType() == EN::FILTER) {
          // ok: a filter does not depend on sort order
        } else if (current->getType() == EN::CALCULATION) {
          // ok: a calculation does not depend on sort order only if it is
          // deterministic
          if (!current->isDeterministic()) {
            ++nodesRelyingOnSort;
          }
        } else if (current->getType() == EN::ENUMERATE_LIST ||
                   current->getType() == EN::ENUMERATE_COLLECTION ||
                   current->getType() == EN::TRAVERSAL ||
                   current->getType() == EN::ENUMERATE_PATHS ||
                   current->getType() == EN::SHORTEST_PATH) {
          // ok, but we cannot remove two different sorts if one of these node
          // types is between them
          // example: in the following query, the one sort will be optimized
          // away:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC SORT i.a
          //   DESC RETURN i
          // but in the following query, the sorts will stay:
          //   FOR i IN [ { a: 1 }, { a: 2 } , { a: 3 } ] SORT i.a ASC LET a =
          //   i.a SORT i.a DESC RETURN i
          ++nodesRelyingOnSort;
        } else {
          // abort at all other type of nodes. we cannot remove a sort beyond
          // them
          // this includes COLLECT and LIMIT
          break;
        }

        if (!current->hasDependency()) {
          // node either has no or more than one dependency. we don't know what
          // to do and must abort
          // note: this will also handle Singleton nodes
          break;
        }

        current->dependencies(stack);
      }

      if (toUnlink.find(n) == toUnlink.end() &&
          sortNode->simplify(plan.get())) {
        // sort node had only constant expressions. it will make no difference
        // if we execute it or not
        // so we can remove it
        toUnlink.emplace(n);
      }
    }
  }

  if (!toUnlink.empty()) {
    plan->unlinkNodes(toUnlink);
  }

  opt->addPlan(std::move(plan), rule, !toUnlink.empty());
}

void arangodb::aql::sortLimitRule(Optimizer* opt,
                                  std::unique_ptr<ExecutionPlan> plan,
                                  OptimizerRule const& rule) {
  bool mod = false;
  // If there isn't a limit node, and at least one sort or gather node,
  // there's nothing to do.
  if (!plan->contains(EN::LIMIT) ||
      (!plan->contains(EN::SORT) && !plan->contains(EN::GATHER))) {
    opt->addPlan(std::move(plan), rule, mod);
    return;
  }

  containers::SmallVector<ExecutionNode*, 8> limitNodes;

  plan->findNodesOfType(limitNodes, EN::LIMIT, true);
  for (ExecutionNode* node : limitNodes) {
    bool hasRemoteBeforeSort{false};
    bool firstSortNode{true};
    auto limitNode = ExecutionNode::castTo<LimitNode*>(node);
    for (ExecutionNode* current = limitNode->getFirstDependency();
         current != nullptr; current = current->getFirstDependency()) {
      if (current->getType() == EN::SORT) {
        // Apply sort-limit optimization to sort node, if it seems reasonable
        auto sortNode = ExecutionNode::castTo<SortNode*>(current);
        if (shouldApplyHeapOptimization(*sortNode, *limitNode)) {
          sortNode->setLimit(limitNode->offset() + limitNode->limit());
          // Make sure LIMIT is always after the SORT
          // this, makes sense only for the closest to LIMIT node.
          // All nodes higher will be protected by the limit set before
          // the first sort node.
          if (firstSortNode) {
            auto& mainLimitNode = *ExecutionNode::castTo<LimitNode*>(limitNode);
            // if we don't have remote breaker we could just replace the limit
            // node otherwise we must have new node to constrain accesss to the
            // sort node with only offset+limit documents
            if (!hasRemoteBeforeSort) {
              plan->unlinkNode(limitNode);
            }
            auto* auxLimitNode =
                hasRemoteBeforeSort
                    ? plan->registerNode(std::make_unique<LimitNode>(
                          plan.get(), plan->nextId(), 0,
                          limitNode->offset() + limitNode->limit()))
                    : limitNode;
            TRI_ASSERT(auxLimitNode);
            if (hasRemoteBeforeSort && mainLimitNode.fullCount()) {
              TRI_ASSERT(limitNode != auxLimitNode);
              auto& tmp = *ExecutionNode::castTo<LimitNode*>(auxLimitNode);
              tmp.setFullCount();
              mainLimitNode.setFullCount(false);
            }
            auto* sortParent = sortNode->getFirstParent();
            TRI_ASSERT(sortParent);
            if (sortParent != auxLimitNode) {
              sortParent->replaceDependency(sortNode, auxLimitNode);
              sortNode->addParent(auxLimitNode);
            }
          }
          firstSortNode = false;
          mod = true;
        }
      } else if (current->getType() == EN::GATHER) {
        // Make sorting gather nodes aware of the limit, so they may skip
        // after it
        auto gatherNode = ExecutionNode::castTo<GatherNode*>(current);
        if (gatherNode->isSortingGather()) {
          gatherNode->setConstrainedSortLimit(limitNode->offset() +
                                              limitNode->limit());
          mod = true;
        }
      } else if (current->getType() == EN::REMOTE) {
        hasRemoteBeforeSort = true;
      }

      // Stop on nodes that may not be between sort & limit (or between
      // sorting gather & limit) for the limit to be applied to the sort (or
      // sorting gather) node safely.
      if (!isAllowedIntermediateSortLimitNode(current)) {
        break;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, mod);
}

namespace {

/// @brief is the node parallelizable?
struct ParallelizableFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  bool _isParallelizable = true;
  bool _hasParallelTraversal = false;

  ~ParallelizableFinder() = default;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* node) override final {
    if ((node->getType() == ExecutionNode::SCATTER ||
         node->getType() == ExecutionNode::DISTRIBUTE) &&
        _hasParallelTraversal) {
      // we cannot parallelize the gather if we have a parallel traversal which
      // itself depends again on a scatter/distribute node, because we are
      // currently lacking synchronization for that scatter/distribute node.
      _isParallelizable = false;
      return true;  // true to abort the whole walking process
        }

    if (node->getType() == ExecutionNode::TRAVERSAL ||
        node->getType() == ExecutionNode::SHORTEST_PATH ||
        node->getType() == ExecutionNode::ENUMERATE_PATHS) {
      auto* gn = ExecutionNode::castTo<GraphNode*>(node);
      _hasParallelTraversal |= gn->options()->parallelism() > 1;
      if (!gn->isLocalGraphNode()) {
        _isParallelizable = false;
        return true;  // true to abort the whole walking process
      }
        }

    // continue inspecting
    return false;
  }
};

/// no modification nodes, ScatterNodes etc
bool isParallelizable(GatherNode* node) {
  if (node->parallelism() == GatherNode::Parallelism::Serial) {
    // node already defined to be serial
    return false;
  }

  ParallelizableFinder finder;
  for (ExecutionNode* e : node->getDependencies()) {
    e->walk(finder);
    if (!finder._isParallelizable) {
      return false;
    }
  }
  return true;
}
}  // namespace

/// @brief parallelize coordinator GatherNodes
void arangodb::aql::parallelizeGatherRule(Optimizer* opt,
                                          std::unique_ptr<ExecutionPlan> plan,
                                          OptimizerRule const& rule) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  containers::SmallVector<ExecutionNode*, 8> graphNodes;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  for (auto node : nodes) {
    GatherNode* gn = ExecutionNode::castTo<GatherNode*>(node);

    if (!gn->isInSubquery() && isParallelizable(gn)) {
      // find all graph nodes and make sure that they all are using satellite
      graphNodes.clear();
      plan->findNodesOfType(
          graphNodes, {EN::TRAVERSAL, EN::SHORTEST_PATH, EN::ENUMERATE_PATHS},
          true);
      bool const allSatellite =
          std::all_of(graphNodes.begin(), graphNodes.end(), [](auto n) {
            GraphNode* graphNode = ExecutionNode::castTo<GraphNode*>(n);
            return graphNode->isLocalGraphNode();
          });

      if (allSatellite) {
        gn->setParallelism(GatherNode::Parallelism::Parallel);
        modified = true;
      }
    } else {
      gn->setParallelism(GatherNode::Parallelism::Serial);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

void arangodb::aql::decayUnnecessarySortedGather(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::GATHER, true);

  bool modified = false;

  for (auto& n : nodes) {
    auto gatherNode = ExecutionNode::castTo<GatherNode*>(n);
    if (gatherNode->elements().empty()) {
      continue;
    }

    auto const* collection = GatherNode::findCollection(*gatherNode);

    // For views (when 'collection == nullptr') we don't need
    // to check number of shards
    // On SmartEdge collections we have 0 shards and we need the elements
    // to be injected here as well. So do not replace it with > 1
    if (collection && collection->numberOfShards() == 1) {
      modified = true;
      gatherNode->elements().clear();
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}