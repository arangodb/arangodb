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
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Collection.h"
#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/ExecutionNode/MaterializeNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/InsertNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/RemoveNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNode/ShortestPathNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionNode/UpdateNode.h"
#include "Aql/ExecutionNode/UpsertNode.h"
#include "Aql/ExecutionNode/WindowNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/SortElement.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Containers/FlatHashSet.h"
#include "Containers/SmallUnorderedMap.h"
#include "Containers/SmallVector.h"
#include "Geo/GeoParams.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"

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
