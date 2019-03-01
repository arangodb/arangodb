////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchViewExecutor.h"

#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

// TODO Eliminate access to the node and the plan!
#include "Aql/ExecutionPlan.h"
#include "IResearch/IResearchViewNode.h"

#include <3rdParty/iresearch/core/search/boolean_filter.hpp>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::iresearch;

typedef std::vector<arangodb::LocalDocumentId> pks_t;

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ExecutorInfos&& infos, std::shared_ptr<IResearchView::Snapshot const> reader,
    RegisterId const outputRegister, Query& query, iresearch::IResearchViewNode const& node)
    : ExecutorInfos(std::move(infos)),
      _outputRegister(outputRegister),
      _reader(std::move(reader)),
      _query(query),
      _node(node) {
  TRI_ASSERT(_reader != nullptr);
  TRI_ASSERT(getOutputRegisters()->find(outputRegister) != getOutputRegisters()->end());
}

RegisterId IResearchViewExecutorInfos::getOutputRegister() const {
  return _outputRegister;
};

Query& IResearchViewExecutorInfos::getQuery() const noexcept { return _query; }

IResearchViewNode const& IResearchViewExecutorInfos::getNode() const {
  return _node;
}

std::shared_ptr<const IResearchView::Snapshot> IResearchViewExecutorInfos::getReader() const {
  return _reader;
}

template <bool ordered>
IResearchViewExecutor<ordered>::IResearchViewExecutor(IResearchViewExecutor::Fetcher& fetcher,
                                                      IResearchViewExecutor::Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _inputRow(CreateInvalidInputRowHint{}),
      _upstreamState(ExecutionState::HASMORE),
      _filterCtx(1),  // arangodb::iresearch::ExpressionExecutionContext
      _ctx(&infos.getQuery(), infos.getNode()),
      _reader(infos.getReader()),
      _filter(irs::filter::prepared::empty()),
      _execCtx(*infos.getQuery().trx(), _ctx),
      _inflight(0),
      _hasMore(true),  // has more data initially
      _volatileSort(true),
      _volatileFilter(true) {
  TRI_ASSERT(infos.getQuery().trx() != nullptr);

  // add expression execution context
  _filterCtx.emplace(_execCtx);
}

template <bool ordered>
std::pair<ExecutionState, typename IResearchViewExecutor<ordered>::Stats>
IResearchViewExecutor<ordered>::produceRow(OutputAqlItemRow& output) {
  IResearchViewStats stats{};

  if (!_inputRow.isInitialized()) {
    if (_upstreamState == ExecutionState::DONE) {
      // There will be no more rows, stop fetching.
      return {ExecutionState::DONE, stats};
    }

    std::tie(_upstreamState, _inputRow) = _fetcher.fetchRow();

    if (_upstreamState == ExecutionState::WAITING) {
      return {_upstreamState, stats};
    }

    if (!_inputRow.isInitialized()) {
      return {ExecutionState::DONE, stats};
    } else if (_upstreamState == ExecutionState::DONE) {
    }

    // reset must be called exactly after we've got a new and valid input row.
    reset();
  }

  ReadContext ctx(infos().numberOfInputRegisters(), _inputRow, output);
  bool documentWritten = next(ctx);

  if (documentWritten) {
    stats.incrScanned();

    return {ExecutionState::HASMORE, stats};
  } else {
    _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};

    // While I do find this elegant, C++ does not guarantee any tail
    // recursion optimization. Thus, to avoid overhead and stack overflows:
    // TODO Remove the recursive call in favour of a loop
    return produceRow(output);
  }
}

template <bool ordered>
const IResearchViewExecutorInfos& IResearchViewExecutor<ordered>::infos() const noexcept {
  return _infos;
}

inline std::shared_ptr<arangodb::LogicalCollection> lookupCollection(  // find collection
    arangodb::transaction::Methods& trx,  // transaction
    TRI_voc_cid_t cid                     // collection identifier
) {
  TRI_ASSERT(trx.state());

  // this is necessary for MMFiles
  trx.pinData(cid);

  // `Methods::documentCollection(TRI_voc_cid_t)` may throw exception
  auto* collection = trx.state()->collection(cid, arangodb::AccessMode::Type::READ);

  if (!collection) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to find collection while reading document from arangosearch "
           "view, cid '"
        << cid << "'";

    return nullptr;  // not a valid collection reference
  }

  return collection->collection();
}

LocalDocumentId readPK(irs::doc_iterator& it,
                       irs::columnstore_reader::values_reader_f const& values) {
  LocalDocumentId documentId;

  if (it.next()) {
    irs::bytes_ref key;
    if (values(it.value(), key)) {
      arangodb::iresearch::DocumentPrimaryKey::read(documentId, key);
    }
  }

  return documentId;
}

