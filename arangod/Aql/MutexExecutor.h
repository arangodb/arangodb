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
  MutexExecutorInfos(std::vector<std::string> clientIds);
};

// The DistributeBlock is actually implemented by specializing
// ExecutionBlockImpl, so this class only exists to identify the specialization.
class MutexExecutor {
 public:
  using Infos = MutexExecutorInfos;

  class ClientBlockData {
   public:
    ClientBlockData(ExecutionEngine& engine, MutexNode const* node,
                    RegisterInfos const& registerInfos);

    auto clear() -> void;
    auto addBlock(SharedAqlItemBlockPtr block, std::vector<size_t> usedIndexes) -> void;

    auto addSkipResult(SkipResult const& skipResult) -> void;
    auto hasDataFor(AqlCall const& call) -> bool;

    auto execute(AqlCallStack callStack, ExecutionState upstreamState)
        -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>;

   private:
    /**
     * @brief This call will join as many blocks as available from the queue
     *        and return them in a SingleBlock. We then use the IdExecutor
     *        to hand out the data contained in these blocks
     *        We do on purpose not give any kind of guarantees on the sizing
     * of this block to be flexible with the implementation, and find a good
     *        trade-off between blocksize and block copy operations.
     *
     * @return SharedAqlItemBlockPtr a joined block from the queue.
     *         SkipResult the skip information matching to this block
     */
    auto popJoinedBlock() -> std::tuple<SharedAqlItemBlockPtr, SkipResult>;

   private:
    AqlItemBlockManager& _blockManager;
    RegisterInfos const& registerInfos;

    std::deque<std::pair<SharedAqlItemBlockPtr, std::vector<size_t>>> _queue;
    SkipResult _skipped{};

    // This is unique_ptr to get away with everything being forward declared...
    std::unique_ptr<ExecutionBlock> _executor;
    bool _executorHasMore = false;
  };

  MutexExecutor(MutexExecutorInfos const& infos);
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
   *        NOTE: Has SideEffects
   *        If the input value does not contain an object, it is modified inplace with
   *        a new Object containing a key value!
   *        Hence this method is not const ;(
   *
   * @param block The input block
   * @param rowIndex
   * @return std::string Identifier used by the client
   */
  std::string getClient(SharedAqlItemBlockPtr block, size_t rowIndex);

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
