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

#ifndef ARANGOD_AQL_TESTS_BLOCK_FETCHER_HELPER_H
#define ARANGOD_AQL_TESTS_BLOCK_FETCHER_HELPER_H

#include "Aql/AllRowsFetcher.h"
#include "Aql/ExecutionState.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <Basics/Common.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>

// TODO rename this file to RowFetcherHelper!

namespace arangodb {

namespace aql {
class AqlItemBlock;
class InputAqlItemRow;
class AqlItemMatrix;
}

namespace tests {
namespace aql {

/**
* @brief Mock for SingleRowFetcher
*/
class SingleRowFetcherHelper : public ::arangodb::aql::SingleRowFetcher {
 public:
  SingleRowFetcherHelper(std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> vPackBuffer,
                         bool returnsWaiting);
  virtual ~SingleRowFetcherHelper();

  std::pair<::arangodb::aql::ExecutionState, ::arangodb::aql::InputAqlItemRow> fetchRow() override;

 private:
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> _vPackBuffer;
  arangodb::velocypack::Slice _data;
  bool _returnedDone = false;
  bool _returnsWaiting;
  uint64_t _nrItems;
  uint64_t _nrCalled;
  bool _didWait;
  arangodb::aql::ResourceMonitor _resourceMonitor;
  std::unique_ptr<arangodb::aql::AqlItemBlock> _itemBlock;
  arangodb::aql::InputAqlItemRow _lastReturnedRow;
};

/**
* @brief Mock for AllRowsFetcher
*/
class AllRowsFetcherHelper : public ::arangodb::aql::AllRowsFetcher {
 public:
  AllRowsFetcherHelper(
      std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> vPackBuffer,
      bool returnsWaiting);
  ~AllRowsFetcherHelper();

  std::pair<::arangodb::aql::ExecutionState,
            ::arangodb::aql::AqlItemMatrix const*>
  fetchAllRows() override;

 private:
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> _vPackBuffer;
  arangodb::velocypack::Slice _data;
  bool _returnedDone = false;
  bool _returnsWaiting;
  uint64_t _nrItems;
  uint64_t _nrRegs;
  uint64_t _nrCalled;
  arangodb::aql::ResourceMonitor _resourceMonitor;
  std::unique_ptr<arangodb::aql::AqlItemMatrix> _matrix;
};


} // aql
} // tests
} // arangodb

#endif
