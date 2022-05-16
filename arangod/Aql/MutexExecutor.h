////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/BlocksWithClients.h"
#include "Aql/DistributeClientBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"

#include <mutex>
#include <tuple>
#include <utility>

namespace arangodb {
namespace aql {

class MutexNode;

class MutexExecutorInfos : public ClientsExecutorInfos {
 public:
  explicit MutexExecutorInfos(std::vector<std::string> clientIds);
};

// The DistributeBlock is actually implemented by specializing
// ExecutionBlockImpl, so this class only exists to identify the specialization.
class MutexExecutor {
 public:
  using Infos = MutexExecutorInfos;
  using ClientBlockData = DistributeClientBlock;

  explicit MutexExecutor(MutexExecutorInfos const& infos);
  ~MutexExecutor() = default;

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

  void acquireLock() noexcept {
    // don't continue if locking fails
    _mutex.lock();
  }

  void releaseLock() noexcept {
    // don't continue if locking fails
    _mutex.unlock();
  }

 private:
  /**
   * @brief Compute which client needs to get this row
   *
   * @param block The input block
   * @param rowIndex
   * @return std::string Identifier used by the client
   */
  std::string const& getClient(SharedAqlItemBlockPtr block, size_t rowIndex);

 private:
  MutexExecutorInfos const& _infos;
  std::mutex _mutex;
  size_t _numClient;
};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template<>
class ExecutionBlockImpl<MutexExecutor>
    : public BlocksWithClientsImpl<MutexExecutor> {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, MutexNode const* node,
                     RegisterInfos registerInfos,
                     MutexExecutorInfos&& executorInfos);

  ~ExecutionBlockImpl() override = default;
};

}  // namespace aql
}  // namespace arangodb
