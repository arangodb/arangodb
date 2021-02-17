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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CLUSTER_NODES_H
#define ARANGOD_AQL_CLUSTER_NODES_H 1

#include "Aql/Ast.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/DistributeConsumerNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ModificationOptions.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "Basics/StringUtils.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <utility>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {
class ExecutionBlock;
class ExecutionPlan;
class IndexNode;
class UpdateNode;
class ReplaceNode;
class RemoveNode;
struct Collection;

/// @brief class RemoteNode
class RemoteNode final : public DistributeConsumerNode {
  friend class ExecutionBlock;

 public:
  /// @brief constructor with an id
  RemoteNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
             std::string server, std::string const& ownName,
             std::string queryId)
      : DistributeConsumerNode(plan, id, ownName),
        _vocbase(vocbase),
        _server(std::move(server)),
        _queryId(std::move(queryId)) {
    // note: server and queryId may be empty and filled later
  }

  RemoteNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return REMOTE; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    return cloneHelper(std::make_unique<RemoteNode>(plan, _id, _vocbase, _server,
                                                    getDistributeId(), _queryId),
                       withDependencies, withProperties);
  }

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief return the database
  TRI_vocbase_t* vocbase() const { return _vocbase; }

  /// @brief return the server name
  std::string server() const { return _server; }

  /// @brief set the server name
  void server(std::string const& server) { _server = server; }

  /// @brief return the query id
  std::string queryId() const { return _queryId; }

  /// @brief set the query id
  void queryId(std::string const& queryId) { _queryId = queryId; }

  /// @brief set the query id
  void queryId(QueryId queryId) {
    _queryId = arangodb::basics::StringUtils::itoa(queryId);
  }

 private:
  /// @brief the underlying database
  TRI_vocbase_t* _vocbase;

  /// @brief our server, can be like "shard:S1000" or like "server:Claus"
  std::string _server;

  /// @brief the ID of the query on the server as a string
  std::string _queryId;
};

/// @brief class ScatterNode
class ScatterNode : public ExecutionNode {
 public:
  enum ScatterType { SERVER = 0, SHARD = 1 };

  /// @brief constructor with an id
  ScatterNode(ExecutionPlan* plan, ExecutionNodeId id, ScatterType type)
      : ExecutionNode(plan, id), _type(type) {}

  ScatterNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override { return SCATTER; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override {
    auto c = std::make_unique<ScatterNode>(plan, _id, getScatterType());
    c->copyClients(clients());
    return cloneHelper(std::move(c), withDependencies, withProperties);
  }

  /// @brief estimateCost
  CostEstimate estimateCost() const override;

  void addClient(DistributeConsumerNode const* client) {
    auto const& distId = client->getDistributeId();
    // We cannot add the same distributeId twice, data is delivered exactly once for each id
    TRI_ASSERT(std::find(_clients.begin(), _clients.end(), distId) == _clients.end());
    _clients.emplace_back(distId);
  }

  /// @brief drop the current clients.
  void clearClients() { _clients.clear(); }

  std::vector<std::string> const& clients() const noexcept { return _clients; }

  /// @brief get the scatter type, this defines the variation on how we
  /// distribute the data onto our clients.
  ScatterType getScatterType() const { return _type; };

  void setScatterType(ScatterType targetType) { _type = targetType; }

 protected:
  void writeClientsToVelocyPack(velocypack::Builder& builder) const;
  bool readClientsFromVelocyPack(velocypack::Slice base);

  void copyClients(std::vector<std::string> const& other) {
    TRI_ASSERT(_clients.empty());
    _clients = other;
  }

 private:
  std::vector<std::string> _clients;

  ScatterType _type;
};

/// @brief class DistributeNode
class DistributeNode final : public ScatterNode, public CollectionAccessingNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructor with an id
 public:
  DistributeNode(ExecutionPlan* plan, ExecutionNodeId id,
                 ScatterNode::ScatterType type, Collection const* collection,
                 Variable const* variable, ExecutionNodeId targetNodeId);

  DistributeNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return DISTRIBUTE; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  Variable const* getVariable() const noexcept { return _variable; }
  
  void setVariable(Variable const* var) noexcept { _variable = var; }
  
  ExecutionNodeId getTargetNodeId() const noexcept { return _targetNodeId; }

 private:
  /// @brief the variable we must inspect to know where to distribute
  Variable const* _variable;
  
