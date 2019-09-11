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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CLUSTER_NODES_H
#define ARANGOD_AQL_CLUSTER_NODES_H 1

#include "Aql/Ast.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ModificationOptions.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "Basics/StringUtils.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace aql {
class ExecutionBlock;
class ExecutionPlan;
class IndexNode;
class UpdateNode;
class ReplaceNode;
class RemoveNode;
struct Collection;

/// @brief class RemoteNode
class RemoteNode final : public ExecutionNode {
  friend class ExecutionBlock;

  /// @brief constructor with an id
 public:
  RemoteNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
             std::string server, std::string ownName, std::string queryId);

  RemoteNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief whether or not this node will forward initializeCursor or shutDown
  /// requests
  void isResponsibleForInitializeCursor(bool value);

  /// @brief whether or not this node will forward initializeCursor or shutDown
  /// requests
  bool isResponsibleForInitializeCursor() const;

  /// @brief return the type of the node
  NodeType getType() const final;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const final;

  /// @brief estimateCost
  CostEstimate estimateCost() const final;

  /// @brief return the database
  TRI_vocbase_t* vocbase() const;

  /// @brief return the server name
  std::string server() const;

  /// @brief set the server name
  void server(std::string const& server);

  /// @brief return our own name
  std::string ownName() const;

  /// @brief set our own name
  void ownName(std::string const& ownName);

  /// @brief return the query id
  std::string queryId() const;

  /// @brief set the query id
  void queryId(std::string const& queryId);

  /// @brief set the query id
  void queryId(QueryId queryId);

 private:
  /// @brief the underlying database
  TRI_vocbase_t* _vocbase;

  /// @brief our server, can be like "shard:S1000" or like "server:Claus"
  std::string _server;

  /// @brief our own identity, in case of the coordinator this is empty,
  /// in case of the DBservers, this is the shard ID as a string
  std::string _ownName;

  /// @brief the ID of the query on the server as a string
  std::string _queryId;

  /// @brief whether or not this node will forward initializeCursor and shutDown
  /// requests
  bool _isResponsibleForInitializeCursor;
};

/// @brief class ScatterNode
class ScatterNode : public ExecutionNode {
 public:
  /// @brief constructor with an id
  ScatterNode(ExecutionPlan* plan, size_t id);

  ScatterNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override;

  /// @brief estimateCost
  CostEstimate estimateCost() const override;

  std::vector<std::string> const& clients() const noexcept;
  std::vector<std::string>& clients() noexcept;

 protected:
  void writeClientsToVelocyPack(VPackBuilder& builder) const;
  bool readClientsFromVelocyPack(VPackSlice base);

 private:
  std::vector<std::string> _clients;
};

/// @brief class DistributeNode
class DistributeNode final : public ScatterNode, public CollectionAccessingNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructor with an id
 public:
  DistributeNode(ExecutionPlan* plan, size_t id, Collection const* collection,
                 Variable const* variable, Variable const* alternativeVariable,
                 bool createKeys, bool allowKeyConversionToObject);

  DistributeNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const final;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const final;

  /// @brief estimateCost
  CostEstimate estimateCost() const final;

  void variable(Variable const* variable);

  void alternativeVariable(Variable const* variable);

  /// @brief set createKeys
  void setCreateKeys(bool b);

  /// @brief set allowKeyConversionToObject
  void setAllowKeyConversionToObject(bool b);

  /// @brief set _allowSpecifiedKeys
  void setAllowSpecifiedKeys(bool b);

 private:
  /// @brief the variable we must inspect to know where to distribute
  Variable const* _variable;

  /// @brief an optional second variable we must inspect to know where to
  /// distribute
  Variable const* _alternativeVariable;

  /// @brief the node is responsible for creating document keys
  bool _createKeys;

  /// @brief allow conversion of key to object
  bool _allowKeyConversionToObject;

  /// @brief allow specified keys in input even in the non-default sharding case
  bool _allowSpecifiedKeys;
};

/// @brief class GatherNode
class GatherNode final : public ExecutionNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;

 public:
  enum class SortMode : uint32_t { MinElement, Heap, Default };

  /// @brief inspect dependencies starting from a specified 'node'
  /// and return first corresponding collection within
  /// a diamond if so exist
  static Collection const* findCollection(GatherNode const& node) noexcept;

  /// @returns sort mode for the specified number of shards
  static SortMode evaluateSortMode(size_t numberOfShards,
                                   size_t shardsRequiredForHeapMerge = 5) noexcept;

  /// @brief constructor with an id
  GatherNode(ExecutionPlan* plan, size_t id, SortMode sortMode) noexcept;

  GatherNode(ExecutionPlan*, arangodb::velocypack::Slice const& base,
             SortElementVector const& elements);

  /// @brief return the type of the node
  NodeType getType() const final;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief estimateCost
  CostEstimate estimateCost() const final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const final;

  /// @brief get Variables used here including ASC/DESC
  SortElementVector const& elements() const;
  SortElementVector& elements();

  void elements(SortElementVector const& src);

  SortMode sortMode() const noexcept;
  void sortMode(SortMode sortMode) noexcept;

 private:
  /// @brief sort elements, variable, ascending flags and possible attribute
  /// paths.
  SortElementVector _elements;

  /// @brief sorting mode
  SortMode _sortmode;
};

/// @brief class RemoteNode
class SingleRemoteOperationNode final : public ExecutionNode, public CollectionAccessingNode {
  friend class ExecutionBlock;
  friend class RedundantCalculationsReplacer;
  friend class SingleRemoteOperationBlock;
  /// @brief constructor with an id
 public:
  SingleRemoteOperationNode(ExecutionPlan* plan, size_t id, NodeType mode,
                            bool replaceIndexNode, std::string const& key,
                            aql::Collection const* collection,
                            ModificationOptions const& options,
                            Variable const* update, Variable const* out,
                            Variable const* OLD, Variable const* NEW);

  // We probably do not need this, because the rule will only be used on the
  // coordinator
  SingleRemoteOperationNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const final;

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(arangodb::HashSet<Variable const*>& vars) const final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const final;

  /// @brief estimateCost
  CostEstimate estimateCost() const final;

  std::string const& key() const;

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
