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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesOptimizerRules.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/IndexNode.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/SortNode.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

void MMFilesOptimizerRules::registerResources() {
  // patch update statements
  OptimizerRulesFeature::registerRule("geo-index-optimizer", geoIndexRule,
               OptimizerRule::applyGeoIndexRule, false, true);
}

struct MMFilesGeoIndexInfo {
  operator bool() const { return distanceNode && valid; }
  void invalidate() { valid = false; }
  MMFilesGeoIndexInfo()
    : collectionNode(nullptr)
    , executionNode(nullptr)
    , indexNode(nullptr)
    , setter(nullptr)
    , expressionParent(nullptr)
    , expressionNode(nullptr)
    , distanceNode(nullptr)
    , index(nullptr)
    , range(nullptr)
    , executionNodeType(EN::NORESULTS)
    , within(false)
    , lessgreaterequal(false)
    , valid(true)
    , constantPair{nullptr,nullptr}
    {}
  EnumerateCollectionNode* collectionNode; // node that will be replaced by (geo) IndexNode
  ExecutionNode* executionNode; // start node that is a sort or filter
  IndexNode* indexNode; // AstNode that is the parent of the Node
  CalculationNode* setter; // node that has contains the condition for filter or sort
  AstNode* expressionParent; // AstNode that is the parent of the Node
  AstNode* expressionNode; // AstNode that contains the sort/filter condition
  AstNode* distanceNode; // AstNode that contains the distance parameters
  std::shared_ptr<arangodb::Index> index; //pointer to geoindex
  AstNode const* range; // range for within
  ExecutionNode::NodeType executionNodeType; // type of execution node sort or filter
  bool within; // is this a within lookup
  bool lessgreaterequal; // is this a check for le/ge (true) or lt/gt (false)
  bool valid; // contains this node a valid condition
  std::vector<std::string> longitude; // access path to longitude
  std::vector<std::string> latitude; // access path to latitude
  std::pair<AstNode*,AstNode*> constantPair;
};

//candidate checking

AstNode* isValueOrRefNode(AstNode* node){
  //TODO - implement me
  return node;
}

MMFilesGeoIndexInfo isDistanceFunction(AstNode* distanceNode, AstNode* expressionParent){
  // the expression must exist and it must be a function call
  auto rv = MMFilesGeoIndexInfo{};
  if(distanceNode->type != NODE_TYPE_FCALL) {
    return rv;
  }

  //get the ast node of the expression
  auto func = static_cast<Function const*>(distanceNode->getData());

  // we're looking for "DISTANCE()", which is a function call
  // with an empty parameters array
  if ( func->externalName != "DISTANCE" || distanceNode->numMembers() != 1  ) {
    return rv;
  }
  rv.distanceNode = distanceNode;
  rv.expressionNode = distanceNode;
  rv.expressionParent = expressionParent;
  return rv;
}

MMFilesGeoIndexInfo isGeoFilterExpression(AstNode* node, AstNode* expressionParent){
  // binary compare must be on top
  bool dist_first = true;
  bool lessEqual = true;
  auto rv = MMFilesGeoIndexInfo{};
  if(  node->type != NODE_TYPE_OPERATOR_BINARY_GE
    && node->type != NODE_TYPE_OPERATOR_BINARY_GT
    && node->type != NODE_TYPE_OPERATOR_BINARY_LE
    && node->type != NODE_TYPE_OPERATOR_BINARY_LT) {

    return rv;
  } 
  if (node->type == NODE_TYPE_OPERATOR_BINARY_GE || node->type == NODE_TYPE_OPERATOR_BINARY_GT) {
    dist_first = false;
  }
  if (node->type == NODE_TYPE_OPERATOR_BINARY_GT || node->type == NODE_TYPE_OPERATOR_BINARY_LT) {
    lessEqual = false;
  }

  if (node->numMembers() != 2){
    return rv;
  }

  AstNode* first = node->getMember(0);
  AstNode* second = node->getMember(1);

  auto eval_stuff = [](bool dist_first, bool lessEqual, MMFilesGeoIndexInfo&& dist_fun, AstNode* value_node){
    if (dist_first && dist_fun && value_node) {
      dist_fun.within = true;
      dist_fun.range = value_node;
      dist_fun.lessgreaterequal = lessEqual;
    } else {
      dist_fun.invalidate();
    }
    return dist_fun;
  };

  rv = eval_stuff(dist_first, lessEqual, isDistanceFunction(first, expressionParent), isValueOrRefNode(second));
  if (!rv) {
    rv = eval_stuff(dist_first, lessEqual, isDistanceFunction(second, expressionParent), isValueOrRefNode(first));
  }

  if(rv){
    //this must be set after checking if the node contains a distance node.
    rv.expressionNode = node;
  }

  return rv;
}

