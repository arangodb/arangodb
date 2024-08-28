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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/BlocksWithClients.h"
#include "Aql/DistributeClientBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/RegisterInfos.h"
#include "Basics/ResultT.h"

#include <velocypack/Builder.h>

#include <string>
#include <vector>

namespace arangodb {
namespace velocypack {
class Slice;
}

class LogicalCollection;

namespace aql {

class AqlItemBlockManager;
class DistributeClientBlock;
class DistributeNode;

class DistributeExecutorInfos : public ClientsExecutorInfos {
 public:
  DistributeExecutorInfos(std::vector<std::string> clientIds,
                          Collection const* collection, RegisterId regId,
                          std::vector<std::string> attribute,
                          ScatterNode::ScatterType type,
                          std::vector<aql::Collection*> satellites);

  auto registerId() const noexcept -> RegisterId;
  auto scatterType() const noexcept -> ScatterNode::ScatterType;

  std::vector<std::string> const& attribute() const noexcept;

  auto getResponsibleClient(arangodb::velocypack::Slice value) const
      -> ResultT<std::string>;

  auto shouldDistributeToAll(arangodb::velocypack::Slice value) const -> bool;

 private:
  RegisterId const _regId;

  /// @brief type of distribution that this nodes follows.
  ScatterNode::ScatterType _type;

  std::vector<std::string> _attribute;

  /// @brief _colectionName: the name of the sharded collection
  Collection const* _collection;

  /// @brief Cache for the Logical Collection. This way it is not refetched
  /// on every document.
  std::shared_ptr<LogicalCollection> _logCol;

  /// @brief list of collections that should be used
  std::vector<aql::Collection*> _satellites;
};

// The DistributeBlock is actually implemented by specializing
// ExecutionBlockImpl, so this class only exists to identify the specialization.
class DistributeExecutor {
 public:
  using Infos = DistributeExecutorInfos;

  using ClientBlockData = DistributeClientBlock;

  DistributeExecutor(DistributeExecutorInfos const& infos);
  ~DistributeExecutor() = default;

  /**
   * @brief Distribute the rows of the given block into the blockMap
   *        NOTE: Has SideEffects
   *        If the input value does not contain an object, it is modified
   * inplace with a new Object containing a key value! Hence this method is not
   * const ;(
   *
   * @param block The block to be distributed
   * @param skipped The rows that have been skipped from upstream
   * @param blockMap Map client => Data. Will provide the required data to the
   * correct client.
   */
  auto distributeBlock(
      SharedAqlItemBlockPtr const& block, SkipResult skipped,
      std::unordered_map<std::string, ClientBlockData>& blockMap) -> void;

 private:
  auto getClient(velocypack::Slice input) const -> std::string;

  DistributeExecutorInfos const& _infos;

  // temporary buffer for building lookup objects
  velocypack::Builder mutable _temp;
};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template<>
class ExecutionBlockImpl<DistributeExecutor>
    : public BlocksWithClientsImpl<DistributeExecutor> {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, DistributeNode const* node,
                     RegisterInfos registerInfos,
                     DistributeExecutorInfos&& executorInfos);

  ~ExecutionBlockImpl() override = default;
};

}  // namespace aql
}  // namespace arangodb
