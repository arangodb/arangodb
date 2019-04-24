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

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "ModificationExecutorTraits.h"
#include "VocBase/LogicalCollection.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace aql {
std::string toString(AllRowsFetcher&) { return "AllRowsFetcher"; }
std::string toString(SingleBlockFetcher<true>&) {
  return "SingleBlockFetcher<true>";
}
std::string toString(SingleBlockFetcher<false>&) {
  return "SingleBlockFetcher<false>";
}
}  // namespace aql
}  // namespace arangodb

template <typename FetcherType>
ModificationExecutorBase<FetcherType>::ModificationExecutorBase(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _prepared(false){};

template <typename Modifier, typename FetcherType>
ModificationExecutor<Modifier, FetcherType>::ModificationExecutor(Fetcher& fetcher, Infos& infos)
    : ModificationExecutorBase<FetcherType>(fetcher, infos), _modifier() {
  this->_infos._trx->pinData(this->_infos._aqlCollection->id());  // important for mmfiles
  // LOG_DEVEL << toString(_modifier) << " "  << toString(this->_fetcher); // <-- enable this first when debugging modification problems
};

template <typename Modifier, typename FetcherType>
ModificationExecutor<Modifier, FetcherType>::~ModificationExecutor() = default;

template <typename Modifier, typename FetcherType>
std::pair<ExecutionState, typename ModificationExecutor<Modifier, FetcherType>::Stats>
ModificationExecutor<Modifier, FetcherType>::produceRows(OutputAqlItemRow& output) {
  ExecutionState state = ExecutionState::HASMORE;
  ModificationExecutor::Stats stats;

  // TODO - fix / improve prefetching if possible
  while (!this->_prepared && (this->_fetcher.upstreamState() !=
                              ExecutionState::DONE /*|| this->_fetcher._prefetched */)) {
    SharedAqlItemBlockPtr block;

    std::tie(state, block) = this->_fetcher.fetchBlockForModificationExecutor(
        _modifier._defaultBlockSize);  // Upsert must use blocksize of one!
                                       // Otherwise it could happen that an insert
                                       // is not seen by subsequent opererations.
    _modifier._block = block;

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(_modifier._block == nullptr);
      return {state, std::move(stats)};
    }

    if (_modifier._block == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, std::move(stats)};
    }

    TRI_IF_FAILURE("ModificationBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(_modifier._block != nullptr);

    // prepares modifier for single row output
    this->_prepared = _modifier.doModifications(this->_infos, stats);

    if (!this->_infos._producesResults) {
      this->_prepared = false;
    }
  }

  if (this->_prepared) {
    TRI_ASSERT(_modifier._block != nullptr);

    // Produces the output
    bool thisBlockHasMore = _modifier.doOutput(this->_infos, output);

    if (thisBlockHasMore) {
      return {ExecutionState::HASMORE, std::move(stats)};
    } else {
      // we need to get a new block
      this->_prepared = false;
    }
  }

  return {this->_fetcher.upstreamState(), std::move(stats)};
}

template class ::arangodb::aql::ModificationExecutor<Insert, SingleBlockFetcher<false /*allowsBlockPassthrough */>>;
template class ::arangodb::aql::ModificationExecutor<Insert, AllRowsFetcher>;
template class ::arangodb::aql::ModificationExecutor<Remove, SingleBlockFetcher<false /*allowsBlockPassthrough */>>;
template class ::arangodb::aql::ModificationExecutor<Remove, AllRowsFetcher>;
template class ::arangodb::aql::ModificationExecutor<Replace, SingleBlockFetcher<false /*allowsBlockPassthrough */>>;
template class ::arangodb::aql::ModificationExecutor<Replace, AllRowsFetcher>;
template class ::arangodb::aql::ModificationExecutor<Update, SingleBlockFetcher<false /*allowsBlockPassthrough */>>;
template class ::arangodb::aql::ModificationExecutor<Update, AllRowsFetcher>;
template class ::arangodb::aql::ModificationExecutor<Upsert, SingleBlockFetcher<false /*allowsBlockPassthrough */>>;
template class ::arangodb::aql::ModificationExecutor<Upsert, AllRowsFetcher>;
