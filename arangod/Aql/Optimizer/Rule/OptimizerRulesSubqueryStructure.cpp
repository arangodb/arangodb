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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Optimizer/Rule/OptimizerRulesSubqueryStructure.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/CollectNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/ExecutionNode/RemoteNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionNode/ScatterNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryEndExecutionNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionNode/SubqueryStartExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Containers/SmallVector.h"

using EN = arangodb::aql::ExecutionNode;

/// @brief pulls out simple subqueries and merges them with the level above
///
/// For example, if we have the input query
///
/// FOR x IN (
///     FOR y IN collection FILTER y.value >= 5 RETURN y.test
///   )
///   RETURN x.a
///
/// then this rule will transform it into:
///
/// FOR tmp IN collection
///   FILTER tmp.value >= 5
///   LET x = tmp.test
///   RETURN x.a
void arangodb::aql::inlineSubqueriesRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::SUBQUERY, true);

  if (nodes.empty()) {
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool modified = false;
  std::vector<ExecutionNode*> subNodes;

  for (auto const& n : nodes) {
    auto subqueryNode = ExecutionNode::castTo<SubqueryNode*>(n);

    if (subqueryNode->isModificationNode()) {
      // can't modify modifying subqueries
      continue;
    }

    if (!subqueryNode->isDeterministic()) {
      // can't inline non-deterministic subqueries
      continue;
    }

    // check if subquery contains a COLLECT node with an INTO variable
    // or a WINDOW node in an inner loop
    bool eligible = true;
    bool containsLimitOrSort = false;
    auto current = subqueryNode->getSubquery();
    TRI_ASSERT(current != nullptr);

    while (current != nullptr) {
      if (current->getType() == EN::WINDOW && subqueryNode->isInInnerLoop()) {
        // WINDOW captures all existing rows in the scope, moving WINDOW
        // ends up with different rows captured
        eligible = false;
        break;
      } else if (current->getType() == EN::COLLECT) {
        if (subqueryNode->isInInnerLoop()) {
          eligible = false;
          break;
        }
        if (ExecutionNode::castTo<CollectNode const*>(current)
                ->hasOutVariable()) {
          // COLLECT ... INTO captures all existing variables in the scope.
          // if we move the subquery from one scope into another, we will end up
          // with different variables captured, so we must not apply the
          // optimization in this case.
          eligible = false;
          break;
        }
      } else if (current->getType() == EN::LIMIT ||
                 current->getType() == EN::SORT) {
        containsLimitOrSort = true;
      }
      current = current->getFirstDependency();
    }

    if (!eligible) {
      continue;
    }

    Variable const* out = subqueryNode->outVariable();
    TRI_ASSERT(out != nullptr);
    // the subquery outvariable and all its aliases
    VarSet subqueryVars;
    subqueryVars.emplace(out);

    // the potential calculation nodes that produce the aliases
    std::vector<ExecutionNode*> aliasNodesToRemoveLater;

    VarSet varsUsed;

    current = n->getFirstParent();
    // now check where the subquery is used
    while (current->hasParent()) {
      if (current->getType() == EN::ENUMERATE_LIST) {
        if (current->isInInnerLoop() && containsLimitOrSort) {
          // exit the loop
          current = nullptr;
          break;
        }

        // we're only interested in FOR loops...
        auto listNode = ExecutionNode::castTo<EnumerateListNode*>(current);
        if (listNode->getMode() == EnumerateListNode::kEnumerateObject) {
          // exit the loop
          current = nullptr;
          break;
        }
        // ...that use our subquery as its input
        if (subqueryVars.find(listNode->inVariable()) != subqueryVars.end()) {
          // bingo!

          // check if the subquery result variable or any of the aliases are
          // used after the FOR loop
          bool mustAbort = false;
          for (auto const& itSub : subqueryVars) {
            if (listNode->isVarUsedLater(itSub)) {
              // exit the loop
              current = nullptr;
              mustAbort = true;
              break;
            }
          }
          if (mustAbort) {
            break;
          }

          for (auto const& toRemove : aliasNodesToRemoveLater) {
            plan->unlinkNode(toRemove, false);
          }

          subNodes.clear();
          subNodes.reserve(4);
          subqueryNode->getSubquery()->getDependencyChain(subNodes, true);
          TRI_ASSERT(!subNodes.empty());
          auto returnNode = ExecutionNode::castTo<ReturnNode*>(subNodes[0]);
          TRI_ASSERT(returnNode->getType() == EN::RETURN);

          modified = true;
          auto queryVariables = plan->getAst()->variables();
          auto previous = n->getFirstDependency();
          auto insert = n->getFirstParent();
          TRI_ASSERT(insert != nullptr);

          // unlink the original SubqueryNode
          plan->unlinkNode(n, false);

          for (auto& it : subNodes) {
            // first unlink them all
            plan->unlinkNode(it, true);

            if (it->getType() == EN::SINGLETON) {
              // reached the singleton node already. that means we can stop
              break;
            }

            // and now insert them one level up
            if (it != returnNode) {
              // we skip over the subquery's return node. we don't need it
              // anymore
              insert->removeDependencies();
              TRI_ASSERT(it != nullptr);
              insert->addDependency(it);
              insert = it;

              // additionally rename the variables from the subquery so they
              // cannot conflict with the ones from the top query
              for (auto const& variable : it->getVariablesSetHere()) {
                queryVariables->renameVariable(variable->id);
              }
            }
          }

          // link the top node in the subquery with the original plan
          if (previous != nullptr) {
            insert->addDependency(previous);
          }

          // remove the list node from the plan
          plan->unlinkNode(listNode, false);

          queryVariables->renameVariable(returnNode->inVariable()->id,
                                         listNode->outVariable()[0]->name);

          // finally replace the variables
          std::unordered_map<VariableId, Variable const*> replacements;
          replacements.try_emplace(listNode->outVariable()[0]->id,
                                   returnNode->inVariable());
          VariableReplacer finder(replacements);
          plan->root()->walk(finder);

          plan->clearVarUsageComputed();
          plan->findVarUsage();

          // abort optimization
          current = nullptr;
        }
      } else if (current->getType() == EN::CALCULATION) {
        auto rootNode = ExecutionNode::castTo<CalculationNode*>(current)
                            ->expression()
                            ->node();
        if (rootNode->type == NODE_TYPE_REFERENCE) {
          if (subqueryVars.find(static_cast<Variable const*>(
                  rootNode->getData())) != subqueryVars.end()) {
            // found an alias for the subquery variable
            subqueryVars.emplace(
                ExecutionNode::castTo<CalculationNode*>(current)
                    ->outVariable());
            aliasNodesToRemoveLater.emplace_back(current);
            current = current->getFirstParent();

            continue;
          }
        }
      }

      if (current == nullptr) {
        break;
      }

      varsUsed.clear();
      current->getVariablesUsedHere(varsUsed);

      bool mustAbort = false;
      for (auto const& itSub : subqueryVars) {
        if (varsUsed.find(itSub) != varsUsed.end()) {
          // we found another node that uses the subquery variable
          // we need to stop the optimization attempts here
          mustAbort = true;
          break;
        }
      }
      if (mustAbort) {
        break;
      }

      current = current->getFirstParent();
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// Essentially mirrors the geo::QueryParams struct, but with
/// abstracts AstNode value objects
struct GeoIndexInfo {
  operator bool() const {
    return collectionNodeToReplace != nullptr && collectionNodeOutVar &&
           collection && index && valid;
  }
  void invalidate() { valid = false; }

  /// node that will be replaced by (geo) IndexNode
  ExecutionNode* collectionNodeToReplace = nullptr;
  Variable const* collectionNodeOutVar = nullptr;

  /// accessed collection
  aql::Collection const* collection = nullptr;
  /// selected index
  std::shared_ptr<Index> index;

  /// Filter calculations to modify
  std::map<ExecutionNode*, Expression*> exesToModify;
  std::set<AstNode const*> nodesToRemove;

  // ============ Distance ============
  AstNode const* distCenterExpr = nullptr;
  AstNode const* distCenterLatExpr = nullptr;
  AstNode const* distCenterLngExpr = nullptr;
  // Expression representing minimum distance
  AstNode const* minDistanceExpr = nullptr;
  // Was operator < or <= used
  bool minInclusive = true;
  // Expression representing maximum distance
  AstNode const* maxDistanceExpr = nullptr;
  // Was operator > or >= used
  bool maxInclusive = true;

  // ============ Near Info ============
  bool sorted = false;
  /// Default order is from closest to farthest
  bool ascending = true;

  // ============ Filter Info ===========
  geo::FilterType filterMode = geo::FilterType::NONE;
  /// variable using the filter mask
  AstNode const* filterExpr = nullptr;

  // ============ Accessed Fields ============
  AstNode const* locationVar = nullptr;   // access to location field
  AstNode const* latitudeVar = nullptr;   // access path to latitude
  AstNode const* longitudeVar = nullptr;  // access path to longitude

  /// contains this node a valid condition
  bool valid = true;
};

// checks 2 parameters of distance function if they represent a valid access to
// latitude and longitude attribute of the geo index.
// distance(a,b,c,d) - possible pairs are (a,b) and (c,d)
static bool distanceFuncArgCheck(ExecutionPlan* plan, AstNode const* latArg,
                                 AstNode const* lngArg, bool supportLegacy,
                                 GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess1;
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess2;
  // first and second should be based on the same document - need to provide
  // the document in order to see which collection is bound to it and if that
  // collections supports geo-index
  if (!latArg->isAttributeAccessForVariable(attributeAccess1, true) ||
      !lngArg->isAttributeAccessForVariable(attributeAccess2, true)) {
    return false;
  }
  TRI_ASSERT(attributeAccess1.first != nullptr);
  TRI_ASSERT(attributeAccess2.first != nullptr);

  ExecutionNode* setter1 = plan->getVarSetBy(attributeAccess1.first->id);
  ExecutionNode* setter2 = plan->getVarSetBy(attributeAccess2.first->id);
  if (setter1 == nullptr || setter1 != setter2 ||
      setter1->getType() != EN::ENUMERATE_COLLECTION) {
    return false;  // expect access of doc.lat, doc.lng or doc.loc[0],
                   // doc.loc[1]
  }

  // get logical collection
  auto collNode = ExecutionNode::castTo<EnumerateCollectionNode*>(setter1);
  if (info.collectionNodeToReplace != nullptr &&
      info.collectionNodeToReplace != collNode) {
    return false;  // should probably never happen
  }

  // we should not access the LogicalCollection directly
  auto indexes = collNode->collection()->indexes();
  // check for suitiable indexes
  for (std::shared_ptr<Index> idx : indexes) {
    // check if current index is a geo-index
    std::size_t fieldNum = idx->fields().size();
    bool isGeo1 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX &&
                  supportLegacy;
    bool isGeo2 = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX &&
                  supportLegacy;
    bool isGeo = idx->type() == Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;

    if ((isGeo2 || isGeo) && fieldNum == 2) {  // individual fields
      // check access paths of attributes in ast and those in index match
      if (idx->fields()[0] == attributeAccess1.second &&
          idx->fields()[1] == attributeAccess2.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;
        }
        info.index = idx;
        info.latitudeVar = latArg;
        info.longitudeVar = lngArg;
        info.collectionNodeToReplace = collNode;
        info.collectionNodeOutVar = collNode->outVariable();
        info.collection = collNode->collection();
        return true;
      }
    } else if ((isGeo1 || isGeo) && fieldNum == 1) {
      std::vector<basics::AttributeName> fields1 = idx->fields()[0];
      std::vector<basics::AttributeName> fields2 = idx->fields()[0];

      velocypack::SupervisedBuffer sb(
          plan->getAst()->query().resourceMonitor());
      VPackBuilder builder(sb);
      idx->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
      bool geoJson = basics::VelocyPackHelper::getBooleanValue(
          builder.slice(), "geoJson", false);

      fields1.back().name += geoJson ? "[1]" : "[0]";
      fields2.back().name += geoJson ? "[0]" : "[1]";
      if (fields1 == attributeAccess1.second &&
          fields2 == attributeAccess2.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;
        }
        info.index = idx;
        info.latitudeVar = latArg;
        info.longitudeVar = lngArg;
        info.collectionNodeToReplace = collNode;
        info.collectionNodeOutVar = collNode->outVariable();
        info.collection = collNode->collection();
        return true;
      }
    }  // if isGeo 1 or 2
  }    // for index in collection
  return false;
}

// checks parameter of GEO_* function
static bool geoFuncArgCheck(ExecutionPlan* plan, AstNode const* args,
                            bool supportLegacy, GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>
      attributeAccess;
  // "arg" is either `[doc.lat, doc.lng]` or `doc.geometry`
  if (args->isArray() && args->numMembers() == 2) {
    return distanceFuncArgCheck(plan, /*lat*/ args->getMemberUnchecked(1),
                                /*lng*/ args->getMemberUnchecked(0),
                                supportLegacy, info);
  } else if (!args->isAttributeAccessForVariable(attributeAccess, true)) {
    return false;  // no attribute access, no index check
  }
  TRI_ASSERT(attributeAccess.first != nullptr);
  ExecutionNode* setter = plan->getVarSetBy(attributeAccess.first->id);
  if (setter == nullptr || setter->getType() != EN::ENUMERATE_COLLECTION) {
    return false;  // expected access of the for doc.attribute
  }

  // get logical collection
  auto collNode = ExecutionNode::castTo<EnumerateCollectionNode*>(setter);
  if (info.collectionNodeToReplace != nullptr &&
      info.collectionNodeToReplace != collNode) {
    return false;  // should probably never happen
  }

  // we should not access the LogicalCollection directly
  auto indexes = collNode->collection()->indexes();
  // check for suitiable indexes
  for (std::shared_ptr<arangodb::Index> idx : indexes) {
    // check if current index is a geo-index
    bool isGeo =
        idx->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_GEO_INDEX;
    if (isGeo && idx->fields().size() == 1) {  // individual fields
      // check access paths of attributes in ast and those in index match
      if (idx->fields()[0] == attributeAccess.second) {
        if (info.index != nullptr && info.index != idx) {
          return false;  // different index
        }
        info.index = idx;
        info.locationVar = args;
        info.collectionNodeToReplace = collNode;
        info.collectionNodeOutVar = collNode->outVariable();
        info.collection = collNode->collection();
        return true;
      }
    }
  }  // for index in collection
  return false;
}

/// returns true if left side is same as right or lhs is null
static bool isValidGeoArg(AstNode const* lhs, AstNode const* rhs) {
  if (lhs == nullptr) {  // lhs is from the GeoIndexInfo struct
    return true;         // if geoindex field is null everything is valid
  } else if (lhs->type != rhs->type) {
    return false;
  } else if (lhs->isArray()) {  // expect `[doc.lng, doc.lat]`
    if (lhs->numMembers() >= 2 && rhs->numMembers() >= 2) {
      return isValidGeoArg(lhs->getMemberUnchecked(0),
                           rhs->getMemberUnchecked(0)) &&
             isValidGeoArg(lhs->getMemberUnchecked(1),
                           rhs->getMemberUnchecked(1));
    }
    return false;
  } else if (lhs->type == NODE_TYPE_REFERENCE) {
    return static_cast<Variable const*>(lhs->getData())->id ==
           static_cast<Variable const*>(rhs->getData())->id;
  }
  // compareAstNodes does not handle non const attribute access
  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> res1,
      res2;
  bool acc1 = lhs->isAttributeAccessForVariable(res1, true);
  bool acc2 = rhs->isAttributeAccessForVariable(res2, true);
  if (acc1 || acc2) {
    return acc1 && acc2 && res1 == res2;  // same variable same path
  }
  return aql::compareAstNodes(lhs, rhs, false) == 0;
}

static bool checkDistanceFunc(ExecutionPlan* plan, AstNode const* funcNode,
                              bool legacy, GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  if (funcNode->type == NODE_TYPE_REFERENCE) {
    // FOR x IN cc LET d = DISTANCE(...) FILTER d > 10 RETURN x
    Variable const* var = static_cast<Variable const*>(funcNode->getData());
    TRI_ASSERT(var != nullptr);
    ExecutionNode* setter = plan->getVarSetBy(var->id);
    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      return false;
    }
    funcNode =
        ExecutionNode::castTo<CalculationNode*>(setter)->expression()->node();
  }
  // get the ast node of the expression
  if (!funcNode || funcNode->type != NODE_TYPE_FCALL ||
      funcNode->numMembers() != 1) {
    return false;
  }
  AstNode* fargs = funcNode->getMemberUnchecked(0);
  auto func = static_cast<Function const*>(funcNode->getData());
  if (fargs->numMembers() >= 4 &&
      func->name == "DISTANCE") {  // allow DISTANCE(a,b,c,d)
    if (info.distCenterExpr != nullptr) {
      return false;  // do not allow mixing of DISTANCE and GEO_DISTANCE
    }
    if (isValidGeoArg(info.distCenterLatExpr, fargs->getMemberUnchecked(2)) &&
        isValidGeoArg(info.distCenterLngExpr, fargs->getMemberUnchecked(3)) &&
        distanceFuncArgCheck(plan, fargs->getMemberUnchecked(0),
                             fargs->getMemberUnchecked(1), legacy, info)) {
      info.distCenterLatExpr = fargs->getMemberUnchecked(2);
      info.distCenterLngExpr = fargs->getMemberUnchecked(3);
      return true;
    } else if (isValidGeoArg(info.distCenterLatExpr,
                             fargs->getMemberUnchecked(0)) &&
               isValidGeoArg(info.distCenterLngExpr,
                             fargs->getMemberUnchecked(1)) &&
               distanceFuncArgCheck(plan, fargs->getMemberUnchecked(2),
                                    fargs->getMemberUnchecked(3), legacy,
                                    info)) {
      info.distCenterLatExpr = fargs->getMemberUnchecked(0);
      info.distCenterLngExpr = fargs->getMemberUnchecked(1);
      return true;
    }
  } else if (fargs->numMembers() == 2 && func->name == "GEO_DISTANCE") {
    if (info.distCenterLatExpr || info.distCenterLngExpr) {
      return false;  // do not allow mixing of DISTANCE and GEO_DISTANCE
    }
    if (isValidGeoArg(info.distCenterExpr, fargs->getMemberUnchecked(1)) &&
        geoFuncArgCheck(plan, fargs->getMemberUnchecked(0), legacy, info)) {
      info.distCenterExpr = fargs->getMemberUnchecked(1);
      return true;
    } else if (isValidGeoArg(info.distCenterExpr,
                             fargs->getMemberUnchecked(0)) &&
               geoFuncArgCheck(plan, fargs->getMemberUnchecked(1), legacy,
                               info)) {
      info.distCenterExpr = fargs->getMemberUnchecked(0);
      return true;
    }
  }
  return false;
}

