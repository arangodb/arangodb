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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TEST_EXECUTOR_H
#define ARANGOD_AQL_TEST_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ExecutorInfos;
template <bool>
class SingleRowFetcher;

class TestExecutorHelperInfos : public ExecutorInfos {
 public:
  TestExecutorHelperInfos(RegisterId inputRegister, RegisterId nrInputRegisters,
                          RegisterId nrOutputRegisters,
                          std::unordered_set<RegisterId> registersToClear,
                          std::unordered_set<RegisterId> registersToKeep);

  TestExecutorHelperInfos() = delete;
  TestExecutorHelperInfos(TestExecutorHelperInfos&&) = default;
  TestExecutorHelperInfos(TestExecutorHelperInfos const&) = delete;
  ~TestExecutorHelperInfos() = default;

  RegisterId getInputRegister() const noexcept { return _inputRegister; };

 private:
  // This is exactly the value in the parent member ExecutorInfo::_inRegs,
  // respectively getInputRegisters().
  RegisterId _inputRegister;
};

/**
 * @brief Implementation of Filter Node
 */
class TestExecutorHelper {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = TestExecutorHelperInfos;
  using Stats = FilterStats;

  TestExecutorHelper() = delete;
  TestExecutorHelper(TestExecutorHelper&&) = default;
  TestExecutorHelper(TestExecutorHelper const&) = delete;
  TestExecutorHelper(Fetcher& fetcher, Infos&);
  ~TestExecutorHelper();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t) const {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number fo rows not supported");
  }

 public:
  Infos& _infos;

 private:
  Fetcher& _fetcher;
  bool _returnedDone = false;
};

}  // namespace aql
}  // namespace arangodb

#endif
