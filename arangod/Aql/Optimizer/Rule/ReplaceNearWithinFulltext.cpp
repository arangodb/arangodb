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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ReplaceNearWithinFulltext.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionNode/SingletonNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/IndexHint.h"
#include "Aql/Optimizer.h"
#include "Aql/Optimizer/Utils/GetAstNode.h"
#include "Aql/Optimizer/Utils/GetFunction.h"
#include "Aql/Query.h"
#include "Aql/SortElement.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StaticStrings.h"
#include "Basics/SupervisedBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

namespace arangodb::aql {

namespace {

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

bool isValueTypeCollection(AstNode const* node) noexcept {
  return node->type == NODE_TYPE_COLLECTION || node->isStringValue();
}

// NEAR(coll, 0 /*lat*/, 0 /*lon*/[, 10 /*limit*/])
struct NearOrWithinParams {
  std::string collection;
  AstNode* latitude = nullptr;
  AstNode* longitude = nullptr;
  AstNode* limit = nullptr;
  AstNode* radius = nullptr;
  AstNode* distanceName = nullptr;

  NearOrWithinParams(AstNode const* node, bool isNear) {
    TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
    ast::FunctionCallNode fcall(node);
    auto args = fcall.getArguments().getElements();
    TRI_ASSERT(args.size() >= 3);
    if (args[0]->isStringValue()) {
      collection = args[0]->getString();
      // otherwise the "" collection will not be found
    }
    latitude = args[1];
    longitude = args[2];
    if (args.size() > 4) {
      distanceName = args[4];
    }

    if (args.size() > 3) {
      if (isNear) {
        limit = args[3];
      } else {
        radius = args[3];
      }
    }
  }
};

// FULLTEXT(collection, "attribute", "search", 100 /*limit*/[, "distance name"])
struct FulltextParams {
  std::string collection;
  std::string attribute;
  AstNode* limit = nullptr;

  explicit FulltextParams(AstNode const* node) {
    TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
    ast::FunctionCallNode fcall(node);
    auto args = fcall.getArguments().getElements();
    TRI_ASSERT(args.size() >= 2);
    if (args[0]->isStringValue()) {
      collection = args[0]->getString();
    }
    if (args[1]->isStringValue()) {
      attribute = args[1]->getString();
    }
    if (args.size() > 3) {
      limit = args[3];
    }
  }
};

AstNode* createSubqueryWithLimit(ExecutionPlan* plan, ExecutionNode* node,
                                 ExecutionNode* first, ExecutionNode* last,
                                 Variable* lastOutVariable, AstNode* limit) {
  if (limit && !(limit->isIntValue() || limit->isNullValue())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        "limit parameter has wrong type");
  }

  auto* ast = plan->getAst();

  ExecutionNode* eSingleton = plan->createNode<SingletonNode>(
      plan, plan->nextId(), ast->bindParameterVariables());

  ExecutionNode* eReturn =
      plan->createNode<ReturnNode>(plan, plan->nextId(), lastOutVariable);

  first->addDependency(eSingleton);
  eReturn->addDependency(last);

  if (limit && !limit->isNullValue()) {
    ExecutionNode* eLimit = plan->createNode<LimitNode>(
        plan, plan->nextId(), 0 /*offset*/, limit->getIntValue());
    plan->insertAfter(last, eLimit);
  }

  Variable* subqueryOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eSubquery = plan->registerSubquery(
      new SubqueryNode(plan, plan->nextId(), eReturn, subqueryOutVariable));

  plan->insertBefore(node, eSubquery);