// contains the AstNode* a supported function?
static bool checkGeoFilterFunction(ExecutionPlan* plan, AstNode const* funcNode,
                                   GeoIndexInfo& info) {
  // note: this only modifies "info" if the function returns true
  // the expression must exist and it must be a function call
  if (funcNode->type != NODE_TYPE_FCALL || funcNode->numMembers() != 1 ||
      info.filterMode != geo::FilterType::NONE) {  // can't handle more than one
    return false;
  }

  auto func = static_cast<Function const*>(funcNode->getData());
  AstNode* fargs = funcNode->getMemberUnchecked(0);
  bool contains = func->name == "GEO_CONTAINS";
  bool intersect = func->name == "GEO_INTERSECTS";
  if ((!contains && !intersect) || fargs->numMembers() != 2) {
    return false;
  }

  AstNode* arg = fargs->getMemberUnchecked(1);
  if (geoFuncArgCheck(plan, arg, /*legacy*/ true, info)) {
    TRI_ASSERT(contains || intersect);
    info.filterMode =
        contains ? geo::FilterType::CONTAINS : geo::FilterType::INTERSECTS;
    info.filterExpr = fargs->getMemberUnchecked(0);
    TRI_ASSERT(info.index);
    return true;
  }
  return false;
}

// checks if a node contanis a geo index function a valid operator
// to use within a filter condition
bool checkGeoFilterExpression(ExecutionPlan* plan, AstNode const* node,
                              GeoIndexInfo& info) {
  // checks @first `smaller` @second
  // note: this only modifies "info" if the function returns true
  auto eval = [&](AstNode const* first, AstNode const* second,
                  bool lessequal) -> bool {
    if (second->type == NODE_TYPE_VALUE &&  // only constants allowed
        info.maxDistanceExpr == nullptr &&  // max distance is not yet set
        checkDistanceFunc(plan, first, /*legacy*/ true, info)) {
      TRI_ASSERT(info.index);
      info.maxDistanceExpr = second;
      info.maxInclusive = info.maxInclusive && lessequal;
      info.nodesToRemove.insert(node);
      return true;
    } else if (first->type == NODE_TYPE_VALUE &&  // only constants allowed
               info.minDistanceExpr ==
                   nullptr &&  // min distance is not yet set
               checkDistanceFunc(plan, second, /*legacy*/ true, info)) {
      info.minDistanceExpr = first;
      info.minInclusive = info.minInclusive && lessequal;
      info.nodesToRemove.insert(node);
      return true;
    }
    return false;
  };

  switch (node->type) {
    case NODE_TYPE_FCALL:
      if (checkGeoFilterFunction(plan, node, info)) {
        info.nodesToRemove.insert(node);
        return true;
      }
      return false;
    // only DISTANCE is allowed with <=, <, >=, >
    case NODE_TYPE_OPERATOR_BINARY_LE:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(0), node->getMember(1), true);
    case NODE_TYPE_OPERATOR_BINARY_LT:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(0), node->getMember(1), false);
    case NODE_TYPE_OPERATOR_BINARY_GE:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(1), node->getMember(0), true);
    case NODE_TYPE_OPERATOR_BINARY_GT:
      TRI_ASSERT(node->numMembers() == 2);
      return eval(node->getMember(1), node->getMember(0), false);
    default:
      return false;
  }
}

