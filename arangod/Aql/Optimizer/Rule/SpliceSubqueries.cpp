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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/SubqueryStartExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryEndExecutionNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer/Rules.h"
#include "Aql/Optimizer/Optimizer.h"
#include "Aql/Ast.h"
#include "Aql/QueryContext.h"

namespace arangodb::aql {

namespace {
void findSubqueriesSuitableForSplicing(
    ExecutionPlan const& plan,
    containers::SmallVector<SubqueryNode*, 8>& result) {
  using EN = ExecutionNode;
  TRI_ASSERT(result.empty());
  using ResultVector = decltype(result);

  using SuitableNodeSet =
      std::set<SubqueryNode*, std::less<>,
               containers::detail::short_alloc<SubqueryNode*, 128,
                                               alignof(SubqueryNode*)>>;

  // This finder adds all subquery nodes in pre-order to its `result` parameter,
  // and all nodes that are suitable for splicing to `suitableNodes`. Suitable
  // means that neither the containing subquery contains unsuitable nodes - at
  // least not in an ancestor of the subquery - nor the subqueryrcontains
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
void spliceSubqueriesRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                          OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<SubqueryNode*, 8> subqueryNodes;
  findSubqueriesSuitableForSplicing(*plan, subqueryNodes);

  // Note that we rely on the order `subqueryNodes` in the sense that, for //
  // nested subqueries, the outer subquery must come before the inner, so we //
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

}  // namespace arangodb::aql