  return ast->createNodeReference(subqueryOutVariable);
}

bool isGeoIndex(arangodb::Index::IndexType type) {
  return type == arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX ||
         type == arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX ||
         type == arangodb::Index::TRI_IDX_TYPE_GEO_INDEX;
}

std::pair<AstNode*, AstNode*> getAttributeAccessFromIndex(
    Ast* ast, AstNode* docRef, NearOrWithinParams& params) {
  AstNode* accessNodeLat = docRef;
  AstNode* accessNodeLon = docRef;
  bool indexFound = false;

  aql::Collection* coll = ast->query().collections().get(params.collection);
  if (!coll) {
    coll = aql::addCollectionToQuery(ast->query(), params.collection,
                                     "NEAR OR WITHIN");
  }

  for (auto& idx : coll->indexes()) {
    if (isGeoIndex(idx->type())) {
      bool isGeo1 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX;
      bool isGeo2 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX;
      bool isGeo = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;

      auto fieldNum = idx->fields().size();
      if ((isGeo2 || isGeo) && fieldNum == 2) {
        auto accessLatitude = idx->fields()[0];
        auto accessLongitude = idx->fields()[1];

        accessNodeLat =
            ast->createNodeAttributeAccess(accessNodeLat, accessLatitude);
        accessNodeLon =
            ast->createNodeAttributeAccess(accessNodeLon, accessLongitude);

        indexFound = true;
      } else if ((isGeo1 || isGeo) && fieldNum == 1) {
        auto accessBase = idx->fields()[0];
        AstNode* base =
            ast->createNodeAttributeAccess(accessNodeLon, accessBase);

        velocypack::SupervisedBuffer sb(ast->query().resourceMonitor());
        VPackBuilder builder(sb);
        idx->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
        bool geoJson = basics::VelocyPackHelper::getBooleanValue(
            builder.slice(), "geoJson", false);

        accessNodeLat = ast->createNodeIndexedAccess(
            base, ast->createNodeValueInt(geoJson ? 1 : 0));
        accessNodeLon = ast->createNodeIndexedAccess(
            base, ast->createNodeValueInt(geoJson ? 0 : 1));
        indexFound = true;
      }
      break;
    }
  }

  if (!indexFound) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING,
                                  params.collection.c_str());
  }

  return std::pair<AstNode*, AstNode*>(accessNodeLat, accessNodeLon);
}

AstNode* replaceNearOrWithin(AstNode* funAstNode, ExecutionNode* calcNode,
                             ExecutionPlan* plan, bool isNear) {
  auto* ast = plan->getAst();
  QueryContext& query = ast->query();
  NearOrWithinParams params(funAstNode, isNear);

  if (isNear && (!params.limit || params.limit->isNullValue())) {
    params.limit = ast->createNodeValueInt(100);
  }

  auto* aqlCollection =
      aql::addCollectionToQuery(query, params.collection, "NEAR OR WITHIN");

  Variable* enumerateOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eEnumerate = plan->createNode<EnumerateCollectionNode>(
      plan, plan->nextId(), aqlCollection, enumerateOutVariable, false,
      IndexHint());

  auto* docRef = ast->createNodeReference(enumerateOutVariable);

  AstNode *accessNodeLat, *accessNodeLon;
  std::tie(accessNodeLat, accessNodeLon) =
      getAttributeAccessFromIndex(ast, docRef, params);

  auto* argsArray = ast->createNodeArray();
  argsArray->addMember(accessNodeLat);
  argsArray->addMember(accessNodeLon);
  argsArray->addMember(params.latitude);
  argsArray->addMember(params.longitude);
  auto* funDist = ast->createNodeFunctionCall("DISTANCE", argsArray, true);

  AstNode* expressionAst = funDist;

  if (!isNear) {
    if (!params.radius || !params.radius->isNumericValue()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "radius argument is not a numeric value");
    }

    expressionAst = ast->createNodeBinaryOperator(
        AstNodeType::NODE_TYPE_OPERATOR_BINARY_LE, funDist, params.radius);
  }

  auto calcExpr = std::make_unique<Expression>(ast, expressionAst);

  Variable* calcOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eCalc = plan->createNode<CalculationNode>(
      plan, plan->nextId(), std::move(calcExpr), calcOutVariable);
  eCalc->addDependency(eEnumerate);

  ExecutionNode* eSortOrFilter = nullptr;
  if (isNear) {
    SortElementVector sortElements;
    sortElements.push_back(SortElement::create(calcOutVariable, /*asc*/ true));
    eSortOrFilter =
        plan->createNode<SortNode>(plan, plan->nextId(), sortElements, false);
  } else {
    eSortOrFilter =
        plan->createNode<FilterNode>(plan, plan->nextId(), calcOutVariable);
  }
  eSortOrFilter->addDependency(eCalc);

  if (params.distanceName) {
    if (!params.distanceName->isStringValue()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "distance argument is not a string");
    }
    AstNode* elem = nullptr;
    AstNode* funDistMerge = nullptr;
    if (isNear) {
      funDistMerge = ast->createNodeReference(calcOutVariable);
    } else {
      funDistMerge = funDist;
    }
    if (params.distanceName->isConstant()) {
      elem = ast->createNodeObjectElement(params.distanceName->getStringView(),
                                          funDistMerge);
    } else {
      elem = ast->createNodeCalculatedObjectElement(params.distanceName,
                                                    funDistMerge);
    }
    auto* obj = ast->createNodeObject();
    obj->addMember(elem);

    auto* argsArrayMerge = ast->createNodeArray();
    argsArrayMerge->addMember(docRef);
    argsArrayMerge->addMember(obj);

    auto* funMerge = ast->createNodeFunctionCall("MERGE", argsArrayMerge, true);

    Variable* calcMergeOutVariable =
        ast->variables()->createTemporaryVariable();
    auto calcMergeExpr = std::make_unique<Expression>(ast, funMerge);
    ExecutionNode* eCalcMerge = plan->createNode<CalculationNode>(
        plan, plan->nextId(), std::move(calcMergeExpr), calcMergeOutVariable);
    plan->insertAfter(eSortOrFilter, eCalcMerge);

    return createSubqueryWithLimit(plan, calcNode, eEnumerate, eCalcMerge,
                                   calcMergeOutVariable, params.limit);
  }

  return createSubqueryWithLimit(plan, calcNode, eEnumerate /* first */,
                                 eSortOrFilter /* last */, enumerateOutVariable,
                                 params.limit);
}

