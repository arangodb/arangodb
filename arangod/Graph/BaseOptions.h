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

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Transaction/Methods.h"

#include <memory>

namespace arangodb {
struct ResourceMonitor;

namespace aql {
struct AstNode;
class ExecutionPlan;
class Expression;
class QueryContext;
struct VarInfo;

}  // namespace aql

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace graph {

class EdgeCursor;
class SingleServerEdgeCursor;
class TraverserCache;

/**
 * @brief Base class for Graph Operation options in AQL
 *        This holds implementations for the following:
 *          - Global Helper Methods and informations
 *            required by graph operations to produce data.
 *            e.g. Index Accesses.
 *          - Specific options/parameters to modify the behaviour
 *            of traversals (e.g. Breadth- or DepthFirstSearch).
 *
 *        There are specialized classes for
 *          - Traversals
 *          - Shortest_Path
 *          - K_Shortest_Path
 *          - K_Paths
 */
struct BaseOptions {
 public:
  struct LookupInfo {
    // This struct does only take responsibility for the expression
    // NOTE: The expression can be nullptr!
    std::vector<transaction::Methods::IndexHandle> idxHandles;
    std::unique_ptr<aql::Expression> expression;
    aql::AstNode* indexCondition;
    // Flag if we have to update _from / _to in the index search condition
    bool conditionNeedUpdate;
    // Position of _from / _to in the index search condition
    size_t conditionMemberToUpdate;

    LookupInfo();
    ~LookupInfo();

    LookupInfo(LookupInfo const&);
    LookupInfo& operator=(LookupInfo const&) = delete;

    LookupInfo(arangodb::aql::QueryContext&, arangodb::velocypack::Slice const&,
               arangodb::velocypack::Slice const&);

    void initializeNonConstExpressions(aql::Ast* ast,
                                       std::unordered_map<aql::VariableId, aql::VarInfo> const& varInfo,
                                       aql::Variable const* indexVariable);

    /// @brief Build a velocypack containing all relevant information
    ///        for DBServer traverser engines.
    void buildEngineInfo(arangodb::velocypack::Builder&) const;

    double estimateCost(size_t& nrItems) const;
  };

 public:
  static std::unique_ptr<BaseOptions> createOptionsFromSlice(
      arangodb::aql::QueryContext& query, arangodb::velocypack::Slice const& definition);

  explicit BaseOptions(arangodb::aql::QueryContext& query);

  /// @brief This copy constructor is only working during planning phase.
  ///        After planning this node should not be copied anywhere.
  ///        When allowAlreadyBuiltCopy is true, the constructor also works
  ///        after the planning phase; however, the options have to be prepared
  ///        again (see GraphNode::prepareOptions() and its overrides)
  BaseOptions(BaseOptions const&, bool allowAlreadyBuiltCopy = false);
  BaseOptions& operator=(BaseOptions const&) = delete;

  BaseOptions(aql::QueryContext&, arangodb::velocypack::Slice, arangodb::velocypack::Slice);

  virtual ~BaseOptions();

  // Creates a complete Object containing all EngineInfo
  // in the given builder.
  virtual void buildEngineInfo(arangodb::velocypack::Builder&) const;

  void setVariable(aql::Variable const*);

  void addLookupInfo(aql::ExecutionPlan* plan, std::string const& collectionName,
                     std::string const& attributeName, aql::AstNode* condition,
                     bool onlyEdgeIndexes = false);

  void clearVariableValues();

  void setVariableValue(aql::Variable const*, aql::AqlValue const);

  void serializeVariables(arangodb::velocypack::Builder&) const;

  void setCollectionToShard(std::unordered_map<std::string, std::string> const&);

  bool produceVertices() const { return _produceVertices; }

  bool produceEdges() const { return _produceEdges; }

  void setProduceVertices(bool value) { _produceVertices = value; }

  void setProduceEdges(bool value) { _produceEdges = value; }

  transaction::Methods* trx() const;

  aql::QueryContext& query() const;

  /// @brief Build a velocypack for cloning in the plan.
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const = 0;

  /// @brief Creates a complete Object containing all index information
  /// in the given builder.
  virtual void toVelocyPackIndexes(arangodb::velocypack::Builder&) const;

  /// @brief Estimate the total cost for this operation
  virtual double estimateCost(size_t& nrItems) const = 0;

  /// @brief whether or not an edge collection shall be excluded
  /// this can be overridden in TraverserOptions
  virtual bool shouldExcludeEdgeCollection(std::string const& name) const {
    return false;
  }

  arangodb::ResourceMonitor& resourceMonitor() const;

  TraverserCache* cache();

  TraverserCache* cache() const;
  void ensureCache();

  void activateCache(bool enableDocumentCache,
                     std::unordered_map<ServerID, aql::EngineId> const* engines);

  std::unordered_map<std::string, std::vector<std::string>> const& collectionToShard() const {
    return _collectionToShard;
  }

  aql::AqlFunctionsInternalCache& aqlFunctionsInternalCache() {
    return _aqlFunctionsInternalCache;
  }

  virtual auto estimateDepth() const noexcept -> uint64_t = 0;

  void setParallelism(size_t p) noexcept { _parallelism = p; }

  size_t parallelism() const { return _parallelism; }

  void isQueryKilledCallback() const;

  void setRefactor(bool r) noexcept { _refactor = r; }

  bool refactor() const { return _refactor; }

  aql::Variable const* tmpVar();  // TODO check public
  arangodb::aql::FixedVarExpressionContext& getExpressionCtx();

  arangodb::aql::FixedVarExpressionContext const& getExpressionCtx() const;

  virtual void initializeIndexConditions(
    aql::Ast* ast, std::unordered_map<aql::VariableId, aql::VarInfo> const& varInfo,
    aql::Variable const* indexVariable);

 protected:
  double costForLookupInfoList(std::vector<LookupInfo> const& list, size_t& createItems) const;

  // Requires an open Object in the given builder an
  // will inject EngineInfo into it.
  // Does not close the builder.
  void injectEngineInfo(arangodb::velocypack::Builder&) const;

  aql::Expression* getEdgeExpression(size_t cursorId, bool& needToInjectVertex) const;

  bool evaluateExpression(aql::Expression*, arangodb::velocypack::Slice varValue);

  void injectLookupInfoInList(std::vector<LookupInfo>&, aql::ExecutionPlan* plan,
                              std::string const& collectionName, std::string const& attributeName,
                              aql::AstNode* condition, bool onlyEdgeIndexes = false);

  void injectTestCache(std::unique_ptr<TraverserCache>&& cache);

 protected:
  mutable arangodb::transaction::Methods _trx;

  // needed for expression evaluation.
  // This entry is required by API, but not actively used here
  arangodb::aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;

  /// This context holds vaues for Variables/References in AqlNodes
  /// it is read from whenever we need to do a calculation in this class.
  /// e.g. edge.weight > a
  /// Here "a" is read from the expression context.
  arangodb::aql::FixedVarExpressionContext _expressionCtx;

  /// @brief Lookup info to find all edges fulfilling the base conditions
  /// This vector holds the information necessary for the Storage layer.
  /// S.t. we can ask the storage for a list of edges. (e.g. holds index
  /// identifiers and index conditions)
  /// Invariant: For every edge collection we read, there will be exactly
  /// one LookupInfo.
  /// These list is consulted only if there is no overwrite for a specific depth
  /// so this resambles "ALL ==" parts of filters.
  std::vector<LookupInfo> _baseLookupInfos;

  /// Reference to the query we are running in. Necessary for internal API calls.
  aql::QueryContext& _query;

  /// Mutable variable that is used to write the current object (vertex or edge) to
  /// in order to test the condition.
  aql::Variable const* _tmpVar;

  /// @brief the traverser cache
  /// This basically caches strings, and items we want to reference multiple times.
  std::unique_ptr<TraverserCache> _cache;

  // @brief - translations for one-shard-databases
  std::unordered_map<std::string, std::vector<std::string>> _collectionToShard;

  /// Section for Options the user has given in the AQL query

  /// @brief a value of 1 (which is the default) means "no parallelism"
  /// If we have more then 1 start vertex, we can start multiple traversals
  /// in parallel. This value defines how many of those we start
  /// Each traversal itself is single-threaded.
  size_t _parallelism;

  /// @brief whether or not the vertex data is memorized for later use in the query.
  bool _produceVertices;

  /// @brief whether or not the edge data is memorized for later use in the query.
  bool _produceEdges{true};

  /// @brief whether or not we are running on a coordinator
  bool const _isCoordinator;

  /// @brief whether or not we are running the refactored version
  /// TODO: This must be removed prior release - (is currently needed for the refactoring)
  bool _refactor;
};

}  // namespace graph
}  // namespace arangodb
