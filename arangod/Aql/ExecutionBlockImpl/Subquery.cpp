////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <Aql/ExecutionBlockImpl.tpp>

namespace arangodb::aql {
template<>
auto ExecutionBlockImpl<SubqueryStartExecutor>::shadowRowForwarding(
    AqlCallStack& stack) -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());

  // The Subquery Start returns DONE after every row.
  // This needs to be resetted as soon as a shadowRow has been produced
  _executorReturnedDone = false;

  if (_lastRange.hasDataRow()) {
    // If we have a dataRow, the executor needs to write it's output.
    // If we get woken up by a dataRow during forwarding of ShadowRows
    // This will return false, and if so we need to call produce instead.
    auto didWrite = _executor.produceShadowRow(_lastRange, *_outputItemRow);
    // Need to report that we have written a row in the call

    if (didWrite) {
      auto& subqueryCall = stack.modifyTopCall();
      subqueryCall.didProduce(1);
      if (_lastRange.hasShadowRow()) {
        // Forward the ShadowRows
        return ExecState::SHADOWROWS;
      }
      return ExecState::NEXTSUBQUERY;
    } else {
      // Woken up after shadowRow forwarding
      // Need to call the Executor
      return ExecState::CHECKCALL;
    }
  } else {
    // Need to forward the ShadowRows
    auto&& [state, shadowRow] = _lastRange.nextShadowRow();
    TRI_ASSERT(shadowRow.isInitialized());
    _outputItemRow->increaseShadowRowDepth(shadowRow);
    TRI_ASSERT(_outputItemRow->produced());
    _outputItemRow->advanceRow();

    // Count that we have now produced a row in the new depth.
    // Note: We need to increment the depth by one, as the we have increased
    // it while writing into the output by one as well.
    countShadowRowProduced(stack, shadowRow.getDepth() + 1);

    if (_lastRange.hasShadowRow()) {
      return ExecState::SHADOWROWS;
    }
    // If we do not have more shadowRows
    // we need to return.

    auto& subqueryCallList = stack.modifyCallListAtDepth(shadowRow.getDepth());

    if (!subqueryCallList.hasDefaultCalls()) {
      return ExecState::DONE;
    }

    auto& subqueryCall = subqueryCallList.modifyNextCall();
    if (subqueryCall.getLimit() == 0 && !subqueryCall.needSkipMore()) {
      return ExecState::DONE;
    }

    _executorReturnedDone = false;

    return ExecState::NEXTSUBQUERY;
  }
}

template<>
auto ExecutionBlockImpl<SubqueryEndExecutor>::shadowRowForwarding(
    AqlCallStack& stack) -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());
  if (!_lastRange.hasShadowRow()) {
    // We got back without a ShadowRow in the LastRange
    // Let client call again
    return ExecState::NEXTSUBQUERY;
  }
  auto&& [state, shadowRow] = _lastRange.nextShadowRow();
  TRI_ASSERT(shadowRow.isInitialized());
  if (shadowRow.isRelevant()) {
    // We need to consume the row, and write the Aggregate to it.
    _executor.consumeShadowRow(shadowRow, *_outputItemRow);
    // we need to reset the ExecutorHasReturnedDone, it will
    // return done after every subquery is fully collected.
    _executorReturnedDone = false;

  } else {
    _outputItemRow->decreaseShadowRowDepth(shadowRow);
  }

  TRI_ASSERT(_outputItemRow->produced());
  _outputItemRow->advanceRow();
  // The stack in used here contains all calls for within the subquery.
  // Hence any inbound subquery needs to be counted on its level

  countShadowRowProduced(stack, shadowRow.getDepth());

  if (state == ExecutorState::DONE) {
    // We have consumed everything, we are
    // Done with this query
    return ExecState::DONE;
  } else if (_lastRange.hasDataRow()) {
    // Multiple concatenated Subqueries
    return ExecState::NEXTSUBQUERY;
  } else if (_lastRange.hasShadowRow()) {
    // We still have shadowRows, we
    // need to forward them
    return ExecState::SHADOWROWS;
  } else if (_outputItemRow->isFull()) {
    // Fullfilled the call
    // Need to return!
    return ExecState::DONE;
  } else {
    // End of input, we are done for now
    // Need to call again
    return ExecState::NEXTSUBQUERY;
  }
}
}  // namespace arangodb::aql
