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

#include "EnumerateCollectionExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

EnumerateCollectionExecutorInfos::EnumerateCollectionExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear, ExecutionEngine* engine,
    Collection const* collection, Variable const* outVariable, bool produceResult,
    std::vector<std::string> const& projections, transaction::Methods* trxPtr,
    std::vector<size_t> const& coveringIndexAttributePositions,
    bool allowCoveringIndexOptimization, bool useRawDocumentPointers, bool random)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    std::make_shared<std::unordered_set<RegisterId>>(), nrInputRegisters,
                    nrOutputRegisters, std::move(registersToClear)),
      _outputRegisterId(outputRegister),
      _engine(engine),
      _collection(collection),
      _outVariable(outVariable),
      _trxPtr(trxPtr),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _projections(projections),
      _allowCoveringIndexOptimization(allowCoveringIndexOptimization),
      _useRawDocumentPointers(useRawDocumentPointers),
      _produceResult(produceResult),
      _random(random) {}

EnumerateCollectionExecutor::EnumerateCollectionExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher){};

EnumerateCollectionExecutor::~EnumerateCollectionExecutor() = default;

std::pair<ExecutionState, EnumerateCollectionStats> EnumerateCollectionExecutor::produceRow(
    OutputAqlItemRow& output) {
  TRI_IF_FAILURE("EnumerateCollectionExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  ExecutionState state;
  EnumerateCollectionStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  if (!waitForSatellites(_infos.getEngine(), _infos.getCollection())) {
    double maxWait = _infos.getEngine()->getQuery()->queryOptions().satelliteSyncWait;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
                                   "collection " + _infos.getCollection()->name() +
                                       " did not come into sync in time (" +
                                       std::to_string(maxWait) + ")");
  }

  this->setProducingFunction(DocumentProducingHelper::buildCallback(
      _documentProducer, _infos.getOutVariable(), _infos.getProduceResult(),
      _infos.getProjections(), _infos.getTrxPtr(), _infos.getCoveringIndexAttributePositions(),
      _infos.getAllowCoveringIndexOptimization(), _infos.getUseRawDocumentPointers()));

  std::unique_ptr<OperationCursor> cursor =
      _infos.getTrxPtr()->indexScan(_infos.getCollection()->name(),
                                    (_infos.getRandom()
                                         ? transaction::Methods::CursorType::ANY
                                         : transaction::Methods::CursorType::ALL));

  while (true) {
    std::tie(state, input) = _fetcher.fetchRow();
    cursor->reset();

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, stats};
    }
    TRI_ASSERT(input.isInitialized());

    if (cursor->hasMore()) {
      // res.reset(requestBlock(atMost, nrOutRegs)); // TODO: not needed anymore?
      // only copy 1st row of registers inherited from previous frame(s)
      // inheritRegisters(cur, res.get(), _pos); // TODO: not needed anymore?

      uint64_t atMost = 1;
      TRI_IF_FAILURE("EnumerateCollectionBlock::moreDocuments") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      bool cursorHasMore;
      if (_infos.getProduceResult()) {
        // properly build up results by fetching the actual documents
        // using nextDocument()
        cursorHasMore = cursor->nextDocument(
            [&](LocalDocumentId const&, VPackSlice slice) {
              _documentProducer(input, output, slice, _infos.getOutputRegisterId());
            },
            atMost);
      } else {
        // performance optimization: we do not need the documents at all,
        // so just call next()
        cursorHasMore = cursor->next(
            [&](LocalDocumentId const&) {
              _documentProducer(input, output, VPackSlice::nullSlice(),
                                _infos.getOutputRegisterId());
            },
            atMost);
      }
      if (!cursorHasMore) {
        TRI_ASSERT(!cursor->hasMore());
      }
    }

    if (!cursor->hasMore()) {
      // we have exhausted this cursor
      // re-initialize fetching of documents
      cursor->reset();
    }

    if (state == ExecutionState::DONE) {
      return {state, stats};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }
}

#ifndef USE_ENTERPRISE
bool EnumerateCollectionExecutor::waitForSatellites(ExecutionEngine* engine,
                                                    Collection const* collection) const {
  return true;
}
#endif