static bool optimizeSortNode(ExecutionPlan* plan, SortNode* sort,
                             GeoIndexInfo& info) {
  // note: info will only be modified if the function returns true
  TRI_ASSERT(sort->getType() == EN::SORT);
  // we're looking for "SORT DISTANCE(x,y,a,b)"
  SortElementVector const& elements = sort->elements();
  if (elements.size() != 1) {  // can't do it
    return false;
  }
  TRI_ASSERT(elements[0].var != nullptr);

  // find the expression that is bound to the variable
  // get the expression node that holds the calculation
  ExecutionNode* setter = plan->getVarSetBy(elements[0].var->id);
  if (setter == nullptr || setter->getType() != EN::CALCULATION) {
    return false;  // setter could be enumerate list node e.g.
  }
  CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(setter);
  Expression* expr = calc->expression();
  if (expr == nullptr || expr->node() == nullptr) {
    return false;  // the expression must exist and must have an astNode
  }

  // info will only be modified if the function returns true
  bool legacy = elements[0].ascending;  // DESC is only supported on S2 index
  if (!info.sorted && checkDistanceFunc(plan, expr->node(), legacy, info)) {
    info.sorted = true;  // do not parse another SORT
    info.ascending = elements[0].ascending;
    if (!ServerState::instance()->isCoordinator()) {
      // we must not remove a sort in the cluster... the results from each
      // shard will be sorted by using the index, however we still need to
      // establish a cross-shard sortedness by distance.
      info.exesToModify.emplace(sort, expr);
      info.nodesToRemove.emplace(expr->node());
    } else {
      // In the cluster case, we want to leave the SORT node in - for now!
      // This is to achieve that the GATHER node which is introduced to
      // distribute the query in the cluster remembers to sort things
      // using merge sort. However, later there will be a rule which
      // moves the sorting to the dbserver. When this rule is triggered,
      // we do not want to reinsert the SORT node on the dbserver, since
      // there, the items are already sorted by means of the geo index.
      // Therefore, we tell the sort node here, not to be reinserted
      // on the dbserver later on.
      // This is crucial to avoid that the SORT node remains and pulls
      // the whole collection out of the geo index, on the grounds that
      // the SORT node wants to sort the results, which is very bad for
      // performance.
      sort->dontReinsertInCluster();
    }
    return true;
  }
  return false;
}

