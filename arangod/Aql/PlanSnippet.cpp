////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "PlanSnippet.h"

#include "Aql/Aggregator.h"
#include "Aql/ClusterNodes.h"
#include "Aql/CollectNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/KShortestPathsNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/QueryContext.h"
#include "Aql/SortNode.h"
#include "Aql/ShortestPathNode.h"
#include "Aql/TraversalNode.h"
#include "Basics/debugging.h"
#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

std::tuple<Collection const*, Variable const*, bool> extractDistributeInfo(
    ExecutionNode const* node) {
  // TODO: this seems a bit verbose, but is at least local & simple
  //       the modification nodes are all collectionaccessing, the graph nodes
  //       are currently assumed to be disjoint, and hence smart, so all
  //       collections are sharded the same way!
  switch (node->getType()) {
    case ExecutionNode::INSERT: {
      auto const* insertNode = ExecutionNode::castTo<InsertNode const*>(node);
      return {insertNode->collection(), insertNode->inVariable(), false};
    } break;
    case ExecutionNode::REMOVE: {
      auto const* removeNode = ExecutionNode::castTo<RemoveNode const*>(node);
      return {removeNode->collection(), removeNode->inVariable(), false};
    } break;
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE: {
      auto const* updateReplaceNode =
          ExecutionNode::castTo<UpdateReplaceNode const*>(node);
      if (updateReplaceNode->inKeyVariable() != nullptr) {
        return {updateReplaceNode->collection(),
                updateReplaceNode->inKeyVariable(), false};
      } else {
        return {updateReplaceNode->collection(),
                updateReplaceNode->inDocVariable(), false};
      }
    } break;
    case ExecutionNode::UPSERT: {
      auto upsertNode = ExecutionNode::castTo<UpsertNode const*>(node);
      return {upsertNode->collection(), upsertNode->inDocVariable(), false};
    } break;
    case ExecutionNode::TRAVERSAL: {
      auto traversalNode = ExecutionNode::castTo<TraversalNode const*>(node);
      TRI_ASSERT(traversalNode->isDisjoint());
      return {traversalNode->collection(), traversalNode->inVariable(), true};
    } break;
    case ExecutionNode::K_SHORTEST_PATHS: {
      auto kShortestPathsNode =
          ExecutionNode::castTo<KShortestPathsNode const*>(node);
      TRI_ASSERT(kShortestPathsNode->isDisjoint());
      // Subtle: KShortestPathsNode uses a reference when returning
      // startInVariable
      return {kShortestPathsNode->collection(),
              &kShortestPathsNode->startInVariable(), true};
    } break;
    case ExecutionNode::SHORTEST_PATH: {
      auto shortestPathNode =
          ExecutionNode::castTo<ShortestPathNode const*>(node);
      TRI_ASSERT(shortestPathNode->isDisjoint());
      return {shortestPathNode->collection(),
              shortestPathNode->startInVariable(), true};
    } break;
    default: {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Cannot distribute " + node->getTypeString() + ".");
    } break;
  }
}

}  // namespace

void PlanSnippet::optimizeAdjacentSnippets(
    std::shared_ptr<PlanSnippet> upperSnippet,
    std::shared_ptr<PlanSnippet> lowerSnippet) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // We can only join validSnippets
  upperSnippet->assertInvariants();
  lowerSnippet->assertInvariants();
  {
    // We can only join adjacent snippts
    auto topLast = upperSnippet->_last;
    TRI_ASSERT(topLast->getFirstParent() == lowerSnippet->_topMost);
  }
  ScopeGuard sg([upperSnippet, lowerSnippet]() noexcept {
    // After we are done, all Snippets still need to be valid
    upperSnippet->assertInvariants();
    lowerSnippet->assertInvariants();
    // And need to be adjacent
    auto topLast = upperSnippet->_last;
    TRI_ASSERT(topLast->getFirstParent() == lowerSnippet->_topMost);
  });
#endif

  while (lowerSnippet->canStealTopNode()) {
    auto candidateToSteal = lowerSnippet->_topMost;
    LOG_DEVEL << "Try stealing " << candidateToSteal->getTypeString() << "("
              << candidateToSteal->id() << ")";
    if (upperSnippet->tryJoinBelow(candidateToSteal)) {
      lowerSnippet->stealTopNode();
    } else {
      // Cannot steal anymore we are done
      break;
    }
  }
}