MMFilesGeoIndexInfo iterativePreorderWithCondition(EN::NodeType type, AstNode* root, MMFilesGeoIndexInfo(*condition)(AstNode*, AstNode*)){
  // returns on first hit
  if (!root){
    return MMFilesGeoIndexInfo{};
  }
  std::vector<std::pair<AstNode*,AstNode*>> nodestack;
  nodestack.push_back({root, nullptr});

  while(nodestack.size()){
    auto current = nodestack.back();
    nodestack.pop_back();
    MMFilesGeoIndexInfo rv = condition(current.first,current.second);
    if (rv) {
      return rv;
    }

    if (type == EN::FILTER){
      if (current.first->type == NODE_TYPE_OPERATOR_BINARY_AND || current.first->type == NODE_TYPE_OPERATOR_NARY_AND ){
        for (std::size_t i = 0; i < current.first->numMembers(); ++i){
          nodestack.push_back({current.first->getMember(i),current.first});
        }
      }
    } else if (type == EN::SORT) {
      // must be the only sort condition
    }
  }
  return MMFilesGeoIndexInfo{};
}

MMFilesGeoIndexInfo geoDistanceFunctionArgCheck(std::pair<AstNode const*, AstNode const*> const& pair, 
                                                ExecutionPlan* plan, MMFilesGeoIndexInfo info){
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> attributeAccess1;
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> attributeAccess2;
   
  // first and second should be based on the same document - need to provide the document
  // in order to see which collection is bound to it and if that collections supports geo-index
  if (!pair.first->isAttributeAccessForVariable(attributeAccess1) || 
      !pair.second->isAttributeAccessForVariable(attributeAccess2)) {
    info.invalidate();
    return info;
  }

  TRI_ASSERT(attributeAccess1.first != nullptr);
  TRI_ASSERT(attributeAccess2.first != nullptr);

  // expect access of the for doc.attribute
  auto setter1 = plan->getVarSetBy(attributeAccess1.first->id);
  auto setter2 = plan->getVarSetBy(attributeAccess2.first->id);

  if (setter1 != nullptr &&
      setter2 != nullptr &&
      setter1 == setter2 &&
      setter1->getType() == EN::ENUMERATE_COLLECTION) {
    auto collNode = reinterpret_cast<EnumerateCollectionNode*>(setter1);
    auto coll = collNode->collection(); //what kind of indexes does it have on what attributes
    auto lcoll = coll->getCollection();
    // TODO - check collection for suitable geo-indexes
    for(auto indexShardPtr : lcoll->getIndexes()){
      // get real index
      arangodb::Index& index = *indexShardPtr.get();

      // check if current index is a geo-index
      if(  index.type() != arangodb::Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX
        && index.type() != arangodb::Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX) {
        continue;
      }

      TRI_ASSERT(index.fields().size() == 2);

      //check access paths of attributes in ast and those in index match
      if (index.fields()[0] == attributeAccess1.second && 
          index.fields()[1] == attributeAccess2.second) {
        info.collectionNode = collNode;
        info.index = indexShardPtr;
        TRI_AttributeNamesJoinNested(attributeAccess1.second, info.longitude, true);
        TRI_AttributeNamesJoinNested(attributeAccess2.second, info.latitude, true);
        return info;
      }
    }
  }

  info.invalidate();
  return info;
}

bool checkDistanceArguments(MMFilesGeoIndexInfo& info, ExecutionPlan* plan){
  if(!info){
    return false;
  }

  auto const& functionArguments = info.distanceNode->getMember(0);
  if(functionArguments->numMembers() < 4){
    return false;
  }

  std::pair<AstNode*,AstNode*> argPair1 = { functionArguments->getMember(0), functionArguments->getMember(1) };
  std::pair<AstNode*,AstNode*> argPair2 = { functionArguments->getMember(2), functionArguments->getMember(3) };

  MMFilesGeoIndexInfo result1 = geoDistanceFunctionArgCheck(argPair1, plan, info /*copy*/);
  MMFilesGeoIndexInfo result2 = geoDistanceFunctionArgCheck(argPair2, plan, info /*copy*/);
  //info now conatins access path to collection

  // xor only one argument pair shall have a geoIndex
  if (  ( !result1 && !result2 ) || ( result1 && result2 ) ){
    info.invalidate();
    return false;
  }

  MMFilesGeoIndexInfo res;
  if(result1){
    info = std::move(result1);
    info.constantPair = std::move(argPair2);
  } else {
    info = std::move(result2);
    info.constantPair = std::move(argPair1);
  }

  return true;
}

