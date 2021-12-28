////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/FixedVarExpressionContext.h"
#include "Basics/Common.h"
#include "Graph/BaseOptions.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"

#include <memory>

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
struct AstNode;
class Expression;
class PruneExpressionEvaluator;
class Query;
class TraversalNode;
struct Variable;
}  // namespace aql

namespace graph {
class EdgeCursor;
}

namespace traverser {

class ClusterTraverser;

struct TraverserOptions : public graph::BaseOptions {
  friend class arangodb::aql::TraversalNode;

 public:
  enum UniquenessLevel { NONE, PATH, GLOBAL };
  enum class Order { DFS, BFS, WEIGHTED };

 protected:
  std::unordered_map<uint64_t, std::vector<LookupInfo>> _depthLookupInfo;

  std::unordered_map<uint64_t, std::unique_ptr<aql::Expression>>
      _vertexExpressions;

  std::unique_ptr<aql::Expression> _baseVertexExpression;

  arangodb::traverser::ClusterTraverser* _traverser;

  /// @brief The condition given in PRUNE (might be empty)
  ///        The Node keeps responsibility
  // TODO [GraphRefactor]: This got changed to shared_ptr as the options of the
  // PathValidator needs the PruneExpression. Using unique_ptrs causes troubles
  // with the auto generated default constructors.
  std::unique_ptr<aql::PruneExpressionEvaluator> _pruneExpression;

  /// @brief The condition given for PostFilters (might be empty)
  ///        The Node keeps responsibility
  ///        This is used to avoid producing paths if the last vertex or edge do
  ///        not match.
  std::unique_ptr<aql::PruneExpressionEvaluator> _postFilterExpression;

  bool _producePathsVertices{true};
  bool _producePathsEdges{true};
  bool _producePathsWeights{true};  // only used by WeightedEnumerator

 public:
  uint64_t minDepth;

  uint64_t maxDepth;

  bool useNeighbors;

  UniquenessLevel uniqueVertices;

  UniquenessLevel uniqueEdges;

  Order mode;

  std::string weightAttribute;

  double defaultWeight;

  std::vector<std::string> vertexCollections;

  std::vector<std::string> edgeCollections;

  bool _isDisjoint = false;

  explicit TraverserOptions(arangodb::aql::QueryContext& query);

  TraverserOptions(arangodb::aql::QueryContext& query,
                   arangodb::velocypack::Slice definition);

  TraverserOptions(arangodb::aql::QueryContext& query,
                   arangodb::velocypack::Slice info,
                   arangodb::velocypack::Slice collections);

  /// @brief This copy constructor is only working during planning phase.
  ///        After planning this node should not be copied anywhere.
  ///        When allowAlreadyBuiltCopy is true, the constructor also works
  ///        after the planning phase; however, the options have to be prepared
  ///        again (see TraversalNode::prepareOptions())
  TraverserOptions(TraverserOptions const& other,
                   bool allowAlreadyBuiltCopy = false);
  TraverserOptions& operator=(TraverserOptions const&) = delete;

  virtual ~TraverserOptions();

  /// @brief Build a velocypack for cloning in the plan.
  void toVelocyPack(arangodb::velocypack::Builder&) const override;

  /// @brief Build a velocypack for indexes
  void toVelocyPackIndexes(arangodb::velocypack::Builder&) const override;

  /// @brief Build a velocypack containing all relevant information
  ///        for DBServer traverser engines.
  void buildEngineInfo(arangodb::velocypack::Builder&) const override;

  /// @brief Whether or not the edge collection shall be excluded
  bool shouldExcludeEdgeCollection(std::string const& name) const override;

  /// @brief Add a lookup info for specific depth
  void addDepthLookupInfo(aql::ExecutionPlan* plan,
                          std::string const& collectionName,
                          std::string const& attributeName,
                          aql::AstNode* condition, uint64_t depth,
                          bool onlyEdgeIndexes = false);

  bool hasDepthLookupInfo() const { return !_depthLookupInfo.empty(); }

