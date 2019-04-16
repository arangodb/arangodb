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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_TRAVERSER_OPTIONS_H
#define ARANGOD_VOC_BASE_TRAVERSER_OPTIONS_H 1

#include "Aql/FixedVarExpressionContext.h"
#include "Basics/Common.h"
#include "Graph/BaseOptions.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"

#include <velocypack/StringRef.h>

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
class EdgeUniquenessChecker;
class TraverserCache;
}  // namespace graph

namespace traverser {

class ClusterTraverser;

struct TraverserOptions : public graph::BaseOptions {
  friend class arangodb::aql::TraversalNode;

 public:
  enum UniquenessLevel { NONE, PATH, GLOBAL };

 protected:
  std::unordered_map<uint64_t, std::vector<LookupInfo>> _depthLookupInfo;

  std::unordered_map<uint64_t, aql::Expression*> _vertexExpressions;

  aql::Expression* _baseVertexExpression;

  arangodb::traverser::ClusterTraverser* _traverser;

  /// @brief The condition given in PRUNE (might be empty)
  ///        The Node keeps responsibility
  std::unique_ptr<aql::PruneExpressionEvaluator> _pruneExpression;

 public:
  uint64_t minDepth;

  uint64_t maxDepth;

  bool useBreadthFirst;

  UniquenessLevel uniqueVertices;

  UniquenessLevel uniqueEdges;

  explicit TraverserOptions(aql::Query* query);

  TraverserOptions(aql::Query* query, arangodb::velocypack::Slice const& definition);

  TraverserOptions(arangodb::aql::Query*, arangodb::velocypack::Slice,
                   arangodb::velocypack::Slice);

  /// @brief This copy constructor is only working during planning phase.
  ///        After planning this node should not be copied anywhere.
  TraverserOptions(TraverserOptions const&);
  TraverserOptions& operator=(TraverserOptions const&) = delete;

  virtual ~TraverserOptions();

  /// @brief Build a velocypack for cloning in the plan.
  void toVelocyPack(arangodb::velocypack::Builder&) const override;

  /// @brief Build a velocypack for indexes
  void toVelocyPackIndexes(arangodb::velocypack::Builder&) const override;

  /// @brief Build a velocypack containing all relevant information
  ///        for DBServer traverser engines.
  void buildEngineInfo(arangodb::velocypack::Builder&) const override;

  /// @brief Add a lookup info for specific depth
  void addDepthLookupInfo(aql::ExecutionPlan* plan, std::string const& collectionName,
                          std::string const& attributeName,
                          aql::AstNode* condition, uint64_t depth);

  bool vertexHasFilter(uint64_t) const;

  bool hasEdgeFilter(int64_t, size_t) const;

  bool evaluateEdgeExpression(arangodb::velocypack::Slice,
                              arangodb::velocypack::StringRef vertexId,
                              uint64_t, size_t) const;

  bool evaluateVertexExpression(arangodb::velocypack::Slice, uint64_t) const;

  graph::EdgeCursor* nextCursor(arangodb::velocypack::StringRef vid, uint64_t);

  void linkTraverser(arangodb::traverser::ClusterTraverser*);

  double estimateCost(size_t& nrItems) const override;

  void activatePrune(std::vector<aql::Variable const*> const&& vars,
                     std::vector<aql::RegisterId> const&& regs, size_t vertexVarIdx,
                     size_t edgeVarIdx, size_t pathVarIdx, aql::Expression* expr);

  bool usesPrune() const { return _pruneExpression != nullptr; }

  aql::PruneExpressionEvaluator* getPruneEvaluator() {
    TRI_ASSERT(usesPrune());
    return _pruneExpression.get();
  }

 private:
  graph::EdgeCursor* nextCursorCoordinator(arangodb::velocypack::StringRef vid, uint64_t);
};
}  // namespace traverser
}  // namespace arangodb
#endif
