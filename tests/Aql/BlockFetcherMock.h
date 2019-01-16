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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TESTS_BLOCK_FETCHER_MOCK_H
#define ARANGOD_AQL_TESTS_BLOCK_FETCHER_MOCK_H

#include "Aql/BlockFetcher.h"
#include "Aql/ExecutionState.h"
#include "Aql/types.h"

#include <stdint.h>

namespace arangodb {
namespace tests {
namespace aql {

class BlockFetcherMock : public ::arangodb::aql::BlockFetcher {
 public:
  explicit BlockFetcherMock(arangodb::aql::ResourceMonitor& monitor,
                            ::arangodb::aql::RegisterId nrRegisters);

 public:
  // mock methods
  std::pair<arangodb::aql::ExecutionState,
            std::shared_ptr<arangodb::aql::InputAqlItemBlockShell>>
  fetchBlock() override;

 private:
  using FetchBlockReturnItem =
      std::pair<arangodb::aql::ExecutionState,
                std::shared_ptr<arangodb::aql::InputAqlItemBlockShell>>;
 public:

  // additional test methods
  BlockFetcherMock& shouldReturn(arangodb::aql::ExecutionState,
                                 std::unique_ptr<arangodb::aql::AqlItemBlock>);
  BlockFetcherMock& shouldReturn(FetchBlockReturnItem);
  BlockFetcherMock& shouldReturn(std::vector<FetchBlockReturnItem>);
  BlockFetcherMock& andThenReturn(FetchBlockReturnItem);
  BlockFetcherMock& andThenReturn(arangodb::aql::ExecutionState,
                                  std::unique_ptr<arangodb::aql::AqlItemBlock>);
  BlockFetcherMock& andThenReturn(std::vector<FetchBlockReturnItem>);

  bool allBlocksFetched() const;
  size_t numFetchBlockCalls() const;

 private:
  std::deque<FetchBlockReturnItem> _itemsToReturn;

  using AqlItemBlockPtr = uintptr_t;

  std::unordered_set<AqlItemBlockPtr> _fetchedBlocks;
  size_t _numFetchBlockCalls;

  ::arangodb::aql::ResourceMonitor& _monitor;
  ::arangodb::aql::AqlItemBlockManager _itemBlockManager;
};

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

#endif  // ARANGOD_AQL_TESTS_BLOCK_FETCHER_MOCK_H
