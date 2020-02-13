////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_TESTS_WAITING_EXECUTION_BLOCK_MOCK_H
#define ARANGODB_TESTS_WAITING_EXECUTION_BLOCK_MOCK_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/ResourceUsage.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace aql {
class AqlItemBlock;
class ExecutionEngine;
class ExecutionNode;
struct ResourceMonitor;
}  // namespace aql

namespace tests {
namespace aql {

/**
 * @brief A Execution block that simulates the WAITING, HASMORE, DONE API.
 */
class WaitingExecutionBlockMock final : public arangodb::aql::ExecutionBlock {
 public:
  /**
   * @brief Define how often this Block should return "WAITING"
   */
  enum WaitingBehaviour {
    NEVER,  // Never return WAITING
    ONCE,  // Return WAITING on the first execute call, afterwards return all blocks
    ALWAYS  // Return WAITING once for every execute Call.
  };

  /**
   * @brief Create a WAITING ExecutionBlockMock
   *
   * @param engine Required by API.
   * @param node Required by API.
   * @param data Must be a shared_ptr to an VPackArray.
   * @param variant The waiting behaviour of this block (default ALWAYS), see WaitingBehaviour
   */
  WaitingExecutionBlockMock(arangodb::aql::ExecutionEngine* engine,
                            arangodb::aql::ExecutionNode const* node,
                            std::deque<arangodb::aql::SharedAqlItemBlockPtr>&& data,
                            WaitingBehaviour variant = WaitingBehaviour::ALWAYS,
                            bool lieAboutHasmore = false);

  virtual std::pair<arangodb::aql::ExecutionState, Result> shutdown(int errorCode) override;

  /**
   * @brief Initialize the cursor. Return values will be alternating.
   *
   * @param items Will be ignored
   * @param pos Will be ignored
   *
   * @return First <WAITING, TRI_ERROR_NO_ERROR>
   *         Second <DONE, TRI_ERROR_NO_ERROR>
   */
  std::pair<arangodb::aql::ExecutionState, arangodb::Result> initializeCursor(
      arangodb::aql::InputAqlItemRow const& input) override;

  /**
   * @brief The return values are alternating. On non-WAITING case
   *        it will return atMost many elements from _data.
   *
   *
   * @param atMost This many elements will be returned at Most
   *
   * @return First: <WAITING, nullptr>
   *         Second: <HASMORE/DONE, _data-part>
   */
  std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr> getSome(size_t atMost) override;

  /**
   * @brief The return values are alternating. On non-WAITING case
   *        it will return atMost, or whatever is not skipped over on data,
   * whichever number is lower.
   *
   *
   * @param atMost This many elements will be skipped at most
   *
   * @return First: <WAITING, 0>
   *         Second: <HASMORE/DONE, min(atMost,_data.length)>
   */
  std::pair<arangodb::aql::ExecutionState, size_t> skipSome(size_t atMost) override;

  std::tuple<arangodb::aql::ExecutionState, size_t, arangodb::aql::SharedAqlItemBlockPtr> execute(
      arangodb::aql::AqlCallStack stack) override;

 private:
  void dropBlock();

  // Implementation of execute
  std::tuple<arangodb::aql::ExecutionState, size_t, arangodb::aql::SharedAqlItemBlockPtr> executeWithoutTrace(
      arangodb::aql::AqlCallStack stack);

 private:
  std::deque<arangodb::aql::SharedAqlItemBlockPtr> _data;
  arangodb::aql::ResourceMonitor _resourceMonitor;
  size_t _inflight;
  bool _returnedDone = false;
  bool _hasWaited;
  WaitingBehaviour _variant;
  bool _lieAboutHasmore = false;
};
}  // namespace aql

}  // namespace tests
}  // namespace arangodb

#endif