AstNode* replaceWithinRectangle(AstNode* funAstNode, ExecutionNode* calcNode,
                                ExecutionPlan* plan) {
  aql::Ast* ast = plan->getAst();

  TRI_ASSERT(funAstNode->type == AstNodeType::NODE_TYPE_FCALL);
  ast::FunctionCallNode fcall(funAstNode);
  auto inputArgs = fcall.getArguments().getElements();

  if (inputArgs.size() < 5) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "WITHIN_RECTANGLE",
        5, 5);
  }

  AstNode const* coll = inputArgs[0];
  AstNode const* lat1 = inputArgs[1];
  AstNode const* lng1 = inputArgs[2];
  AstNode const* lat2 = inputArgs[3];
  AstNode const* lng2 = inputArgs[4];

  if (!isValueTypeCollection(coll)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  std::string cname = coll->getString();

  aql::Collection* collection =
      aql::addCollectionToQuery(ast->query(), cname, "WITHIN_RECTANGLE");

  if (coll->type != NODE_TYPE_COLLECTION) {
    auto const& resolver = ast->query().resolver();
    coll = ast->createNodeCollection(resolver, coll->getStringView(),
                                     AccessMode::Type::READ);
  }

  std::shared_ptr<arangodb::Index> index;
  for (auto& idx : collection->indexes()) {
    if (isGeoIndex(idx->type())) {
      index = idx;
      break;
    }
  }
  if (!index) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING,
                                  cname.c_str());
  }

  Variable* collVar = ast->variables()->createTemporaryVariable();
  AstNode* forNode = ast->createNodeFor(collVar, coll, nullptr);

  AstNode* loop = ast->createNodeArray(5);
  auto fn = [&](AstNode const* lat, AstNode const* lon) {
    AstNode* arr = ast->createNodeArray(2);
    arr->addMember(lon);
    arr->addMember(lat);
    loop->addMember(arr);
  };
  fn(lat1, lng1);
  fn(lat1, lng2);
  fn(lat2, lng2);
  fn(lat2, lng1);
  fn(lat1, lng1);
  AstNode* polygon = ast->createNodeObject();
  polygon->addMember(ast->createNodeObjectElement(
      "type", ast->createNodeValueString("Polygon", 7)));
  AstNode* coords = ast->createNodeArray(1);
  coords->addMember(loop);
  polygon->addMember(ast->createNodeObjectElement("coordinates", coords));

  AstNode* fargs = ast->createNodeArray(2);
  fargs->addMember(polygon);

  if (index->fields().size() == 2) {
    AstNode* arr = ast->createNodeArray(2);
    arr->addMember(ast->createNodeAccess(collVar, index->fields()[1]));
    arr->addMember(ast->createNodeAccess(collVar, index->fields()[0]));
    fargs->addMember(arr);
  } else {
    velocypack::SupervisedBuffer sb(ast->query().resourceMonitor());
    VPackBuilder builder(sb);
    index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
    bool geoJson = basics::VelocyPackHelper::getBooleanValue(builder.slice(),
                                                             "geoJson", false);
    if (geoJson) {
      fargs->addMember(ast->createNodeAccess(collVar, index->fields()[0]));
    } else {
      AstNode* arr = ast->createNodeArray(2);
      AstNode* access = ast->createNodeAccess(collVar, index->fields()[0]);
      arr->addMember(
          ast->createNodeIndexedAccess(access, ast->createNodeValueInt(1)));
      arr->addMember(
          ast->createNodeIndexedAccess(access, ast->createNodeValueInt(0)));
      fargs->addMember(arr);
    }
  }
  AstNode* geoContainsCall =
      ast->createNodeFunctionCall("GEO_CONTAINS", fargs, true);

  AstNode* filterNode = ast->createNodeFilter(geoContainsCall);

  AstNode* returnNode =
      ast->createNodeReturn(ast->createNodeReference(collVar));

  AstNode* rootNode = ast->createNodeSubquery();

  rootNode->addMember(forNode);
  rootNode->addMember(filterNode);
  rootNode->addMember(returnNode);

  ExecutionNode* subquery = plan->fromNode(rootNode);
  if (subquery == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  Variable* v = ast->variables()->createTemporaryVariable();
  SubqueryNode* sqn = plan->registerSubquery(
      new SubqueryNode(plan, plan->nextId(), subquery, v));
  plan->insertDependency(calcNode, sqn);
  return ast->createNodeReference(v);
}

