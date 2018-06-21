
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
/// @author Wilfried Goesgens
/// @author Jan Christoph Uhde
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

// in plan below
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

// in plan above
ExecutionNode* hasSingleDep(ExecutionNode* in){
  auto deps = in->getDependencies();
  if(deps.size() == 1){
    return deps.front();
  }
  return nullptr;
}

ExecutionNode* hasSingleDep(ExecutionNode* in, EN::NodeType type){
  auto* dep = hasSingleDep(in);
  if(dep) {
    if(dep->getType() == type){
      return dep;
    }
  }
  return nullptr;
}

ExecutionNode* hasSingleDep(ExecutionNode* in, std::vector<EN::NodeType> types){
  auto* dep = hasSingleDep(in);
  if(dep) {
    for(auto const& type : types){
      if(dep->getType() == type){
        return dep;
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
  if (idx && idx->type() == type ){
    return idx;
  }
  return nullptr;
}

std::vector<AstNode const*> hasBinaryCompare(ExecutionNode* node){
  // returns any AstNode in the expression that is
  // a binary comparison.
  TRI_ASSERT(node->getType() == EN::INDEX);
  IndexNode* indexNode = static_cast<IndexNode*>(node);
  AstNode const* cond = indexNode->condition()->root();
  std::vector<AstNode const*> result;

  auto preVisitor = [&result] (AstNode const* node) {
    if (node == nullptr) {
      return false;
    };

    if(node->type == NODE_TYPE_OPERATOR_BINARY_EQ){
      result.push_back(node);
      return false;
    }

    //skip over NARY AND/OR
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

std::string getFirstKey(std::vector<AstNode const*> compares){
  for(auto const* node : compares){
    AstNode const* keyNode = node->getMemberUnchecked(0);
    if(keyNode->type == AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS && keyNode->stringEquals("_key")) {
      keyNode = node->getMemberUnchecked(1);
    }
    if (keyNode->isStringValue()){
      return keyNode->getString();
    }
  }
  return "";
}

bool depIsSingletonOrConstCalc(ExecutionNode* node){
  while (node){
    node = node->getFirstDependency();
    if(node->getType() == EN::SINGLETON){
      //LOG_DEVEL << "reached singleton";
      return true;
    }

    if(node->getType() != EN::CALCULATION){
      //LOG_DEVEL << node->getTypeString() << " not a calculation node";
      return false;
    }

    if(!static_cast<CalculationNode*>(node)->arangodb::aql::ExecutionNode::getVariablesUsedHere().empty()){
      //LOG_DEVEL << "calculation not constant";
      return false;
    }
  }
  return false;
}

bool parentIsReturnOrConstCalc(ExecutionNode* node){
  node = node->getFirstParent();
  while (node){
    auto type = node->getType();
    // we do not need to check the order because
    // we expect a valid plan
    if(type != EN::CALCULATION && type != EN::RETURN){
      return false;
    }
    node = node->getFirstParent();
  }
  return true;
}

void replaceNode(ExecutionPlan* plan, ExecutionNode* oldNode, ExecutionNode* newNode){
  if(oldNode == plan->root()) {
    for(auto* dep : oldNode->getDependencies()) {
      newNode->addDependency(dep);
    }
    LOG_DEVEL << "replacing root node";
    plan->root(newNode,true);
  } else {
    // replaceNode does not seem to work well with subqueries
    // if the subqueries root is replaced.
    // It looks like Subquery node will still point to
    // the old node.

    TRI_ASSERT(oldNode != plan->root());
    LOG_DEVEL << "replacing " << oldNode->getTypeString();
    LOG_DEVEL << "with " << newNode->getTypeString();
    plan->replaceNode(oldNode, newNode);
    TRI_ASSERT(!oldNode->hasDependency());
    TRI_ASSERT(!oldNode->hasParent());
  }
}

bool substituteClusterSingleDocumentOperationsIndex(Optimizer* opt,
                                                    ExecutionPlan* plan,
                                                    OptimizerRule const* rule) {
  bool modified = false;
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, EN::INDEX, false);

  if(nodes.size() != 1){
    //LOG_DEVEL << "plan has " << nodes.size() << "!=1 index nodes";
    return modified;
  }

  for(auto* node : nodes){
    LOG_DEVEL << "substitute single document operation INDEX";

    if(!depIsSingletonOrConstCalc(node)){
      LOG_DEVEL << "dependency is not singleton or const calculation";
      continue;
    }

    Index* index = hasSingleIndexHandle(node, Index::TRI_IDX_TYPE_PRIMARY_INDEX);
    if (index){
      IndexNode* indexNode = static_cast<IndexNode*>(node);
      auto binaryCompares = hasBinaryCompare(node);
      std::string key = getFirstKey(binaryCompares);
      if(key.empty()){
        LOG_DEVEL << "could not extract key from index condition";
        continue;
      }

      auto* parentModification = hasSingleParent(node,{EN::INSERT, EN::REMOVE, EN::UPDATE, EN::REPLACE});

      if (parentModification){
        auto mod = static_cast<ModificationNode*>(parentModification);
        auto parentType = parentModification->getType();
        auto const& vec = mod->getVariablesUsedHere();

        LOG_DEVEL << "optimize modification node of type: "
                  << ExecutionNode::getTypeString(parentType)
                  << "  " << vec.size();

        Variable const* update = nullptr;
        Variable const* keyVar = nullptr;


        if ( parentType == EN::REMOVE) {
          keyVar = vec.front();
          TRI_ASSERT(vec.size() == 1);
        } else {
          update = vec.front();
          if(vec.size() > 1){
            keyVar = vec.back();
          }
        }

        if(keyVar && indexNode->outVariable()->id != keyVar->id){
          LOG_DEVEL << "the index out var is not used in modification as keyVariable";
          continue;
        }

        if(!keyVar){
          if (update && indexNode->outVariable()->id == update->id){
            // the update document is already described by the key provided
            // in the index condition
            update = nullptr;
          } else {
            LOG_DEVEL << "the index out var is not used in modification as keyVariable";
            continue;
          }
        }

        LOG_DEVEL << "key:" << key;
        LOG_DEVEL_IF(update) << "update" << update->name;
        ExecutionNode* singleOperationNode = plan->registerNode(
          new SingleRemoteOperationNode(
            plan, plan->nextId(),
            parentType,
            key, mod->collection(),
            mod->getOptions(),
            update,
            nullptr,
            mod->getOutVariableOld(),
            mod->getOutVariableNew()
          )
        );

        LOG_DEVEL << "mod is modification node" << mod->isModificationNode();

        if(!parentIsReturnOrConstCalc(mod)){
          LOG_DEVEL << "parents are not calc* return";
          continue;
        }

        replaceNode(plan, mod, singleOperationNode);
        plan->unlinkNode(indexNode);
        modified = true;
      } else if(parentIsReturnOrConstCalc(node)){
        LOG_DEVEL << "optimize SELECT with key: " << key;

        ExecutionNode* singleOperationNode = plan->registerNode(
            new SingleRemoteOperationNode(plan, plan->nextId()
                                         ,EN::INDEX, key, indexNode->collection(), ModificationOptions{}
                                         , nullptr /*in*/ , indexNode->outVariable() /*out*/, nullptr /*old*/, nullptr /*new*/)
        );
        replaceNode(plan, indexNode, singleOperationNode);
        modified = true;

      } else {
        LOG_DEVEL << "plan following the index node is too complex";
      }
    } else {
      LOG_DEVEL << "is not primary or has more indexes";
    }
  }
  return modified;
}

bool substituteClusterSingleDocumentOperationsNoIndex(Optimizer* opt,
                                                      ExecutionPlan* plan,
                                                      OptimizerRule const* rule) {
  bool modified = false;
  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, {EN::INSERT, EN::REMOVE, EN::UPDATE, EN::REPLACE}, false);

  if(nodes.size() != 1){
    //LOG_DEVEL << "plan has " << nodes.size() << "!=1 modification nodes";
    return modified;
  }

  for(auto* node : nodes){
    LOG_DEVEL << "substitute single document operation NO INDEX";

    auto mod = static_cast<ModificationNode*>(node);

    if(!depIsSingletonOrConstCalc(node)){
      LOG_DEVEL << "optimization too complex (debIsSingleOrConstCalc)";
      continue;
    }

    if(!parentIsReturnOrConstCalc (node)){
      LOG_DEVEL << "parents are not [calc]*->[return]";
      continue;
    }

    auto depType = mod->getType();
    Variable const* update = nullptr;
    Variable const* keyVar = nullptr;
    std::string key = "";
    auto const& vec = mod->getVariablesUsedHere();

    LOG_DEVEL << "optimize modification node of type: "
              << ExecutionNode::getTypeString(depType)
              << "  " << vec.size();

    if ( depType == EN::REMOVE) {
      keyVar = vec.front();
      TRI_ASSERT(vec.size() == 1);
    } else {
      update = vec.front(); // this can be same as keyvar!
                            // this use is possible because we
                            // not delete the variable's setter
      if (vec.size() > 1) {
        keyVar = vec.back();
      }
    }

    LOG_DEVEL_IF(update) << "inVariable id: " << update->name;
    LOG_DEVEL_IF(keyVar) << "keyVariable id: " << keyVar->name;

    ExecutionNode* cursor = node;
    CalculationNode* calc = nullptr;

    if(keyVar){
      LOG_DEVEL << "inspecting keyVar";
      std::unordered_set<Variable const*> keySet;
      keySet.emplace(keyVar);

      while(cursor){
        cursor = hasSingleDep(cursor, EN::CALCULATION);
        if(cursor){
          CalculationNode* c = static_cast<CalculationNode*>(cursor);
          if(c->setsVariable(keySet)){
           LOG_DEVEL << "found calculation that sets key-expression";
           calc = c;
           break;
          }
        }
      }

      if(!calc){
        LOG_DEVEL << "calculation missing";
        continue;
      }
      AstNode const* expr = calc->expression()->node();
      if(expr->isStringValue()){
        key = expr->getString();
      } else if (expr->isObject()){
        for(std::size_t i = 0 ; i < expr->numMembers(); i++){
          auto* anode = expr->getMemberUnchecked(i);
          if( anode->getString() == "_key"){
            key = anode->getMember(0)->getString();
            break;
          }
        }
      } else {
        // write more code here if we
        // want to support thinks like:
        //
        // DOCUMENT("foo/bar")
      }

      if(key.empty()){
        LOG_DEVEL << "could not extract key";
        continue;
      }
    }

    if(!depIsSingletonOrConstCalc(cursor)){
      LOG_DEVEL << "dependencies are not [calc]*->[singleton]";
      continue;
    }

    LOG_DEVEL << mod->collection()->name();
    LOG_DEVEL_IF(update) << "inputvar " << update->name;

    ExecutionNode* singleOperationNode = plan->registerNode(
      new SingleRemoteOperationNode(
        plan, plan->nextId(),
        depType,
        key, mod->collection(),
        mod->getOptions(),
        update /*in*/,
        nullptr,
        mod->getOutVariableOld(),
        mod->getOutVariableNew()
      )
    );

    LOG_DEVEL << "mod is modification node" << mod->isModificationNode();
    replaceNode(plan, mod, singleOperationNode);

    if(calc){
      LOG_DEVEL << "unlinkNode clac setting var: " << calc->getVariablesSetHere()[0]->name;
      plan->unlinkNode(calc);
    }
    modified = true;

    if(update) {
      auto setter = plan->getVarSetBy(update->id);
      LOG_DEVEL_IF(!setter) << "setter not found";
      LOG_DEVEL_IF(setter) << "setter for " << update->name << " is of type" << setter->getTypeString();
    }

  } // for node : nodes

  return modified;
}

void arangodb::aql::substituteClusterSingleDocumentOperations(Optimizer* opt,
                                                              std::unique_ptr<ExecutionPlan> plan,
                                                              OptimizerRule const* rule) {
  //log_devel << "enter singleoperationnode rule";
  bool modified = false;

  for(auto const& fun : { &substituteClusterSingleDocumentOperationsIndex
                        , &substituteClusterSingleDocumentOperationsNoIndex
                        }
  ){
    modified = fun(opt, plan.get(), rule);
    if(modified){ break; }
  }

  LOG_DEVEL_IF(modified) << "applied singleOperationNode rule !!!!!!!!!!!!!!!!!";

  //LOG_DEVEL << plan->toVelocyPack(plan->getAst(),true)->toJson();
  opt->addPlan(std::move(plan), rule, modified);
  //LOG_DEVEL << "exit singleOperationNode rule";
}
