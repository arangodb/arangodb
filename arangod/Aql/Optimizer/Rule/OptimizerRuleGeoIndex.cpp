////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "OptimizerRuleGeoIndex.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Optimizer.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Variable.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

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


void arangodb::aql::geoIndexRule(Optimizer* opt,
                                 std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  bool mod = false;

  plan->findNodesOfType(nodes, EN::ENUMERATE_COLLECTION, true);
  for (ExecutionNode* node : nodes) {
    GeoIndexInfo info;
    info.collectionNodeToReplace = node;

    ExecutionNode* current = node->getFirstParent();
    LimitNode* limit = nullptr;
    bool canUseSortLimit = true;
    bool mustRespectIdxHint = false;
    auto enumerateColNode =
        ExecutionNode::castTo<EnumerateCollectionNode const*>(node);
    auto const& colNodeHints = enumerateColNode->hint();
    if (colNodeHints.isForced() && colNodeHints.isSimple()) {
      auto indexes = enumerateColNode->collection()->indexes();
      auto& idxNames = colNodeHints.candidateIndexes();
      for (auto const& idxName : idxNames) {
        for (std::shared_ptr<Index> const& idx : indexes) {
          if (idx->name() == idxName) {
            auto idxType = idx->type();
            if ((idxType != Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX) &&
                (idxType != Index::IndexType::TRI_IDX_TYPE_GEO2_INDEX) &&
                (idxType != Index::IndexType::TRI_IDX_TYPE_GEO_INDEX)) {
              mustRespectIdxHint = true;
            } else {
              info.index = idx;
            }
            break;
          }
        }
      }
    }

    while (current) {
      if (current->getType() == EN::FILTER) {
        // picking up filter conditions is always allowed
        optimizeFilterNode(plan.get(),
                           ExecutionNode::castTo<FilterNode*>(current), info);
      } else if (current->getType() == EN::SORT && canUseSortLimit) {
        // only pick up a sort clause if we haven't seen another loop yet
        if (!optimizeSortNode(
                plan.get(), ExecutionNode::castTo<SortNode*>(current), info)) {
          // 1. EnumerateCollectionNode x
          // 2. SortNode x.abc ASC
          // 3. LimitNode n,m  <-- cannot reuse LIMIT node here
          // limit = nullptr;
          break;  // stop parsing on non-optimizable SORT
        }
      } else if (current->getType() == EN::LIMIT && canUseSortLimit) {
        // only pick up a limit clause if we haven't seen another loop yet
        limit = ExecutionNode::castTo<LimitNode*>(current);
        break;  // stop parsing after first LIMIT
      } else if (current->getType() == EN::RETURN ||
                 current->getType() == EN::COLLECT) {
        break;  // stop parsing on return or collect
      } else if (current->getType() == EN::INDEX ||
                 current->getType() == EN::ENUMERATE_COLLECTION ||
                 current->getType() == EN::ENUMERATE_LIST ||
                 current->getType() == EN::ENUMERATE_IRESEARCH_VIEW ||
                 current->getType() == EN::TRAVERSAL ||
                 current->getType() == EN::ENUMERATE_PATHS ||
                 current->getType() == EN::SHORTEST_PATH) {
        // invalidate limit and sort. filters can still be used
        limit = nullptr;
        info.sorted = false;
        // don't allow picking up either sort or limit from here on
        canUseSortLimit = false;
      }
      current = current->getFirstParent();  // inspect next node
    }

    // if info is valid we try to optimize ENUMERATE_COLLECTION
    if (info && info.collectionNodeToReplace == node) {
      if (!mustRespectIdxHint &&
          applyGeoOptimization(plan.get(), limit, info)) {
        mod = true;
      }
    }
  }

  opt->addPlan(std::move(plan), rule, mod);
}
