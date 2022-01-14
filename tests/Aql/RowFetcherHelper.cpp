////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RowFetcherHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortExecutor.h"

#include "Logger/LogMacros.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::aql;

namespace {}  // namespace

// -----------------------------------------
// - SECTION SINGLEROWFETCHER              -
// -----------------------------------------

template<::arangodb::aql::BlockPassthrough passBlocksThrough>
SingleRowFetcherHelper<passBlocksThrough>::SingleRowFetcherHelper(
    AqlItemBlockManager& manager,
    std::shared_ptr<VPackBuffer<uint8_t>> const& vPackBuffer,
    bool returnsWaiting)
    : SingleRowFetcherHelper(manager, 1, returnsWaiting,
                             vPackBufferToAqlItemBlock(manager, vPackBuffer)) {}

template<::arangodb::aql::BlockPassthrough passBlocksThrough>
SingleRowFetcherHelper<passBlocksThrough>::SingleRowFetcherHelper(
    ::arangodb::aql::AqlItemBlockManager& manager, size_t const blockSize,
    bool const returnsWaiting, ::arangodb::aql::SharedAqlItemBlockPtr input)
    : SingleRowFetcher<passBlocksThrough>(),
      _returnsWaiting(returnsWaiting),
      _nrItems(input == nullptr ? 0 : input->numRows()),
      _blockSize(blockSize),
      _itemBlockManager(manager),
      _itemBlock(std::move(input)),
      _lastReturnedRow{CreateInvalidInputRowHint{}} {
  TRI_ASSERT(_blockSize > 0);
}

template<::arangodb::aql::BlockPassthrough passBlocksThrough>
SingleRowFetcherHelper<passBlocksThrough>::~SingleRowFetcherHelper() = default;

// -----------------------------------------
// - SECTION CONSTFETCHER              -
// -----------------------------------------

ConstFetcherHelper::ConstFetcherHelper(
    AqlItemBlockManager& itemBlockManager,
    std::shared_ptr<VPackBuffer<uint8_t>> vPackBuffer)
    : ConstFetcher(), _vPackBuffer(std::move(vPackBuffer)) {
  if (_vPackBuffer != nullptr) {
    _data = VPackSlice(_vPackBuffer->data());
  } else {
    _data = VPackSlice::nullSlice();
  }
  if (_data.isArray()) {
    auto nrItems = _data.length();
    if (nrItems > 0) {
      VPackSlice oneRow = _data.at(0);
      TRI_ASSERT(oneRow.isArray());
      arangodb::aql::RegisterCount nrRegs =
          static_cast<arangodb::aql::RegisterCount>(oneRow.length());
      auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
      for (RegisterId::value_t i = 0; i < nrRegs; i++) {
        inputRegisters->emplace(i);
      }
      SharedAqlItemBlockPtr block{
          new AqlItemBlock(itemBlockManager, nrItems, nrRegs)};
      VPackToAqlItemBlock(_data, nrRegs, *block);
      SkipResult skipRes{};
      this->injectBlock(block, skipRes);
    }
  }
};

ConstFetcherHelper::~ConstFetcherHelper() = default;

template class ::arangodb::tests::aql::SingleRowFetcherHelper<
    ::arangodb::aql::BlockPassthrough::Disable>;
template class ::arangodb::tests::aql::SingleRowFetcherHelper<
    ::arangodb::aql::BlockPassthrough::Enable>;