PlanSnippet::DistributionInput::DistributionInput(ExecutionNode* node)
    : _distributeOnNode(node), _createKeys{false} {
  TRI_ASSERT(_distributeOnNode != nullptr);
  auto [collection, inputVariable, isGraphNode] =
      ::extractDistributeInfo(_distributeOnNode);
  _variable = inputVariable;
  if (_distributeOnNode->getType() == ExecutionNode::INSERT ||
      _distributeOnNode->getType() == ExecutionNode::UPSERT) {
    _createKeys = true;
  }
}

ExecutionNode* PlanSnippet::DistributionInput::createDistributeNode(
    ExecutionPlan* plan) const {
  // TODO: Need to Handle Enterprise Graphs
  return plan->createNode<DistributeNode>(plan, plan->nextId(),
                                          ScatterNode::ScatterType::SHARD,
                                          collection(), variable(), targetId());
}

ExecutionNode* PlanSnippet::DistributionInput::createDistributeInputNode(
    ExecutionPlan* plan) const {
  // TODO: Need to Handle Enterprise Graphs
  // TODO: Relink inputVariable
  auto collection = this->collection();
  auto inputVariable = variable();
  auto alternativeVariable = static_cast<Variable const*>(nullptr);

  auto allowKeyConversionToObject = bool{false};
  auto allowSpecifiedKeys = bool{false};

  auto fixupGraphInput = bool{false};

  std::function<void(Variable * variable)> setInVariable;
  bool ignoreErrors = false;

  // TODO: this seems a bit verbose, but is at least local & simple
  //       the modification nodes are all collectionaccessing, the graph nodes
  //       are currently assumed to be disjoint, and hence smart, so all
  //       collections are sharded the same way!
  switch (_distributeOnNode->getType()) {
    case ExecutionNode::INSERT: {
      auto* insertNode = ExecutionNode::castTo<InsertNode*>(_distributeOnNode);
      allowKeyConversionToObject = true;
      setInVariable = [insertNode](Variable* var) {
        insertNode->setInVariable(var);
      };
    } break;
    case ExecutionNode::REMOVE: {
      auto* removeNode = ExecutionNode::castTo<RemoveNode*>(_distributeOnNode);
      allowKeyConversionToObject = true;
      ignoreErrors = removeNode->getOptions().ignoreErrors;
      setInVariable = [removeNode](Variable* var) {
        removeNode->setInVariable(var);
      };
    } break;
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE: {
      // TODO Check the input variable handling.
      // Potentially align with usage in other parts of this file
      auto* updateReplaceNode =
          ExecutionNode::castTo<UpdateReplaceNode*>(_distributeOnNode);
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
    } break;
    case ExecutionNode::UPSERT: {
      // TODO Figure out how to implement this
      TRI_ASSERT(false);
      // an UPSERT node has two input variables!
      auto* upsertNode = ExecutionNode::castTo<UpsertNode*>(_distributeOnNode);
      collection = upsertNode->collection();
      inputVariable = upsertNode->inDocVariable();
      alternativeVariable = upsertNode->insertVariable();
      ignoreErrors = upsertNode->getOptions().ignoreErrors;
      allowKeyConversionToObject = true;
      allowSpecifiedKeys = true;
      setInVariable = [upsertNode](Variable* var) {
        upsertNode->setInsertVariable(var);
      };
    } break;
    case ExecutionNode::TRAVERSAL: {
      auto* traversalNode =
          ExecutionNode::castTo<TraversalNode*>(_distributeOnNode);
      TRI_ASSERT(traversalNode->isDisjoint());
      allowKeyConversionToObject = true;
      fixupGraphInput = true;
      setInVariable = [traversalNode](Variable* var) {
        traversalNode->setInVariable(var);
      };
    } break;
    case ExecutionNode::K_SHORTEST_PATHS: {
      auto* kShortestPathsNode =
          ExecutionNode::castTo<KShortestPathsNode*>(_distributeOnNode);
      TRI_ASSERT(kShortestPathsNode->isDisjoint());
      allowKeyConversionToObject = true;
      fixupGraphInput = true;
      setInVariable = [kShortestPathsNode](Variable* var) {
        kShortestPathsNode->setStartInVariable(var);
      };
    } break;
    case ExecutionNode::SHORTEST_PATH: {
      auto* shortestPathNode =
          ExecutionNode::castTo<ShortestPathNode*>(_distributeOnNode);
      TRI_ASSERT(shortestPathNode->isDisjoint());
      allowKeyConversionToObject = true;
      fixupGraphInput = true;
      setInVariable = [shortestPathNode](Variable* var) {
        shortestPathNode->setStartInVariable(var);
      };
    } break;
    default: {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Cannot distribute " + _distributeOnNode->getTypeString() + ".");
    } break;
  }

  TRI_ASSERT(inputVariable != nullptr);
  TRI_ASSERT(collection != nullptr);
  // allowSpecifiedKeys can only be true for UPSERT
  TRI_ASSERT(_distributeOnNode->getType() == ExecutionNode::UPSERT ||
             !allowSpecifiedKeys);

  // We insert an additional calculation node to create the input for our
  // distribute node.
  auto* ast = plan->getAst();
  Variable* variable = ast->variables()->createTemporaryVariable();
  auto args = ast->createNodeArray();
  char const* function;
  args->addMember(ast->createNodeReference(inputVariable));
  if (fixupGraphInput) {
    function = "MAKE_DISTRIBUTE_GRAPH_INPUT";
  } else {
    if (createKeys()) {
      function = "MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION";
      if (alternativeVariable) {
        args->addMember(ast->createNodeReference(alternativeVariable));
      } else {
        args->addMember(ast->createNodeValueNull());
      }
      auto flags = ast->createNodeObject();
      flags->addMember(ast->createNodeObjectElement(
          "allowSpecifiedKeys", ast->createNodeValueBool(allowSpecifiedKeys)));
      flags->addMember(ast->createNodeObjectElement(
          "ignoreErrors", ast->createNodeValueBool(ignoreErrors)));
      auto const& collectionName = collection->name();
      flags->addMember(ast->createNodeObjectElement(
          "collection", ast->createNodeValueString(collectionName.c_str(),
                                                   collectionName.length())));
      // args->addMember(ast->createNodeValueString(collectionName.c_str(),
      // collectionName.length()));

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

  auto expr = std::make_unique<Expression>(
      ast, ast->createNodeFunctionCall(function, args, true));
  return plan->createNode<CalculationNode>(plan, plan->nextId(),
                                           std::move(expr), variable);
}

bool PlanSnippet::DistributionInput::testAndMoveInputIfShardKeysUntouched(
    CalculationNode const* node) {
  VarSet varsUsed{};
  node->getVariablesUsedHere(varsUsed);
  if (varsUsed.size() != 1) {
    // We can only keep track of a single input variable.
    // And that is actually all that we need.
    return false;
  }
  // We should only try this if we are actually on the producer
  // of the input in question.
  TRI_ASSERT(node->outVariable() == _variable);

  auto expr = node->expression();
  AstNode const* n = expr->node();
  TRI_ASSERT(n != nullptr);
  // TODO Implement isProjection()
  //   Should only be true, if read from a single input
  //   and do not modify any value.
  //   Functions, Operators, and such shall return false
  /*
  if (expr->isProjection()) {
  */
  auto nextInputVariable = *varsUsed.begin();
  _variable = nextInputVariable;
  return true;
}

Variable const* PlanSnippet::DistributionInput::variable() const {
  TRI_ASSERT(_variable != nullptr);
  return _variable;
}

Collection const* PlanSnippet::DistributionInput::collection() const {
  auto [collection, inputVariable, isGraphNode] =
      ::extractDistributeInfo(_distributeOnNode);
  return collection;
}

ExecutionNodeId PlanSnippet::DistributionInput::targetId() const {
  return _distributeOnNode->id();
}

PlanSnippet::GatherOutput::GatherOutput() : _collect{nullptr}, _elements{} {}

ExecutionNode* PlanSnippet::GatherOutput::createGatherNode(
    ExecutionPlan* plan) const {
  auto gatherNode = plan->createNode<GatherNode>(
      plan, plan->nextId(), getGatherSortMode(), getGatherParallelism());
  // TODO: Performance note, we copy the elements once here, we could MOVE
  // them in and adapt gatherNode to accept a move instead of a const&
  gatherNode->elements(_elements);
  return gatherNode;
}

ExecutionNode* PlanSnippet::GatherOutput::eventuallyCreateCollectNode(
    ExecutionPlan* plan) {
  if (_collect == nullptr) {
    // Nothing to collect, no need to create another Collect
    return nullptr;
  }
  switch (_collect->aggregationMethod()) {
    case CollectOptions::CollectMethod::DISTINCT: {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
    }
    case CollectOptions::CollectMethod::COUNT: {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      break;
    }
    case CollectOptions::CollectMethod::UNDEFINED:
    case CollectOptions::CollectMethod::HASH:
    case CollectOptions::CollectMethod::SORTED: {
      // clone a COLLECT v1 = expr, v2 = expr ... operation from the
      // coordinator to the DB server(s), and leave an aggregate COLLECT
      // node on the coordinator for total aggregation

      // The original optimizer rule did exclude Collects without
      // output variables.
      TRI_ASSERT(!_collect->hasOutVariable());
      std::vector<AggregateVarInfo> dbServerAggVars;
      for (auto const& it : _collect->aggregateVariables()) {
        std::string_view func = Aggregator::pushToDBServerAs(it.type);
        // Original code does not optimize here, this is not possible at this
        // place
        // TODO: CHECK we can never create a Collect statement triggering this
        // assert
        TRI_ASSERT(!func.empty());
        auto outVariable =
            plan->getAst()->variables()->createTemporaryVariable();
        dbServerAggVars.emplace_back(
            AggregateVarInfo{outVariable, it.inVar, std::string(func)});
      }

      // create new group variables
      auto const& groupVars = _collect->groupVariables();
      std::vector<GroupVarInfo> outVars;
      outVars.reserve(groupVars.size());
      std::unordered_map<Variable const*, Variable const*> replacements;

      for (auto const& it : groupVars) {
        // create new out variables
        auto out = plan->getAst()->variables()->createTemporaryVariable();
        replacements.try_emplace(it.inVar, out);
        outVars.emplace_back(GroupVarInfo{out, it.inVar});
      }

      auto dbCollectNode = plan->createNode<CollectNode>(
          plan, plan->nextId(), _collect->getOptions(), outVars,
          dbServerAggVars, nullptr, nullptr, std::vector<Variable const*>(),
          _collect->variableMap(), false);
      dbCollectNode->aggregationMethod(_collect->aggregationMethod());
      dbCollectNode->specialized();

      std::vector<GroupVarInfo> copy;
      size_t i = 0;
      for (GroupVarInfo const& it : _collect->groupVariables()) {
        // replace input variables
        copy.emplace_back(GroupVarInfo{/*outVar*/ it.outVar,
                                       /*inVar*/ outVars[i].outVar});
        ++i;
      }
      _collect->groupVariables(copy);

      size_t j = 0;
      for (AggregateVarInfo& it : _collect->aggregateVariables()) {
        it.inVar = dbServerAggVars[j].outVar;
        it.type = Aggregator::runOnCoordinatorAs(it.type);
        ++j;
      }

      bool removeGatherNodeSort = (dbCollectNode->aggregationMethod() !=
                                   CollectOptions::CollectMethod::SORTED);

      // in case we need to keep the sortedness of the GatherNode,
      // we may need to replace some variable references in it due
      // to the changes we made to the COLLECT node
      if (!removeGatherNodeSort && !replacements.empty()) {
        adjustSortElements(plan, replacements);
      }
      return dbCollectNode;
    }
  }
}

void PlanSnippet::GatherOutput::adjustSortElements(
    ExecutionPlan* plan,
    std::unordered_map<arangodb::aql::Variable const*,
                       arangodb::aql::Variable const*> const& replacements) {
  std::string cmp;
  arangodb::basics::StringBuffer buffer(128, false);
  for (auto& it : _elements) {
    // replace variables
    auto it2 = replacements.find(it.var);

    if (it2 != replacements.end()) {
      // match with our replacement table
      it.var = (*it2).second;
      it.attributePath.clear();
    } else {
      // no match. now check all our replacements and compare how
      // their sources are actually calculated (e.g. #2 may mean
      // "foo.bar")
      cmp = it.toString();
      for (auto const& it3 : replacements) {
        auto setter = plan->getVarSetBy(it3.first->id);
        if (setter == nullptr ||
            setter->getType() != ExecutionNode::CALCULATION) {
          continue;
        }
        auto* expr = arangodb::aql::ExecutionNode::castTo<
                         arangodb::aql::CalculationNode const*>(setter)
                         ->expression();
        try {
          // stringifying an expression may fail with "too long" error
          buffer.clear();
          expr->stringify(&buffer);
          if (cmp.size() == buffer.size() &&
              cmp.compare(0, cmp.size(), buffer.c_str(), buffer.size()) == 0) {
            // finally a match!
            it.var = it3.second;
            it.attributePath.clear();
            break;
          }
        } catch (...) {
        }
      }
    }
  }
}

bool PlanSnippet::GatherOutput::tryAndIncludeSortNode(SortNode const* sort) {
  if (_collect != nullptr) {
    // If we have taken a collect, we cannot
    // take another SORT afterwards.
    return false;
  }
  // Can include the Sort
  // TODO: Performance note, we copy the elements once here, we did not do this
  // before
  _elements = sort->elements();
  return true;
}

void PlanSnippet::GatherOutput::memorizeCollect(CollectNode* collect) {
  // We cannot pass a second collect that we still need to react on.
  TRI_ASSERT(_collect == nullptr);
  _collect = collect;
}

GatherNode::SortMode PlanSnippet::GatherOutput::getGatherSortMode() const {
  // TODO: Implement me, is somehow based on collection / number of shards.
  return GatherNode::SortMode::MinElement;
}

GatherNode::Parallelism PlanSnippet::GatherOutput::getGatherParallelism()
    const {
  // TODO: Implement me, is based on Collection.
  return GatherNode::Parallelism::Parallel;
}

bool PlanSnippet::DistributionInput::createKeys() const { return _createKeys; }

PlanSnippet::PlanSnippet(ExecutionNode* node)
    : _topMost{node}, _last{node}, _distributeOn{nullptr} {
  // We cannot plan nullptr;
  TRI_ASSERT(node != nullptr);
  switch (node->getType()) {
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::UPSERT:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::K_SHORTEST_PATHS:
    case ExecutionNode::SHORTEST_PATH: {
      _distributeOn = std::make_unique<DistributionInput>(node);
      _isOnCoordinator = false;
      break;
    }
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW:
    case ExecutionNode::INDEX: {
      // TODO: Set Scatter Information
      _isOnCoordinator = false;
      break;
    }
    case ExecutionNode::MATERIALIZE: {
      // TODO: howto handle this?
      _isOnCoordinator = false;
      break;
    }
    case ExecutionNode::RETURN:
    case ExecutionNode::SORT:
    case ExecutionNode::REMOTESINGLE:
    case ExecutionNode::ENUMERATE_LIST:
    case ExecutionNode::COLLECT: {
      _isOnCoordinator = true;
      break;
    }
    case ExecutionNode::CALCULATION: {
      // TODO: Assert the we can only start here if Calculation forces us to be
      // on Coordinator, otherwise we should never instantiate with CALC
      _isOnCoordinator = true;
      break;
    }
    default:
      // We cannot start a new Snippet distribution based on a node of this type
      // (yet)
      LOG_DEVEL << "Try to start a new PlanSnippet on " << node->getTypeString()
                << " which is unsupported";
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "Try to start a new PlanSnippet on " +
                                         node->getTypeString() +
                                         " which is unsupported");
  }
  assertInvariants();
}

bool PlanSnippet::tryJoinAbove(ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->plan() == _last->plan());
  bool didJoin = false;
  // NOTE: Do never add a DEFAULT case here.
  // We want the compiler to help us on new nodes
  // in order to remind us that we need to implementat
  // their behaviour.
  switch (node->getType()) {
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::SINGLETON: {
      // Nodes that can be trivially joined, and have no effect on Sharding
      _topMost = node;
      didJoin = true;
      break;
    }
    case ExecutionNode::ENUMERATE_LIST: {
      if (_isOnCoordinator || _distributeOn == nullptr ||
          !_distributeOn->createKeys()) {
        // We cannot trivally include an ENUMERATE_LIST
        // if we need to generate the keys.
        // This operation has to be on the coordinator,
        // and has to be executed once for each row, so
        // pushing it above ENUMERATE_LIST is prohibited.
        _topMost = node;
        didJoin = true;
      }
      break;
    }

    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX: {
      if (!_isOnCoordinator && _distributeOn != nullptr &&
          !_distributeOn->createKeys()) {
        // Test if our input falls out of a Collection.
        // This guarantees that we stay on the same Shard.
        // However, if we need to create Keys, we need to return back to
        // Coordinator once for key creation Node.
        // TODO: Clarify do we need to assert the collection?
        auto prod = ExecutionNode::castTo<DocumentProducingNode*>(node);
        if (prod->outVariable() == _distributeOn->variable()) {
          _topMost = node;
          didJoin = true;
        }
      }
      break;
    }
    case ExecutionNode::INSERT:
    case ExecutionNode::REMOVE:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE: {
      // Nodes that cannot be joined, and do have effect on Sharding
      break;
    }
    case ExecutionNode::SORT:
    case ExecutionNode::COLLECT: {
      if (_isOnCoordinator) {
        // On they way up the stack we can always join Sort and Collect on
        // coordinators
        _topMost = node;
        didJoin = true;
      }
      break;
    }

    case ExecutionNode::CALCULATION: {
      if (!_isOnCoordinator) {
        auto calc = ExecutionNode::castTo<CalculationNode*>(node);
        // TODO for simplicity we could move this into CalculationNode.
        bool const isOneShard =
            calc->plan()->getAst()->query().vocbase().isOneShard();
        if (calc->expression()->willUseV8() ||
            !calc->expression()->canRunOnDBServer(isOneShard)) {
          // We cannot join with a calculation that is forced to be on
          // coordinators
          break;
        }

        if (_distributeOn != nullptr) {
          if (calc->outVariable() == _distributeOn->variable()) {
            if (_distributeOn->testAndMoveInputIfShardKeysUntouched(calc)) {
              // This Calculation just reads some input, no transformations
              // going on, we can include this here, and distribute on it's
              // input
              _topMost = node;
              didJoin = true;
            }
            // Otherwise we cannot include the origin of distribution
            break;
          }
        }
      }

      // Nodes that can be trivially joined, and have no effect on Sharding
      _topMost = node;
      didJoin = true;
      break;
    }
    case ExecutionNode::REMOTESINGLE:
    case ExecutionNode::ASYNC:
    case ExecutionNode::MUTEX:
    case ExecutionNode::WINDOW:
    case ExecutionNode::SUBQUERY:
    case ExecutionNode::SUBQUERY_END:
    case ExecutionNode::SUBQUERY_START:
    case ExecutionNode::RETURN:
    case ExecutionNode::NORESULTS:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS:
    case ExecutionNode::UPSERT:
    case ExecutionNode::MATERIALIZE:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      // Nodes that Are not handled yet
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "PlanSnippet Join-Above not implemented for " +
              node->getTypeString());
      break;
    }

    case ExecutionNode::MAX_NODE_TYPE_VALUE:
    case ExecutionNode::GATHER:
    case ExecutionNode::REMOTE:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::SCATTER: {
      // These Cluster Nodes will be added by this rule.
      // They are not allowed to be present in the Plan yet.
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL_AQL,
          "PlanSnippet Join-Above triggered non-supported Node " +
              node->getTypeString());
    }
  }
  assertInvariants();
  return didJoin;
}

