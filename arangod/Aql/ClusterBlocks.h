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

#ifndef ARANGOD_AQL_CLUSTER_BLOCKS_H
#define ARANGOD_AQL_CLUSTER_BLOCKS_H 1

#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/SortRegister.h"
#include "Basics/Common.h"
#include "Cluster/ClusterComm.h"
#include "Rest/GeneralRequest.h"

#include <velocypack/Builder.h>

namespace arangodb {

namespace httpclient {
class SimpleHttpResult;
}

namespace transaction {
class Methods;
}

struct ClusterCommResult;

namespace aql {
class AqlItemBlock;
struct Collection;
class ExecutionEngine;

class BlockWithClients : public ExecutionBlock {
 public:
  BlockWithClients(ExecutionEngine* engine, ExecutionNode const* ep,
                   std::vector<std::string> const& shardIds);

  ~BlockWithClients() override = default;

 public:
  /// @brief initializeCursor
  std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) override;

  /// @brief return aql item block
  void returnBlock(AqlItemBlock*& block) noexcept;

  /// @brief hasMoreState
  ExecutionState getHasMoreState();

  /// @brief moved shutdown function from ExecutionBlock
  std::pair<ExecutionState, Result> internalShutdown(int);

  /// @brief shutdown
  std::pair<ExecutionState, Result> shutdown(int) override;

  /// @brief getSome: shouldn't be used, use skipSomeForShard
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief skipSome: shouldn't be used, use skipSomeForShard
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief getSomeForShard
  virtual std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSomeForShard(
      size_t atMost, std::string const& shardId) = 0;

  /// @brief skipSomeForShard
  virtual std::pair<ExecutionState, size_t> skipSomeForShard(size_t atMost,
                                                             std::string const& shardId) = 0;

 protected:
  /// @brief getClientId: get the number <clientId> (used internally)
  /// corresponding to <shardId>
  size_t getClientId(std::string const& shardId) const;

 protected:
  /// @brief _shardIdMap: map from shardIds to clientNrs
  std::unordered_map<std::string, size_t> _shardIdMap;

  /// @brief _nrClients: total number of clients
  size_t _nrClients;

 private:
  bool _wasShutdown;
};

}  // namespace aql
}  // namespace arangodb

#endif
