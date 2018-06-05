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

  using NodeInfo = std::tuple<CalculationNode*, AstNode*, JSFunctionType>;
  constexpr int c_cn = 0;
  constexpr int c_an = 1; //node to function call (near/within/fulltext)
  constexpr int c_type = 2;

  AstNode* getAst(CalculationNode* c){
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
      auto* cAst = getAst(c);
      if(cAst->callsFunction()){
        auto* fun = getFunction(cAst);
        if(fun){
          LOG_DEVEL << "loop " << fun->name;
          if (fun->name == std::string("NEAR")){
            rv.emplace_back(c, cAst, JSFunctionType::Near);
          }
          if (fun->name == std::string("WITHIN")){
            rv.emplace_back(c, cAst, JSFunctionType::Within);
          }
          if (fun->name == std::string("FULLTEXT")){
            rv.emplace_back(c, cAst, JSFunctionType::FullText);
          }
        }
      }
    }
    return rv;
  }

  bool replaceNear(NodeInfo info){
    auto* cAst = std::get<c_an>(info);
    auto* fun = getFunction(cAst);

    LOG_DEVEL << fun->name;
    nearParams params(cAst);

    // get logical
    // find out if index is available

    // replace calculation node (add subquery)
    //    Do this for all instnaces of NEAR, WITHIN, FULLTEXT in calculation.
    // create subquery (with index node - limit node - add distance name to result)
    // delete function call form calculation node

    LOG_DEVEL << "collection: " << params.collection;
    LOG_DEVEL << "achtung der willi";

    return true;
  }

  bool replaceWithin(NodeInfo info){
    LOG_DEVEL << "replaceNear";
    auto* cAst = std::get<c_an>(info);
    auto* fun = getFunction(cAst);

    LOG_DEVEL << fun->name;

    return true;
  }

  bool replaceFulltext(NodeInfo info){
    LOG_DEVEL << "replaceNear";
    auto* cAst = std::get<c_an>(info);
    auto* fun = getFunction(cAst);

    LOG_DEVEL << fun->name;

    return true;
  }

}

void arangodb::aql::replaceJSFunctions(Optimizer* opt
                                      ,std::unique_ptr<ExecutionPlan> plan
                                      ,OptimizerRule const* rule){

  bool modified = false;

  auto nodes = getCandidates(plan.get());
  for(auto& node : nodes){
    switch (std::get<c_type>(node)) {
      case JSFunctionType::FullText:
        modified = replaceFulltext(node);
        break;
      case JSFunctionType::Near:
        modified = replaceNear(node);
        break;
      case JSFunctionType::Within:
        modified = replaceWithin(node);
        break;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);

}; // replaceJSFunctions




