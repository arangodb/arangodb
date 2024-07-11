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

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
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
#include "Aql/OptimizerRules.h"
#include "Aql/Query.h"
#include "Aql/SortElement.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

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
    AstNode* arr = node->getMember(0);
    TRI_ASSERT(arr->type == AstNodeType::NODE_TYPE_ARRAY);
    if (arr->getMember(0)->isStringValue()) {
      collection = arr->getMember(0)->getString();
      // otherwise the "" collection will not be found
    }
    latitude = arr->getMember(1);
    longitude = arr->getMember(2);
    if (arr->numMembers() > 4) {
      distanceName = arr->getMember(4);
    }

    if (arr->numMembers() > 3) {
      if (isNear) {
        limit = arr->getMember(3);
      } else {
        radius = arr->getMember(3);
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
    AstNode* arr = node->getMember(0);
    TRI_ASSERT(arr->type == AstNodeType::NODE_TYPE_ARRAY);
    if (arr->getMember(0)->isStringValue()) {
      collection = arr->getMember(0)->getString();
    }
    if (arr->getMember(1)->isStringValue()) {
      attribute = arr->getMember(1)->getString();
    }
    if (arr->numMembers() > 3) {
      limit = arr->getMember(3);
    }
  }
};

AstNode* getAstNode(CalculationNode* c) noexcept {
  return c->expression()->nodeForModification();
}

Function* getFunction(AstNode const* ast) noexcept {
  if (ast->type == AstNodeType::NODE_TYPE_FCALL) {
    return static_cast<Function*>(ast->getData());
  }
  return nullptr;
}

AstNode* createSubqueryWithLimit(ExecutionPlan* plan, ExecutionNode* node,
                                 ExecutionNode* first, ExecutionNode* last,
                                 Variable* lastOutVariable, AstNode* limit) {
  // Creates a subquery of the following form:
  //
  //    singleton
  //        |
  //      first
  //        |
  //       ...
  //        |
  //       last
  //        |
  //     [limit]
  //        |
  //      return
  //
  // The subquery is then injected into the plan before the given `node`
  // This function returns an `AstNode*` of type reference to the subquery's
  // `outVariable` that can be used to replace the expression (or only a
  // part) of a `CalculationNode`.
  //
  if (limit && !(limit->isIntValue() || limit->isNullValue())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        "limit parameter has wrong type");
  }

  auto* ast = plan->getAst();

  /// singleton
  ExecutionNode* eSingleton =
      plan->createNode<SingletonNode>(plan, plan->nextId());

  /// return
  /// link output of index with the return node
  ExecutionNode* eReturn =
      plan->createNode<ReturnNode>(plan, plan->nextId(), lastOutVariable);

  /// link nodes together
  first->addDependency(eSingleton);
  eReturn->addDependency(last);

  /// add optional limit node
  if (limit && !limit->isNullValue()) {
    ExecutionNode* eLimit = plan->createNode<LimitNode>(
        plan, plan->nextId(), 0 /*offset*/, limit->getIntValue());
    plan->insertAfter(last, eLimit);  // inject into plan
  }

  /// create subquery
  Variable* subqueryOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eSubquery = plan->registerSubquery(
      new SubqueryNode(plan, plan->nextId(), eReturn, subqueryOutVariable));

  plan->insertBefore(node, eSubquery);

  // return reference to outVariable
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
    if (::isGeoIndex(idx->type())) {
      // we take the first index that is found
      bool isGeo1 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX;
      bool isGeo2 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX;
      bool isGeo = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;

      auto fieldNum = idx->fields().size();
      if ((isGeo2 || isGeo) && fieldNum == 2) {  // individual fields
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

        VPackBuilder builder;
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

  }  // for index in collection

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

  // RETURN (
  //  FOR d IN col
  //    SORT DISTANCE(d.lat, d.long, param.lat, param.lon) // NEAR
  //    // FILTER DISTANCE(d.lat, d.long, param.lat, param.lon) < param.radius
  //    //WHITHIN MERGE(d, { param.distname : DISTANCE(d.lat, d.long, param.lat,
  //    param.lon)}) LIMIT param.limit // NEAR RETURN d MERGE {param.distname :
  //    calculated_distance}
  // )

  //// enumerate collection
  auto* aqlCollection =
      aql::addCollectionToQuery(query, params.collection, "NEAR OR WITHIN");

  Variable* enumerateOutVariable = ast->variables()->createTemporaryVariable();
  // link output of index with the return node
  ExecutionNode* eEnumerate = plan->createNode<EnumerateCollectionNode>(
      plan, plan->nextId(), aqlCollection, enumerateOutVariable, false,
      IndexHint());

  //// build sort condition - DISTANCE(d.lat, d.long, param.lat, param.lon)
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

  //// build filter condition for
  if (!isNear) {  // WITHIN(coll, lat, lon, radius, distName)
    if (!params.radius || !params.radius->isNumericValue()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          "radius argument is not a numeric value");
    }

    expressionAst = ast->createNodeBinaryOperator(
        AstNodeType::NODE_TYPE_OPERATOR_BINARY_LE, funDist, params.radius);
  }

  //// create calculation node used in SORT or FILTER
  // Calculation Node will acquire ownership
  auto calcExpr = std::make_unique<Expression>(ast, expressionAst);

  // put condition into calculation node
  Variable* calcOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eCalc = plan->createNode<CalculationNode>(
      plan, plan->nextId(), std::move(calcExpr), calcOutVariable);
  eCalc->addDependency(eEnumerate);

  //// create SORT or FILTER
  ExecutionNode* eSortOrFilter = nullptr;
  if (isNear) {
    // use calculation node in sort node
    SortElementVector sortElements{SortElement{calcOutVariable, /*asc*/ true}};
    eSortOrFilter =
        plan->createNode<SortNode>(plan, plan->nextId(), sortElements, false);
  } else {
    eSortOrFilter =
        plan->createNode<FilterNode>(plan, plan->nextId(), calcOutVariable);
  }
  eSortOrFilter->addDependency(eCalc);

  //// create MERGE(d, { param.distname : DISTANCE(d.lat, d.long, param.lat,
  /// param.lon)})
  if (params.distanceName) {  // return without merging the distance into the
                              // result
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
      // NOTE - recycling the Ast seems to work - tested with ASAN
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

    //// wrap plan part into subquery
    return createSubqueryWithLimit(plan, calcNode, eEnumerate, eCalcMerge,
                                   calcMergeOutVariable, params.limit);
  }  // merge

  //// wrap plan part into subquery
  return createSubqueryWithLimit(plan, calcNode, eEnumerate /* first */,
                                 eSortOrFilter /* last */, enumerateOutVariable,
                                 params.limit);
}