// checks a single sort or filter node
static void optimizeFilterNode(ExecutionPlan* plan, FilterNode* fn,
                               GeoIndexInfo& info) {
  TRI_ASSERT(fn->getType() == EN::FILTER);

  // filter nodes always have one input variable
  auto variable = ExecutionNode::castTo<FilterNode const*>(fn)->inVariable();
  // now check who introduced our variable
  ExecutionNode* setter = plan->getVarSetBy(variable->id);
  if (setter == nullptr || setter->getType() != EN::CALCULATION) {
    return;
  }
  CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(setter);
  Expression* expr = calc->expression();
  if (expr->node() == nullptr) {
    return;  // the expression must exist and must have an AstNode
  }

  Ast::traverseReadOnly(
      expr->node(),
      [&](AstNode const* node) {  // pre
        if (node->isSimpleComparisonOperator() ||
            node->type == arangodb::aql::NODE_TYPE_FCALL ||
            node->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND ||
            node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND) {
          return true;
        }
        return false;
      },
      [&](AstNode const* node) {  // post
        if (!node->isSimpleComparisonOperator() &&
            node->type != arangodb::aql::NODE_TYPE_FCALL) {
          return;
        }
        if (checkGeoFilterExpression(plan, node, info)) {
          info.exesToModify.try_emplace(fn, expr);
        }
      });
}

