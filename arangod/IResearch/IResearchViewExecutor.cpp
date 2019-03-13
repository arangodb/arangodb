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
#include "search/score.hpp"

// TODO Eliminate access to the node and the plan!
#include "Aql/ExecutionPlan.h"
#include "IResearch/IResearchViewNode.h"

#include <3rdParty/iresearch/core/search/boolean_filter.hpp>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::iresearch;

typedef std::vector<arangodb::LocalDocumentId> pks_t;

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ExecutorInfos&& infos, std::shared_ptr<iresearch::IResearchView::Snapshot const> reader,
    RegisterId firstOutputRegister, RegisterId numScoreRegisters, Query& query,
    iresearch::IResearchViewNode const& node)
    : ExecutorInfos(std::move(infos)),
      _outputRegister(firstOutputRegister),
      _numScoreRegisters(numScoreRegisters),
      _reader(std::move(reader)),
      _query(query),
      _node(node) {
  TRI_ASSERT(_reader != nullptr);
  TRI_ASSERT(getOutputRegisters()->find(firstOutputRegister) !=
             getOutputRegisters()->end());
}

RegisterId IResearchViewExecutorInfos::getOutputRegister() const {
  return _outputRegister;
};

RegisterId IResearchViewExecutorInfos::getNumScoreRegisters() const {
  return _numScoreRegisters;
};

Query& IResearchViewExecutorInfos::getQuery() const noexcept { return _query; }

IResearchViewNode const& IResearchViewExecutorInfos::getNode() const {
  return _node;
}

std::shared_ptr<const IResearchView::Snapshot> IResearchViewExecutorInfos::getReader() const {
  return _reader;
}

bool IResearchViewExecutorInfos::isScoreReg(RegisterId reg) const {
  return getOutputRegister() < reg && reg <= getOutputRegister() + getNumScoreRegisters();
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
      _volatileSort(ordered), // TODO Seems to me this should just be replaced by ordered everywhere
      _volatileFilter(true) // TODO Set this correctly. It's always true when ordered is true, but not necessarily when ordered is false.
      {
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

  ReadContext ctx(infos().getOutputRegister(), _inputRow, output);
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
    irs::doc_id_t const docId = it.value();
    if (values(docId, key)) {
      bool const readSuccess =
          arangodb::iresearch::DocumentPrimaryKey::read(documentId, key);

      TRI_ASSERT(readSuccess == documentId.isSet());

      if (!readSuccess) {
        // TODO This warning was previously emitted only in the ordered case.
        // Must it stay that way?
        // TODO Maybe this should be an exception?
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
            << "failed to read document primary key while reading document "
               "from arangosearch view, doc_id '"
            << docId << "'";
      }
    }
  }

  return documentId;
}

template <bool ordered>
bool IResearchViewExecutor<ordered>::next(ReadContext& ctx) {
  // TODO This is just taken from the previous implementation, but maybe _filter
  // shouldn't be a nullptr in either case?
  TRI_ASSERT(ordered || _filter != nullptr);

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
      // in the ordered case we have to write scores as well as a document
      if /* constexpr */ (ordered) {
        // evaluate scores
        _scr->evaluate();

        // in arangodb we assume all scorers return float_t
        auto begin = reinterpret_cast<const float_t*>(_scrVal.begin());
        auto end = reinterpret_cast<const float_t*>(_scrVal.end());

        // scorer register are placed consecutively after the document output register
        RegisterId scoreReg = ctx.docOutReg + 1;

        // copy scores, registerId's are sequential
        for (; begin != end; ++begin, ++scoreReg) {
          TRI_ASSERT(infos().isScoreReg(scoreReg));
          AqlValue a{AqlValueHintDouble{*begin}};
          bool mustDestroy = false;
          AqlValueGuard guard{a, mustDestroy};
          ctx.outputRow.moveValueInto(scoreReg, ctx.inputRow, guard);
        }

        // we should have written exactly all score registers by now
        TRI_ASSERT(!infos().isScoreReg(scoreReg));
      }

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

  if /* constexpr */ (ordered) {
    _scr = _itr->attributes().get<irs::score>().get();

    if (_scr) {
      _scrVal = _scr->value();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto const numScores =
          static_cast<size_t>(std::distance(_scrVal.begin(), _scrVal.end())) /
          sizeof(float_t);

      IResearchViewNode const& viewNode = infos().getNode();

      TRI_ASSERT(numScores == viewNode.scorers().size());
#endif
    } else {
      _scr = &irs::score::no_score();
      _scrVal = irs::bytes_ref::NIL;
    }
  }

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
          ctx.outputRow.moveValueInto(ctx.docOutReg, ctx.inputRow, guard);
        };
      },

      [](ReadContext& ctx) {
        // capture only one reference to potentially avoid heap allocation
        return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
          AqlValue a{AqlValueHintDocumentNoCopy(doc.begin())};
          bool mustDestroy = true;
          AqlValueGuard guard{a, mustDestroy};
          ctx.outputRow.moveValueInto(ctx.docOutReg, ctx.inputRow, guard);
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

template <bool ordered>
bool IResearchViewExecutor<ordered>::readDocument(
    LogicalCollection const& collection, irs::doc_id_t const docId,
    irs::columnstore_reader::values_reader_f const& pkValues,
    IndexIterator::DocumentCallback const& callback) {
  TRI_ASSERT(pkValues);

  arangodb::LocalDocumentId docPk;
  irs::bytes_ref tmpRef;

  if (!pkValues(docId, tmpRef) ||
      !arangodb::iresearch::DocumentPrimaryKey::read(docPk, tmpRef)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to read document primary key while reading document from "
           "arangosearch view, doc_id '"
        << docId << "'";

    return false;  // not a valid document reference
  }

  return collection.readDocumentWithCallback(infos().getQuery().trx(), docPk, callback);
}

template class ::arangodb::aql::IResearchViewExecutor<false>;
template class ::arangodb::aql::IResearchViewExecutor<true>;