bool PlanSnippet::tryJoinBelow(ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->plan() == _last->plan());
  bool didJoin = false;
  // NOTE: Do never add a DEFAULT case here.
  // We want the compiler to help us on new nodes
  // in order to remind us that we need to implementat
  // their behaviour.
  switch (node->getType()) {
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::ENUMERATE_LIST: {
      // Nodes that can be trivially joined, and have no effect on Sharding
      _last = node;
      didJoin = true;
      break;
    }
    case ExecutionNode::SORT: {
      auto sort = ExecutionNode::castTo<SortNode*>(node);
      if (_gatherOutput.tryAndIncludeSortNode(sort)) {
        // We can include and adapt this sort node, so join it.
        _last = node;
        didJoin = true;
      }
      break;
    }
    case ExecutionNode::COLLECT: {
      // We can NEVER include the collect itself.
      // But we need can optimize by doing a pre-collect
      // on the DBServer already.
      // So memorize the collect!
      auto collect = ExecutionNode::castTo<CollectNode*>(node);
      _gatherOutput.memorizeCollect(collect);
      break;
    }
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX:
    case ExecutionNode::INSERT:
    case ExecutionNode::REMOVE:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE: {
      // Nodes that cannot be joined, and do have effect on Sharding
      break;
    }

    case ExecutionNode::CALCULATION: {
      if (_distributeOn != nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_NOT_IMPLEMENTED,
            "PlanSnippet Join-Above not implemented for " +
                node->getTypeString() + " if distributeOn is set");
      }

      if (!_isOnCoordinator) {
        auto calc = ExecutionNode::castTo<CalculationNode*>(node);
        // TODO for simplicity we could move this into CalculationNode.
        bool const isOneShard =
            calc->plan()->getAst()->query().vocbase().isOneShard();
        if (calc->expression()->willUseV8() ||
            !calc->expression()->canRunOnDBServer(isOneShard)) {
          // We cannot join with a calculation that is forced to be on
          // coordinators
          break;
        }
      }

      // Nodes that can be trivially joined, and have no effect on Sharding
      _last = node;
      didJoin = true;
      break;
    }
    case ExecutionNode::REMOTESINGLE:
    case ExecutionNode::ASYNC:
    case ExecutionNode::MUTEX:
    case ExecutionNode::WINDOW:
    case ExecutionNode::SUBQUERY:
    case ExecutionNode::SUBQUERY_END:
    case ExecutionNode::SUBQUERY_START:
    case ExecutionNode::RETURN:
    case ExecutionNode::NORESULTS:
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS:
    case ExecutionNode::UPSERT:
    case ExecutionNode::MATERIALIZE:
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      // Nodes that Are not handled yet
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "PlanSnippet Join-Above not implemented for " +
              node->getTypeString());
      break;
    }

    case ExecutionNode::MAX_NODE_TYPE_VALUE:
    case ExecutionNode::GATHER:
    case ExecutionNode::REMOTE:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::SCATTER:
    case ExecutionNode::SINGLETON: {
      // These Cluster Nodes will be added by this rule.
      // They are not allowed to be present in the Plan yet.
      // MaxNode is always disallowed, and SINGLETON can only
      // be the topMost on the first snippet, so cannot be
      // added on downward joins
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL_AQL,
          "PlanSnippet Join-below triggered non-supported Node " +
              node->getTypeString());
    }
  }
  assertInvariants();
  return didJoin;
}