// modify plan

// builds a condition that can be used with the index interface and
// contains all parameters required by the MMFilesGeoIndex
static std::unique_ptr<Condition> buildGeoCondition(ExecutionPlan* plan,
                                                    GeoIndexInfo const& info) {
  Ast* ast = plan->getAst();
  // shared code to add symbolic `doc.geometry` or `[doc.lng, doc.lat]`
  auto addLocationArg = [ast, &info](AstNode* args) {
    if (info.locationVar) {
      args->addMember(info.locationVar);
    } else if (info.latitudeVar && info.longitudeVar) {
      AstNode* array = ast->createNodeArray(2);
      array->addMember(info.longitudeVar);  // GeoJSON ordering
      array->addMember(info.latitudeVar);
      args->addMember(array);
    } else {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported geo type");
    }
  };

  TRI_ASSERT(info.index);
  auto cond = std::make_unique<Condition>(ast);
  bool hasCenter = info.distCenterLatExpr || info.distCenterExpr;
  bool hasDistLimit = info.maxDistanceExpr || info.minDistanceExpr;
  TRI_ASSERT(!hasCenter || hasDistLimit || info.sorted);
  if (hasCenter && (hasDistLimit || info.sorted)) {
    // create GEO_DISTANCE(...) [<|<=|>=|>] Var
    AstNode* args = ast->createNodeArray(2);
    if (info.distCenterLatExpr && info.distCenterLngExpr) {  // legacy
      TRI_ASSERT(!info.distCenterExpr);
      // info.sorted && info.ascending &&
      AstNode* array = ast->createNodeArray(2);
      array->addMember(info.distCenterLngExpr);  // GeoJSON ordering
      array->addMember(info.distCenterLatExpr);
      args->addMember(array);
    } else {
      TRI_ASSERT(info.distCenterExpr);
      TRI_ASSERT(!info.distCenterLatExpr && !info.distCenterLngExpr);
      args->addMember(info.distCenterExpr);  // center location
    }

    addLocationArg(args);
    AstNode* func = ast->createNodeFunctionCall("GEO_DISTANCE", args, true);

    TRI_ASSERT(info.maxDistanceExpr || info.minDistanceExpr || info.sorted);
    if (info.minDistanceExpr != nullptr) {
      AstNodeType t = info.minInclusive ? NODE_TYPE_OPERATOR_BINARY_GE
                                        : NODE_TYPE_OPERATOR_BINARY_GT;
      cond->andCombine(
          ast->createNodeBinaryOperator(t, func, info.minDistanceExpr));
    }
    if (info.maxDistanceExpr != nullptr) {
      AstNodeType t = info.maxInclusive ? NODE_TYPE_OPERATOR_BINARY_LE
                                        : NODE_TYPE_OPERATOR_BINARY_LT;
      cond->andCombine(
          ast->createNodeBinaryOperator(t, func, info.maxDistanceExpr));
    }
    if (info.minDistanceExpr == nullptr && info.maxDistanceExpr == nullptr &&
        info.sorted) {
      // hack to pass on the sort-to-point info
      AstNodeType t = NODE_TYPE_OPERATOR_BINARY_LT;
      std::string const& u = arangodb::StaticStrings::Unlimited;
      AstNode* cc = ast->createNodeValueString(u.c_str(), u.length());
      cond->andCombine(ast->createNodeBinaryOperator(t, func, cc));
    }
  }
  if (info.filterMode != geo::FilterType::NONE) {
    // create GEO_CONTAINS / GEO_INTERSECTS
    TRI_ASSERT(info.filterExpr);
    TRI_ASSERT(info.locationVar || (info.longitudeVar && info.latitudeVar));

    AstNode* args = ast->createNodeArray(2);
    args->addMember(info.filterExpr);
    addLocationArg(args);
    if (info.filterMode == geo::FilterType::CONTAINS) {
      cond->andCombine(ast->createNodeFunctionCall("GEO_CONTAINS", args, true));
    } else if (info.filterMode == geo::FilterType::INTERSECTS) {
      cond->andCombine(
          ast->createNodeFunctionCall("GEO_INTERSECTS", args, true));
    } else {
      TRI_ASSERT(false);
    }
  }

  cond->normalize(plan);
  return cond;
}

