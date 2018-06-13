
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRules.h"
#include "Aql/ClusterNodes.h"
#include "Aql/CollectNode.h"
#include "Aql/CollectOptions.h"
#include "Aql/Collection.h"
#include "Aql/ConditionFinder.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/ShortestPathNode.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"
#include "Aql/TraversalConditionFinder.h"
#include "Aql/TraversalNode.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/SmallVector.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Cluster/ClusterInfo.h"
#include "Geo/GeoParams.h"
#include "GeoIndex/Index.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"
#include "VocBase/Methods/Collections.h"

#include <boost/optional.hpp>
#include <tuple>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

////////////////////////////////////////////////////////////////////////////////
struct DetectSingleDocumentOperations final : public WalkerWorker<ExecutionNode> {
  ExecutionPlan* _plan;
  IndexNode* _indexNode;
  UpdateNode* _updateNode;
  ReplaceNode* _replaceNode;
  RemoveNode* _removeNode;

 public:
  explicit DetectSingleDocumentOperations(ExecutionPlan* plan)
      : _plan(plan),
        _indexNode(nullptr),
        _updateNode(nullptr),
        _replaceNode(nullptr),
        _removeNode(nullptr)
  {}

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* en) override final {
    switch (en->getType()) {
      case EN::TRAVERSAL:
      case EN::SHORTEST_PATH:
      case EN::ENUMERATE_LIST:
#ifdef USE_IRESEARCH
      case EN::ENUMERATE_IRESEARCH_VIEW:
#endif
      case EN::SUBQUERY:
      case EN::FILTER:
      case EN::ENUMERATE_COLLECTION:
      case EN::CALCULATION:
      case EN::COLLECT:
      case EN::INSERT:
      case EN::UPSERT:
      case EN::NORESULTS:
      case EN::SCATTER:
      case EN::DISTRIBUTE:
      case EN::GATHER:
      case EN::REMOTE:
      case EN::SORT:
      case EN::LIMIT:  // LIMIT is criterion to stop
        _indexNode = nullptr;
        return true;   // abort.

      case EN::UPDATE:
        _updateNode = ExecutionNode::castTo<UpdateNode*>(en);
        return false;
      case EN::REPLACE:
        _replaceNode = ExecutionNode::castTo<ReplaceNode*>(en);
        return false;
      case EN::REMOVE:
        _removeNode =  ExecutionNode::castTo<RemoveNode*>(en);
        return false;
      case EN::SINGLETON:
      case EN::RETURN:
        return false;
      case EN::INDEX:
        if (_indexNode == nullptr) {
          _indexNode = ExecutionNode::castTo<IndexNode*>(en);
          auto node = _indexNode->condition()->root();

          if ((node->type == NODE_TYPE_OPERATOR_NARY_OR) && (node->numMembers() == 1)) {
            auto subNode = node->getMemberUnchecked(0);
            if ((subNode->type == NODE_TYPE_OPERATOR_NARY_AND) && (subNode->numMembers() == 1)) {
              subNode = node->getMemberUnchecked(0);
              if ((subNode->type == NODE_TYPE_OPERATOR_BINARY_EQ) && (subNode->numMembers() == 2)) {
                return false;
              }
            }
          }
          return true;
        }
        else {
          _indexNode = nullptr;
          return true;
        }

      default: {
        // should not reach this point
        _indexNode = nullptr;
        TRI_ASSERT(false);
      }
    }
    return true;
  }
};

void arangodb::aql::substituteClusterSingleDocumentOperations(Optimizer* opt,
                                                              std::unique_ptr<ExecutionPlan> plan,
                                                              OptimizerRule const* rule) {
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::FILTER, true);

  bool modified = false;

  ExecutionNode* firstNode = nodes[0];

  DetectSingleDocumentOperations finder(plan.get());
  firstNode->walk(finder);

  if (finder._indexNode != nullptr) {
    modified = true;
    auto roNode = new SingleRemoteOperationNode(finder._indexNode,
                                                finder._updateNode,
                                                finder._replaceNode,
                                                finder._removeNode);
    plan->replaceNode(finder._indexNode, roNode);

    if (finder._updateNode != nullptr) {
      plan->unlinkNode(finder._updateNode);
    }
    if (finder._replaceNode != nullptr) {
      plan->unlinkNode(finder._replaceNode);
    }
    if (finder._removeNode != nullptr) {
      plan->unlinkNode(finder._removeNode);
    }

  }

  opt->addPlan(std::move(plan), rule, modified);
}
////////////////////////////////////////////////////////////////////////////////
