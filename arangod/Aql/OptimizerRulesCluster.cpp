
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
ExecutionNode* hasSingleParent(ExecutionNode* in){
  auto parents = in->getParents();
  if(parents.size() == 1){
    return parents.front();
  }
  return nullptr;
}

ExecutionNode* hasSingleParent(ExecutionNode* in, EN::NodeType type){
  auto* parent = hasSingleParent(in);
  if(parent) {
    if(parent->getType() == type){
      return parent;
    }
  }
  return nullptr;
}

ExecutionNode* hasSingleParent(ExecutionNode* in, std::vector<EN::NodeType> types){
  auto* parent = hasSingleParent(in);
  if(parent) {
    for(auto const& type : types){
      if(parent->getType() == type){
        return parent;
      }
    }

  }
  return nullptr;
}

Index* hasSingleIndexHandle(ExecutionNode* node){
  TRI_ASSERT(node->getType() == EN::INDEX);
  IndexNode* indexNode = static_cast<IndexNode*>(node);
  auto indexHandleVec = indexNode->getIndexes();
  if (indexHandleVec.size() == 1){
    return indexHandleVec.front().getIndex().get();
  }
  return nullptr;
}

Index* hasSingleIndexHandle(ExecutionNode* node, Index::IndexType type){
  auto* idx = hasSingleIndexHandle(node);
  if (idx->type() == type ){
    return idx;
  }
  return nullptr;
}

bool hasBinaryCompare(ExecutionNode* node){
  TRI_ASSERT(node->getType() == EN::INDEX);
  IndexNode* indexNode = static_cast<IndexNode*>(node);
  AstNode const* cond = indexNode->condition()->root();

  bool result;

  auto preVisitor = [&result] (AstNode const* node) {
    if (node == nullptr) {
      return false;
    };

    if(node->type == NODE_TYPE_OPERATOR_BINARY_EQ){
      result=true;
      return false;
    }

    //skip over NARY AND AND OR
    if(node->type == NODE_TYPE_OPERATOR_NARY_OR ||
       node->type == NODE_TYPE_OPERATOR_NARY_AND) {
      return true;
    } else {
      return false;
    }

  };
  auto postVisitor = [](AstNode const*){};
  Ast::traverseReadOnly(cond, preVisitor, postVisitor);

  return result;
}

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
          return !hasBinaryCompare(en);
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
  bool modified = false;
  if(false) {
    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> nodes{a};
    plan->findNodesOfType(nodes, EN::FILTER, true);

    ExecutionNode* firstNode = nodes[0];

    DetectSingleDocumentOperations finder(plan.get());
    firstNode->walk(finder);

    if (finder._indexNode != nullptr) {
      modified = true;
      auto roNode = new SingleRemoteOperationNode(finder._indexNode,
                                                  finder._updateNode,
                                                  finder._replaceNode,
                                                  finder._removeNode);
      plan->registerNode(roNode);
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
  }

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::INDEX, true);

  for(auto* node : nodes){
    Index* index = hasSingleIndexHandle(node, Index::TRI_IDX_TYPE_PRIMARY_INDEX);
    if (index){
      IndexNode* indexNode = static_cast<IndexNode*>(node);
      LOG_DEVEL << "has compare";
      if(!hasBinaryCompare(node)){
        // do nothing if index does not work on a single document
        break;
      }
      //check fields and ranges ==
      auto* parentModification = hasSingleParent(indexNode,{EN::INSERT, EN::REMOVE, EN::UPDATE, EN::UPSERT, EN::REPLACE});
      auto* parentSelect = hasSingleParent(indexNode,EN::RETURN);

      if (parentModification){
        LOG_DEVEL << "optimize modification node";
        auto parentType = parentModification->getType();
          LOG_DEVEL << ExecutionNode::getTypeString(parentType);
          if (parentType == EN::RETURN){

          } else if ( parentType == EN::INSERT) {
          } else if ( parentType == EN::REMOVE) {
          } else if ( parentType == EN::UPDATE) {
          } else if ( parentType == EN::UPSERT) {
          } else if ( parentType == EN::REPLACE) {
          }
      } else if(parentSelect){
        LOG_DEVEL << "optimize SELECT";
      }
    }
  }


  opt->addPlan(std::move(plan), rule, modified);
}
////////////////////////////////////////////////////////////////////////////////