// applys the optimization for a candidate
static bool applyGeoOptimization(ExecutionPlan* plan, LimitNode* ln,
                                 GeoIndexInfo const& info) {
  TRI_ASSERT(info.collection != nullptr);
  TRI_ASSERT(info.collectionNodeToReplace != nullptr);
  TRI_ASSERT(info.index);

  // verify that all vars used in the index condition are valid
  auto const& valid = info.collectionNodeToReplace->getVarsValid();
  auto checkVars = [&valid](AstNode const* expr) {
    if (expr != nullptr) {
      VarSet varsUsed;
      Ast::getReferencedVariables(expr, varsUsed);
      for (Variable const* v : varsUsed) {
        if (valid.find(v) == valid.end()) {
          return false;  // invalid variable foud
        }
      }
    }
    return true;
  };
  if (!checkVars(info.distCenterExpr) || !checkVars(info.distCenterLatExpr) ||
      !checkVars(info.distCenterLngExpr) || !checkVars(info.filterExpr)) {
    return false;
  }

  size_t limit = 0;
  if (ln != nullptr) {
    limit = ln->offset() + ln->limit();
    TRI_ASSERT(limit != SIZE_MAX);
  }

  IndexIteratorOptions opts;
  opts.sorted = info.sorted;
  opts.ascending = info.ascending;
  opts.limit = limit;
  opts.evaluateFCalls = false;  // workaround to avoid evaluating "doc.geo"
  std::unique_ptr<Condition> condition(buildGeoCondition(plan, info));
  auto inode = plan->createNode<IndexNode>(
      plan, plan->nextId(), info.collection, info.collectionNodeOutVar,
      std::vector<transaction::Methods::IndexHandle>{
          transaction::Methods::IndexHandle{info.index}},
      false,  // here we are not using inverted index so
              // for sure no "whole" coverage
      std::move(condition), opts);
  plan->replaceNode(info.collectionNodeToReplace, inode);

  // remove expressions covered by our index
  Ast* ast = plan->getAst();
  for (std::pair<ExecutionNode*, Expression*> pair : info.exesToModify) {
    AstNode* root = pair.second->nodeForModification();
    auto pre = [&](AstNode const* node) -> bool {
      return node == root || Ast::isAndOperatorType(node->type);
    };
    auto visitor = [&](AstNode* node) -> AstNode* {
      if (Ast::isAndOperatorType(node->type)) {
        std::vector<AstNode*> keep;  // always shallow copy node
        for (std::size_t i = 0; i < node->numMembers(); i++) {
          AstNode* child = node->getMemberUnchecked(i);
          if (info.nodesToRemove.find(child) == info.nodesToRemove.end()) {
            keep.push_back(child);
          }
        }

        if (keep.size() > 2) {
          AstNode* n = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
          for (size_t i = 0; i < keep.size(); i++) {
            n->addMember(keep[i]);
          }
          return n;
        } else if (keep.size() == 2) {
          return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                               keep[0], keep[1]);
        } else if (keep.size() == 1) {
          return keep[0];
        }
        return node == root ? nullptr : ast->createNodeValueBool(true);
      } else if (info.nodesToRemove.find(node) != info.nodesToRemove.end()) {
        return node == root ? nullptr : ast->createNodeValueBool(true);
      }
      return node;
    };
    auto post = [](AstNode const*) {};
    AstNode* newNode = Ast::traverseAndModify(root, pre, visitor, post);
    if (newNode == nullptr) {  // if root was removed, unlink FILTER or SORT
      plan->unlinkNode(pair.first);
    } else if (newNode != root) {
      pair.second->replaceNode(newNode);
    }
  }

  // signal that plan has been changed
  return true;
}