AstNode* replaceFullText(AstNode* funAstNode, ExecutionNode* calcNode,
                         ExecutionPlan* plan) {
  auto* ast = plan->getAst();
  QueryContext& query = ast->query();

  TRI_ASSERT(funAstNode->type == NODE_TYPE_FCALL);
  FulltextParams params(funAstNode);

  std::shared_ptr<arangodb::Index> index;
  std::vector<basics::AttributeName> field;
  TRI_ParseAttributeString(params.attribute, field, false);

  aql::Collection* coll = query.collections().get(params.collection);
  if (!coll) {
    coll = addCollectionToQuery(query, params.collection, "FULLTEXT");
  }

  for (auto& idx : coll->indexes()) {
    if (idx->type() ==
        arangodb::Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      if (basics::AttributeName::isIdentical(idx->fields()[0], field, false)) {
        index = idx;
        break;
      }
    }
  }

  if (!index) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,
                                  params.collection.c_str());
  }

  auto* aqlCollection =
      aql::addCollectionToQuery(query, params.collection, "FULLTEXT");
  auto condition = std::make_unique<Condition>(ast);
  condition->andCombine(funAstNode);
  condition->normalize(plan);
  Variable* indexOutVariable = ast->variables()->createTemporaryVariable();

  ExecutionNode* eIndex = plan->createNode<IndexNode>(
      plan, plan->nextId(), aqlCollection, indexOutVariable,
      std::vector<transaction::Methods::IndexHandle>{
          transaction::Methods::IndexHandle{index}},
      false, std::move(condition), IndexIteratorOptions());

  return createSubqueryWithLimit(plan, calcNode, eIndex, eIndex,
                                 indexOutVariable, params.limit);
}

}  // namespace

void replaceNearWithinFulltextRule(Optimizer* opt,
                                   std::unique_ptr<ExecutionPlan> plan,
                                   OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for (auto const& node : nodes) {
    auto visitor = [&modified, &node, &plan](AstNode* astnode) {
      auto* fun = getFunction(astnode);
      if (fun) {
        AstNode* replacement = nullptr;
        if (fun->name == "NEAR") {
          replacement =
              replaceNearOrWithin(astnode, node, plan.get(), true /*isNear*/);
          TRI_ASSERT(replacement);
        } else if (fun->name == "WITHIN") {
          replacement =
              replaceNearOrWithin(astnode, node, plan.get(), false /*isNear*/);
          TRI_ASSERT(replacement);
        } else if (fun->name == "WITHIN_RECTANGLE") {
          replacement = replaceWithinRectangle(astnode, node, plan.get());
          TRI_ASSERT(replacement);
        } else if (fun->name == "FULLTEXT") {
          replacement = replaceFullText(astnode, node, plan.get());
          TRI_ASSERT(replacement);
        }

        if (replacement) {
          modified = true;
          return replacement;
        }
      }

      return astnode;
    };

    CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(node);
    auto* original = getAstNode(calc);
    auto* replacement = Ast::traverseAndModify(original, visitor);

    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

}  // namespace arangodb::aql
