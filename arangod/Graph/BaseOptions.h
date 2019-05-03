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

#ifndef ARANGOD_GRAPH_BASE_OPTIONS_H
#define ARANGOD_GRAPH_BASE_OPTIONS_H 1

#include "Aql/FixedVarExpressionContext.h"
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Transaction/Methods.h"

namespace arangodb {

namespace aql {
struct AstNode;
class ExecutionPlan;
class Expression;
class Query;

}  // namespace aql

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace graph {

class EdgeCursor;
class TraverserCache;

struct BaseOptions {
 protected:
  struct LookupInfo {
    // This struct does only take responsibility for the expression
    // NOTE: The expression can be nullptr!
    std::vector<transaction::Methods::IndexHandle> idxHandles;
    aql::Expression* expression;
    aql::AstNode* indexCondition;
    // Flag if we have to update _from / _to in the index search condition
    bool conditionNeedUpdate;
    // Position of _from / _to in the index search condition
    size_t conditionMemberToUpdate;

    LookupInfo();
    ~LookupInfo();

    LookupInfo(LookupInfo const&);
    LookupInfo& operator=(LookupInfo const&) = delete;

    LookupInfo(arangodb::aql::Query*, arangodb::velocypack::Slice const&,
               arangodb::velocypack::Slice const&);

    /// @brief Build a velocypack containing all relevant information
    ///        for DBServer traverser engines.
    void buildEngineInfo(arangodb::velocypack::Builder&) const;

    double estimateCost(size_t& nrItems) const;
  };

 public:
  static std::unique_ptr<BaseOptions> createOptionsFromSlice(
      arangodb::aql::Query* query, arangodb::velocypack::Slice const& definition);

  explicit BaseOptions(arangodb::aql::Query* query);

  /// @brief This copy constructor is only working during planning phase.
  ///        After planning this node should not be copied anywhere.
  explicit BaseOptions(BaseOptions const&);
  BaseOptions& operator=(BaseOptions const&) = delete;

  BaseOptions(arangodb::aql::Query*, arangodb::velocypack::Slice, arangodb::velocypack::Slice);

  virtual ~BaseOptions();

  // Creates a complete Object containing all EngineInfo
  // in the given builder.
  virtual void buildEngineInfo(arangodb::velocypack::Builder&) const;

  void setVariable(aql::Variable const*);

  void addLookupInfo(aql::ExecutionPlan* plan, std::string const& collectionName,
                     std::string const& attributeName, aql::AstNode* condition);

  void clearVariableValues();

  void setVariableValue(aql::Variable const*, aql::AqlValue const);

  void serializeVariables(arangodb::velocypack::Builder&) const;

  transaction::Methods* trx() const;

  aql::Query* query() const;

  TraverserCache* cache() const;

  /// @brief Build a velocypack for cloning in the plan.
  virtual void toVelocyPack(arangodb::velocypack::Builder&) const = 0;

  // Creates a complete Object containing all index information
  // in the given builder.
  virtual void toVelocyPackIndexes(arangodb::velocypack::Builder&) const;

  /// @brief Estimate the total cost for this operation
  virtual double estimateCost(size_t& nrItems) const = 0;

  TraverserCache* cache();

  void activateCache(bool enableDocumentCache,
                     std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines);

 protected:
  double costForLookupInfoList(std::vector<LookupInfo> const& list, size_t& createItems) const;

  // Requires an open Object in the given builder an
  // will inject index information into it.
  // Does not close the builder.
  void injectVelocyPackIndexes(arangodb::velocypack::Builder&) const;

  // Requires an open Object in the given builder an
  // will inject EngineInfo into it.
  // Does not close the builder.
  void injectEngineInfo(arangodb::velocypack::Builder&) const;

  aql::Expression* getEdgeExpression(size_t cursorId, bool& needToInjectVertex) const;

  bool evaluateExpression(aql::Expression*, arangodb::velocypack::Slice varValue) const;

  void injectLookupInfoInList(std::vector<LookupInfo>&, aql::ExecutionPlan* plan,
                              std::string const& collectionName,
                              std::string const& attributeName, aql::AstNode* condition);

  EdgeCursor* nextCursorLocal(arangodb::velocypack::StringRef vid,
                              std::vector<LookupInfo> const&);

  void injectTestCache(std::unique_ptr<TraverserCache>&& cache);

 protected:
  aql::Query* _query;

  aql::FixedVarExpressionContext* _ctx;

  transaction::Methods* _trx;

  /// @brief Lookup info to find all edges fulfilling the base conditions
  std::vector<LookupInfo> _baseLookupInfos;

  aql::Variable const* _tmpVar;
  bool const _isCoordinator;

  /// @brief the traverser cache
  std::unique_ptr<TraverserCache> _cache;
};

}  // namespace graph
}  // namespace arangodb
#endif
