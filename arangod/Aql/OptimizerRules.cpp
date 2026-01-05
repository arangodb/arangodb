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

// static node types used by some optimizer rules
// having them statically available avoids having to build the lists over
// and over for each AQL query
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

}  // namespace

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;
using namespace arangodb::iresearch;
using EN = arangodb::aql::ExecutionNode;

namespace arangodb {
namespace aql {

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

// modify plan

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
