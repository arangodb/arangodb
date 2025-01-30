////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "IndexAggregateScanExecutor.h"
#include "Aql/DocumentProducingExpressionContext.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/Aggregator.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"

#define LOG_AGG_SCAN LOG_DEVEL_IF(false)

namespace arangodb::aql {

namespace {

struct SliceSpanExpressionContext : DocumentProducingExpressionContext {
  SliceSpanExpressionContext(
      transaction::Methods& trx, QueryContext& query,
      aql::AqlFunctionsInternalCache& cache,
      std::vector<std::pair<VariableId, RegisterId>> const& varsToRegister,
      InputAqlItemRow const& inputRow,
      containers::FlatHashMap<VariableId, size_t> varsToIndex)
      : DocumentProducingExpressionContext(trx, query, cache, varsToRegister,
                                           inputRow),
        _varsToIndex(std::move(varsToIndex)) {}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // only used for assertions
  bool isLateMaterialized() const noexcept override { return false; }
#endif

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override {
    mustDestroy = doCopy;
    auto const searchId = variable->id;
    {
      auto it = _varsToIndex.find(searchId);
      if (it != _varsToIndex.end()) {
        if (doCopy) {
          return AqlValue(_sliceSpan[it->second]);
        }
        return AqlValue(AqlValueHintSliceNoCopy{_sliceSpan[it->second]});
      }
    }
    {
      auto it = _varsToRegister.find(searchId);
      if (it != _varsToRegister.end()) {
        TRI_ASSERT(_inputRow.isInitialized());
        if (doCopy) {
          return _inputRow.getValue(it->second).clone();
        }
        return _inputRow.getValue(it->second);
      }
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, absl::StrCat("variable not found '", variable->name,
                                         "' in SliceSpanExpressionContext"));
  }

  containers::FlatHashMap<VariableId, size_t> const _varsToIndex;
  std::span<VPackSlice> _sliceSpan;
};

}  // namespace

auto IndexAggregateScanExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  LOG_DEVEL << "skip rows";
  LocalDocumentId docId;
  bool hasMore = true;
  size_t skipped = 0;
  auto vpackOptions = &_infos.query->vpackOptions();
  while (inputRange.hasDataRow() && clientCall.needSkipMore()) {
    // read one group
    if (not _inputRow.isInitialized()) {
      LOG_AGG_SCAN << "NEXT INPUT ROW";
      _inputRow = inputRange.peekDataRow().second;

      _iterator->cacheCurrentKey(_currentGroupKeySlices);
      LOG_AGG_SCAN << "[SCAN] Group keys "
                   << inspection::json(_currentGroupKeySlices);
    }

    while (hasMore) {
      // advance iterator
      // TODO one can improve this maybe by seeking forward
      hasMore = _iterator->next(_keySlices, docId, _projectionSlices);

      LOG_AGG_SCAN << "[SCAN] Next has more " << hasMore;
      if (not hasMore) {
        break;
      }

      LOG_AGG_SCAN << "[SCAN] Found keys " << inspection::json(_keySlices);

      // check if the keys still match
      for (size_t k = 0; k < _keySlices.size(); k++) {
        if (not basics::VelocyPackHelper::equal(
                _keySlices[k], _currentGroupKeySlices[k], true, vpackOptions)) {
          goto endOfGroup;
        }
      }
    }
  endOfGroup:

    LOG_AGG_SCAN << "[SCAN] End of group";
    clientCall.didSkip(1);
    skipped += 1;

    if (hasMore) {
      _iterator->cacheCurrentKey(_currentGroupKeySlices);
      LOG_AGG_SCAN << "[SCAN] New group keys "
                   << inspection::json(_currentGroupKeySlices);
    } else {
      _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
      inputRange.advanceDataRow();
    }
  }

  return std::make_tuple(inputRange.upstreamState(), Stats{}, skipped,
                         AqlCall{});
}

