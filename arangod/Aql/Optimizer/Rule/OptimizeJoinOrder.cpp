////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "OptimizeJoinOrder.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/Expression.h"
#include "Aql/Optimizer.h"
#include "Aql/QueryContext.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"

#include <ranges>

namespace arangodb::aql {

using AttributePath = std::vector<std::string_view>;

template<typename Operator>
auto findBestSelectivity(Operator op, double initial,
                         Collection const* collection,
                         std::vector<AttributePath> attributes) -> double {
  for (auto& index : collection->indexes()) {
    if (index->type() != Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX &&
        index->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX &&
        index->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      continue;
    }

    if (not index->hasSelectivityEstimate()) {
      continue;
    }

    bool covers = true;
    auto indexAttributes = index->fieldNames();

    for (auto& attr : indexAttributes) {
      AttributePath path;
      std::copy(attr.begin(), attr.end(), std::back_inserter(path));
      if (std::find(attributes.begin(), attributes.end(), path) ==
          attributes.end()) {
        // index attribute is not part of the join, we can't use this index
        covers = false;
        break;
      }
    }

    if (!covers) {
      continue;
    }

    double current = index->selectivityEstimate();
    LOG_DEVEL << "INDEX SELECTIVITY " << index->id()
              << " fields = " << indexAttributes << " -> " << current;
    initial = op(initial, current);
  }

  return initial;
}

auto computeAttributeSetApproxDistinctValues(
    ExecutionPlan const* plan, Collection const* collection,
    std::vector<AttributePath> attributes) -> double {
  auto& trx = plan->getAst()->query().trxForOptimization();

  LOG_DEVEL << "COMPUTING SELECTIVITY FOR ATTRIBUTE SET " << collection->name()
            << " " << attributes;

  auto documentCount =
      collection->count(&trx, transaction::CountType::kTryCache);
  // we want to compute the approximate number of distinct tuples of this
  // attribute set. For that we search for indexes that index a subset
  // of our attributes. We take the best estimate.
  // number of distinct values = count * selectivity
  double best =
      findBestSelectivity(std::less<double>{}, 0, collection, attributes);

  return static_cast<double>(documentCount) * best;
}

struct JoinGraph {
  struct Node {
    EnumerateCollectionNode* executionNode;

    std::vector<AttributePath> conditions;
    double relevantDocumentCount = 0;

    Node(EnumerateCollectionNode* executionNode)
        : executionNode(executionNode) {}
  };

  using NodeId = uint32_t;

  struct Edge {
    Node *from, *to;

    double selectivity = 1.0;

    std::vector<AttributePath> fromAttributes;
    std::vector<AttributePath> toAttributes;
  };

  auto nodeForVariable(Variable const* variable) -> Node* {
    auto iter = nodes.find(variable);
    if (iter == nodes.end()) {
      return nullptr;
    } else {
      return &iter->second;
    }
  }

  void addNode(EnumerateCollectionNode* en) {
    nodes.emplace(en->outVariable(), en);
  }

  auto ensureEdge(Variable const* v, Variable const* w) -> Edge& {
    // find vertices for variables
    auto* from = nodeForVariable(v);
    auto* to = nodeForVariable(w);
    ADB_PROD_ASSERT(from != nullptr && to != nullptr);

    // now find the edge
    auto iter = std::find_if(edges.begin(), edges.end(), [&](auto const& e) {
      return e.from == from && e.to == to;
    });
    if (iter == edges.end()) {
      edges.emplace_back(from, to);
      return edges.back();
    } else {
      return *iter;
    }
  }

  auto addJoinCondition(Variable const* v, AttributePath vAttributes,
                        Variable const* w, AttributePath wAttributes) -> Edge& {
    auto& edge = ensureEdge(v, w);
    if (edge.from->executionNode->outVariable() == v) {
      edge.fromAttributes.emplace_back(std::move(vAttributes));
      edge.toAttributes.emplace_back(std::move(wAttributes));
    } else {
      edge.toAttributes.emplace_back(std::move(vAttributes));
      edge.fromAttributes.emplace_back(std::move(wAttributes));
    }
    return edge;
  }

