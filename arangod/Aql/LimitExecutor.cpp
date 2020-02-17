////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "LimitExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Logger/LogMacros.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

LimitExecutorInfos::LimitExecutorInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                                       // cppcheck-suppress passedByValue
                                       std::unordered_set<RegisterId> registersToClear,
                                       // cppcheck-suppress passedByValue
                                       std::unordered_set<RegisterId> registersToKeep,
                                       size_t offset, size_t limit, bool fullCount)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    std::make_shared<std::unordered_set<RegisterId>>(),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _offset(offset),
      _limit(limit),
      _fullCount(fullCount) {}

LimitExecutor::LimitExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _lastRowToOutput(CreateInvalidInputRowHint{}) {}

LimitExecutor::~LimitExecutor() = default;

auto LimitExecutor::limitFulfilled() const noexcept -> bool {
  return remainingOffset() + remainingLimit() == 0;
}

auto LimitExecutor::calculateUpstreamCall(AqlCall const& clientCall) const -> AqlCall {
  auto upstreamCall = AqlCall{};

  // Offsets can simply be added.
  upstreamCall.offset = clientCall.getOffset() + remainingOffset();

  // To get the limit for upstream, we must subtract the downstream offset from
  // our limit, and take the minimum of this and the downstream limit.
  auto const localLimitMinusDownstreamOffset =
      remainingLimit() - std::min(remainingLimit(), clientCall.getOffset());
  auto const limit =
      std::min<AqlCall::Limit>(clientCall.getLimit(), localLimitMinusDownstreamOffset);

  // Generally, we create a hard limit. However, if we get a soft limit from
  // downstream that is lower than our hard limit, we use that instead.
  bool const useSoftLimit = clientCall.hasSoftLimit() &&
                            clientCall.getLimit() < localLimitMinusDownstreamOffset;

  if (useSoftLimit) {
    upstreamCall.softLimit = limit;
    upstreamCall.fullCount = false;
  } else {
    upstreamCall.hardLimit = limit;
    upstreamCall.fullCount = infos().isFullCountEnabled();
  }

  return upstreamCall;
}

auto LimitExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  // I think this *should* be the case, because we're passthrough. However,
  // isFull() ignores shadow rows in the passthrough case, which it probably
  // should not.
  // static_assert(Properties::allowsBlockPassthrough == BlockPassthrough::Enable);
  // TRI_ASSERT(input.hasDataRow() == !output.isFull());

  auto const& clientCall = output.getClientCall();
  TRI_ASSERT(clientCall.getOffset() == 0);

  auto stats = LimitStats{};

  auto const upstreamCall = calculateUpstreamCall(clientCall);

  // We may not yet skip more than our offset, fullCount must happen after all
  // rows were produced only.
  auto const skipped = input.skip(upstreamCall.getOffset());
  _counter += skipped;
  if (infos().isFullCountEnabled()) {
    stats.incrFullCountBy(skipped);
  }

  // We may not get data rows until our offset is reached
  TRI_ASSERT(upstreamCall.getOffset() == skipped || !input.hasDataRow());

  auto numRowsWritten = size_t{0};
  while (numRowsWritten < upstreamCall.getLimit() && input.hasDataRow() &&
         !output.isFull()) {
    output.copyRow(input.nextDataRow().second);
    output.advanceRow();
    ++numRowsWritten;
  }
  _counter += numRowsWritten;
  if (infos().isFullCountEnabled()) {
    stats.incrFullCountBy(numRowsWritten);
    if (limitFulfilled()) {
      stats.incrFullCountBy(input.skipAll());
    }
  }

  // When the limit is fulfilled, there must not be any skipped rows left.
  // The offset is handled first, and skipped rows due to fullCount may only
  // appear after the limit is fullfilled; and those are already counted.
  TRI_ASSERT(!limitFulfilled() || input.skippedInFlight() == 0);

  // We're passthrough, we must not have any input left when the limit is fulfilled
  TRI_ASSERT(!limitFulfilled() || !input.hasDataRow());

  return {input.upstreamState(), stats, calculateUpstreamCall(clientCall)};
}

auto LimitExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto upstreamCall = calculateUpstreamCall(call);

  if (ADB_UNLIKELY(inputRange.skippedInFlight() < upstreamCall.getOffset() &&
                   inputRange.hasDataRow())) {
    static_assert(Properties::allowsBlockPassthrough == BlockPassthrough::Enable,
                  "For LIMIT with passthrough to work, there must no input "
                  "rows before the offset was skipped.");
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL,
                                   "Unexpected input while skipping: got data "
                                   "rows before offset was reached.");
  }

  // We may not yet skip more than the combined offsets, fullCount must happen
  // after all rows were produced only.
  auto const skippedTotal = inputRange.skip(upstreamCall.getOffset());
  auto const skippedForDownstream = std::min(skippedTotal, call.getOffset());
  // Report at most what was asked for, the rest is our own offset.
  call.didSkip(skippedForDownstream);
  _counter += skippedTotal;
  auto stats = LimitStats{};
  if (infos().isFullCountEnabled()) {
    stats.incrFullCountBy(skippedTotal);
  }

  return {inputRange.upstreamState(), stats, skippedForDownstream,
          calculateUpstreamCall(call)};
}