// here inputRange will only include one item (Singleton)
auto IndexAggregateScanExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                             OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  LocalDocumentId docId;
  bool hasMore = true;
  auto vpackOptions = &_infos.query->vpackOptions();
  size_t produce_rows_count = 0;
  size_t hasMoreCount = 0;
  while (inputRange.hasDataRow() && !output.isFull()) {
    // read one group
    produce_rows_count++;

    // read first row
    if (not _inputRow.isInitialized()) {
      LOG_DEVEL << "Initialize input row";
      LOG_AGG_SCAN << "NEXT INPUT ROW";
      _inputRow = inputRange.peekDataRow().second;

      _iterator->cacheCurrentKey(_currentGroupKeySlices);
      LOG_AGG_SCAN << "[SCAN] Group keys "
                   << inspection::json(_currentGroupKeySlices);
    }

    VPackBuilder builder;
    builder.openObject();
    _inputRow.toVelocyPack(nullptr, builder);
    builder.close();
    LOG_DEVEL << fmt::format("input range has data row: {}",
                             inspection::json(builder.slice()));

    // aggregate everything in one group
    SliceSpanExpressionContext context{
        _trx,
        *_infos.query,
        _functionsCache,
        {/* no external variables supported yet */},
        _inputRow,
        _variablesToProjectionsRelative};
    while (hasMore) {
      hasMoreCount++;
      // evaluate aggregator expression
      context._sliceSpan = _projectionSlices;
      LOG_AGG_SCAN << "[SCAN] Found Projections "
                   << inspection::json(_projectionSlices);
      for (size_t k = 0; k < _infos.aggregations.size(); k++) {
        bool mustDestroy;
        // LOG_DEVEL << "before expression exectuion";
        AqlValue result =
            _infos.aggregations[k].expression->execute(&context, mustDestroy);
        // LOG_DEVEL << "after expression execution";
        AqlValueGuard guard(result, mustDestroy);
        LOG_AGG_SCAN << "[SCAN] Agg " << k << " reduce with value ";
        _aggregatorInstances[k]->reduce(result);
      }

      // advance iterator
      hasMore = _iterator->next(_keySlices, docId, _projectionSlices);
      // LOG_DEVEL << inspection::json(_keySlices) << ", "
      //           << inspection::json(docId) << ","
      //           << inspection::json(_projectionSlices);

      LOG_AGG_SCAN << "[SCAN] Next has more " << hasMore;
      if (not hasMore) {
        break;
      }

      // check if the keys still match
      LOG_AGG_SCAN << "[SCAN] Found keys " << inspection::json(_keySlices);
      for (size_t k = 0; k < _keySlices.size(); k++) {
        if (not basics::VelocyPackHelper::equal(
                _keySlices[k], _currentGroupKeySlices[k], true, vpackOptions)) {
          goto endOfGroup;
        }
      }
    }
  endOfGroup:  // due to either change of group values or iterator exhausted

    LOG_AGG_SCAN << "[SCAN] End of group";

    // fill output
    for (size_t k = 0; k < _infos.groups.size(); k++) {
      output.moveValueInto(_infos.groups[k].outputRegister, _inputRow,
                           _currentGroupKeySlices[k]);
    }
    for (size_t k = 0; k < _infos.aggregations.size(); k++) {
      AqlValue r = _aggregatorInstances[k]->stealValue();
      AqlValueGuard guard{r, true};
      LOG_AGG_SCAN << "Agg " << k << " final value = " << r.toDouble();
      output.moveValueInto(_infos.aggregations[k].outputRegister, _inputRow,
                           &guard);
    }
    output.advanceRow();

    if (hasMore) {  // happens if iterator has more but keys of next don't match
      _iterator->cacheCurrentKey(_currentGroupKeySlices);
      LOG_AGG_SCAN << "[SCAN] New group keys "
                   << inspection::json(_currentGroupKeySlices);
    } else {
      _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
      inputRange.advanceDataRow();
      return std::make_tuple(inputRange.upstreamState(), Stats{}, AqlCall{});
    }
  }

  LOG_DEVEL << produce_rows_count << ", " << hasMoreCount;
  return std::make_tuple(inputRange.upstreamState(), Stats{}, AqlCall{});
}

IndexAggregateScanExecutor::IndexAggregateScanExecutor(Fetcher& fetcher,
                                                       Infos& infos)
    : _fetcher(fetcher), _infos(infos), _trx{_infos.query->newTrxContext()} {
  _currentGroupKeySlices.resize(_infos.groups.size());
  _keySlices.resize(_infos.groups.size());

  // create index iterator
  IndexStreamOptions streamOptions;
  streamOptions.usedKeyFields.reserve(infos.groups.size());
  std::transform(infos.groups.begin(), infos.groups.end(),
                 std::back_inserter(streamOptions.usedKeyFields),
                 [](auto const& i) { return i.indexField; });
  size_t offset = 0;
  for (auto const& [var, index_field] : _infos._expressionVariables) {
    streamOptions.projectedFields.emplace_back(index_field);
    _variablesToProjectionsRelative.emplace(var, offset++);
  }
  LOG_AGG_SCAN << "[SCAN] constructing stream with options " << streamOptions;
  _iterator = _infos.index->streamForCondition(&_trx, streamOptions);
  _iterator->test();

  // fill projection slices
  _projectionSlices.resize(streamOptions.projectedFields.size());
  bool hasMore = _iterator->reset(_currentGroupKeySlices, {});
  LOG_AGG_SCAN << "[SCAN] after reset at "
               << inspection::json(_currentGroupKeySlices)
               << " hasMore= " << hasMore;
  _iterator->load(_projectionSlices);
  LOG_AGG_SCAN << "[SCAN] projections are  "
               << inspection::json(_projectionSlices);
  LOG_DEVEL << "[SCAN] after reset at "
            << inspection::json(_currentGroupKeySlices) << " with projections "
            << inspection::json(_projectionSlices);

  // set aggregators
  _aggregatorInstances.reserve(_infos.aggregations.size());
  for (auto const& agg : _infos.aggregations) {
    auto const& factory = Aggregator::factoryFromTypeString(agg.type);
    auto instance = factory.operator()(&_infos.query->vpackOptions());

    _aggregatorInstances.emplace_back(std::move(instance))->reset();
  }
}

template class ExecutionBlockImpl<IndexAggregateScanExecutor>;

}  // namespace arangodb::aql