  bool vertexHasFilter(uint64_t) const;

  bool hasEdgeFilter(int64_t, size_t) const;

  bool hasWeightAttribute() const;

  bool hasVertexCollectionRestrictions() const;

  bool evaluateEdgeExpression(arangodb::velocypack::Slice,
                              std::string_view vertexId, uint64_t, size_t);

  bool evaluateVertexExpression(arangodb::velocypack::Slice, uint64_t);

  bool checkSmartDestination(VPackSlice edge,
                             std::string_view sourceVertex) const;

  bool destinationCollectionAllowed(velocypack::Slice edge,
                                    std::string_view sourceVertex) const;

  void linkTraverser(arangodb::traverser::ClusterTraverser*);

  std::unique_ptr<arangodb::graph::EdgeCursor> buildCursor(uint64_t depth);

  double estimateCost(size_t& nrItems) const override;

  std::unique_ptr<aql::PruneExpressionEvaluator> createPruneEvaluator(
      std::vector<aql::Variable const*> vars, std::vector<aql::RegisterId> regs,
      size_t vertexVarIdx, size_t edgeVarIdx, size_t pathVarIdx,
      aql::Expression* expr);

  std::unique_ptr<aql::PruneExpressionEvaluator> createPostFilterEvaluator(
      std::vector<aql::Variable const*> vars, std::vector<aql::RegisterId> regs,
      size_t vertexVarIdx, size_t edgeVarIdx, aql::Expression* expr);

  void activatePrune(std::vector<aql::Variable const*> vars,
                     std::vector<aql::RegisterId> regs, size_t vertexVarIdx,
                     size_t edgeVarIdx, size_t pathVarIdx,
                     aql::Expression* expr);

  void activatePostFilter(std::vector<aql::Variable const*> vars,
                          std::vector<aql::RegisterId> regs,
                          size_t vertexVarIdx, size_t edgeVarIdx,
                          aql::Expression* expr);

  bool usesPrune() const { return _pruneExpression != nullptr; }
  bool usesPostFilter() const { return _postFilterExpression != nullptr; }

  bool isUseBreadthFirst() const { return mode == Order::BFS; }

  bool isUniqueGlobalVerticesAllowed() const {
    return mode == Order::BFS || mode == Order::WEIGHTED;
  }

  double weightEdge(VPackSlice edge) const;

  aql::PruneExpressionEvaluator* getPruneEvaluator() {
    TRI_ASSERT(usesPrune());
    return _pruneExpression.get();
  }

  aql::PruneExpressionEvaluator* getPostFilterEvaluator() {
    TRI_ASSERT(usesPostFilter());
    return _postFilterExpression.get();
  }

  auto estimateDepth() const noexcept -> uint64_t override;

  auto setProducePaths(bool vertices, bool edges, bool weights) noexcept
      -> void {
    _producePathsVertices = vertices;
    _producePathsEdges = edges;
    _producePathsWeights = weights;
  }

  auto producePathsVertices() const noexcept -> bool {
    return _producePathsVertices;
  }
  auto producePathsEdges() const noexcept -> bool { return _producePathsEdges; }
  auto producePathsWeights() const noexcept -> bool {
    return _producePathsWeights && mode == Order::WEIGHTED;
  }

  auto explicitDepthLookupAt() const -> std::unordered_set<std::size_t>;

  auto setDisjoint() -> void;
  auto isDisjoint() const -> bool;

  auto isSatelliteLeader() const -> bool;

  auto getEdgeDestination(arangodb::velocypack::Slice edge,
                          std::string_view origin) const -> std::string_view;

  void initializeIndexConditions(
      aql::Ast* ast,
      std::unordered_map<aql::VariableId, aql::VarInfo> const& varInfo,
      aql::Variable const* indexVariable) override;

  void calculateIndexExpressions(aql::Ast* ast) override;

 private:
  void readProduceInfo(VPackSlice obj);
};
}  // namespace traverser
}  // namespace arangodb
