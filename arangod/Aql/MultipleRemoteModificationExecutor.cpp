////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "Aql/MultipleRemoteModificationExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/QueryContext.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "VocBase/LogicalCollection.h"

#include <algorithm>

namespace arangodb::aql {

MultipleRemoteModificationExecutor::MultipleRemoteModificationExecutor(
    Fetcher& fetcher, Infos& info)
    : _trx(info._query.newTrxContext()),
      _info(info),
      _upstreamState(ExecutionState::HASMORE) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
};

[[nodiscard]] auto MultipleRemoteModificationExecutor::produceRows(
    AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, MultipleRemoteModificationExecutor::Stats,
                  AqlCall> {
  auto stats = Stats{};

  if (input.hasDataRow()) {
    auto [state, row] = input.nextDataRow();
    auto result = doMultipleRemoteModificationOperation(row, stats);
    if (result.ok()) {
      doMultipleRemoteModificationOutput(row, output, result);
    }
  }

  return {input.upstreamState(), stats, AqlCall{}};
}

[[nodiscard]] auto MultipleRemoteModificationExecutor::skipRowsRange(
    AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, MultipleRemoteModificationExecutor::Stats,
                  size_t, AqlCall> {
  auto stats = Stats{};

  if (input.hasDataRow()) {
    auto [state, row] = input.nextDataRow();
    auto result = doMultipleRemoteModificationOperation(row, stats);
    if (result.ok()) {
      call.didSkip(1);
      return {input.upstreamState(), stats, 1, AqlCall{}};
    }
  }
  return {input.upstreamState(), stats, 0, AqlCall{}};
}

auto MultipleRemoteModificationExecutor::doMultipleRemoteModificationOperation(
    InputAqlItemRow& input, Stats& stats) -> OperationResult {
  _info._options.silent = false;
  _info._options.returnOld =
      _info._options.returnOld || _info._outputRegisterId.isValid();

  OperationResult result(Result(), _info._options);

  int possibleWrites = 0;  // TODO - get real statistic values!

  TRI_ASSERT(_info._input1RegisterId.isValid());
  AqlValue const& inDocument = input.getValue(_info._input1RegisterId);
  if (_info._options.returnOld &&
      !_info._options.isOverwriteModeUpdateReplace()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN,
        "OLD is only available when using INSERT with overwriteModes "
        "'update' or 'replace'");
  }

  result = _trx.insert(_info._aqlCollection->name(), inDocument.slice(),
                       _info._options);
  possibleWrites = inDocument.slice().length();
  // TODO consider the result of the operation

  // check operation result
  if (!_info._ignoreErrors) {  // TODO remove if
    if (result.ok() && result.hasSlice()) {
      for (auto it : VPackArrayIterator(result.slice())) {
        auto errorNum = it.get("errorNum");
        auto errorMessage = it.get("errorMessage");
        if (errorNum.isNumber() && errorMessage.isString()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(ErrorCode(errorNum.getInt()),
                                         errorMessage.stringView());
        }
      }
    }
  }

  stats.incrWritesExecuted(possibleWrites);
  stats.incrScannedIndex();
  return result;
}

auto MultipleRemoteModificationExecutor::doMultipleRemoteModificationOutput(
    InputAqlItemRow& input, OutputAqlItemRow& output, OperationResult& result)
    -> void {
  OperationOptions& options = _info._options;

  if (!(_info._outputRegisterId.isValid() ||
        _info._outputOldRegisterId.isValid() ||
        _info._outputNewRegisterId.isValid())) {
    if (_info._hasParent) {
      output.copyRow(input);
    }
    return;  //  _info._hasParent;
  }

  // Fill itemblock
  // create block that can hold a result with one entry and a number of
  // variables corresponding to the amount of out variables

  // only copy 1st row of registers inherited from previous frame(s)
  TRI_ASSERT(result.ok());
  VPackSlice outDocument = VPackSlice::nullSlice();
  if (result.buffer) {
    outDocument = result.slice().resolveExternal();
  }

  VPackSlice oldDocument = VPackSlice::nullSlice();
  VPackSlice newDocument = VPackSlice::nullSlice();

  TRI_ASSERT(_info._outputRegisterId.isValid() ||
             _info._outputOldRegisterId.isValid() ||
             _info._outputNewRegisterId.isValid());

  // place documents as in the out variable slots of the result
  if (_info._outputRegisterId.isValid()) {
    AqlValue value(outDocument);
    AqlValueGuard guard(value, true);
    output.moveValueInto(_info._outputRegisterId, input, guard);
  }

  if (_info._outputOldRegisterId.isValid()) {
    TRI_ASSERT(options.returnOld);
    AqlValue value(oldDocument);
    AqlValueGuard guard(value, true);
    output.moveValueInto(_info._outputOldRegisterId, input, guard);
  }

  if (_info._outputNewRegisterId.isValid()) {
    TRI_ASSERT(options.returnNew);
    AqlValue value(newDocument);
    AqlValueGuard guard(value, true);
    output.moveValueInto(_info._outputNewRegisterId, input, guard);
  }

  TRI_IF_FAILURE("MultipleRemoteModificationOperationBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}
}  // namespace arangodb::aql