  /// @brief the id of the target ExecutionNode this DistributeNode belongs to.
  ExecutionNodeId _targetNodeId;
};

/// @brief class GatherNode
class GatherNode final : public ExecutionNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  enum class SortMode : uint32_t { MinElement, Heap, Default };

  enum class Parallelism : uint8_t {
    Undefined = 0,
    Serial = 2,
    Parallel = 4
  };

  /// @brief inspect dependencies starting from a specified 'node'
  /// and return first corresponding collection within
  /// a diamond if so exist
  static Collection const* findCollection(GatherNode const& node) noexcept;

  /// @returns sort mode for the specified number of shards
  static SortMode evaluateSortMode(size_t numberOfShards,
                                   size_t shardsRequiredForHeapMerge = 5) noexcept;

  static Parallelism evaluateParallelism(Collection const& collection) noexcept;

  /// @brief constructor with an id
  GatherNode(ExecutionPlan* plan, ExecutionNodeId id, SortMode sortMode,
             Parallelism parallelism = Parallelism::Undefined) noexcept;

  GatherNode(ExecutionPlan*, arangodb::velocypack::Slice const& base,
             SortElementVector const& elements);

  /// @brief return the type of the node
  NodeType getType() const override final { return GATHER; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    auto other = std::make_unique<GatherNode>(plan, _id, _sortmode, _parallelism);
    other->setConstrainedSortLimit(constrainedSortLimit());
    return cloneHelper(std::move(other), withDependencies, withProperties);
  }

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief get Variables used here including ASC/DESC
  SortElementVector const& elements() const { return _elements; }
  SortElementVector& elements() { return _elements; }

  void elements(SortElementVector const& src) { _elements = src; }

  SortMode sortMode() const noexcept { return _sortmode; }
  void sortMode(SortMode sortMode) noexcept { _sortmode = sortMode; }

  void setConstrainedSortLimit(size_t limit) noexcept;

  size_t constrainedSortLimit() const noexcept;

  bool isSortingGather() const noexcept;

  void setParallelism(Parallelism value);

  Parallelism parallelism() const noexcept {
    return _parallelism;
  }

 private:
  /// @brief the underlying database
  TRI_vocbase_t* _vocbase;

  /// @brief sort elements, variable, ascending flags and possible attribute
  /// paths.
  SortElementVector _elements;

  /// @brief sorting mode
  SortMode _sortmode;

  /// @brief parallelism
  Parallelism _parallelism;

  /// @brief In case this was created from a constrained heap sorting node, this
  /// is its limit (which is greater than zero). Otherwise, it's zero.
  size_t _limit;
};

/// @brief class RemoteNode
class SingleRemoteOperationNode final : public ExecutionNode, public CollectionAccessingNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;
  friend class SingleRemoteOperationBlock;
  /// @brief constructor with an id
 public:
  SingleRemoteOperationNode(ExecutionPlan* plan, ExecutionNodeId id, NodeType mode,
                            bool replaceIndexNode, std::string const& key,
                            aql::Collection const* collection,
                            ModificationOptions const& options,
                            Variable const* update, Variable const* out,
                            Variable const* OLD, Variable const* NEW);

  // We probably do not need this, because the rule will only be used on the
  // coordinator
  SingleRemoteOperationNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
      : ExecutionNode(plan, base), CollectionAccessingNode(plan, base) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "single remote operation node deserialization not implemented.");
  }

  /// @brief return the type of the node
  NodeType getType() const override final { return REMOTESINGLE; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    auto n =
        std::make_unique<SingleRemoteOperationNode>(plan, _id, _mode, _replaceIndexNode,
                                                    _key, collection(), _options,
                                                    _inVariable, _outVariable,
                                                    _outVariableOld, _outVariableNew);
    CollectionAccessingNode::cloneInto(*n);
    return cloneHelper(std::move(n), withDependencies, withProperties);
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(VarSet& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  std::string const& key() const { return _key; }

 private:
  // whether we replaced an index node
  bool _replaceIndexNode;

  /// the key of the document we're intending to work with
  std::string _key;

  NodeType _mode;
  Variable const* _inVariable;
  Variable const* _outVariable;

  /// @brief output variable ($OLD)
  Variable const* _outVariableOld;

  /// @brief output variable ($NEW)
  Variable const* _outVariableNew;

  /// @brief modification operation options
  ModificationOptions _options;
};

}  // namespace aql
}  // namespace arangodb

#endif
