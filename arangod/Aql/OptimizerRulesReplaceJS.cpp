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

  AstNode* getAstNode(CalculationNode* c){
    return c->expression()->nodeForModification();
  }

  Function* getFunction(AstNode const* ast){
    if (ast->type == AstNodeType::NODE_TYPE_FCALL){
      return static_cast<Function*>(ast->getData());
    }
    return nullptr;
  }



  AstNode* replaceNear(AstNode* funAstNode, ExecutionPlan* plan){
    auto* fun = getFunction(funAstNode);

    LOG_DEVEL << fun->name;
    nearParams params(funAstNode);

    // get logical
    // find out if index is available

    // replace calculation node (add subquery)
    //    Do this for all instnaces of NEAR, WITHIN, FULLTEXT in calculation.
    // create subquery (with index node - limit node - add distance name to result)
    // delete function call form calculation node

    LOG_DEVEL << "collection: " << params.collection;

    return nullptr;
  }

  AstNode* replaceWithin(AstNode* funAstNode, ExecutionPlan* plan){
    LOG_DEVEL << "replaceNear";
    auto* fun = getFunction(funAstNode);
    LOG_DEVEL << fun->name;
    return nullptr;
  }

  AstNode* replaceFullText(AstNode* funAstNode, ExecutionPlan* plan){
    LOG_DEVEL << "replaceNear";
    auto* fun = getFunction(funAstNode);

    LOG_DEVEL << fun->name;
    // store params as ast nodes?
    fulltextParams params(funAstNode);
    LOG_DEVEL << params.collection;
    LOG_DEVEL << params.attribute;
    LOG_DEVEL << params.search;

    auto ast = plan->getAst();

    //// create subquery plan for fulltext


    ExecutionNode* eSingleton = plan->registerNode(
        new SingletonNode(plan, plan->nextId())
    );

    auto* trx = ast->query()->trx();
    auto& vocbase = trx->vocbase();
    auto aqlCollection = ast->query()->collections()->get(params.collection);
		auto condition = std::make_unique<Condition>(funAstNode->getMember(0));

    std::shared_ptr<arangodb::Index> index = nullptr;
		try {
			std::vector<basics::AttributeName> field;
			TRI_ParseAttributeString(params.attribute, field, false);
			auto indexes = trx->indexesForCollection(params.collection);
			for(auto& idx : indexes){
				if(idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX) {
					if(basics::AttributeName::isIdentical(idx->fields()[0], field, false)){
						index = idx;
						break;
					}
				}
			}
		} catch (...) { }

		if(!index){
			return nullptr;
		}

		// get somewhere else
    Variable* indexInVariable = ast->variables()->createTemporaryVariable();


    ExecutionNode* eIndex =
      new IndexNode(
        plan,
        plan->nextId(),
        &vocbase,
        aqlCollection,
        indexInVariable,
        std::vector<transaction::Methods::IndexHandle> {
					transaction::Methods::IndexHandle{index}
        },
        condition.get(),
        IndexIteratorOptions()
      );
	  plan->registerNode(eIndex);


    Variable* returnInVariable = ast->variables()->createTemporaryVariable(); //FIXME
    ExecutionNode* eReturn = plan->registerNode(
        new ReturnNode(plan, plan->nextId(), returnInVariable)
    );
    plan->registerNode(eReturn);
    eReturn->addDependency(eSingleton);


    Variable* subqueryOutVariable = ast->variables()->createTemporaryVariable();
    plan->registerSubquery(
        new SubqueryNode(plan, plan->nextId(), eReturn, subqueryOutVariable)
    );

    return ast->createNodeReference(subqueryOutVariable);
  }

}

void arangodb::aql::replaceJSFunctions(Optimizer* opt
                                      ,std::unique_ptr<ExecutionPlan> plan
                                      ,OptimizerRule const* rule){

  bool modified = false;

  auto visitor = [&modified,&plan](AstNode* astnode){
    auto* fun = getFunction(astnode);
    AstNode* replacement = nullptr;
    if(fun){
      LOG_DEVEL << "loop " << fun->name;
      if (fun->name == std::string("NEAR")){
        replacement = replaceNear(astnode,plan.get());
      }
      if (fun->name == std::string("WITHIN")){
        replacement = replaceWithin(astnode,plan.get());
      }
      if (fun->name == std::string("FULLTEXT")){
        replacement = replaceFullText(astnode,plan.get());
      }
    }
    if (replacement) {
      modified = true;
      return replacement;
    }
    return astnode;
  };

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for(auto const& node : nodes){
    Ast::traverseAndModify(getAstNode(static_cast<CalculationNode*>(node)),visitor);
  }

  opt->addPlan(std::move(plan), rule, modified);

}; // replaceJSFunctions