  auto getEdgesForNode(Node* node) -> std::vector<Edge*> {
    std::vector<Edge*> result;
    for (auto& e : edges) {
      if (e.from == node || e.to == node) {
        result.emplace_back(&e);
      }
    }
    return result;
  }

  std::map<Variable const*, Node> nodes;
  std::vector<Edge> edges;
};

auto gatherEstimates(ExecutionPlan* plan, JoinGraph& graph) {
  // compute selectivity for all edges
  for (auto& e : graph.edges) {
    auto fromAttributes = e.fromAttributes;
    std::copy(e.from->conditions.begin(), e.from->conditions.end(),
              std::back_inserter(fromAttributes));
    auto fromSelectivity = computeAttributeSetApproxDistinctValues(
        plan, e.from->executionNode->collection(), fromAttributes);
    auto toAttributes = e.toAttributes;
    std::copy(e.to->conditions.begin(), e.to->conditions.end(),
              std::back_inserter(toAttributes));
    auto toSelectivity = computeAttributeSetApproxDistinctValues(
        plan, e.to->executionNode->collection(), toAttributes);

    e.selectivity = 1. / std::max(fromSelectivity, toSelectivity);

    LOG_DEVEL << "EDGE SELECTIVITY "
              << e.from->executionNode->outVariable()->name << " -> "
              << e.to->executionNode->outVariable()->name << " = "
              << e.selectivity;
  }

  for (auto& [var, node] : graph.nodes) {
    auto selectivity = computeAttributeSetApproxDistinctValues(
        plan, node.executionNode->collection(), node.conditions);

    node.relevantDocumentCount =
        node.executionNode->collection()->count(
            &plan->getAst()->query().trxForOptimization(),
            transaction::CountType::kTryCache) *
        selectivity;
    LOG_DEVEL << "NODE SELECTIVITY " << node.executionNode->outVariable()->name
              << " = " << node.relevantDocumentCount;
  }
}

auto optimize(ExecutionPlan* plan, JoinGraph& graph)
    -> std::vector<ExecutionNode*> {
  gatherEstimates(plan, graph);

  std::vector<ExecutionNode*> result;
  for (auto& [var, n] : graph.nodes) {
    result.push_back(n.executionNode);
  }
  return result;
}

auto extractAttributeAccess(AstNode const* n,
                            std::vector<std::string_view>& path)
    -> Variable const* {
  if (n->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    auto var = extractAttributeAccess(n->getMemberUnchecked(0), path);
    if (var != nullptr) {
      auto attr = n->getStringView();
      if (path.empty() && attr == "_id") {
        attr = "_key";
      }
      path.emplace_back(attr);
    }
    return var;
  } else if (n->type == NODE_TYPE_REFERENCE) {
    return static_cast<Variable const*>(n->getData());
  } else {
    return nullptr;
  }
}

auto extractAttributeAccess(AstNode const* n)
    -> std::optional<std::pair<Variable const*, AttributePath>> {
  AttributePath path;
  auto var = extractAttributeAccess(n, path);
  if (var != nullptr) {
    return std::make_pair(var, std::move(path));
  }
  return std::nullopt;
}

void handleExpression(JoinGraph& graph, ExecutionPlan* plan,
                      AstNode const* root) {
  Condition cond{plan->getAst()};
  cond.andCombine(root);
  cond.normalize();
  root = cond.root();

  // check if the expression has a single or branch
  if (root->type != AstNodeType::NODE_TYPE_OPERATOR_NARY_OR ||
      root->numMembers() != 1) {
    // add as residual
    return;
  }

  auto ands = root->getMemberUnchecked(0);
  if (ands->type != AstNodeType::NODE_TYPE_OPERATOR_NARY_AND) {
    // add as residual
    return;
  }

  for (size_t i = 0; i < ands->numMembers(); i++) {
    auto cond = ands->getMemberUnchecked(i);

    // check if this is a equijoin
    if (cond->type != NODE_TYPE_OPERATOR_BINARY_EQ) {
      // no - add as residual
      continue;
    }

    auto checkIsGraphVariableAccess =
        [&](std::optional<std::pair<Variable const*, AttributePath>>
                maybeAccess)
        -> std::optional<
            std::tuple<Variable const*, JoinGraph::Node*, AttributePath>> {
      if (maybeAccess.has_value()) {
        auto& [var, path] = *maybeAccess;
        if (auto node = graph.nodeForVariable(var); node != nullptr) {
          return std::make_tuple(var, node, std::move(path));
        }
      }

      return std::nullopt;
    };

    auto lhs = cond->getMemberUnchecked(0);
    auto maybeLhsAccess =
        checkIsGraphVariableAccess(extractAttributeAccess(lhs));

    auto rhs = cond->getMemberUnchecked(1);
    auto maybeRhsAccess =
        checkIsGraphVariableAccess(extractAttributeAccess(rhs));

    if (maybeRhsAccess.has_value() and not maybeLhsAccess.has_value()) {
      std::swap(maybeLhsAccess, maybeRhsAccess);
    }

    if (maybeLhsAccess.has_value() && maybeRhsAccess.has_value()) {
      auto& [lshVar, lhsNode, lhsPath] = maybeLhsAccess.value();
      auto& [rshVar, rhsNode, rhsPath] = maybeRhsAccess.value();

      graph.addJoinCondition(lshVar, lhsPath, rshVar, rhsPath);
    } else if (maybeLhsAccess.has_value()) {
      auto& [lshVar, lhsNode, lhsPath] = maybeLhsAccess.value();
      lhsNode->conditions.emplace_back(std::move(lhsPath));
    }
  }
}

void optimizeJoinOrder(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                       OptimizerRule const& rule) {
  plan->show();

  std::unique_ptr<JoinGraph> graph;

  ExecutionNode* firstDependency = nullptr;
  ExecutionNode* nextNode = nullptr;
  for (ExecutionNode* n = plan->root()->getSingleton(); n != nullptr;
       n = nextNode) {
    nextNode = n->getFirstParent();

    switch (n->getType()) {
      case ExecutionNode::ENUMERATE_COLLECTION: {
        auto* en = ExecutionNode::castTo<EnumerateCollectionNode*>(n);
        if (graph == nullptr) {  // create new graph if necessary
          // TODO record variables available here, so that we know what is
          // _constant_ for this join
          graph = std::make_unique<JoinGraph>();
          firstDependency = n->getFirstDependency();
        }
        graph->addNode(en);
        plan->unlinkNode(n);
        break;
      }

      case ExecutionNode::FILTER: {
        if (graph == nullptr) {
          continue;
        }

        auto* filter = ExecutionNode::castTo<FilterNode*>(n);
        auto* calc = ExecutionNode::castTo<CalculationNode*>(
            plan->getVarSetBy(filter->inVariable()->id));

        auto* root = calc->expression()->node();
        handleExpression(*graph, plan.get(), root);
      }

      case ExecutionNode::CALCULATION:
        break;  // nothing to do here
      default:
        // finalize current graph
        if (graph != nullptr) {
          // compute the correct join order, place filter statements and
          // residuals
          auto nodes = optimize(plan.get(), *graph);

          // insert all enumerate collection nodes in the optimized order
          // after first dependency
          TRI_ASSERT(firstDependency != nullptr);
          for (auto& node : std::views::reverse(nodes)) {
            plan->insertAfter(firstDependency, node);
          }

          graph = nullptr;
        }
    }
  }

  plan->show();
  opt->addPlan(std::move(plan), rule, true);
}

}  // namespace arangodb::aql