//checks a single sort or filter node
MMFilesGeoIndexInfo identifyGeoOptimizationCandidate(ExecutionNode::NodeType type, ExecutionPlan* plan, ExecutionNode* n){
  ExecutionNode* setter = nullptr;
  auto rv = MMFilesGeoIndexInfo{};
  switch(type){
    case EN::SORT: {
      auto node = static_cast<SortNode*>(n);
      auto& elements = node->getElements();

      // we're looking for "SORT DISTANCE(x,y,a,b) ASC", which has just one sort criterion
      if ( !(elements.size() == 1 && elements[0].ascending)) {
        //test on second makes sure the SORT is ascending
        return rv;
      }

      //variable of sort expression
      auto variable = elements[0].var;
      TRI_ASSERT(variable != nullptr);

      //// find the expression that is bound to the variable
      // get the expression node that holds the calculation
      setter = plan->getVarSetBy(variable->id);
    }
    break;

    case EN::FILTER: {
      auto node = static_cast<FilterNode*>(n);

      // filter nodes always have one input variable
       auto varsUsedHere = node->getVariablesUsedHere();
      TRI_ASSERT(varsUsedHere.size() == 1);

      // now check who introduced our variable
      auto variable = varsUsedHere[0];
      setter = plan->getVarSetBy(variable->id);
    }
    break;

    default:
      return rv;
  }

  // common part - extract astNode from setter witch is a calculation node
  if (setter == nullptr || setter->getType() != EN::CALCULATION) {
    return rv;
  }

  auto expression = static_cast<CalculationNode*>(setter)->expression();

  // the expression must exist and it must have an astNode
  if (expression == nullptr || expression->node() == nullptr){
    // not the right type of node
    return rv;
  }
  AstNode* node = expression->nodeForModification();

  //FIXME -- technical debt -- code duplication / not all cases covered
  switch(type){
    case EN::SORT: {
      // check comma separated parts of condition cond0, cond1, cond2
      rv = isDistanceFunction(node,nullptr);
    }
    break;

    case EN::FILTER: {
      rv = iterativePreorderWithCondition(type, node, &isGeoFilterExpression);
    }
    break;

    default:
      rv.invalidate(); // not required but make sure the result is invalid
  }

  rv.executionNode = n;
  rv.executionNodeType = type;
  rv.setter = static_cast<CalculationNode*>(setter);

  checkDistanceArguments(rv, plan);

  return rv;
};

//modify plan

// builds a condition that can be used with the index interface and
// contains all parameters required by the MMFilesGeoIndex 
std::unique_ptr<Condition> buildGeoCondition(ExecutionPlan* plan, MMFilesGeoIndexInfo& info) {
  AstNode* lat = info.constantPair.first;
  AstNode* lon = info.constantPair.second;
  auto ast = plan->getAst();
  auto varAstNode = ast->createNodeReference(info.collectionNode->outVariable());

  auto args = ast->createNodeArray(info.within ? 4 : 3);
  args->addMember(varAstNode); // collection
  args->addMember(lat); // latitude
  args->addMember(lon); // longitude
  
  AstNode* cond = nullptr;
  if (info.within) {
    // WITHIN
    args->addMember(info.range);
    auto lessValue =  ast->createNodeValueBool(info.lessgreaterequal);
    args->addMember(lessValue);
    cond = ast->createNodeFunctionCall("WITHIN", args);
  } else {
    // NEAR
    cond = ast->createNodeFunctionCall("NEAR", args);
  }

  TRI_ASSERT(cond != nullptr);
  
  auto condition = std::make_unique<Condition>(ast);
  condition->andCombine(cond);
  condition->normalize(plan);
  return condition;
}

