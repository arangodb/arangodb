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
#include "search/boolean_filter.hpp"
#include "search/score.hpp"

// TODO Eliminate access to the plan if possible!
// I think it is used for two things only:
//  - to get the Ast, which can simply be passed on its own, and
//  - to call plan->getVarSetBy() in aql::Expression, which could be removed by
//    passing (a reference to) plan->_varSetBy instead.
// But changing this, even only for the IResearch part, would involve more
// refactoring than I currently find appropriate for this.
#include "Aql/ExecutionPlan.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::iresearch;

typedef std::vector<arangodb::LocalDocumentId> pks_t;

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ExecutorInfos&& infos, std::shared_ptr<const IResearchView::Snapshot> reader,
    RegisterId firstOutputRegister, RegisterId numScoreRegisters, Query& query,
    std::vector<Scorer> const& scorers, ExecutionPlan const& plan, Variable const& outVariable,
    aql::AstNode const& filterCondition, std::pair<bool, bool> volatility,
    IResearchViewExecutorInfos::VarInfoMap const& varInfoMap, int depth)
    : ExecutorInfos(std::move(infos)),
      _outputRegister(firstOutputRegister),
      _numScoreRegisters(numScoreRegisters),
      _reader(std::move(reader)),
      _query(query),
      _scorers(scorers),
      _plan(plan),
      _outVariable(outVariable),
      _filterCondition(filterCondition),
      _volatileSort(volatility.second),
      // `_volatileSort` implies `_volatileFilter`
      _volatileFilter(_volatileSort || volatility.first),
      _varInfoMap(varInfoMap),
      _depth(depth) {
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

std::shared_ptr<const IResearchView::Snapshot> IResearchViewExecutorInfos::getReader() const {
  return _reader;
}

bool IResearchViewExecutorInfos::isScoreReg(RegisterId reg) const {
  return getOutputRegister() < reg && reg <= getOutputRegister() + getNumScoreRegisters();
}

std::vector<arangodb::iresearch::Scorer> const& IResearchViewExecutorInfos::scorers() const
    noexcept {
  return _scorers;
}

ExecutionPlan const& IResearchViewExecutorInfos::plan() const noexcept {
  return _plan;
}

Variable const& IResearchViewExecutorInfos::outVariable() const noexcept {
  return _outVariable;
}

aql::AstNode const& IResearchViewExecutorInfos::filterCondition() const noexcept {
  return _filterCondition;
}

IResearchViewExecutorInfos::VarInfoMap const& IResearchViewExecutorInfos::varInfoMap() const
    noexcept {
  return _varInfoMap;
}

int IResearchViewExecutorInfos::getDepth() const noexcept { return _depth; }

bool IResearchViewExecutorInfos::volatileSort() const noexcept {
  return _volatileSort;
}

bool IResearchViewExecutorInfos::volatileFilter() const noexcept {
  return _volatileFilter;
}

template <bool ordered>
IResearchViewExecutor<ordered>::IResearchViewExecutor(IResearchViewExecutor::Fetcher& fetcher,
                                                      IResearchViewExecutor::Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _inputRow(CreateInvalidInputRowHint{}),
      _upstreamState(ExecutionState::HASMORE),
      _pkBuffer(ExecutionBlock::DefaultBatchSize()),
      _filterCtx(1),  // arangodb::iresearch::ExpressionExecutionContext
      _ctx(&infos.getQuery(), infos.numberOfOutputRegisters(),
           infos.outVariable(), infos.varInfoMap(), infos.getDepth()),
      _reader(infos.getReader()),
      _filter(irs::filter::prepared::empty()),
      _execCtx(*infos.getQuery().trx(), _ctx),
      _inflight(0),
      _hasMore(true),  // has more data initially
      _isInitialized(false) {
  TRI_ASSERT(infos.getQuery().trx() != nullptr);

  TRI_ASSERT(ordered == (infos.getNumScoreRegisters() != 0));

  // add expression execution context
  _filterCtx.emplace(_execCtx);
}

template <bool ordered>
std::pair<ExecutionState, typename IResearchViewExecutor<ordered>::Stats>
IResearchViewExecutor<ordered>::produceRow(OutputAqlItemRow& output) {
  IResearchViewStats stats{};
  bool documentWritten = false;

  while (!documentWritten) {
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
      }

      // reset must be called exactly after we've got a new and valid input row.
      reset();
    }

    ReadContext ctx(infos().getOutputRegister(), _inputRow, output);
    documentWritten = next(ctx);

    if (documentWritten) {
      stats.incrScanned();
    } else {
      _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
      // no document written, repeat.
    }
  }

  return {ExecutionState::HASMORE, stats};
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
    // TODO Register AQL warning instead?
    LOG_TOPIC("ebced", WARN, arangodb::iresearch::TOPIC)
        << "failed to find collection while reading document from arangosearch "
           "view, cid '"
        << cid << "'";

    return nullptr;  // not a valid collection reference
  }

  return collection->collection();
}

