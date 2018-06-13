
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Willi & Obi
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

bool depIsSingletonOrConstCalc(ExecutionNode* node){
    while (node){
      node = node->getFirstDependency();
      if(node->getType() == EN::SINGLETON){
        LOG_DEVEL << "reached singleton";
        return true;
      }

      if(node->getType() != EN::CALCULATION){
        LOG_DEVEL << node->getTypeString() << " not a clac node";
        return false;
      }
      if(!static_cast<CalculationNode*>(node)->arangodb::aql::ExecutionNode::getVariablesUsedHere().empty()){
        LOG_DEVEL << "calculation not constant";
        return false;
      }
    }
   return false;
}

void arangodb::aql::substituteClusterSingleDocumentOperations(Optimizer* opt,
                                                              std::unique_ptr<ExecutionPlan> plan,
                                                              OptimizerRule const* rule) {
  bool modified = false;
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::INDEX, true);

  if(nodes.size() != 1){ //more than one index - we replace simple expressions only
    opt->addPlan(std::move(plan), rule, modified);
    return;
  }

  for(auto* node : nodes){
    LOG_DEVEL << "";
    LOG_DEVEL << "substitute single document operation";
    if(!depIsSingletonOrConstCalc(node)){
      LOG_DEVEL << "dep is not singleton or const claculation";
      continue;
    }

    Index* index = hasSingleIndexHandle(node, Index::TRI_IDX_TYPE_PRIMARY_INDEX);
    if (index){
      if(!hasBinaryCompare(node)){
        // do nothing if index does not work on a single document
        LOG_DEVEL << "has no valid compare";
        continue;
      }
      auto* parentModification = hasSingleParent(node,{EN::INSERT, EN::REMOVE, EN::UPDATE, EN::UPSERT, EN::REPLACE});
      auto* parentSelect = hasSingleParent(node,EN::RETURN);

      if (parentModification){
        LOG_DEVEL << "optimize modification node";
        auto parentType = parentModification->getType();
          LOG_DEVEL << ExecutionNode::getTypeString(parentType);
          if (parentType == EN::RETURN){

          //check that opeation uses the document provided by the index
          //check that calculation is used in the modification

          } else if ( parentType == EN::INSERT) {
          } else if ( parentType == EN::REMOVE) {
          } else if ( parentType == EN::UPDATE) {
          } else if ( parentType == EN::UPSERT) {
          } else if ( parentType == EN::REPLACE) {
          }
      } else if(parentSelect){
        LOG_DEVEL << "optimize SELECT";
      } else {
        LOG_DEVEL << "plan following the index node is too complex";
      }
    } else {
      LOG_DEVEL << "is not primary or has more indexes";
    }
  }
  opt->addPlan(std::move(plan), rule, modified);
}