void arangodb::aql::optimizeSubqueriesRule(Optimizer* opt,
                                           std::unique_ptr<ExecutionPlan> plan,
                                           OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::CALCULATION, true);

  // value type is {limit value, referenced by, used for counting}
  std::unordered_map<
      ExecutionNode*,
      std::tuple<int64_t, std::unordered_set<ExecutionNode const*>, bool>>
      subqueryAttributes;

  for (auto const& n : nodes) {
    auto cn = ExecutionNode::castTo<CalculationNode*>(n);
    AstNode const* root = cn->expression()->node();
    if (root == nullptr) {
      continue;
    }

    auto visitor = [&subqueryAttributes, &plan,
                    n](AstNode const* node) -> bool {
      std::pair<ExecutionNode*, int64_t> found{nullptr, 0};
      bool usedForCount = false;

      if (node->type == NODE_TYPE_REFERENCE) {
        Variable const* v = static_cast<Variable const*>(node->getData());
        auto setter = plan->getVarSetBy(v->id);
        if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
          // we found a subquery result being used somehow in some
          // way that will make the optimization produce wrong results
          found.first = setter;
          found.second = -1;  // negative values will disable the optimization
        }
      } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
        auto sub = node->getMemberUnchecked(0);
        if (sub->type == NODE_TYPE_REFERENCE) {
          Variable const* v = static_cast<Variable const*>(sub->getData());
          auto setter = plan->getVarSetBy(v->id);
          auto index = node->getMemberUnchecked(1);
          if (index->type == NODE_TYPE_VALUE && index->isNumericValue() &&
              setter != nullptr && setter->getType() == EN::SUBQUERY) {
            found.first = setter;
            found.second = index->getIntValue() + 1;  // x[0] => LIMIT 1
            if (found.second <= 0) {
              // turn optimization off
              found.second = -1;
            }
          }
        }
      } else if (node->type == NODE_TYPE_FCALL && node->numMembers() > 0) {
        auto func = static_cast<Function const*>(node->getData());
        auto args = node->getMember(0);
        if (func->name == "FIRST" || func->name == "LENGTH" ||
            func->name == "COUNT") {
          if (args->numMembers() > 0 &&
              args->getMember(0)->type == NODE_TYPE_REFERENCE) {
            Variable const* v =
                static_cast<Variable const*>(args->getMember(0)->getData());
            auto setter = plan->getVarSetBy(v->id);
            if (setter != nullptr && setter->getType() == EN::SUBQUERY) {
              found.first = setter;
              if (func->name == "FIRST") {
                found.second = 1;  // FIRST(x) => LIMIT 1
              } else {
                found.second = -1;
                usedForCount = true;
              }
            }
          }
        }
      }

      if (found.first != nullptr) {
        auto it = subqueryAttributes.find(found.first);
        if (it == subqueryAttributes.end()) {
          subqueryAttributes.try_emplace(
              found.first,
              std::make_tuple(found.second,
                              std::unordered_set<ExecutionNode const*>{n},
                              usedForCount));
        } else {
          auto& sq = (*it).second;
          if (usedForCount) {
            // COUNT + LIMIT together will turn off the optimization
            std::get<2>(sq) = (std::get<0>(sq) <= 0);
            std::get<0>(sq) = -1;
            std::get<1>(sq).clear();
          } else {
            if (found.second <= 0 || std::get<0>(sq) < 0) {
              // negative value will turn off the optimization
              std::get<0>(sq) = -1;
              std::get<1>(sq).clear();
            } else {
              // otherwise, use the maximum of the limits needed, and insert
              // current node into our "safe" list
              std::get<0>(sq) = std::max(std::get<0>(sq), found.second);
              std::get<1>(sq).emplace(n);
            }
            std::get<2>(sq) = false;
          }
        }
        // don't descend further
        return false;
      }

      // descend further
      return true;
    };

    Ast::traverseReadOnly(root, visitor, [](AstNode const*) {});
  }

  for (auto const& it : subqueryAttributes) {
    ExecutionNode* node = it.first;
    TRI_ASSERT(node->getType() == EN::SUBQUERY);
    auto sn = ExecutionNode::castTo<SubqueryNode const*>(node);

    if (sn->isModificationNode()) {
      // cannot push a LIMIT into data-modification subqueries
      continue;
    }

    auto const& sq = it.second;
    int64_t limitValue = std::get<0>(sq);
    bool usedForCount = std::get<2>(sq);
    if (limitValue <= 0 && !usedForCount) {
      // optimization turned off
      continue;
    }

    // scan from the subquery node to the bottom of the ExecutionPlan to check
    // if any of the following nodes also use the subquery result
    auto out = sn->outVariable();
    VarSet used;
    bool invalid = false;

    auto current = node->getFirstParent();
    while (current != nullptr) {
      auto const& referencedBy = std::get<1>(sq);
      if (referencedBy.find(current) == referencedBy.end()) {
        // node not found in "safe" list
        // now check if it uses the subquery's out variable
        used.clear();
        current->getVariablesUsedHere(used);
        if (used.find(out) != used.end()) {
          invalid = true;
          break;
        }
      }
      // continue iteration
      current = current->getFirstParent();
    }

    if (invalid) {
      continue;
    }

    auto root = sn->getSubquery();
    if (root != nullptr && root->getType() == EN::RETURN) {
      // now inject a limit
      auto f = root->getFirstDependency();
      TRI_ASSERT(f != nullptr);

      if (std::get<2>(sq)) {
        Ast* ast = plan->getAst();
        // generate a calculation node that only produces "true"
        auto expr =
            std::make_unique<Expression>(ast, ast->createNodeValueBool(true));
        Variable* outVariable = ast->variables()->createTemporaryVariable();
        auto calcNode = plan->createNode<CalculationNode>(
            plan.get(), plan->nextId(), std::move(expr), outVariable);
        plan->insertAfter(f, calcNode);
        // change the result value of the existing Return node
        TRI_ASSERT(root->getType() == EN::RETURN);
        ExecutionNode::castTo<ReturnNode*>(root)->inVariable(outVariable);
        modified = true;
        continue;
      }

      if (f->getType() == EN::LIMIT) {
        // subquery already has a LIMIT node at its end
        // no need to do anything
        continue;
      }

      auto limitNode = plan->createNode<LimitNode>(plan.get(), plan->nextId(),
                                                   0, limitValue);
      plan->insertAfter(f, limitNode);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

namespace {

void findSubqueriesSuitableForSplicing(
    ExecutionPlan const& plan,
    containers::SmallVector<SubqueryNode*, 8>& result) {
  TRI_ASSERT(result.empty());
  using ResultVector = decltype(result);

  using SuitableNodeSet =
      std::set<SubqueryNode*, std::less<>,
               containers::detail::short_alloc<SubqueryNode*, 128,
                                               alignof(SubqueryNode*)>>;

  // This finder adds all subquery nodes in pre-order to its `result` parameter,
  // and all nodes that are suitable for splicing to `suitableNodes`. Suitable
  // means that neither the containing subquery contains unsuitable nodes - at
  // least not in an ancestor of the subquery - nor the subquery contains
  // unsuitable nodes (directly, not recursively).
  //
  // It will be used in a fashion where the recursive walk on subqueries is done
  // *before* the recursive walk on dependencies.
  // It maintains a stack of bools for every subquery level. The topmost bool
  // holds whether we've encountered a skipping block so far.
  // When leaving a subquery, we decide whether it is suitable for splicing by
  // inspecting the two topmost bools in the stack - the one belonging to the
  // insides of the subquery, which we're going to pop right now, and the one
  // belonging to the containing subquery.
  //
  // *All* subquery nodes will be added to &result in pre-order, and all
  // *suitable* subquery nodes will be added to &suitableNodes. The latter can
  // be omitted later, as soon as support for spliced subqueries / shadow rows
  // is complete.

  class Finder final
      : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
   public:
    explicit Finder(ResultVector& result, SuitableNodeSet& suitableNodes)
        : _result{result}, _suitableNodes{suitableNodes} {}

    bool before(ExecutionNode* node) final {
      TRI_ASSERT(node->getType() != EN::MUTEX);  // should never appear here
      if (node->getType() == ExecutionNode::SUBQUERY) {
        _result.emplace_back(ExecutionNode::castTo<SubqueryNode*>(node));
      }
      return false;
    }

    bool enterSubquery(ExecutionNode*, ExecutionNode*) final {
      ++_isSuitableLevel;
      return true;
    }

    void leaveSubquery(ExecutionNode* subQuery, ExecutionNode*) final {
      TRI_ASSERT(_isSuitableLevel != 0);
      --_isSuitableLevel;
      _suitableNodes.emplace(ExecutionNode::castTo<SubqueryNode*>(subQuery));
    }

   private:
    // all subquery nodes will be added to _result in pre-order
    ResultVector& _result;
    // only suitable subquery nodes will be added to this set
    SuitableNodeSet& _suitableNodes;
    size_t _isSuitableLevel{1};  // push the top-level query
  };

  using SuitableNodeArena = SuitableNodeSet::allocator_type::arena_type;
  SuitableNodeArena suitableNodeArena{};
  SuitableNodeSet suitableNodes{suitableNodeArena};

  auto finder = Finder{result, suitableNodes};
  plan.root()->walkSubqueriesFirst(finder);

  {  // remove unsuitable nodes from result
    auto i = size_t{0};
    auto j = size_t{0};
    for (; j < result.size(); ++j) {
      TRI_ASSERT(i <= j);
      if (suitableNodes.count(result[j]) > 0) {
        if (i != j) {
          TRI_ASSERT(suitableNodes.count(result[i]) == 0);
          result[i] = result[j];
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          // To allow for the assert above
          result[j] = nullptr;
#endif
        }
        ++i;
      }
    }
    TRI_ASSERT(i <= result.size());
    result.resize(i);
  }
}
}  // namespace

// Splices in subqueries by replacing subquery nodes by
// a SubqueryStartNode and a SubqueryEndNode with the subquery's nodes
// in between.
void arangodb::aql::spliceSubqueriesRule(Optimizer* opt,
                                         std::unique_ptr<ExecutionPlan> plan,
                                         OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<SubqueryNode*, 8> subqueryNodes;
  findSubqueriesSuitableForSplicing(*plan, subqueryNodes);

  // Note that we rely on the order `subqueryNodes` in the sense that, for
  // nested subqueries, the outer subquery must come before the inner, so we
  // don't iterate over spliced queries here.
  auto forAllDeps = [](ExecutionNode* node, auto cb) {
    for (; node != nullptr; node = node->getFirstDependency()) {
      TRI_ASSERT(node->getType() != ExecutionNode::SUBQUERY_START);
      TRI_ASSERT(node->getType() != ExecutionNode::SUBQUERY_END);
      cb(node);
    }
  };

  for (auto const& sq : subqueryNodes) {
    modified = true;
    auto evenNumberOfRemotes = true;

    forAllDeps(sq->getSubquery(), [&](auto node) {
      node->setIsInSplicedSubquery(true);
      if (node->getType() == ExecutionNode::REMOTE) {
        evenNumberOfRemotes = !evenNumberOfRemotes;
      }
    });

    auto const addClusterNodes = !evenNumberOfRemotes;

    {  // insert SubqueryStartNode

      // Create new start node
      auto start = plan->createNode<SubqueryStartNode>(
          plan.get(), plan->nextId(), sq->outVariable());

      // start and end inherit this property from the subquery node
      start->setIsInSplicedSubquery(sq->isInSplicedSubquery());

      // insert a SubqueryStartNode before the SubqueryNode
      plan->insertBefore(sq, start);
      // remove parent/dependency relation between sq and start
      TRI_ASSERT(start->getParents().size() == 1);
      sq->removeDependency(start);
      TRI_ASSERT(start->getParents().size() == 0);
      TRI_ASSERT(start->getDependencies().size() == 1);
      TRI_ASSERT(sq->getDependencies().size() == 0);
      TRI_ASSERT(sq->getParents().size() == 1);

      {  // remove singleton
        ExecutionNode* const singleton = sq->getSubquery()->getSingleton();
        std::vector<ExecutionNode*> const parents = singleton->getParents();
        TRI_ASSERT(parents.size() == 1);
        auto oldSingletonParent = parents[0];
        TRI_ASSERT(oldSingletonParent->getDependencies().size() == 1);
        // All parents of the Singleton of the subquery become parents of the
        // SubqueryStartNode. The singleton will be deleted after.
        for (auto* x : parents) {
          TRI_ASSERT(x != nullptr);
          x->replaceDependency(singleton, start);
        }
        TRI_ASSERT(oldSingletonParent->getDependencies().size() == 1);
        TRI_ASSERT(start->getParents().size() == 1);

        if (addClusterNodes) {
          auto const scatterNode = plan->createNode<ScatterNode>(
              plan.get(), plan->nextId(), ScatterNode::SHARD);
          auto const remoteNode = plan->createNode<RemoteNode>(
              plan.get(), plan->nextId(), &plan->getAst()->query().vocbase(),
              "", "", "");
          scatterNode->setIsInSplicedSubquery(true);
          remoteNode->setIsInSplicedSubquery(true);
          plan->insertAfter(start, scatterNode);
          plan->insertAfter(scatterNode, remoteNode);

          TRI_ASSERT(remoteNode->getDependencies().size() == 1);
          TRI_ASSERT(scatterNode->getDependencies().size() == 1);
          TRI_ASSERT(remoteNode->getParents().size() == 1);
          TRI_ASSERT(scatterNode->getParents().size() == 1);
          TRI_ASSERT(oldSingletonParent->getFirstDependency() == remoteNode);
          TRI_ASSERT(remoteNode->getFirstDependency() == scatterNode);
          TRI_ASSERT(scatterNode->getFirstDependency() == start);
          TRI_ASSERT(start->getFirstParent() == scatterNode);
          TRI_ASSERT(scatterNode->getFirstParent() == remoteNode);
          TRI_ASSERT(remoteNode->getFirstParent() == oldSingletonParent);
        } else {
          TRI_ASSERT(oldSingletonParent->getFirstDependency() == start);
          TRI_ASSERT(start->getFirstParent() == oldSingletonParent);
        }
      }
    }

    {  // insert SubqueryEndNode

      ExecutionNode* subqueryRoot = sq->getSubquery();
      Variable const* inVariable = nullptr;

      if (subqueryRoot->getType() == ExecutionNode::RETURN) {
        // The SubqueryEndExecutor can read the input from the return Node.
        auto subqueryReturn = ExecutionNode::castTo<ReturnNode*>(subqueryRoot);
        inVariable = subqueryReturn->inVariable();
        // Every return can only have a single dependency
        TRI_ASSERT(subqueryReturn->getDependencies().size() == 1);
        subqueryRoot = subqueryReturn->getFirstDependency();
        TRI_ASSERT(!plan->isRoot(subqueryReturn));
        plan->unlinkNode(subqueryReturn, true);
      }

      // Create new end node
      auto end = plan->createNode<SubqueryEndNode>(
          plan.get(), plan->nextId(), inVariable, sq->outVariable());
      // start and end inherit this property from the subquery node
      end->setIsInSplicedSubquery(sq->isInSplicedSubquery());
      // insert a SubqueryEndNode after the SubqueryNode sq
      plan->insertAfter(sq, end);

      end->replaceDependency(sq, subqueryRoot);

      TRI_ASSERT(end->getDependencies().size() == 1);
      TRI_ASSERT(end->getParents().size() == 1);
    }
    TRI_ASSERT(sq->getDependencies().empty());
    TRI_ASSERT(sq->getParents().empty());
  }

  if (modified) {
    plan->root()->invalidateCost();
  }
  opt->addPlan(std::move(plan), rule, modified);
}
