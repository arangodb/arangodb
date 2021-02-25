////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/ExecutionStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/StringUtils.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <analysis/token_attributes.hpp>
#include <search/boolean_filter.hpp>
#include <search/score.hpp>
#include <search/cost.hpp>
#include <utility>

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
using namespace arangodb::basics;
using namespace arangodb::iresearch;

namespace {

constexpr irs::payload NoPayload;

size_t calculateSkipAllCount(CountApproximate approximation,
                             size_t currentPos,
                             irs::doc_iterator* docs) {
  TRI_ASSERT(docs);
  size_t skipped{0};
  switch (approximation) {
    case CountApproximate::Cost:
      {
        auto* cost = irs::get<irs::cost>(*docs);
        if (ADB_LIKELY(cost)) {
          skipped =  cost->estimate();
          skipped -= std::min(skipped, currentPos);
          break;
        }
      }
      [[fallthrough]];
    default:
      // check for unknown approximation or absence of the cost attribute.
      // fallback to exact anyway
      TRI_ASSERT(CountApproximate::Exact == approximation);
      while (docs->next()) {
        skipped++;
      }
      break;
  }
  return skipped;
}

inline std::shared_ptr<arangodb::LogicalCollection> lookupCollection(  // find collection
    arangodb::transaction::Methods& trx,  // transaction
    DataSourceId cid,                     // collection identifier
    aql::QueryContext& query) {
  TRI_ASSERT(trx.state());

  // `Methods::documentCollection(DataSourceId)` may throw exception
  auto* collection = trx.state()->collection(cid, arangodb::AccessMode::Type::READ);

  if (!collection) {
    std::stringstream msg;
    msg << "failed to find collection while reading document from arangosearch "
           "view, cid '"
        << cid << "'";
    query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

    return nullptr;  // not a valid collection reference
  }

  return collection->collection();
}

inline irs::doc_iterator::ptr pkColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.column_reader(DocumentPrimaryKey::PK());

  return reader ? reader->iterator() : nullptr;
}

inline irs::doc_iterator::ptr sortColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.sort();

  return reader ? reader->iterator() : nullptr;
}

