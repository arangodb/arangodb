////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_MUTEX_EXECUTOR_H
#define ARANGOD_AQL_MUTEX_EXECUTOR_H

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
   *        If the input value does not contain an object, it is modified inplace with
   *        a new Object containing a key value!
   *        Hence this method is not const ;(
   *
   * @param block The block to be distributed
   * @param skipped The rows that have been skipped from upstream
   * @param blockMap Map client => Data. Will provide the required data to the correct client.
   */
  auto distributeBlock(SharedAqlItemBlockPtr block, SkipResult skipped,
                       std::unordered_map<std::string, ClientBlockData>& blockMap) -> void;
  
  void acquireLock() {
    _mutex.lock();
  }

  void releaseLock() {
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
template <>
class ExecutionBlockImpl<MutexExecutor>
    : public BlocksWithClientsImpl<MutexExecutor> {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, MutexNode const* node,
                     RegisterInfos registerInfos, MutexExecutorInfos&& executorInfos);

  ~ExecutionBlockImpl() override = default;
};

//
//
//// MutexExecutor is actually implemented by specializing ExecutionBlockImpl,
//// so this class only exists to identify the specialization.
//class MutexExecutor final {};
//
///**
// * @brief See ExecutionBlockImpl.h for documentation.
// */
//template <>
//class ExecutionBlockImpl<MutexExecutor> : public ExecutionBlock {
// public:
//  ExecutionBlockImpl(ExecutionEngine* engine, MutexNode const* node);
//
//  ~ExecutionBlockImpl() override = default;
//
//  std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> execute(AqlCallStack stack) override;
//
//  std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) override;
//
//  std::pair<ExecutionState, Result> shutdown(int errorCode) override;
//
// private:
//  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSomeWithoutTrace(size_t atMost);
//
//  std::pair<ExecutionState, size_t> skipSomeWithoutTrace(size_t atMost);
//
// private:
//  std::mutex _mutex;
//  bool _isShutdown;
//};


}  // namespace aql
}  // namespace arangodb

#endif
