////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_DISTRIBUTE_CONSUMER_NODE_H
#define ARANGOD_AQL_DISTRIBUTE_CONSUMER_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionNodeId.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <utility>

namespace arangodb::aql {

class ExecutionBlock;
class ExecutionPlan;
class ScatterNode;

class DistributeConsumerNode : public ExecutionNode {
 public:
  DistributeConsumerNode(ExecutionPlan* plan, ExecutionNodeId id, std::string distributeId)
      : ExecutionNode(plan, id),
        _distributeId(std::move(distributeId)),
        _isResponsibleForInitializeCursor(true) {}

  DistributeConsumerNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override { return DISTRIBUTE_CONSUMER; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                          std::unordered_set<ExecutionNode const*>& seen) const override;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief whether or not this node will forward initializeCursor or shutDown
  /// requests
  void isResponsibleForInitializeCursor(bool value) {
    _isResponsibleForInitializeCursor = value;
  }

  /// @brief whether or not this node will forward initializeCursor or shutDown
  /// requests
  bool isResponsibleForInitializeCursor() const {
    return _isResponsibleForInitializeCursor;
  }

  /// @brief Set the distributeId
  void setDistributeId(std::string const& distributeId) {
    _distributeId = distributeId;
  }

  /// @brief get the distributeId
  std::string const& getDistributeId() const { return _distributeId; }

  /// @brief clone execution Node recursively, this makes the class abstract
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override {
    // This node is not allowed to be cloned.
    // Clone specialization!
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "DistributeConsumerNode cannot be cloned");
  }

  CostEstimate estimateCost() const override {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "DistributeConsumerNode cannot be estimated");
  }

 protected:
  void toVelocyPackHelperInternal(arangodb::velocypack::Builder& nodes, unsigned flags,
                                  std::unordered_set<ExecutionNode const*>& seen) const;

 private:
  /// @brief our own distributeId. If it is set, we will fetch only ata for
  /// this id from upstream.
  /// For now this can be: empty, a serverId or a shardId, but there is no
  /// built-in limit for these value types.
  std::string _distributeId;

  /// @brief whether or not this node will forward initializeCursor and
  /// shutDown requests
  bool _isResponsibleForInitializeCursor;
};

}  // namespace arangodb::aql

#endif
