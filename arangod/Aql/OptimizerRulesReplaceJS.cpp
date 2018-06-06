////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

namespace {

  enum class JSFunctionType {
    Near,
    Within,
    FullText
  };

  //NEAR(coll, 0 /*lat*/, 0 /*lon*/[, 10 /*limit*/])
  struct nearParams{
    std::string collection;
    double latitude;
    double longitude;
    std::size_t limit;
    std::string distanceName;

    nearParams(AstNode const* node){
      TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
      AstNode* arr = node->getMember(0);
      TRI_ASSERT(arr->type == AstNodeType::NODE_TYPE_ARRAY);
      collection = arr->getMember(0)->getString();
      latitude = arr->getMember(1)->getDoubleValue();
      longitude = arr->getMember(2)->getDoubleValue();
      limit = arr->getMember(3)->getIntValue();
      if(arr->numMembers() > 4){
        distanceName = arr->getMember(4)->getString();
      }
    }
  };

  //FULLTEXT(collection, "attribute", "search", 100 /*limit*/[, "distance name"])
  struct fulltextParams{
    std::string collection;
    std::string attribute;
    std::string search;
    AstNode* limit;

    fulltextParams(AstNode const* node){
      TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
      AstNode* arr = node->getMember(0);
      TRI_ASSERT(arr->type == AstNodeType::NODE_TYPE_ARRAY);
      collection = arr->getMember(0)->getString();
      attribute = arr->getMember(1)->getString();
      search = arr->getMember(2)->getString();
      if(arr->numMembers() > 3){
        limit = arr->getMember(3);
      } else {
        limit = nullptr;
      }
    }
  };

  using NodeInfo = std::tuple<CalculationNode*, AstNode*, JSFunctionType>;
  constexpr int c_calcNode = 0;
  constexpr int c_astNode = 1; //node to function call (near/within/fulltext)
  constexpr int c_type = 2;

  AstNode* getAstNode(CalculationNode* c){
    return c->expression()->nodeForModification();
  }

  Function* getFunction(AstNode const* ast){
    if (ast->type == AstNodeType::NODE_TYPE_FCALL){
      return static_cast<Function*>(ast->getData());
    }
    return nullptr;
  }



  std::vector<NodeInfo>
  getCandidates(ExecutionPlan* plan){
    std::vector<NodeInfo> rv;

    SmallVector<ExecutionNode*>::allocator_type::arena_type a;
    SmallVector<ExecutionNode*> nodes{a};
    plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

    for(auto const& n : nodes){
      auto* c = static_cast<CalculationNode*>(n);
      auto visitor = [&rv,&c](AstNode* a){
        auto* fun = getFunction(a);
        if(fun){
          LOG_DEVEL << "loop " << fun->name;
          if (fun->name == std::string("NEAR")){
            rv.emplace_back(c, a, JSFunctionType::Near);
          }
          if (fun->name == std::string("WITHIN")){
            rv.emplace_back(c, a, JSFunctionType::Within);
          }
          if (fun->name == std::string("FULLTEXT")){
            rv.emplace_back(c, a, JSFunctionType::FullText);
          }
        }
        return a;
      };
      Ast::traverseAndModify(getAstNode(c),visitor);
      // cat not be read only because we want a non-const
      // pointer in the tuple
    }
    return rv;
  }

  bool replaceNear(NodeInfo info, ExecutionPlan* plan){
    auto* astNode = std::get<c_astNode>(info);
    auto* fun = getFunction(astNode);

    LOG_DEVEL << fun->name;
    nearParams params(astNode);

    // get logical
    // find out if index is available

    // replace calculation node (add subquery)
    //    Do this for all instnaces of NEAR, WITHIN, FULLTEXT in calculation.
    // create subquery (with index node - limit node - add distance name to result)
    // delete function call form calculation node

    LOG_DEVEL << "collection: " << params.collection;

    return true;
  }

  bool replaceWithin(NodeInfo info, ExecutionPlan* plan){
    LOG_DEVEL << "replaceNear";
    auto* cAst = std::get<c_astNode>(info);
    auto* fun = getFunction(cAst);

    LOG_DEVEL << fun->name;

    return true;
  }

  bool replaceFulltext(NodeInfo info, ExecutionPlan* plan){
    LOG_DEVEL << "replaceNear";
    auto* cAst = std::get<c_astNode>(info);
    auto* fun = getFunction(cAst);

    LOG_DEVEL << fun->name;
    // store params as ast nodes?
    fulltextParams params(cAst);
    LOG_DEVEL << params.collection;
    LOG_DEVEL << params.attribute;
    LOG_DEVEL << params.search;

    auto ast = plan->getAst();
    CalculationNode* calcNode = std::get<c_calcNode>(info);

    //// create subquery plan for fulltext

    AstNode* subRootNode = ast->createNodeSubquery();


    // create node for index [and limit]
    AstNode* dummy = nullptr;

    AstNode* indexAstNode = ast->createNodeLimit(dummy/*offset*/,params.limit/*count*/);
    AstNode* resultExpression = indexAstNode;

    if(params.limit){
      AstNode* limitAstNode = ast->createNodeLimit(dummy,dummy);
      indexAstNode->addMember(limitAstNode);
      resultExpression = limitAstNode;
    }

    // Result expression must contain the calculations result
    // provided by fulltext index and optional limit
    // is limit correct?


    AstNode* subReturnNode = ast->createNodeReturn(resultExpression);
    subRootNode->addMember(subReturnNode);

    //// create real plan from ast
    ExecutionNode* subquery = plan->fromNode(subRootNode);
    Variable* outVariable = plan->getAst()->variables()->createTemporaryVariable();
    SubqueryNode* subqueryNode = plan->registerSubquery(
        new SubqueryNode(plan, plan->nextId(), subquery, outVariable)
    );
    // outvariable let outvar = SUBQUERY(.....)

    plan->insertDependency(calcNode, subqueryNode); // why do we need this
                                                    // node is registed and
                                                    // the outvariable will
                                                    // be put in the original
                                                    // CN what should already
                                                    // define the dependency

    // FIXME - expression gets only replaced if function ast node is
    // the first in calculation node's expression
    if(std::get<c_astNode>(info) == getAstNode(std::get<c_calcNode>(info))) {
      auto* replacement = ast->createNodeReference(outVariable);
      std::get<c_calcNode>(info)->expression()->replaceNode(replacement);
    } else {
      // IMPLEMENT
      // what are examples of this case? fulltext eg is not meant to be used in a filter.
      // query: ""
    }

    return true;
  }

}

void arangodb::aql::replaceJSFunctions(Optimizer* opt
                                      ,std::unique_ptr<ExecutionPlan> plan
                                      ,OptimizerRule const* rule){

  bool modified = false;

  auto candidates = getCandidates(plan.get());
  for(auto& candidate : candidates){
    switch (std::get<c_type>(candidate)) {
      case JSFunctionType::FullText:
        modified = replaceFulltext(candidate, plan.get());
        break;
      case JSFunctionType::Near:
        modified = replaceNear(candidate, plan.get());
        break;
      case JSFunctionType::Within:
        modified = replaceWithin(candidate, plan.get());
        break;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);

}; // replaceJSFunctions