/// @brief replace WITHIN_RECTANGLE
AstNode* replaceWithinRectangle(AstNode* funAstNode, ExecutionNode* calcNode,
                                ExecutionPlan* plan) {
  aql::Ast* ast = plan->getAst();

  TRI_ASSERT(funAstNode->type == AstNodeType::NODE_TYPE_FCALL);
  AstNode* fargs = funAstNode->getMember(0);
  TRI_ASSERT(fargs->type == AstNodeType::NODE_TYPE_ARRAY);

  if (fargs->numMembers() < 5) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "WITHIN_RECTANGLE",
        5, 5);
  }

  AstNode const* coll = fargs->getMemberUnchecked(0);
  AstNode const* lat1 = fargs->getMemberUnchecked(1);
  AstNode const* lng1 = fargs->getMemberUnchecked(2);
  AstNode const* lat2 = fargs->getMemberUnchecked(3);
  AstNode const* lng2 = fargs->getMemberUnchecked(4);

  if (!::isValueTypeCollection(coll)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // check for suitable indexes
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
    if (::isGeoIndex(idx->type())) {
      index = idx;
      break;
    }
  }
  if (!index) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING,
                                  cname.c_str());
  }

  // FOR part
  Variable* collVar = ast->variables()->createTemporaryVariable();
  AstNode* forNode = ast->createNodeFor(collVar, coll, nullptr);

  // Create GEO_CONTAINS function
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

  fargs = ast->createNodeArray(2);
  fargs->addMember(polygon);

  // GEO_CONTAINS, needs GeoJson [Lon, Lat] ordering
  if (index->fields().size() == 2) {
    AstNode* arr = ast->createNodeArray(2);
    arr->addMember(ast->createNodeAccess(collVar, index->fields()[1]));
    arr->addMember(ast->createNodeAccess(collVar, index->fields()[0]));
    fargs->addMember(arr);
  } else {
    VPackBuilder builder;
    index->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
    bool geoJson = basics::VelocyPackHelper::getBooleanValue(builder.slice(),
                                                             "geoJson", false);
    if (geoJson) {
      fargs->addMember(ast->createNodeAccess(collVar, index->fields()[0]));
    } else {  // combined [lat, lon] field
      AstNode* arr = ast->createNodeArray(2);
      AstNode* access = ast->createNodeAccess(collVar, index->fields()[0]);
      arr->addMember(
          ast->createNodeIndexedAccess(access, ast->createNodeValueInt(1)));
      arr->addMember(
          ast->createNodeIndexedAccess(access, ast->createNodeValueInt(0)));
      fargs->addMember(arr);
    }
  }
  AstNode* fcall = ast->createNodeFunctionCall("GEO_CONTAINS", fargs, true);

  // FILTER part
  AstNode* filterNode = ast->createNodeFilter(fcall);

  // RETURN part
  AstNode* returnNode =
      ast->createNodeReturn(ast->createNodeReference(collVar));

  // create an on-the-fly subquery for a full collection access
  AstNode* rootNode = ast->createNodeSubquery();

  // add nodes to subquery
  rootNode->addMember(forNode);
  rootNode->addMember(filterNode);
  rootNode->addMember(returnNode);

  // produce the proper ExecutionNodes from the subquery AST
  ExecutionNode* subquery = plan->fromNode(rootNode);
  if (subquery == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // and register a reference to the subquery result in the expression
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

  /// index
  //  we create this first as creation of this node is more
  //  likely to fail than the creation of other nodes

  //  index - part 1 - figure out index to use
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
      if (basics::AttributeName::isIdentical(
              idx->fields()[0], field, false /*ignore expansion in last?!*/)) {
        index = idx;
        break;
      }
    }
  }

  if (!index) {  // not found or error
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,
                                  params.collection.c_str());
  }

  // index part 2 - get remaining vars required for index creation
  auto* aqlCollection =
      aql::addCollectionToQuery(query, params.collection, "FULLTEXT");
  auto condition = std::make_unique<Condition>(ast);
  condition->andCombine(funAstNode);
  condition->normalize(plan);
  // create a fresh out variable
  Variable* indexOutVariable = ast->variables()->createTemporaryVariable();

  ExecutionNode* eIndex = plan->createNode<IndexNode>(
      plan, plan->nextId(), aqlCollection, indexOutVariable,
      std::vector<transaction::Methods::IndexHandle>{
          transaction::Methods::IndexHandle{index}},
      false,  // here we are not using inverted index so for sure
              // no "whole" coverage
      std::move(condition), IndexIteratorOptions());

  //// wrap plan part into subquery
  return createSubqueryWithLimit(plan, calcNode, eIndex, eIndex,
                                 indexOutVariable, params.limit);
}

}  // namespace

