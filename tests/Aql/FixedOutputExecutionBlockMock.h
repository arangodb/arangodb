////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Aql/ExecutionBlock.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <deque>

namespace arangodb {

namespace tests {
namespace aql {

/**
 * @brief FixedOutputExecutionBlockMock
 *
 * This Mock is used to simulate specific predefined output to `execute` calls.
 * The given data will be returned from front to back on each call.
 * This way we can generate specific situations and test how the requester does
 * react to it.
 */
class FixedOutputExecutionBlockMock final
    : public arangodb::aql::ExecutionBlock {
 public:
  FixedOutputExecutionBlockMock(
      arangodb::aql::ExecutionEngine* engine,
      arangodb::aql::ExecutionNode const* node,
      std::deque<arangodb::aql::SharedAqlItemBlockPtr>&& data);

  std::pair<arangodb::aql::ExecutionState, arangodb::Result> initializeCursor(
      arangodb::aql::InputAqlItemRow const& input) override;

  std::tuple<arangodb::aql::ExecutionState, arangodb::aql::SkipResult,
             arangodb::aql::SharedAqlItemBlockPtr>
  execute(arangodb::aql::AqlCallStack const& stack) override;

 private:
  arangodb::aql::RegisterInfos _infos;
  std::deque<arangodb::aql::SharedAqlItemBlockPtr> _blockData;
};

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
