////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "MultipleRemoteModificationExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/QueryContext.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::aql {

MultipleRemoteModificationInfos::MultipleRemoteModificationInfos(
    ExecutionEngine* engine, RegisterId inputRegister,
    RegisterId outputNewRegisterId, RegisterId outputOldRegisterId,
    RegisterId outputRegisterId, arangodb::aql::QueryContext& query,
    OperationOptions options, aql::Collection const* aqlCollection,
    ConsultAqlWriteFilter consultAqlWriteFilter, IgnoreErrors ignoreErrors,
    IgnoreDocumentNotFound ignoreDocumentNotFound, bool hasParent,
    bool isExclusive)
    : ModificationExecutorInfos(
          engine, inputRegister, RegisterPlan::MaxRegisterId,
          RegisterPlan::MaxRegisterId, outputNewRegisterId, outputOldRegisterId,
          outputRegisterId, query, std::move(options), aqlCollection,
          ExecutionBlock::DefaultBatchSize, ProducesResults(false),
          consultAqlWriteFilter, ignoreErrors, DoCount(true), IsReplace(false),
          ignoreDocumentNotFound),
      _hasParent(hasParent),
      _isExclusive(isExclusive) {}

MultipleRemoteModificationExecutor::MultipleRemoteModificationExecutor(
    Fetcher& fetcher, Infos& info)
    : _ctx(std::make_shared<transaction::StandaloneContext>(
          info._query.vocbase(), info._query.operationOrigin())),
      _trx(createTransaction(_ctx, info)),
      _info(info),
      _upstreamState(ExecutionState::HASMORE) {
  _trx.addHint(transaction::Hints::Hint::GLOBAL_MANAGED);
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
};

transaction::Methods MultipleRemoteModificationExecutor::createTransaction(
    std::shared_ptr<transaction::Context> ctx, Infos& info) {
  transaction::Options opts;
  opts.waitForSync = info._options.waitForSync;
  if (info._isExclusive) {
    // exclusive transaction
    return {std::move(ctx),
            /*read*/ {},
            /*write*/ {},
            /*exclusive*/ {info._aqlCollection->name()}, opts};
  }
  // write transaction
  return {std::move(ctx), /*read*/ {}, /*write*/ {info._aqlCollection->name()},
          /*exclusive*/ {}, opts};
}

[[nodiscard]] auto MultipleRemoteModificationExecutor::produceRows(
    AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, MultipleRemoteModificationExecutor::Stats,
                  AqlCall> {
  auto stats = Stats{};

  if (input.hasDataRow()) {
    if (_hasFetchedDataRow) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "MultipleRemoteModificationExecutor must "
                                     "fetch data from produceRows only once");
    }
    _hasFetchedDataRow = true;
    auto [state, row] = input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    if (input.hasDataRow()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "input from MultipleRemoteModificationExecutor should not have more "
          "than one data row");
    }
    auto result = doMultipleRemoteOperations(row, stats);
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
  // note: this code currently is never triggered, because the optimizer
  // rule is not firing when the query uses a LIMIT
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "optimizer rule not supposed to be used with keyword LIMIT");
#if 0
  auto stats = Stats{};

  if (input.hasDataRow()) {
    auto [state, row] = input.nextDataRow();
    auto result = doMultipleRemoteOperations(row, stats);
    if (result.ok()) {
      call.didSkip(1);
      return {input.upstreamState(), stats, 1, AqlCall{}};
    }
  }
  return {input.upstreamState(), stats, 0, AqlCall{}};
#endif
}

auto MultipleRemoteModificationExecutor::doMultipleRemoteOperations(
    InputAqlItemRow& input, Stats& stats) -> OperationResult {
  _info._options.silent = false;
  _info._options.returnOld =
      _info._options.returnOld || _info._outputRegisterId.isValid();

  // currently not supported
  TRI_ASSERT(!_info._options.returnOld);
  TRI_ASSERT(!_info._options.returnNew);

  TRI_ASSERT(_info._input1RegisterId.isValid());
  AqlValue const& inDocument = input.getValue(_info._input1RegisterId);
  if (_info._options.returnOld &&
      !_info._options.isOverwriteModeUpdateReplace()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN,
        "OLD is only available when using INSERT with overwriteModes "
        "'update' or 'replace'");
  }

  auto res = _trx.begin();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto result = _trx.insert(_info._aqlCollection->name(), inDocument.slice(),
                            _info._options);
  if (result.fail()) {
    THROW_ARANGO_EXCEPTION(result.result);
  }

  if (result.countErrorCodes.contains(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // check operation result
  if (!_info._ignoreErrors) {
    if (!result.countErrorCodes.empty()) {
      THROW_ARANGO_EXCEPTION(result.countErrorCodes.begin()->first);
    }
  }

  uint64_t writesExecuted = 0;
  uint64_t writesIgnored = 0;

  if (result.hasSlice()) {
    for (auto it : VPackArrayIterator(result.slice())) {
      if (it.isObject() && it.get(StaticStrings::Error).isTrue()) {
        ++writesIgnored;
      } else {
        ++writesExecuted;
      }
    }
  }

  res = _trx.commit();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  stats.incrWritesExecuted(writesExecuted);
  stats.incrWritesIgnored(writesIgnored);
  // the increment of index is not correct when the executor doesn't apply the
  // multiple document optimization rule, but, as it was only incrementing for
  // index lookup operations which don't apply this rule for now, we don't
  // increment the index
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
    return;
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
    output.moveValueInto(_info._outputRegisterId, input, &guard);
  }

  // RETURN OLD: current unsupported
  if (_info._outputOldRegisterId.isValid()) {
    TRI_ASSERT(options.returnOld);
    AqlValue value(oldDocument);
    AqlValueGuard guard(value, true);
    output.moveValueInto(_info._outputOldRegisterId, input, &guard);
  }

  // RETURN NEW: current unsupported
  if (_info._outputNewRegisterId.isValid()) {
    TRI_ASSERT(options.returnNew);
    AqlValue value(newDocument);
    AqlValueGuard guard(value, true);
    output.moveValueInto(_info._outputNewRegisterId, input, &guard);
  }
}

template class ExecutionBlockImpl<MultipleRemoteModificationExecutor>;

}  // namespace arangodb::aql
