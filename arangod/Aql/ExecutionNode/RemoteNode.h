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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode/DistributeConsumerNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <string>
#include <utility>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {
class ExecutionBlock;
class ExecutionEngine;
class ExecutionPlan;
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

  /// @brief return the amount of bytes used
  size_t getMemoryUsedBytes() const override;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan,
                       bool withDependencies) const override final;

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
  void queryId(QueryId queryId);

 protected:
  /// @brief export to VelocyPack
  void doToVelocyPack(arangodb::velocypack::Builder&,
                      unsigned flags) const override final;

 private:
  /// @brief the underlying database
  TRI_vocbase_t* _vocbase;

  /// @brief our server, can be like "shard:S1000" or like "server:Claus"
  std::string _server;

  /// @brief the ID of the query on the server as a string
  std::string _queryId;
};

}  // namespace aql
}  // namespace arangodb