template <bool ordered>
void IResearchViewExecutor<ordered>::fillPkBuffer(irs::doc_iterator& it,
                                                  irs::columnstore_reader::values_reader_f const& values,
                                                  size_t atMost) {
  while (!_pkBuffer.full() && it.next() && _pkBuffer.size() < atMost) {
    irs::bytes_ref key;
    irs::doc_id_t const docId = it.value();
    if (values(docId, key)) {
      LocalDocumentId documentId;
      bool const readSuccess =
          arangodb::iresearch::DocumentPrimaryKey::read(documentId, key);

      TRI_ASSERT(readSuccess == documentId.isSet());

      if (readSuccess) {
        _pkBuffer.push_back(documentId);
      } else {
        std::stringstream str;
        str << "failed to read document primary key while reading document "
               "from arangosearch view, doc_id '"
            << docId << "'";
        _infos.getQuery().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                          str.str().c_str());
      }
    }
  }
}

template <bool ordered>
LocalDocumentId IResearchViewExecutor<ordered>::getNextPk(
    irs::doc_iterator& it,
    irs::columnstore_reader::values_reader_f const& values, size_t atMost) {
  LocalDocumentId id = LocalDocumentId::none();

  if (_pkBuffer.empty()) {
    // Try to fill the buffer if it's empty
    fillPkBuffer(it, values, atMost);
  }

  if (!_pkBuffer.empty()) {
    // Read a key if there's something in the buffer
    id = _pkBuffer.front();
    _pkBuffer.pop_front();
    return id;
  }

  return id;
}

template <bool ordered>
bool IResearchViewExecutor<ordered>::next(ReadContext& ctx) {
  TRI_ASSERT(_filter != nullptr);

  size_t const count = _reader->size();
  for (; _readerOffset < count; ++_readerOffset, _itr.reset()) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    auto const cid = _reader->cid(_readerOffset);  // CID is constant until resetIterator()
    auto collection = lookupCollection(*infos().getQuery().trx(), cid);

    if (!collection) {
      std::stringstream str;
      str << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid << "'";
      _infos.getQuery().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                        str.str().c_str());

      continue;
    }

    TRI_ASSERT(_pkReader);

    // try to read a document PK from iresearch
    LocalDocumentId documentId = getNextPk(*_itr, _pkReader, ctx.outputRow.numRowsLeftToWrite());

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

        // We should either have no _scrVals to evaluate, or all
        TRI_ASSERT(begin == nullptr || !infos().isScoreReg(scoreReg));

        while (infos().isScoreReg(scoreReg)) {
          AqlValue value{};
          ctx.outputRow.cloneValueInto(scoreReg, ctx.inputRow, value);
          ++scoreReg;
        }

        // we should have written exactly all score registers by now
        TRI_ASSERT(!infos().isScoreReg(scoreReg));
        TRI_ASSERT(ctx.outputRow.produced());
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
    // TODO Register AQL warning instead?
    LOG_TOPIC("bd01b", WARN, arangodb::iresearch::TOPIC)
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

      TRI_ASSERT(numScores == infos().scorers().size());
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

  ExecutionPlan const* plan = &infos().plan();

  arangodb::iresearch::QueryContext const queryCtx = {infos().getQuery().trx(),
                                                      plan, plan->getAst(), &_ctx,
                                                      &infos().outVariable()};

  if (infos().volatileFilter() || !_isInitialized) {  // `_volatileSort` implies `_volatileFilter`
    irs::Or root;

    if (!arangodb::iresearch::FilterFactory::filter(&root, queryCtx,
                                                    infos().filterCondition())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "failed to build filter while querying arangosearch view, query '" +
              infos().filterCondition().toVelocyPack(true)->toJson() + "'");
    }

    if (infos().volatileSort() || !_isInitialized) {
      irs::order order;
      irs::sort::ptr scorer;

      for (auto const& scorerNode : infos().scorers()) {
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

    _isInitialized = true;
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
    // TODO Register AQL warning instead?
    LOG_TOPIC("6442f", WARN, arangodb::iresearch::TOPIC)
        << "failed to read document primary key while reading document from "
           "arangosearch view, doc_id '"
        << docId << "'";

    return false;  // not a valid document reference
  }

  return collection.readDocumentWithCallback(infos().getQuery().trx(), docPk, callback);
}

template<bool ordered>
size_t IResearchViewExecutor<ordered>::numberOfRowsInFlight() const {
  // not implemented
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

template class ::arangodb::aql::IResearchViewExecutor<false>;
template class ::arangodb::aql::IResearchViewExecutor<true>;
