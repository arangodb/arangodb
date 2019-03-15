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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include <boost/optional.hpp>
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "ModificationExecutorTraits.h"
#include "VocBase/LogicalCollection.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

ModificationExecutorBase::ModificationExecutorBase(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _prepared(false){};

template <typename Modifier>
ModificationExecutor<Modifier>::ModificationExecutor(Fetcher& fetcher, Infos& infos)
    : ModificationExecutorBase(fetcher, infos), _modifier() {
  _infos._trx->pinData(_infos._aqlCollection->id());  // important for mmfiles
};

template <typename Modifier>
ModificationExecutor<Modifier>::~ModificationExecutor() = default;

template <typename Modifier>
std::pair<ExecutionState, typename ModificationExecutor<Modifier>::Stats>
ModificationExecutor<Modifier>::produceRow(OutputAqlItemRow& output) {
  ExecutionState state = ExecutionState::HASMORE;
  ModificationExecutor::Stats stats;

  // TODO - fix / improve prefetching if possible
  while (!_prepared &&
         (_fetcher.upstreamState() != ExecutionState::DONE || _fetcher._prefetched)) {
    std::shared_ptr<AqlItemBlockShell> block;
    std::tie(state, block) = _fetcher.fetchBlock(
        _modifier._defaultBlockSize);  // Upsert must use blocksize of one!
                                       // Otherwise it could happen that an insert
                                       // is not seen by subsequent opererations.
    _modifier._block = block;

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!_modifier._block);
      return {state, std::move(stats)};
    }

    if (!_modifier._block) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, std::move(stats)};
    }

    TRI_IF_FAILURE("ModificationBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(_modifier._block);
    TRI_ASSERT(_modifier._block->hasBlock());

    // prepares modifier for single row output
    _prepared = _modifier.doModifications(_infos, stats);

    if (!_infos._producesResults) {
      _prepared = false;
    }
  }

  if (_prepared) {
    TRI_ASSERT(_modifier._block);
    TRI_ASSERT(_modifier._block->hasBlock());

    // LOG_DEVEL << "call doOutput";
    // Produces the output
    bool thisBlockHasMore = _modifier.doOutput(this->_infos, output);

    if (thisBlockHasMore) {
      // LOG_DEVEL << "doOutput OPRES HASMORE";
      return {ExecutionState::HASMORE, std::move(stats)};
    } else {
      // LOG_DEVEL << "doOutput NEED NEW BLOCK";
      // we need to get a new block
      _prepared = false;
    }
  }

  // LOG_DEVEL << "exit produceRow";
  return {_fetcher.upstreamState(), std::move(stats)};
}

template class ::arangodb::aql::ModificationExecutor<Insert>;
template class ::arangodb::aql::ModificationExecutor<Remove>;
template class ::arangodb::aql::ModificationExecutor<Replace>;
template class ::arangodb::aql::ModificationExecutor<Update>;
template class ::arangodb::aql::ModificationExecutor<Upsert>;