void PlanSnippet::assertInvariants() const {
#if ARANGODB_ENABLE_MAINTAINER_MODE
  // The top of the snippet has to end somewhere
  TRI_ASSERT(_topMost != nullptr);
  // The bottom of the snippet has to end somewhere
  TRI_ASSERT(_last != nullptr);
  // We can never work on two plans at the same time here
  TRI_ASSERT(_last->plan() == _topMost->plan());
  // If we have a distributeOn information, we have to be on DBServer
  TRI_ASSERT(_distributeOn == nullptr || !_isOnCoordinator);
  // The communications nodes can only be inserted as the last step.
  // After the call this snippet cannot use any of it's operations anymore.
  TRI_ASSERT(!_communicationNodesInserted);
#endif
}

void PlanSnippet::insertCommunicationNodes() {
  // has to fullful all variants on first call.
  // Will violate variants on second call, and after
  // this method is completed (_communicationNodesInserted == true);
  assertInvariants();

  if (!_isOnCoordinator) {
    // The DBServer snippets decide how they want to be comunicated to.
    // Coordinator snippets do not need to modify the plan with communication
    // nodes
    if (_topMost->getType() != ExecutionNode::SINGLETON) {
      // If we end in singleton, there is no call back to coordinator required.
      // Otherwise, use Remote to connect to Coordinator
      // And place a Distribute or Scatter operation on the Coordinator
      addRemoteAbove();
      addDistributeAbove();
    }

    // Use Remote to go back to Coordinator
    // On coordinator deploy a GatherNode, to merge data back into single
    // stream.
    addCollectBelow();
    addRemoteBelow();
    addGatherBelow();
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _communicationNodesInserted = true;
#endif
}

void PlanSnippet::addRemoteAbove() {
  auto* plan = _topMost->plan();
  TRI_vocbase_t* vocbase = &plan->getAst()->query().vocbase();
  auto remoteNode =
      plan->createNode<RemoteNode>(plan, plan->nextId(), vocbase, "", "", "");
  TRI_ASSERT(remoteNode);
  plan->insertBefore(_topMost, remoteNode);
  _topMost = remoteNode;
}

void PlanSnippet::addDistributeAbove() {
  auto* plan = _topMost->plan();
  if (_distributeOn == nullptr) {
    // Scatter variant, we cannot distribute based on any known value
    auto scatterNode = plan->createNode<ScatterNode>(
        plan, plan->nextId(), ScatterNode::ScatterType::SHARD);
    TRI_ASSERT(scatterNode);
    plan->insertBefore(_topMost, scatterNode);
    _topMost = scatterNode;
  } else {
    auto distNode = _distributeOn->createDistributeNode(plan);
    plan->insertBefore(_topMost, distNode);
    _topMost = distNode;
    auto distInputNode = _distributeOn->createDistributeInputNode(plan);
    plan->insertBefore(_topMost, distInputNode);
    _topMost = distInputNode;
  }
}

void PlanSnippet::addRemoteBelow() {
  auto* plan = _topMost->plan();
  TRI_vocbase_t* vocbase = &plan->getAst()->query().vocbase();
  auto remoteNode =
      plan->createNode<RemoteNode>(plan, plan->nextId(), vocbase, "", "", "");
  TRI_ASSERT(remoteNode);
  plan->insertAfter(_last, remoteNode);
  _last = remoteNode;
}

void PlanSnippet::addCollectBelow() {
  auto* plan = _topMost->plan();
  auto collectNode = _gatherOutput.eventuallyCreateCollectNode(plan);
  if (collectNode != nullptr) {
    plan->insertAfter(_last, collectNode);
    _last = collectNode;
  }
}

void PlanSnippet::addGatherBelow() {
  auto* plan = _topMost->plan();
  auto gatherNode = _gatherOutput.createGatherNode(plan);
  TRI_ASSERT(gatherNode);
  plan->insertAfter(_last, gatherNode);
  _last = gatherNode;
}

bool PlanSnippet::canStealTopNode() const {
  if (isLastNodeInSnippet(_topMost)) {
    // We cannot steal the last node, we would need to join the snippet
    return false;
  }
  // Nothing prevents stealing, so yes you can take it if you like
  return true;
}

bool PlanSnippet::isLastNodeInSnippet(ExecutionNode const* node) const {
  return node == _last;
}

void PlanSnippet::stealTopNode() {
  // Can only steal on valid snippets
  assertInvariants();
  // Can only steal if we are allowed to steal :grin:
  TRI_ASSERT(canStealTopNode());

  // Move up the pointer to the parent
  // This is guaranteed to not be nullptr
  // as the root node of the query is a _last node
  // and therefore not stealable.
  _topMost = _topMost->getFirstParent();
  assertInvariants();
}

ExecutionNode* PlanSnippet::getLowestNode() const { return _last; }

bool PlanSnippet::isOnCoordinator() const { return _isOnCoordinator; }

std::ostream& arangodb::aql::operator<<(std::ostream& os,
                                        PlanSnippet const& s) {
  return os << s._last->getTypeString() << "(" << s._last->id() << ") -> "
            << s._topMost->getTypeString() << "(" << s._topMost->id()
            << ") located on "
            << (s._isOnCoordinator ? "Coordinator" : "DBServer");
}
