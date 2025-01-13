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
        mustDestroy = true;
        return AqlValue(_sliceSpan[it->second]);
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
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto IndexAggregateScanExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                             OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  LocalDocumentId docId;
  bool hasMore;
  while (inputRange.hasDataRow() && !output.isFull()) {
    // read one group
    if (not _inputRow.isInitialized()) {
      ExecutorState state;
      std::tie(state, _inputRow) = inputRange.nextDataRow();
      if (state == ExecutorState::DONE) {
        break;
      }

      hasMore = _iterator->next(_keySlices, docId, _projectionSlices);
      if (not hasMore) {
        break;
      }

      _iterator->cacheCurrentKey(_currentGroupKeySlices);
    }

    SliceSpanExpressionContext context{
        _trx,
        *_infos.query,
        _functionsCache,
        {/* no external variables supported yet */},
        _inputRow,
        _infos._expressionVariables};
    while (hasMore) {
      // evaluate aggregator expression
      context._sliceSpan = _projectionSlices;
      for (size_t k = 0; k < _infos.aggregations.size(); k++) {
        bool mustDestroy;
        AqlValue result =
            _infos.aggregations[k].expression.execute(&context, mustDestroy);
        AqlValueGuard guard(result, mustDestroy);
        _aggregatorInstances[k]->reduce(result);
      }

      // advance iterator
      hasMore = _iterator->next(_keySlices, docId, _projectionSlices);
      if (not hasMore) {
        break;
      }

      // check if the keys still match
      for (size_t k = 0; k < _keySlices.size(); k++) {
        if (not basics::VelocyPackHelper::equal(
                _keySlices[k], _currentGroupKeySlices[k], true,
                &_infos.query->vpackOptions())) {
          goto endOfGroup;
        }
      }
    }
  endOfGroup:

    // fill output
    for (size_t k = 0; k < _infos.groups.size(); k++) {
      output.moveValueInto(_infos.groups[k].outputRegister,
                           inputRange.peekDataRow().second,
                           _currentGroupKeySlices[k]);
    }
    for (size_t k = 0; k < _infos.aggregations.size(); k++) {
      output.moveValueInto(_infos.aggregations[k].outputRegister,
                           inputRange.peekDataRow().second,
                           _aggregatorInstances[k]->stealValue());
      _aggregatorInstances[k].reset();
    }
    output.advanceRow();

    if (hasMore) {
      _iterator->cacheCurrentKey(_currentGroupKeySlices);
    }
  }

  return std::make_tuple(inputRange.upstreamState(), Stats{}, AqlCall{});
}

IndexAggregateScanExecutor::IndexAggregateScanExecutor(Fetcher& fetcher,
                                                       Infos& infos)
    : _fetcher(fetcher), _infos(infos), _trx{_infos.query->newTrxContext()} {
  _currentGroupKeySlices.resize(_infos.groups.size());
  _keySlices.resize(_infos.groups.size());
  _projectionSlices.resize(_infos.aggregations.size());

  IndexStreamOptions streamOptions;

  streamOptions.usedKeyFields.reserve(infos.groups.size());
  std::transform(infos.groups.begin(), infos.groups.end(),
                 std::back_inserter(streamOptions.usedKeyFields),
                 [](auto const& i) { return i.indexField; });

  _iterator = _infos.index->streamForCondition(&_trx, streamOptions);

  _aggregatorInstances.reserve(_infos.aggregations.size());
  for (auto const& agg : _infos.aggregations) {
    auto const& factory = Aggregator::factoryFromTypeString(agg.type);
    auto instance = factory.operator()(&_infos.query->vpackOptions());

    _aggregatorInstances.emplace_back(std::move(instance))->reset();
  }
}

template class ExecutionBlockImpl<IndexAggregateScanExecutor>;

}  // namespace arangodb::aql