inline void reset(ColumnIterator& column, irs::doc_iterator::ptr&& itr) noexcept {
  TRI_ASSERT(itr);
  column.itr = std::move(itr);
  column.value = irs::get<irs::payload>(*column.itr);
  if (ADB_UNLIKELY(!column.value)) {
    column.value = &NoPayload;
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                       IResearchViewExecutorBase
///////////////////////////////////////////////////////////////////////////////

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    std::shared_ptr<const IResearchView::Snapshot> reader, OutRegisters outRegisters, std::vector<RegisterId> scoreRegisters,
    arangodb::aql::QueryContext& query, std::vector<Scorer> const& scorers,
    std::pair<arangodb::iresearch::IResearchViewSort const*, size_t> sort,
    IResearchViewStoredValues const& storedValues, ExecutionPlan const& plan,
    Variable const& outVariable, aql::AstNode const& filterCondition,
    std::pair<bool, bool> volatility, IResearchViewExecutorInfos::VarInfoMap const& varInfoMap,
    int depth, IResearchViewNode::ViewValuesRegisters&& outNonMaterializedViewRegs, iresearch::CountApproximate countApproximate)
    : _scoreRegisters(std::move(scoreRegisters)),
      _reader(std::move(reader)),
      _query(query),
      _scorers(scorers),
      _sort(std::move(sort)),
      _storedValues(storedValues),
      _plan(plan),
      _outVariable(outVariable),
      _filterCondition(filterCondition),
      _volatileSort(volatility.second),
      // `_volatileSort` implies `_volatileFilter`
      _volatileFilter(_volatileSort || volatility.first),
      _varInfoMap(varInfoMap),
      _depth(depth),
      _outNonMaterializedViewRegs(std::move(outNonMaterializedViewRegs)),
      _countApproximate(countApproximate),
      _filterConditionIsEmpty(::filterConditionIsEmpty(&_filterCondition)) {
  TRI_ASSERT(_reader != nullptr);
  std::tie(_documentOutReg, _collectionPointerReg) = std::visit(
      overload{[&](aql::IResearchViewExecutorInfos::MaterializeRegisters regs) {
                 return std::pair{regs.documentOutReg, aql::RegisterPlan::MaxRegisterId};
               },
               [&](aql::IResearchViewExecutorInfos::LateMaterializeRegister regs) {
                 return std::pair{regs.documentOutReg, regs.collectionOutReg};
               },
               [&](aql::IResearchViewExecutorInfos::NoMaterializeRegisters) {
                 return std::pair{aql::RegisterPlan::MaxRegisterId,
                                  aql::RegisterPlan::MaxRegisterId};
               }},
      outRegisters);
}

IResearchViewNode::ViewValuesRegisters const& IResearchViewExecutorInfos::getOutNonMaterializedViewRegs() const
    noexcept {
  return _outNonMaterializedViewRegs;
}

std::shared_ptr<const arangodb::iresearch::IResearchView::Snapshot> IResearchViewExecutorInfos::getReader() const
    noexcept {
  return _reader;
}

aql::QueryContext& IResearchViewExecutorInfos::getQuery() noexcept { return _query; }

const std::vector<arangodb::iresearch::Scorer>& IResearchViewExecutorInfos::scorers() const
    noexcept {
  return _scorers;
}

std::vector<RegisterId> const& IResearchViewExecutorInfos::getScoreRegisters() const noexcept {
  return _scoreRegisters;
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

const IResearchViewExecutorInfos::VarInfoMap& IResearchViewExecutorInfos::varInfoMap() const
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

const std::pair<const arangodb::iresearch::IResearchViewSort*, size_t>& IResearchViewExecutorInfos::sort() const
    noexcept {
  return _sort;
}

IResearchViewStoredValues const& IResearchViewExecutorInfos::storedValues() const noexcept {
  return _storedValues;
}

auto IResearchViewExecutorInfos::getDocumentRegister() const noexcept -> RegisterId {
  return _documentOutReg;
}

auto IResearchViewExecutorInfos::getCollectionRegister() const noexcept -> RegisterId {
  return _collectionPointerReg;
}

IResearchViewStats::IResearchViewStats() noexcept : _scannedIndex(0) {}
void IResearchViewStats::incrScanned() noexcept { _scannedIndex++; }
void IResearchViewStats::incrScanned(size_t value) noexcept {
  _scannedIndex = _scannedIndex + value;
}
size_t IResearchViewStats::getScanned() const noexcept { return _scannedIndex; }
ExecutionStats& aql::operator+=(ExecutionStats& executionStats,
                                const IResearchViewStats& iResearchViewStats) noexcept {
  executionStats.scannedIndex += iResearchViewStats.getScanned();
  return executionStats;
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                       IResearchViewExecutorBase
///////////////////////////////////////////////////////////////////////////////

template <typename Impl, typename Traits>
template <arangodb::iresearch::MaterializeType, typename>
IndexIterator::DocumentCallback IResearchViewExecutorBase<Impl, Traits>::ReadContext::copyDocumentCallback(
    ReadContext& ctx) {
  typedef std::function<IndexIterator::DocumentCallback(ReadContext&)> CallbackFactory;

  static CallbackFactory const callbackFactory{[](ReadContext& ctx) {
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      AqlValue a{AqlValueHintCopy(doc.begin())};
      AqlValueGuard guard{a, true};
      ctx.outputRow.moveValueInto(ctx.getDocumentReg(), ctx.inputRow, guard);
      return true;
    };
  }};

  return callbackFactory(ctx);
}
template <typename Impl, typename Traits>
IResearchViewExecutorBase<Impl, Traits>::ReadContext::ReadContext(
    aql::RegisterId documentOutReg, aql::RegisterId collectionPointerReg,
    InputAqlItemRow& inputRow, OutputAqlItemRow& outputRow)
    : inputRow(inputRow),
      outputRow(outputRow),
      documentOutReg(documentOutReg),
      collectionPointerReg(collectionPointerReg) {
  if constexpr (static_cast<unsigned int>(Traits::MaterializeType &
                                          iresearch::MaterializeType::Materialize) ==
                static_cast<unsigned int>(iresearch::MaterializeType::Materialize)) {
    callback = this->copyDocumentCallback(*this);
  }
}

template <typename Impl, typename Traits>
IResearchViewExecutorBase<Impl, Traits>::IndexReadBufferEntry::IndexReadBufferEntry(size_t keyIdx) noexcept
    : _keyIdx(keyIdx) {}

template <typename Impl, typename Traits>
template <typename ValueType>
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::IndexReadBuffer::ScoreIterator::ScoreIterator(
    std::vector<AqlValue>& scoreBuffer, size_t keyIdx, size_t numScores) noexcept
    : _scoreBuffer(scoreBuffer), _scoreBaseIdx(keyIdx * numScores), _numScores(numScores) {
  TRI_ASSERT(_scoreBaseIdx + _numScores <= _scoreBuffer.size());
}

template <typename Impl, typename Traits>
template <typename ValueType>
std::vector<AqlValue>::iterator IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<
    ValueType>::IndexReadBuffer::ScoreIterator::begin() noexcept {
  return _scoreBuffer.begin() + static_cast<ptrdiff_t>(_scoreBaseIdx);
}

template <typename Impl, typename Traits>
template <typename ValueType>
std::vector<AqlValue>::iterator IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<
    ValueType>::IndexReadBuffer::ScoreIterator::end() noexcept {
  return _scoreBuffer.begin() + static_cast<ptrdiff_t>(_scoreBaseIdx + _numScores);
}

template <typename Impl, typename Traits>
template <typename ValueType>
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::IndexReadBuffer(size_t const numScoreRegisters)
    : _numScoreRegisters(numScoreRegisters), _keyBaseIdx(0) {}

template <typename Impl, typename Traits>
template <typename ValueType>
ValueType const& IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::getValue(
    const IResearchViewExecutorBase::IndexReadBufferEntry bufferEntry) const noexcept {
  assertSizeCoherence();
  TRI_ASSERT(bufferEntry._keyIdx < _keyBuffer.size());
  return _keyBuffer[bufferEntry._keyIdx];
}

template <typename Impl, typename Traits>
template <typename ValueType>
typename IResearchViewExecutorBase<Impl, Traits>::template IndexReadBuffer<ValueType>::ScoreIterator
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::getScores(
    const IResearchViewExecutorBase::IndexReadBufferEntry bufferEntry) noexcept {
  assertSizeCoherence();
  return ScoreIterator{_scoreBuffer, bufferEntry._keyIdx, _numScoreRegisters};
}

template <typename Impl, typename Traits>
template <typename ValueType>
template <typename... Args>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pushValue(Args&&... args) {
  _keyBuffer.emplace_back(std::forward<Args>(args)...);
}

template <typename Impl, typename Traits>
template <typename ValueType>
std::vector<irs::bytes_ref>& IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::getStoredValues() noexcept {
  return _storedValuesBuffer;
}

template <typename Impl, typename Traits>
template <typename ValueType>
std::vector<irs::bytes_ref> const&
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::getStoredValues() const
    noexcept {
  return _storedValuesBuffer;
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pushScore(float_t const scoreValue) {
  _scoreBuffer.emplace_back(AqlValueHintDouble{static_cast<double>(scoreValue)});
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pushScoreNone() {
  _scoreBuffer.emplace_back();
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::reset() noexcept {
  // Should only be called after everything was consumed
  TRI_ASSERT(empty());
  _keyBaseIdx = 0;
  _keyBuffer.clear();
  _scoreBuffer.clear();
  _storedValuesBuffer.clear();
}

template <typename Impl, typename Traits>
template <typename ValueType>
size_t IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::size() const
    noexcept {
  assertSizeCoherence();
  return _keyBuffer.size() - _keyBaseIdx;
}

template <typename Impl, typename Traits>
template <typename ValueType>
bool IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::empty() const
    noexcept {
  return size() == 0;
}

template <typename Impl, typename Traits>
template <typename ValueType>
typename IResearchViewExecutorBase<Impl, Traits>::IndexReadBufferEntry
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pop_front() noexcept {
  TRI_ASSERT(!empty());
  TRI_ASSERT(_keyBaseIdx < _keyBuffer.size());
  assertSizeCoherence();
  IndexReadBufferEntry entry{_keyBaseIdx};
  ++_keyBaseIdx;
  return entry;
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::assertSizeCoherence() const
    noexcept {
  TRI_ASSERT(_scoreBuffer.size() == _keyBuffer.size() * _numScoreRegisters);
}

template <typename Impl, typename Traits>
IResearchViewExecutorBase<Impl, Traits>::IResearchViewExecutorBase(
    IResearchViewExecutorBase::Fetcher&, IResearchViewExecutorBase::Infos& infos)
    : _trx(infos.getQuery().newTrxContext()),
      _infos(infos),
      _inputRow(CreateInvalidInputRowHint{}),  // TODO: Remove me after refactor
      _indexReadBuffer(_infos.getScoreRegisters().size()),
      _ctx(_trx, infos.getQuery(), _aqlFunctionsInternalCache,
           infos.outVariable(), infos.varInfoMap(), infos.getDepth()),
      _filterCtx(_ctx),  // arangodb::iresearch::ExpressionExecutionContext
      _reader(infos.getReader()),
      _filter(irs::filter::prepared::empty()),
      _isInitialized(false) {
}

template <typename Impl, typename Traits>
std::tuple<ExecutorState, typename IResearchViewExecutorBase<Impl, Traits>::Stats, AqlCall>
IResearchViewExecutorBase<Impl, Traits>::produceRows(AqlItemBlockInputRange& inputRange,
                                                     OutputAqlItemRow& output) {
  IResearchViewStats stats{};
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  while (inputRange.hasDataRow() && !output.isFull()) {
    bool documentWritten = false;

    while (!documentWritten) {
      if (!_inputRow.isInitialized()) {
        std::tie(std::ignore, _inputRow) = inputRange.peekDataRow();

        if (!_inputRow.isInitialized()) {
          return {inputRange.upstreamState(), stats, upstreamCall};
        }

        // reset must be called exactly after we've got a new and valid input row.
        static_cast<Impl&>(*this).reset();
      }

      ReadContext ctx(infos().getDocumentRegister(),
                      infos().getCollectionRegister(), _inputRow, output);
      documentWritten = next(ctx);

      if (documentWritten) {
        stats.incrScanned();
        output.advanceRow();
      } else {
        _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
        inputRange.advanceDataRow();
        // no document written, repeat.
      }
    }
  }

  return {inputRange.upstreamState(), stats, upstreamCall};
}

template <typename Impl, typename Traits>
std::tuple<ExecutorState, typename IResearchViewExecutorBase<Impl, Traits>::Stats, size_t, AqlCall>
IResearchViewExecutorBase<Impl, Traits>::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                                       AqlCall& call) {
  TRI_ASSERT(_indexReadBuffer.empty());
  auto& impl = static_cast<Impl&>(*this);

  while (inputRange.hasDataRow() && call.shouldSkip()) {
    if (!_inputRow.isInitialized()) {
      auto rowState = ExecutorState::HASMORE;
      std::tie(rowState, _inputRow) = inputRange.peekDataRow();

      if (!_inputRow.isInitialized()) {
        TRI_ASSERT(rowState == ExecutorState::DONE);
        break;
      }

      // reset must be called exactly after we've got a new and valid input row.
      impl.reset();
    }
    TRI_ASSERT(_inputRow.isInitialized());
    if (call.getOffset() > 0) {
      // OffsetPhase need to skip atMost offset
      call.didSkip(impl.skip(call.getOffset()));
    } else {
      TRI_ASSERT(call.getLimit() == 0 && call.hasHardLimit());
      // skip all - fullCount phase
      call.didSkip(impl.skipAll());
    }
    TRI_ASSERT(_indexReadBuffer.empty());

    if (call.shouldSkip()) {
      // We still need to fetch more
      // trigger refetch of new input row
      inputRange.advanceDataRow();
      _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    }
  }

  IResearchViewStats stats{};
  stats.incrScanned(call.getSkipCount());

  AqlCall upstreamCall{};
  if (!call.needsFullCount()) {
    // Do not overfetch too much.
    upstreamCall.softLimit = call.getOffset() + std::min(call.softLimit, call.hardLimit);
    // else we do unlimited softLimit.
    // we are going to return everything anyways.
  }
  return {inputRange.upstreamState(), stats, call.getSkipCount(), upstreamCall};
}

template <typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::next(ReadContext& ctx) {
  auto& impl = static_cast<Impl&>(*this);

  while (true) {
    if (_indexReadBuffer.empty()) {
      impl.fillBuffer(ctx);
    }

    if (_indexReadBuffer.empty()) {
      return false;
    }

    IndexReadBufferEntry bufferEntry = _indexReadBuffer.pop_front();

    if (ADB_LIKELY(impl.writeRow(ctx, bufferEntry))) {
      break;
    } else {
      // to get correct stats we should continue looking for
      // other documents inside this one call
      LOG_TOPIC("550cd", TRACE, arangodb::iresearch::TOPIC)
          << "failed to write row in node executor";
    }
  }
  return true;
}

template <typename Impl, typename Traits>
typename IResearchViewExecutorBase<Impl, Traits>::Infos const&
IResearchViewExecutorBase<Impl, Traits>::infos() const noexcept {
  return _infos;
}

template <typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::fillScores(ReadContext const& /*ctx*/,
                                                         float_t const* begin,
                                                         float_t const* end) {
  TRI_ASSERT(Traits::Ordered);

  // scorer registers are placed right before document output register
  // is used here currently only for assertions.
  std::vector<RegisterId> const& scoreRegs = infos().getScoreRegisters();
  size_t numScoreReg = 0;

  // copy scores, registerId's are sequential
  for (; begin != end; ++begin, ++numScoreReg) {
    TRI_ASSERT(numScoreReg < scoreRegs.size());
    _indexReadBuffer.pushScore(*begin);
  }

  // We should either have no _scrVals to evaluate, or all
//  TRI_ASSERT(begin == nullptr || numScoreReg >= scoreRegs.size());

  while (numScoreReg < scoreRegs.size()) {
    _indexReadBuffer.pushScoreNone();
    ++numScoreReg;
  }

  // we should have written exactly all score registers by now
  TRI_ASSERT(numScoreReg == scoreRegs.size());
}

template <typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::reset() {
  _ctx._inputRow = _inputRow;

  ExecutionPlan const* plan = &infos().plan();
  iresearch::QueryContext const queryCtx = { &_trx, plan, plan->getAst(),
                                            &_ctx, _reader.get(), &infos().outVariable()};

  if (infos().volatileFilter() || !_isInitialized) {  // `_volatileSort` implies `_volatileFilter`
    irs::Or root;

    auto rv = FilterFactory::filter(&root, queryCtx, infos().filterCondition());

    if (rv.fail()) {
      arangodb::velocypack::Builder builder;
      infos().filterCondition().toVelocyPack(builder, true);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          rv.errorNumber(),
          StringUtils::concatT("failed to build filter while querying "
                               "arangosearch view, query '",
                               builder.toJson(), "': ", rv.errorMessage()));
    }

    if (infos().volatileSort() || !_isInitialized) {
      irs::order order;
      irs::sort::ptr scorer;

      for (auto const& scorerNode : infos().scorers()) {
        TRI_ASSERT(scorerNode.node);

        if (!OrderFactory::scorer(&scorer, *scorerNode.node, queryCtx)) {
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
    _filter = root.prepare(*_reader, _order, irs::no_boost(), &_filterCtx);

    _isInitialized = true;
  }
}

template <typename Impl, typename Traits>
template <arangodb::iresearch::MaterializeType, typename>
bool IResearchViewExecutorBase<Impl, Traits>::writeLocalDocumentId(
    ReadContext& ctx, LocalDocumentId const& documentId, LogicalCollection const& collection) {
  // we will need collection Id also as View could produce documents from multiple collections
  if (ADB_LIKELY(documentId.isSet())) {
    {
      // For sake of performance we store raw pointer to collection
      // It is safe as pipeline work inside one process
      static_assert(sizeof(void*) <= sizeof(uint64_t),
                    "Pointer not fits in uint64_t");
      AqlValue a(AqlValueHintUInt(reinterpret_cast<uint64_t>(&collection)));
      AqlValueGuard guard{a, true};
      ctx.outputRow.moveValueInto(ctx.getCollectionPointerReg(), ctx.inputRow, guard);
    }
    {
      AqlValue a(AqlValueHintUInt(documentId.id()));
      AqlValueGuard guard{a, true};
      ctx.outputRow.moveValueInto(ctx.getDocumentIdReg(), ctx.inputRow, guard);
    }
    return true;
  } else {
    return false;
  }
}

template <typename Impl, typename Traits>
inline bool IResearchViewExecutorBase<Impl, Traits>::writeStoredValue(
    ReadContext& ctx, std::vector<irs::bytes_ref> const& storedValues,
    size_t index, std::map<size_t, RegisterId> const& fieldsRegs) {
  TRI_ASSERT(index < storedValues.size());
  auto const& storedValue = storedValues[index];
  TRI_ASSERT(!storedValue.empty());
  auto const totalSize = storedValue.size();
  auto slice = VPackSlice(storedValue.c_str());
  size_t size = 0;
  size_t i = 0;
  for (auto const& [fieldNum, registerId] : fieldsRegs) {
    while (i < fieldNum) {
      size += slice.byteSize();
      TRI_ASSERT(size <= totalSize);
      if (ADB_UNLIKELY(size > totalSize)) {
        return false;
      }
      slice = VPackSlice(slice.end());
      ++i;
    }
    TRI_ASSERT(!slice.isNone());
    AqlValue v(slice);
    AqlValueGuard guard{v, true};
    ctx.outputRow.moveValueInto(registerId, ctx.inputRow, guard);
  }
  return true;
}

template <typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::writeRow(ReadContext& ctx,
                                                       IndexReadBufferEntry bufferEntry,
                                                       LocalDocumentId const& documentId,
                                                       LogicalCollection const& collection) {
  TRI_ASSERT(documentId.isSet());
  if constexpr (Traits::MaterializeType == MaterializeType::Materialize) {
    // read document from underlying storage engine, if we got an id
    if (ADB_UNLIKELY(!collection.getPhysical()->read(&_trx, documentId, ctx.callback))) {
      return false;
    }
  } else if constexpr ((Traits::MaterializeType & MaterializeType::LateMaterialize) ==
                       MaterializeType::LateMaterialize) {
    // no need to look into collection. Somebody down the stream will do materialization. Just emit LocalDocumentIds
    if (ADB_UNLIKELY(!writeLocalDocumentId(ctx, documentId, collection))) {
      return false;
    }
  }
  if constexpr ((Traits::MaterializeType & MaterializeType::UseStoredValues) ==
                MaterializeType::UseStoredValues) {
    auto const& columnsFieldsRegs = infos().getOutNonMaterializedViewRegs();
    TRI_ASSERT(!columnsFieldsRegs.empty());
    auto const& storedValues = _indexReadBuffer.getStoredValues();
    auto index = bufferEntry.getKeyIdx() * columnsFieldsRegs.size();
    TRI_ASSERT(index < storedValues.size());
    for (auto it = columnsFieldsRegs.cbegin(); it != columnsFieldsRegs.cend(); ++it) {
      if (ADB_UNLIKELY(!writeStoredValue(ctx, storedValues, index++, it->second))) {
        return false;
      }
    }
  } else if constexpr (Traits::MaterializeType == MaterializeType::NotMaterialize &&
                       !Traits::Ordered) {
    ctx.outputRow.copyRow(ctx.inputRow);
  }
  // in the ordered case we have to write scores as well as a document
  if constexpr (Traits::Ordered) {
    // scorer register are placed right before the document output register
    std::vector<RegisterId> const& scoreRegisters = infos().getScoreRegisters();
    auto scoreRegIter = scoreRegisters.begin();
    for (auto& it : _indexReadBuffer.getScores(bufferEntry)) {
      TRI_ASSERT(scoreRegIter != scoreRegisters.end());
      AqlValueGuard guard{it, false};

      ctx.outputRow.moveValueInto(*scoreRegIter, ctx.inputRow, guard);
      ++scoreRegIter;
    }

    // we should have written exactly all score registers by now
    TRI_ASSERT(scoreRegIter == scoreRegisters.end());
  } else {
    UNUSED(bufferEntry);
  }
  return true;
}

template <typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::readStoredValues(irs::document const& doc,
                                                               size_t index) {
  TRI_ASSERT(index < _storedValuesReaders.size());
  auto const& reader = _storedValuesReaders[index];
  TRI_ASSERT(reader.itr);
  TRI_ASSERT(reader.value);
  auto const& payload = reader.value->value;
  auto& storedValues = _indexReadBuffer.getStoredValues();
  storedValues.emplace_back();
  auto& storedValue = storedValues.back();
  bool const found = (doc.value == reader.itr->seek(doc.value));
  if (found && !payload.empty()) {
    storedValue = payload;
  } else {
    storedValue = ref<irs::byte_type>(VPackSlice::nullSlice());
  }
}

template <typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::pushStoredValues(irs::document const& doc,
                                                               size_t storedValuesIndex /*= 0*/) {
  auto const& columnsFieldsRegs = _infos.getOutNonMaterializedViewRegs();
  TRI_ASSERT(!columnsFieldsRegs.empty());
  auto index = storedValuesIndex * columnsFieldsRegs.size();
  for (auto it = columnsFieldsRegs.cbegin(); it != columnsFieldsRegs.cend(); ++it) {
    readStoredValues(doc, index++);
  }
}

template <typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::getStoredValuesReaders(
    irs::sub_reader const& segmentReader, size_t storedValuesIndex /*= 0*/) {
  auto const& columnsFieldsRegs = _infos.getOutNonMaterializedViewRegs();
  if (!columnsFieldsRegs.empty()) {
    auto columnFieldsRegs = columnsFieldsRegs.cbegin();
    auto index = storedValuesIndex * columnsFieldsRegs.size();
    if (IResearchViewNode::SortColumnNumber == columnFieldsRegs->first) {
      auto sortReader = ::sortColumn(segmentReader);
      if (ADB_UNLIKELY(!sortReader)) {
        LOG_TOPIC("bc5bd", WARN, arangodb::iresearch::TOPIC)
            << "encountered a sub-reader without a sort column while "
               "executing a query, ignoring";
        return false;
      }
      ::reset(_storedValuesReaders[index++], std::move(sortReader));
      ++columnFieldsRegs;
    }
    // if stored values exist
    if (columnFieldsRegs != columnsFieldsRegs.cend()) {
      auto const& columns = _infos.storedValues().columns();
      TRI_ASSERT(!columns.empty());
      for (; columnFieldsRegs != columnsFieldsRegs.cend(); ++columnFieldsRegs) {
        TRI_ASSERT(IResearchViewNode::SortColumnNumber < columnFieldsRegs->first);
        auto const storedColumnNumber = static_cast<size_t>(columnFieldsRegs->first);
        TRI_ASSERT(storedColumnNumber < columns.size());
        auto const* storedValuesReader =
            segmentReader.column_reader(columns[storedColumnNumber].name);
        if (ADB_UNLIKELY(!storedValuesReader)) {
          LOG_TOPIC("af7ec", WARN, arangodb::iresearch::TOPIC)
              << "encountered a sub-reader without a stored value column while "
                 "executing a query, ignoring";
          return false;
        }

        ::reset(_storedValuesReaders[index++], storedValuesReader->iterator());
      }
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                           IResearchViewExecutor
///////////////////////////////////////////////////////////////////////////////

template <bool ordered, MaterializeType materializeType>
IResearchViewExecutor<ordered, materializeType>::IResearchViewExecutor(Fetcher& fetcher,
                                                                       Infos& infos)
    : Base(fetcher, infos),
      _pkReader(),
      _itr(),
      _readerOffset(0),
      _currentSegmentPos(0),
      _totalPos(0),
      _scr(&irs::score::no_score()),
      _numScores(0) {
  this->_storedValuesReaders.resize(this->_infos.getOutNonMaterializedViewRegs().size());
}

template <bool ordered, MaterializeType materializeType>
void IResearchViewExecutor<ordered, materializeType>::evaluateScores(ReadContext const& ctx) {
  // This must not be called in the unordered case.
  TRI_ASSERT(ordered);

  // in arangodb we assume all scorers return float_t
  auto begin = reinterpret_cast<float_t const*>(_scr->evaluate());

  this->fillScores(ctx, begin, begin + _numScores);
}

template <bool ordered, MaterializeType materializeType>
bool IResearchViewExecutor<ordered, materializeType>::readPK(LocalDocumentId& documentId) {
  TRI_ASSERT(!documentId.isSet());
  TRI_ASSERT(_itr);
  TRI_ASSERT(_doc);
  TRI_ASSERT(_pkReader.itr);
  TRI_ASSERT(_pkReader.value);

  if (_itr->next()) {
    ++_totalPos;
    ++_currentSegmentPos;
    if (_doc->value == _pkReader.itr->seek(_doc->value)) {
      bool const readSuccess = DocumentPrimaryKey::read(documentId, _pkReader.value->value);

      TRI_ASSERT(readSuccess == documentId.isSet());

      if (ADB_UNLIKELY(!readSuccess)) {
        LOG_TOPIC("6442f", WARN, arangodb::iresearch::TOPIC)
            << "failed to read document primary key while reading document "
               "from arangosearch view, doc_id '"
            << _doc->value << "'";
      }
    }

    return true;
  }

  return false;
}

template <bool ordered, MaterializeType materializeType>
void IResearchViewExecutor<ordered, materializeType>::fillBuffer(IResearchViewExecutor::ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  size_t const atMost = ctx.outputRow.numRowsLeft();

  size_t const count = this->_reader->size();

  for (; _readerOffset < count;) {
    if (!_itr) {
      if (!this->_indexReadBuffer.empty()) {
        // We may not reset the iterator and continue with the next reader if we
        // still have documents in the buffer, as the cid changes with each
        // reader.
        break;
      }

      if (!resetIterator()) {
        continue;
      }

      // CID is constant until the next resetIterator(). Save the corresponding
      // collection so we don't have to look it up every time.

      DataSourceId const cid = this->_reader->cid(_readerOffset);
      aql::QueryContext& query = this->_infos.getQuery();
      auto collection = lookupCollection(this->_trx, cid, query);

      if (!collection) {
        std::stringstream msg;
        msg << "failed to find collection while reading document from "
               "arangosearch view, cid '"
            << cid.id() << "'";
        query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

        // We don't have a collection, skip the current reader.
        ++_readerOffset;
        _currentSegmentPos = 0;
        _itr.reset();
        _doc = nullptr;
        continue;
      }

      _collection = collection.get();
      this->_indexReadBuffer.reset();
    }

    TRI_ASSERT(_pkReader.itr);
    TRI_ASSERT(_pkReader.value);

    LocalDocumentId documentId;

    // try to read a document PK from iresearch
    bool const iteratorExhausted = !readPK(documentId);

    if (!documentId.isSet()) {
      // No document read, we cannot write it.

      if (iteratorExhausted) {
        // The iterator is exhausted, we need to continue with the next reader.
        ++_readerOffset;
        _currentSegmentPos = 0;
        _itr.reset();
        _doc = nullptr;
      }
      continue;
    }

    // The CID must stay the same for all documents in the buffer
    TRI_ASSERT(this->_collection->id() == this->_reader->cid(_readerOffset));

    this->_indexReadBuffer.pushValue(documentId);

    // in the ordered case we have to write scores as well as a document
    if constexpr (ordered) {
      // Writes into _scoreBuffer
      evaluateScores(ctx);
    }

    if constexpr ((materializeType & MaterializeType::UseStoredValues) ==
                  MaterializeType::UseStoredValues) {
      TRI_ASSERT(_doc);
      this->pushStoredValues(*_doc);
    }
    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next reader.
      ++_readerOffset;
      _currentSegmentPos = 0;
      _itr.reset();
      _doc = nullptr;

      // Here we have at least one document in _indexReadBuffer, so we may not
      // add documents from a new reader.
      break;
    }
    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
}

template <bool ordered, MaterializeType materializeType>
bool IResearchViewExecutor<ordered, materializeType>::resetIterator() {
  TRI_ASSERT(this->_filter);
  TRI_ASSERT(!_itr);

  auto& segmentReader = (*this->_reader)[_readerOffset];

  auto it = ::pkColumn(segmentReader);

  if (ADB_UNLIKELY(!it)) {
    LOG_TOPIC("bd01b", WARN, arangodb::iresearch::TOPIC)
        << "encountered a sub-reader without a primary key column while "
           "executing a query, ignoring";
    return false;
  }

  ::reset(_pkReader, std::move(it));

  if constexpr ((materializeType & MaterializeType::UseStoredValues) ==
                MaterializeType::UseStoredValues) {
    if (ADB_UNLIKELY(!this->getStoredValuesReaders(segmentReader))) {
      return false;
    }
  }

  _itr = this->_filter->execute(segmentReader, this->_order, &this->_filterCtx);
  TRI_ASSERT(_itr);
  _doc = irs::get<irs::document>(*_itr);
  TRI_ASSERT(_doc);

  if constexpr (ordered) {
    _scr = irs::get<irs::score>(*_itr);

    if (!_scr) {
      _scr = &irs::score::no_score();
      _numScores = 0;
    } else {
      _numScores = this->infos().scorers().size();
    }
  }

  _itr = segmentReader.mask(std::move(_itr));
  TRI_ASSERT(_itr);
  _currentSegmentPos = 0;
  return true;
}

template <bool ordered, MaterializeType materializeType>
void IResearchViewExecutor<ordered, materializeType>::reset() {
  Base::reset();

  // reset iterator state
  _itr.reset();
  _doc = nullptr;
  _readerOffset = 0;
  _currentSegmentPos = 0;
  _totalPos = 0;
}

template <bool ordered, MaterializeType materializeType>
size_t IResearchViewExecutor<ordered, materializeType>::skip(size_t limit) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);

  size_t const toSkip = limit;

  for (size_t count = this->_reader->size(); _readerOffset < count; ++_readerOffset) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    while (limit && _itr->next()) {
      ++_currentSegmentPos;
      ++_totalPos;
      --limit;
    }

    if (!limit) {
      break;  // do not change iterator if already reached limit
    }
    _itr.reset();
    _doc = nullptr;
  }
  saveCollection();
  return toSkip - limit;
}

template <bool ordered, MaterializeType materializeType>
size_t IResearchViewExecutor<ordered, materializeType>::skipAll() {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);

  size_t skipped = 0;

  if (_readerOffset < this->_reader->size()) {
    if (this->infos().filterConditionIsEmpty()) {
      skipped =  this->_reader->live_docs_count();
      TRI_ASSERT(_totalPos <= skipped);
      skipped -= std::min(skipped, _totalPos);
      _readerOffset = this->_reader->size();
      _currentSegmentPos = 0;
    } else {
      for (size_t count = this->_reader->size(); _readerOffset < count;
           ++_readerOffset, _currentSegmentPos = 0) {
        if (!_itr && !resetIterator()) {
          continue;
        }
        skipped += calculateSkipAllCount(this->infos().countApproximate(), _currentSegmentPos, _itr.get());
        _itr.reset();
        _doc = nullptr;
      }
    }
  }
  return skipped;
}

template <bool ordered, MaterializeType materializeType>
void IResearchViewExecutor<ordered, materializeType>::saveCollection() {
  // We're in the middle of a reader, save the collection in case produceRows()
  // needs it.
  if (_itr) {
    // CID is constant until the next resetIterator(). Save the corresponding
    // collection so we don't have to look it up every time.

    DataSourceId const cid = this->_reader->cid(_readerOffset);
    aql::QueryContext& query = this->_infos.getQuery();
    auto collection = lookupCollection(this->_trx, cid, query);

    if (!collection) {
      std::stringstream msg;
      msg << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid.id() << "'";
      query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

      // We don't have a collection, skip the current reader.
      ++_readerOffset;
      _currentSegmentPos = 0;
      _itr.reset();
      _doc = nullptr;
    }

    this->_indexReadBuffer.reset();
    _collection = collection.get();
  }
}

template <bool ordered, MaterializeType materializeType>
bool IResearchViewExecutor<ordered, materializeType>::writeRow(
    IResearchViewExecutor::ReadContext& ctx,
    IResearchViewExecutor::IndexReadBufferEntry bufferEntry) {
  TRI_ASSERT(_collection);

  return Base::writeRow(ctx, bufferEntry,
                        this->_indexReadBuffer.getValue(bufferEntry), *_collection);
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                      IResearchViewMergeExecutor
///////////////////////////////////////////////////////////////////////////////

template <bool ordered, MaterializeType materializeType>
IResearchViewMergeExecutor<ordered, materializeType>::IResearchViewMergeExecutor(Fetcher& fetcher,
                                                                                 Infos& infos)
    : Base{fetcher, infos},
      _heap_it{MinHeapContext{*infos.sort().first, infos.sort().second, _segments}} {
  TRI_ASSERT(infos.sort().first);
  TRI_ASSERT(!infos.sort().first->empty());
  TRI_ASSERT(infos.sort().first->size() >= infos.sort().second);
  TRI_ASSERT(infos.sort().second);
}

template <bool ordered, MaterializeType materializeType>
IResearchViewMergeExecutor<ordered, materializeType>::Segment::Segment(
    irs::doc_iterator::ptr&& docs, irs::document const& doc,
    irs::score const& score, size_t numScores,
    LogicalCollection const& collection,
    irs::doc_iterator::ptr&& pkReader,
    size_t index,
    irs::doc_iterator* sortReaderRef,
    irs::payload const* sortReaderValue,
    irs::doc_iterator::ptr&& sortReader) noexcept
  : docs(std::move(docs)),
    doc(&doc),
    score(&score),
    numScores(numScores),
    collection(&collection),
    segmentIndex(index),
    sortReaderRef(sortReaderRef),
    sortValue(sortReaderValue),
    sortReader(std::move(sortReader)) {
  TRI_ASSERT(this->docs);
  TRI_ASSERT(this->doc);
  TRI_ASSERT(this->score);
  TRI_ASSERT(this->collection);
  TRI_ASSERT(this->sortReaderRef);
  TRI_ASSERT(this->sortValue);
  ::reset(this->pkReader, std::move(pkReader));
  TRI_ASSERT(this->pkReader.itr);
  TRI_ASSERT(this->pkReader.value);
}

template <bool ordered, MaterializeType materializeType>
IResearchViewMergeExecutor<ordered, materializeType>::MinHeapContext::MinHeapContext(
    const IResearchViewSort& sort, size_t sortBuckets, std::vector<Segment>& segments) noexcept
    : _less(sort, sortBuckets), _segments(&segments) {}

template <bool ordered, MaterializeType materializeType>
bool IResearchViewMergeExecutor<ordered, materializeType>::MinHeapContext::operator()(const size_t i) const {
  assert(i < _segments->size());
  auto& segment = (*_segments)[i];
  while (segment.docs->next()) {
    auto const doc = segment.docs->value();

    if (doc == segment.sortReaderRef->seek(doc)) {
      return true;
    }

    // FIXME read pk as well
  }
  return false;
}

template <bool ordered, MaterializeType materializeType>
bool IResearchViewMergeExecutor<ordered, materializeType>::MinHeapContext::operator()(
    const size_t lhs, const size_t rhs) const {
  assert(lhs < _segments->size());
  assert(rhs < _segments->size());
  return _less((*_segments)[rhs].sortValue->value,
               (*_segments)[lhs].sortValue->value);
}

template <bool ordered, MaterializeType materializeType>
void IResearchViewMergeExecutor<ordered, materializeType>::evaluateScores(
    ReadContext const& ctx, irs::score const& score, size_t numScores) {
  // This must not be called in the unordered case.
  TRI_ASSERT(ordered);

  // in arangodb we assume all scorers return float_t
  auto begin = reinterpret_cast<float_t const*>(score.evaluate());

  this->fillScores(ctx, begin, begin + numScores);
}

template <bool ordered, MaterializeType materializeType>
void IResearchViewMergeExecutor<ordered, materializeType>::reset() {
  Base::reset();

  _segments.clear();
  auto const size = this->_reader->size();
  _segments.reserve(size);

  auto const& columnsFieldsRegs = this->_infos.getOutNonMaterializedViewRegs();
  auto const storedValuesCount = columnsFieldsRegs.size();
  this->_storedValuesReaders.resize(size * storedValuesCount);
  auto isSortReaderUsedInStoredValues =
      storedValuesCount > 0 &&
      IResearchViewNode::SortColumnNumber == columnsFieldsRegs.cbegin()->first;

  for (size_t i = 0; i < size; ++i) {
    auto& segment = (*this->_reader)[i];

    irs::doc_iterator::ptr it =
        segment.mask(this->_filter->execute(segment, this->_order, &this->_filterCtx));
    TRI_ASSERT(it);

    auto const* doc = irs::get<irs::document>(*it);
    TRI_ASSERT(doc);

    auto const* score = &irs::score::no_score();
    size_t numScores = 0;

    if constexpr (ordered) {
      auto* scoreRef = irs::get<irs::score>(*it);

      if (scoreRef) {
        score = scoreRef;
        numScores = this->infos().scorers().size();
      }
    }

    DataSourceId const cid = this->_reader->cid(i);
    aql::QueryContext& query = this->_infos.getQuery();
    auto collection = lookupCollection(this->_trx, cid, query);

    if (!collection) {
      std::stringstream msg;
      msg << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid.id() << "'";
      query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

      // We don't have a collection, skip the current segment.
      continue;
    }

    auto pkReader = ::pkColumn(segment);

    if (ADB_UNLIKELY(!pkReader)) {
      LOG_TOPIC("ee041", WARN, arangodb::iresearch::TOPIC)
          << "encountered a sub-reader without a primary key column while "
             "executing a query, ignoring";
      continue;
    }

    if constexpr ((materializeType & MaterializeType::UseStoredValues) ==
                  MaterializeType::UseStoredValues) {
      if (ADB_UNLIKELY(!this->getStoredValuesReaders(segment, i))) {
        continue;
      }
    } else {
      TRI_ASSERT(!isSortReaderUsedInStoredValues);
    }

    if (isSortReaderUsedInStoredValues) {
      TRI_ASSERT(i * storedValuesCount < this->_storedValuesReaders.size());
      auto& sortReader = this->_storedValuesReaders[i * storedValuesCount];

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             *collection, std::move(pkReader), i,
                             sortReader.itr.get(), sortReader.value, nullptr);
    } else {
      auto itr = ::sortColumn(segment);

      if (ADB_UNLIKELY(!itr)) {
        LOG_TOPIC("af4cd", WARN, arangodb::iresearch::TOPIC)
            << "encountered a sub-reader without a sort column while "
               "executing a query, ignoring";
        continue;
      }

      irs::payload const* sortValue = irs::get<irs::payload>(*itr);
      if (ADB_UNLIKELY(!sortValue)) {
        sortValue = &NoPayload;
      }

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             *collection, std::move(pkReader), i,
                             itr.get(), sortValue, std::move(itr));
    }
  }

  _heap_it.reset(_segments.size());
}

template <bool ordered, MaterializeType materializeType>
LocalDocumentId IResearchViewMergeExecutor<ordered, materializeType>::readPK(
    IResearchViewMergeExecutor<ordered, materializeType>::Segment const& segment) {
  TRI_ASSERT(segment.doc);
  TRI_ASSERT(segment.pkReader.itr);
  TRI_ASSERT(segment.pkReader.value);

  LocalDocumentId documentId;

  if (segment.doc->value == segment.pkReader.itr->seek(segment.doc->value)) {
    bool const readSuccess = DocumentPrimaryKey::read(documentId, segment.pkReader.value->value);

    TRI_ASSERT(readSuccess == documentId.isSet());

    if (ADB_UNLIKELY(!readSuccess)) {
      LOG_TOPIC("6423f", WARN, arangodb::iresearch::TOPIC)
          << "failed to read document primary key while reading document "
             "from arangosearch view, doc_id '"
          << segment.doc->value << "'";
    }
  }

  return documentId;
}

template <bool ordered, MaterializeType materializeType>
void IResearchViewMergeExecutor<ordered, materializeType>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  size_t const atMost = ctx.outputRow.numRowsLeft();

  while (_heap_it.next()) {
    auto& segment = _segments[_heap_it.value()];
    ++segment.segmentPos;
    TRI_ASSERT(segment.docs);
    TRI_ASSERT(segment.doc);
    TRI_ASSERT(segment.score);
    TRI_ASSERT(segment.collection);
    TRI_ASSERT(segment.pkReader.itr);
    TRI_ASSERT(segment.pkReader.value);

    // try to read a document PK from iresearch
    LocalDocumentId const documentId = readPK(segment);

    if (!documentId.isSet()) {
      // No document read, we cannot write it.
      continue;
    }

    this->_indexReadBuffer.pushValue(documentId, segment.collection);

    // in the ordered case we have to write scores as well as a document
    if constexpr (ordered) {
      // Writes into _scoreBuffer
      evaluateScores(ctx, *segment.score, segment.numScores);
    }

    if constexpr ((materializeType & MaterializeType::UseStoredValues) ==
                  MaterializeType::UseStoredValues) {
      TRI_ASSERT(segment.doc);
      this->pushStoredValues(*segment.doc, segment.segmentIndex);
    }

    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
}

template <bool ordered, MaterializeType materializeType>
size_t IResearchViewMergeExecutor<ordered, materializeType>::skip(size_t limit) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter != nullptr);

  size_t const toSkip = limit;

  while (limit && _heap_it.next()) {
    ++this->_segments[_heap_it.value()].segmentPos;
    --limit;
  }

  return toSkip - limit;
}

template <bool ordered, MaterializeType materializeType>
size_t IResearchViewMergeExecutor<ordered, materializeType>::skipAll() {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter != nullptr);

  size_t skipped = 0;
  if (_heap_it.size()) {
    for (auto& segment : _segments) {
      TRI_ASSERT(segment.docs);
      if (this->infos().filterConditionIsEmpty()) {
        TRI_ASSERT(segment.segmentIndex < this->_reader->size());
        auto const live_docs_count =  (*this->_reader)[segment.segmentIndex].live_docs_count();
        TRI_ASSERT(segment.segmentPos <= live_docs_count);
        skipped += live_docs_count - segment.segmentPos;
      } else {
       skipped +=  calculateSkipAllCount(this->infos().countApproximate(),
                                         segment.segmentPos, segment.docs.get());
      }
    }
    // Adjusting by count of docs already consumed by heap but not consumed by executor.
    // This count is heap size minus 1 already consumed by executor after heap.next.
    // But we should adjust by the heap size only if the heap was advanced at least once
    // (heap is actually filled on first next) or we have nothing consumed from doc iterators!
    if (!_segments.empty() && this->infos().countApproximate() == CountApproximate::Exact &&
        !this->infos().filterConditionIsEmpty() &&
        _heap_it.size() && this->_segments[_heap_it.value()].segmentPos) {
       skipped += (_heap_it.size() - 1);
    }
    _heap_it.reset();
  }
  return skipped;
}

template <bool ordered, MaterializeType materializeType>
bool IResearchViewMergeExecutor<ordered, materializeType>::writeRow(
    IResearchViewMergeExecutor::ReadContext& ctx,
    IResearchViewMergeExecutor::IndexReadBufferEntry bufferEntry) {
  auto const& id = this->_indexReadBuffer.getValue(bufferEntry);
  LocalDocumentId const& documentId = id.first;
  TRI_ASSERT(documentId.isSet());
  LogicalCollection const* collection = id.second;
  TRI_ASSERT(collection);

  return Base::writeRow(ctx, bufferEntry, documentId, *collection);
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                 explicit template instantiation
///////////////////////////////////////////////////////////////////////////////

template class ::arangodb::aql::IResearchViewExecutor<false, MaterializeType::NotMaterialize>;
template class ::arangodb::aql::IResearchViewExecutor<false, MaterializeType::LateMaterialize>;
template class ::arangodb::aql::IResearchViewExecutor<false, MaterializeType::Materialize>;
template class ::arangodb::aql::IResearchViewExecutor<false, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>;
template class ::arangodb::aql::IResearchViewExecutor<false, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>;

template class ::arangodb::aql::IResearchViewExecutor<true, MaterializeType::NotMaterialize>;
template class ::arangodb::aql::IResearchViewExecutor<true, MaterializeType::LateMaterialize>;
template class ::arangodb::aql::IResearchViewExecutor<true, MaterializeType::Materialize>;
template class ::arangodb::aql::IResearchViewExecutor<true, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>;
template class ::arangodb::aql::IResearchViewExecutor<true, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>;

template class ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::NotMaterialize>;
template class ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::LateMaterialize>;
template class ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::Materialize>;
template class ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>;
template class ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>;

template class ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::NotMaterialize>;
template class ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::LateMaterialize>;
template class ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::Materialize>;
template class ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>;
template class ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>;

template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<false, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<false, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<true, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<true, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<false, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<true, MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
