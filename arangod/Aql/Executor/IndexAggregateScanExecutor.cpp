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
#include <velocypack/Options.h>
#include "Aql/DocumentProducingExpressionContext.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/Aggregator.h"
#include "Basics/VelocyPackHelper.h"

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

bool isEqual(std::vector<VPackSlice> const& a, std::vector<VPackSlice> const& b,
             velocypack::Options const& options) {
  for (size_t k = 0; k < a.size(); k++) {
    if (not basics::VelocyPackHelper::equal(a[k], b[k], true, &options)) {
      return false;
    }
  }
  return true;
}

void aggregate(
    std::vector<std::unique_ptr<Aggregator>>& aggregators,
    std::vector<IndexAggregateScanInfos::Aggregation> const& aggregations,
    SliceSpanExpressionContext& context) {
  for (size_t k = 0; k < aggregations.size(); k++) {
    bool mustDestroy;
    AqlValue result =
        aggregations[k].expression->execute(&context, mustDestroy);
    AqlValueGuard guard(result, mustDestroy);
    LOG_AGG_SCAN << "[SCAN] Agg " << k << " reduce with value ";
    aggregators[k]->reduce(result);
  }
}

}  // namespace

auto IndexAggregateScanExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  if (not inputRange.hasDataRow()) {
    return std::make_tuple(inputRange.upstreamState(), Stats{}, 0, AqlCall{});
  }
  TRI_ASSERT(inputRange.countDataRows() == 1) << fmt::format(
      "IndexAggregateScanExecutor expects to be run just after a SingletonNode "
      "with one input row, but input range has {} rows",
      inputRange.countDataRows());

  LocalDocumentId docId;
  bool hasMore = true;
  size_t skipped = 0;
  auto vpackOptions = &_infos.query->vpackOptions();

  while (clientCall.needSkipMore() && hasMore) {
    _iterator->cacheCurrentKey(_currentGroupKeySlices);
    LOG_AGG_SCAN << "[SCAN] Group keys "
                 << inspection::json(_currentGroupKeySlices);

    while (true) {
      // TODO one can improve this maybe by seeking forward
      hasMore = _iterator->next(_keySlices, docId, _projectionSlices);
      LOG_AGG_SCAN << fmt::format("[SCAN] Found keys {} and projectsion {}",
                                  inspection::json(_keySlices),
                                  inspection::json(_projectionSlices));

      LOG_AGG_SCAN << "[SCAN] Next has more " << hasMore;
      if (not hasMore) {
        break;
      }

      if (not isEqual(_keySlices, _currentGroupKeySlices, *vpackOptions)) {
        break;
      }
    }

    LOG_AGG_SCAN << "[SCAN] End of group";

    clientCall.didSkip(1);
    skipped += 1;

    if (not hasMore) {
      inputRange.advanceDataRow();
      return std::make_tuple(inputRange.upstreamState(), Stats{}, skipped,
                             AqlCall{});
    }
  }

  return std::make_tuple(inputRange.upstreamState(), Stats{}, skipped,
                         AqlCall{});
}

// here inputRange will only include one item (Singleton)
auto IndexAggregateScanExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                             OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  if (not inputRange.hasDataRow()) {
    return std::make_tuple(inputRange.upstreamState(), Stats{}, AqlCall{});
  }

  TRI_ASSERT(inputRange.countDataRows() == 1) << fmt::format(
      "IndexAggregateScanExecutor expects to be run just after a SingletonNode "
      "with one input row, but input range has {} rows",
      inputRange.countDataRows());

  _inputRow = inputRange.peekDataRow().second;

  LocalDocumentId docId;
  bool hasMore = true;
  auto vpackOptions = &_infos.query->vpackOptions();

  SliceSpanExpressionContext context{
      _trx,
      *_infos.query,
      _functionsCache,
      {/* no external variables supported yet */},
      _inputRow,
      _variablesToProjectionsRelative};

  while (!output.isFull() && hasMore) {
    _iterator->cacheCurrentKey(_currentGroupKeySlices);
    LOG_AGG_SCAN << "[SCAN] Group keys "
                 << inspection::json(_currentGroupKeySlices);

    while (true) {
      context._sliceSpan = _projectionSlices;
      aggregate(_aggregatorInstances, _infos.aggregations, context);

      hasMore = _iterator->next(_keySlices, docId, _projectionSlices);
      LOG_AGG_SCAN << fmt::format("[SCAN] Found keys {} and projections {}",
                                  inspection::json(_keySlices),
                                  inspection::json(_projectionSlices));

      LOG_AGG_SCAN << "[SCAN] Next has more " << hasMore;
      if (not hasMore) {
        break;
      }

      if (not isEqual(_keySlices, _currentGroupKeySlices, *vpackOptions)) {
        break;
      }
    }

    LOG_AGG_SCAN << "[SCAN] End of group";

    for (size_t k = 0; k < _infos.groups.size(); k++) {
      output.moveValueInto(_infos.groups[k].outputRegister, _inputRow,
                           _currentGroupKeySlices[k]);
    }
    for (size_t k = 0; k < _infos.aggregations.size(); k++) {
      AqlValue r = _aggregatorInstances[k]->stealValue();
      AqlValueGuard aggregatorGuard{r, true};
      LOG_AGG_SCAN << "Agg " << k << " final value = " << r.toDouble();
      output.moveValueInto(_infos.aggregations[k].outputRegister, _inputRow,
                           &aggregatorGuard);
    }
    output.advanceRow();

    if (not hasMore) {
      inputRange.advanceDataRow();
      return std::make_tuple(inputRange.upstreamState(), Stats{}, AqlCall{});
    }
  }

  return std::make_tuple(inputRange.upstreamState(), Stats{}, AqlCall{});
}

IndexAggregateScanExecutor::IndexAggregateScanExecutor(Fetcher& fetcher,
                                                       Infos& infos)
    : _fetcher(fetcher), _infos(infos), _trx{_infos.query->newTrxContext()} {
  _currentGroupKeySlices.resize(_infos.groups.size());
  _keySlices.resize(_infos.groups.size());

  // create index iterator
  // the groups define the keyFields of the iterator,
  // the expressionVariables (all variables in aggregate expressions) define the
  // projectionFields.
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

  // fill projection slices
  _projectionSlices.resize(streamOptions.projectedFields.size());
  bool hasMore = _iterator->reset(_currentGroupKeySlices, {});
  LOG_AGG_SCAN << "[SCAN] after reset at "
               << inspection::json(_currentGroupKeySlices)
               << " hasMore= " << hasMore;
  _iterator->load(_projectionSlices);
  LOG_AGG_SCAN << "[SCAN] projections are  "
               << inspection::json(_projectionSlices);

  // create aggregators
  _aggregatorInstances.reserve(_infos.aggregations.size());
  for (auto const& agg : _infos.aggregations) {
    auto const& factory = Aggregator::factoryFromTypeString(agg.type);
    auto instance = factory.operator()(&_infos.query->vpackOptions());

    _aggregatorInstances.emplace_back(std::move(instance))->reset();
  }
}

template class ExecutionBlockImpl<IndexAggregateScanExecutor>;

}  // namespace arangodb::aql