void replaceGeoCondition(ExecutionPlan* plan, MMFilesGeoIndexInfo& info){
  if (info.expressionParent && info.executionNodeType == EN::FILTER) {
    auto ast = plan->getAst();
    CalculationNode* newNode = nullptr;
    Expression* expr = new Expression(ast, static_cast<CalculationNode*>(info.setter)->expression()->nodeForModification()->clone(ast));
 
    try {
      newNode = new CalculationNode(plan, plan->nextId(), expr, static_cast<CalculationNode*>(info.setter)->outVariable());
    } catch (...) {
      delete expr;
      throw;
    }

    plan->registerNode(newNode);
    plan->replaceNode(info.setter, newNode);

    bool done = false;
    ast->traverseAndModify(newNode->expression()->nodeForModification(),[&done](AstNode* node, void* data) {
      if (done) {
        return node;
      }
      if (node->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        for (std::size_t i = 0; i < node->numMembers(); i++){
          if (isGeoFilterExpression(node->getMemberUnchecked(i),node)) {
            done = true;
            return node->getMemberUnchecked(i ? 0 : 1);
          }
        }
      }
      return node;
    },
    nullptr);

    if(done){
      return;
    }

    auto replaceInfo = iterativePreorderWithCondition(EN::FILTER, newNode->expression()->nodeForModification(), &isGeoFilterExpression);
    if (newNode->expression()->nodeForModification() == replaceInfo.expressionParent) {
      if (replaceInfo.expressionParent->type == NODE_TYPE_OPERATOR_BINARY_AND){
        for (std::size_t i = 0; i < replaceInfo.expressionParent->numMembers(); ++i) {
          if (replaceInfo.expressionParent->getMember(i) != replaceInfo.expressionNode) {
            newNode->expression()->replaceNode(replaceInfo.expressionParent->getMember(i));
            return;
          }
        }
      }
    }

    //else {
    //  // COULD BE IMPROVED
    //  if(replaceInfo.expressionParent->type == NODE_TYPE_OPERATOR_BINARY_AND){
    //    // delete ast node - we would need the parent of expression parent to delete the node
    //    // we do not have it available here so we just replace the the node with true
    //    return;
    //  }
    //}

    //fallback
    auto replacement = ast->createNodeValueBool(true);
    for (std::size_t i = 0; i < replaceInfo.expressionParent->numMembers(); ++i) {
      if (replaceInfo.expressionParent->getMember(i) == replaceInfo.expressionNode) {
        replaceInfo.expressionParent->removeMemberUnchecked(i);
        replaceInfo.expressionParent->addMember(replacement);
      }
    }
  }
}

// applys the optimization for a candidate
bool applyGeoOptimization(bool near, ExecutionPlan* plan, MMFilesGeoIndexInfo& first, MMFilesGeoIndexInfo& second) {
  if (!first && !second) {
    return false;
  }

  if (!first) {
    first = std::move(second);
    second.invalidate();
  }

  // We are not allowed to be a inner loop
  if (first.collectionNode->isInInnerLoop() && first.executionNodeType == EN::SORT) {
    return false;
  }

  std::unique_ptr<Condition> condition(buildGeoCondition(plan, first));

  auto inode = new IndexNode(
          plan, plan->nextId(), first.collectionNode->vocbase(),
          first.collectionNode->collection(), first.collectionNode->outVariable(),
          std::vector<transaction::Methods::IndexHandle>{transaction::Methods::IndexHandle{first.index}},
          condition.get(), false);
  plan->registerNode(inode);
  condition.release();

  plan->replaceNode(first.collectionNode,inode);

  replaceGeoCondition(plan, first);
  replaceGeoCondition(plan, second);

  // if executionNode is sort OR a filter without further sub conditions
  // the node can be unlinked
  auto unlinkNode = [&](MMFilesGeoIndexInfo& info) {
    if (info && !info.expressionParent) {
      if (!arangodb::ServerState::instance()->isCoordinator() || info.executionNodeType == EN::FILTER) {
        plan->unlinkNode(info.executionNode);
      } else if (info.executionNodeType == EN::SORT) {
        //make sure sort is not reinserted in cluster
        static_cast<SortNode*>(info.executionNode)->_reinsertInCluster = false;
      }
    }
  };

  unlinkNode(first);
  unlinkNode(second);

  //signal that plan has been changed
  return true;
}

void MMFilesOptimizerRules::geoIndexRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const* rule) {

  SmallVector<ExecutionNode*>::allocator_type::arena_type a;
  SmallVector<ExecutionNode*> nodes{a};
  bool modified = false;
  //inspect each return node and work upwards to SingletonNode
  plan->findEndNodes(nodes, true);
  
  for (auto& node : nodes) {
    MMFilesGeoIndexInfo sortInfo{};
    MMFilesGeoIndexInfo filterInfo{};
    auto current = node;

    while (current) {
      switch(current->getType()) {
        case EN::SORT:{
          sortInfo = identifyGeoOptimizationCandidate(EN::SORT, plan.get(), current);
          break;
        }
        case EN::FILTER: {
          filterInfo = identifyGeoOptimizationCandidate(EN::FILTER, plan.get(), current);
          break;
        }
        case EN::ENUMERATE_COLLECTION: {
          EnumerateCollectionNode* collnode = static_cast<EnumerateCollectionNode*>(current);
          if( (sortInfo   && sortInfo.collectionNode!= collnode)
            ||(filterInfo && filterInfo.collectionNode != collnode)
            ){
            filterInfo.invalidate();
            sortInfo.invalidate();
            break;
          }
          if (applyGeoOptimization(true, plan.get(), filterInfo, sortInfo)){
            modified = true;
            filterInfo.invalidate();
            sortInfo.invalidate();
          }
          break;
        }

        case EN::INDEX:
        case EN::COLLECT:{
          filterInfo.invalidate();
          sortInfo.invalidate();
          break;
        }

        default: {
          //skip - do nothing
          break;
        }
      }

      current = current->getFirstDependency(); //inspect next node
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