template <bool ordered>
bool IResearchViewExecutor<ordered>::next(ReadContext& ctx) {
  TRI_ASSERT(_filter);

  size_t const count = _reader->size();
  for (; _readerOffset < count; ++_readerOffset, _itr.reset()) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    auto const cid = _reader->cid(_readerOffset);  // CID is constant until resetIterator()
    auto collection = lookupCollection(*infos().getQuery().trx(), cid);

    if (!collection) {
      // TODO shouldn't this be an exception?
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid << "'";
      continue;
    }

    TRI_ASSERT(_pkReader);

    // try to read a document PK from iresearch
    LocalDocumentId documentId = readPK(*_itr, _pkReader);

    // read document from underlying storage engine, if we got an id
    if (documentId.isSet() &&
        collection->readDocumentWithCallback(infos().getQuery().trx(),
                                             documentId, ctx.callback)) {
      // we read and wrote a document, return true. we don't know if there are more.
      return true;  // do not change iterator if already reached limit
    }
  }

  // no documents found, we're exhausted.
  return false;
}

inline irs::columnstore_reader::values_reader_f pkColumn(irs::sub_reader const& segment) {
  auto const* reader =
      segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());

  return reader ? reader->values() : irs::columnstore_reader::values_reader_f{};
}

template <bool ordered>
bool IResearchViewExecutor<ordered>::resetIterator() {
  TRI_ASSERT(_filter);
  TRI_ASSERT(!_itr);

  auto& segmentReader = (*_reader)[_readerOffset];

  _pkReader = ::pkColumn(segmentReader);

  if (!_pkReader) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "encountered a sub-reader without a primary key column while "
           "executing a query, ignoring";
    return false;
  }

  _itr = segmentReader.mask(_filter->execute(segmentReader, _order, _filterCtx));

  return true;
}

template <bool ordered>
IndexIterator::DocumentCallback IResearchViewExecutor<ordered>::ReadContext::copyDocumentCallback(
    IResearchViewExecutor<ordered>::ReadContext& ctx) {
  auto* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  typedef std::function<IndexIterator::DocumentCallback(IResearchViewExecutor<ordered>::ReadContext&)> CallbackFactory;

  static CallbackFactory const callbackFactories[]{
      [](ReadContext& ctx) {
        // capture only one reference to potentially avoid heap allocation
        return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
          AqlValue a{AqlValueHintCopy(doc.begin())};
          bool mustDestroy = true;
          AqlValueGuard guard{a, mustDestroy};
          ctx.outputRow.moveValueInto(ctx.curRegs, ctx.inputRow, guard);
        };
      },

      [](ReadContext& ctx) {
        // capture only one reference to potentially avoid heap allocation
        return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
          AqlValue a{AqlValueHintDocumentNoCopy(doc.begin())};
          bool mustDestroy = true;
          AqlValueGuard guard{a, mustDestroy};
          ctx.outputRow.moveValueInto(ctx.curRegs, ctx.inputRow, guard);
        };
      }};

  return callbackFactories[size_t(engine->useRawDocumentPointers())](ctx);
}

template <bool ordered>
void IResearchViewExecutor<ordered>::reset() {
  // This is from IResearchViewUnorderedBlock::reset():
  // reset iterator state
  _itr.reset();
  _readerOffset = 0;

  // The rest is from IResearchViewBlockBase::reset():
  _ctx._inputRow = _inputRow;

  auto& viewNode = infos().getNode();
  // auto& viewNode = *ExecutionNode::castTo<IResearchViewNode const*>(getPlanNode());
  auto* plan = const_cast<ExecutionPlan*>(viewNode.plan());

  arangodb::iresearch::QueryContext const queryCtx = {infos().getQuery().trx(),
                                                      plan, plan->getAst(), &_ctx,
                                                      &viewNode.outVariable()};

  if (_volatileFilter) {  // `_volatileSort` implies `_volatileFilter`
    irs::Or root;

    if (!arangodb::iresearch::FilterFactory::filter(&root, queryCtx,
                                                    viewNode.filterCondition())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "failed to build filter while querying arangosearch view, query '" +
              viewNode.filterCondition().toVelocyPack(true)->toJson() + "'");
    }

    if (_volatileSort) {
      irs::order order;
      irs::sort::ptr scorer;

      for (auto const& scorerNode : viewNode.scorers()) {
        TRI_ASSERT(scorerNode.node);

        if (!arangodb::iresearch::OrderFactory::scorer(&scorer, *scorerNode.node, queryCtx)) {
          // failed to append sort
          THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
        }

        // sorting order doesn't matter
        order.add(true, std::move(scorer));
      }

      // compile order
      _order = order.prepare();
    }

    // compile filter
    _filter = root.prepare(*_reader, _order, irs::boost::no_boost(), _filterCtx);

    auto const& volatility = viewNode.volatility();
    _volatileSort = volatility.second;
    _volatileFilter = _volatileSort || volatility.first;
  }
}

template class ::arangodb::aql::IResearchViewExecutor<false>;