//! @brief replace legacy JS Functions with pure AQL
void arangodb::aql::replaceNearWithinFulltextRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for (auto const& node : nodes) {
    auto visitor = [&modified, &node, &plan](AstNode* astnode) {
      auto* fun = getFunction(
          astnode);  // if fun != nullptr -> astnode->type NODE_TYPE_FCALL
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

    // replace root node if it was modified
    // TraverseAndModify has no access to roots parent
    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

void arangodb::aql::replaceLikeWithRangeRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for (auto node : nodes) {
    auto visitor = [&modified, &plan](AstNode* node) {
      auto* func = getFunction(node);
      if (func != nullptr && func->name == "LIKE") {
        // optimize a LIKE(x, y) into a plain x == y or a range scan in case the
        // search is case-sensitive and the pattern is either a full match or a
        // left-most prefix.
        // this is desirable in 99.999% of all cases, but would be incompatible
        // for search terms that are non-strings. LIKE(1, '1') would behave
        // differently when executed via the AQL LIKE function than would be 1
        // == '1'. for left-most prefix searches (e.g. LIKE(text, 'abc%')) we
        // need to determine the upper bound for the range scan. We use the
        // originally supplied string for the upper bound and append a \uFFFF
        // character to it, which compares higher than other characters.
        bool caseInsensitive = false;  // this is the default behavior of LIKE
        auto args = node->getMember(0);
        if (args->numMembers() >= 3) {
          caseInsensitive =
              true;  // we have 3 arguments, set case-sensitive to false now
          auto caseArg = args->getMember(2);
          if (caseArg->isConstant()) {
            // ok, we can figure out at compile time if the parameter is true or
            // false
            caseInsensitive = caseArg->isTrue();
          }
        }

        auto patternArg = args->getMember(1);

        if (!caseInsensitive && patternArg->isStringValue() &&
            args->getMember(0)->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          AstNode const* sub = args->getMember(0);
          while (sub != nullptr && sub->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            sub = sub->getMember(0);
          }
          if (sub == nullptr || sub->type != NODE_TYPE_REFERENCE) {
            return node;
          }
          auto setter = plan->getVarSetBy(
              static_cast<Variable const*>(sub->getData())->id);
          if (setter == nullptr ||
              setter->getType() != EN::ENUMERATE_COLLECTION) {
            // setter could be a view. for views we do not want to change the
            // LIKE function invocation because it might result in a
            // pessimization
            return node;
          }
          auto cn =
              ExecutionNode::castTo<EnumerateCollectionNode const*>(setter);
          auto const& hint = cn->hint();
          if (hint.isDisabled()) {
            // no index should be used. no need for the optimization
            return node;
          }
          if (hint.isSimple()) {
            // we have an index hint
            Collection const* c = cn->collection();

            // check if any of the indexes suggested in the index hint is
            // an inverted index. if so, we disable the optimization
            for (auto const& name : hint.candidateIndexes()) {
              auto idx = c->getCollection()->lookupIndex(name);
              if (idx != nullptr &&
                  idx->type() == Index::TRI_IDX_TYPE_INVERTED_INDEX) {
                // usage of an inverted index -> prevent optimization
                return node;
              }
            }
          }

          // we can go ahead with the optimization

          // optimization only possible for case-sensitive LIKE
          std::string unescapedPattern;
          auto [wildcardFound, wildcardIsLastChar] =
              AqlFunctionsInternalCache::inspectLikePattern(
                  unescapedPattern, patternArg->getStringView());

          if (!wildcardFound) {
            TRI_ASSERT(!wildcardIsLastChar);

            // can turn LIKE into ==
            modified = true;
            Ast* ast = plan->getAst();

            char const* p = ast->resources().registerString(
                unescapedPattern.data(), unescapedPattern.size());
            AstNode* pattern =
                ast->createNodeValueString(p, unescapedPattern.size());

            return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 args->getMember(0), pattern);
          }

          if (!unescapedPattern.empty()) {
            // can turn LIKE into >= && <=
            modified = true;
            Ast* ast = plan->getAst();

            char const* p = ast->resources().registerString(
                unescapedPattern.data(), unescapedPattern.size());
            AstNode* pattern =
                ast->createNodeValueString(p, unescapedPattern.size());
            AstNode* lhs = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_GE, args->getMember(0), pattern);

            // add a new end character \uFFFF that is expected to sort "higher"
            // than anything else (note: \xef\xbf\xbf is equivalent to \uFFFF).
            constexpr std::string_view upper = "\xef\xbf\xbf";
            unescapedPattern.append(upper);
            p = ast->resources().registerString(unescapedPattern.data(),
                                                unescapedPattern.size());
            pattern = ast->createNodeValueString(p, unescapedPattern.size());
            AstNode* rhs = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_LT, args->getMember(0), pattern);

            AstNode* op = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_AND, lhs, rhs);
            // add >= && <=, but keep LIKE in place to properly handle case
            return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                 op, node);
          }
        }
      }

      return node;
    };

    CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(node);
    auto* original = getAstNode(calc);
    auto* replacement = Ast::traverseAndModify(original, visitor);

    // replace root node if it was modified
    // TraverseAndModify has no access to roots parent
    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
