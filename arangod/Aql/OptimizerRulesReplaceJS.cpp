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
    std::uint64_t limit;

    fulltextParams(AstNode const* node){
      TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
      AstNode* arr = node->getMember(0);
      TRI_ASSERT(arr->type == AstNodeType::NODE_TYPE_ARRAY);
      collection = arr->getMember(0)->getString();
      attribute = arr->getMember(1)->getString();
      if(arr->numMembers() > 3){
        limit = arr->getMember(3)->getIntValue();
      } else {
        limit = 0;
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
    auto* ast = plan->getAst();
    auto* query = ast->query();
    auto* trx = query->trx();
    nearParams params(funAstNode);
    (void) trx;


    LOG_DEVEL << "replaceNear";
    auto* fun = getFunction(funAstNode);
    LOG_DEVEL << fun->name;
    LOG_DEVEL << "collection: " << params.collection;

    return nullptr;
  }

  AstNode* replaceWithin(AstNode* funAstNode, ExecutionPlan* plan){
    auto* ast = plan->getAst();
    auto* query = ast->query();
    auto* trx = query->trx();
    (void) trx;

    LOG_DEVEL << "replaceWithin";
    auto* fun = getFunction(funAstNode);
    LOG_DEVEL << fun->name;
    return nullptr;
  }

  AstNode* replaceFullText(AstNode* funAstNode, ExecutionNode* calcNode, ExecutionPlan* plan){
    auto* ast = plan->getAst();
    auto* query = ast->query();
    auto* trx = query->trx();

    fulltextParams params(funAstNode);

    //// create subquery plan for fulltext
    //
    //    singleton
    //        |
    //      index
    //        |
    //     [limit]
    //        |
    //      return


    /// index
    //  we create this first as creation of this node is more
    //  likely to fail than the creation of other nodes

    //  index - part 1 - figure out index to use
    std::shared_ptr<arangodb::Index> index = nullptr;
		try {
			std::vector<basics::AttributeName> field;
			TRI_ParseAttributeString(params.attribute, field, false);
			auto indexes = trx->indexesForCollection(params.collection);
			for(auto& idx : indexes){
				if(idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX) {
					if(basics::AttributeName::isIdentical(idx->fields()[0], field, false /*ignore expansion in last?!*/)) {
						index = idx;
						break;
					}
				}
			}
		} catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AQL) << "Error while looking for collection ("
                                  << params.collection << "): "
                                  << e.what();
    } catch (...){
      LOG_TOPIC(ERR, Logger::AQL) << "Error while looking for collection ("
                                  << params.collection << ")";
    }

		if(!index){ // not found or error
      LOG_DEVEL << "no valid index found";
			return nullptr;
		}

    // index part 2 - get remaining vars required for index creation
    auto& vocbase = trx->vocbase();
    auto* aqlCollection = query->collections()->get(params.collection);
		auto condition = std::make_unique<Condition>(ast);
		condition->andCombine(funAstNode);
		// create a fresh out variable
    Variable* indexOutVariable = ast->variables()->createTemporaryVariable();

    ExecutionNode* eIndex = plan->registerNode(
      new IndexNode(
        plan,
        plan->nextId(),
        &vocbase,
        aqlCollection,
        indexOutVariable,
        std::vector<transaction::Methods::IndexHandle> {
					transaction::Methods::IndexHandle{index}
        },
        std::move(condition),
        IndexIteratorOptions()
      )
    );

    /// singleton
    ExecutionNode* eSingleton = plan->registerNode(
        new SingletonNode(plan, plan->nextId())
    );

    /// return
    ExecutionNode* eReturn = plan->registerNode(
        // link output of index with the return node
        new ReturnNode(plan, plan->nextId(), indexOutVariable)
    );

    /// link nodes together
    eIndex->addDependency(eSingleton);
    eReturn->addDependency(eIndex);

    /// add optional limit node
    if(params.limit) {
      ExecutionNode* eLimit = plan->registerNode(
        new LimitNode(plan, plan->nextId(), 0 /*offset*/, params.limit)
      );
      plan->insertAfter(eIndex, eLimit); // inject into plan
    }

    /// create subquery
    Variable* subqueryOutVariable = ast->variables()->createTemporaryVariable();
    ExecutionNode* eSubquery = plan->registerSubquery(
        new SubqueryNode(plan, plan->nextId(), eReturn, subqueryOutVariable)
    );

    auto* dep = calcNode->getFirstDependency();
    calcNode->replaceDependency(dep, eSubquery);
    eSubquery->addDependency(dep);

    // this replaces the FunctionCall-AstNode in the
    // expression of the calculation node.
    return ast->createNodeReference(subqueryOutVariable);
  }

} // namespace

void arangodb::aql::replaceJSFunctions(Optimizer* opt
                                      ,std::unique_ptr<ExecutionPlan> plan
                                      ,OptimizerRule const* rule){

  bool modified = false;

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for(auto const& node : nodes){
    auto visitor = [&modified, &node, &plan](AstNode* astnode){
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
          replacement = replaceFullText(astnode,node,plan.get());
        }
      }
      if (replacement) {
        LOG_DEVEL << "replace";
        modified = true;
        return replacement;
      }
      return astnode;
    };

    // replace root node if it was modified
    // TraverseAndModify has no access to roots parent
    CalculationNode* calc = static_cast<CalculationNode*>(node);
    auto* original = getAstNode(calc);
    auto* replacement = Ast::traverseAndModify(original,visitor);

    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);

}; // replaceJSFunctions
