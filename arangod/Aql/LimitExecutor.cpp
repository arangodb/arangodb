////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "LimitExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

LimitExecutorInfos::LimitExecutorInfos(size_t offset, size_t limit, bool fullCount)
    : _offset(offset), _limit(limit), _fullCount(fullCount) {}

LimitExecutor::LimitExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _lastRowToOutput(CreateInvalidInputRowHint{}) {}

LimitExecutor::~LimitExecutor() = default;

auto LimitExecutor::limitFulfilled() const noexcept -> bool {
  return remainingOffset() + remainingLimit() == 0;
}

auto LimitExecutor::calculateUpstreamCall(AqlCall const& clientCall) const -> AqlCall {
  auto upstreamCall = AqlCall{};

  auto const limitedClientOffset = std::min(clientCall.getOffset(), remainingLimit());

  // Offsets must be added, but the client's offset is limited by our limit.
  upstreamCall.offset = remainingOffset() + limitedClientOffset;

  // To get the limit for upstream, we must subtract the downstream offset from
  // our limit, and take the minimum of this and the downstream limit.
  auto const localLimitMinusDownstreamOffset = remainingLimit() - limitedClientOffset;
  auto const limit =
      std::min<AqlCall::Limit>(clientCall.getLimit(), localLimitMinusDownstreamOffset);

  // Generally, we create a hard limit. However, if we get a soft limit from
  // downstream that is lower than our hard limit, we use that instead.
  bool const useSoftLimit = !clientCall.hasHardLimit() &&
                            clientCall.getLimit() < localLimitMinusDownstreamOffset;

  if (useSoftLimit) {
    // fullCount may only be set with a hard limit
    TRI_ASSERT(!clientCall.needsFullCount());

    upstreamCall.softLimit = limit;
    upstreamCall.fullCount = false;
  } else if (clientCall.needsFullCount() && 0 == clientCall.getLimit() && !_didProduceRows) {
    // The request is a hard limit of 0 together with fullCount, so we always
    // need to skip both our offset and our limit, regardless of the client's
    // offset.
    // If we ever got a client limit > 0 and thus produced rows, we may not send
    // a non-zero offset, and thus must avoid this branch.
    TRI_ASSERT(!useSoftLimit);
    TRI_ASSERT(clientCall.hasHardLimit());

    upstreamCall.offset = upstreamCall.offset + remainingLimit();
    upstreamCall.hardLimit = std::size_t(0);
    // We need to send fullCount upstream iff this is fullCount-enabled LIMIT
    // block.
    upstreamCall.fullCount = infos().isFullCountEnabled();
  } else {
    TRI_ASSERT(!useSoftLimit);
    TRI_ASSERT(!clientCall.needsFullCount() || 0 < clientCall.getLimit() || _didProduceRows);

    upstreamCall.hardLimit = limit;
    // We need the fullCount if we need to report it ourselves.
    // If the client needs full count, we need to skip up to our limit upstream.
    // But currently the execute API does not allow limited skipping after
    // fetching rows, so we must pass fullCount upstream, even if that's
    // inefficient.
    // This is going to be fixed in a later PR.
    upstreamCall.fullCount = infos().isFullCountEnabled() || clientCall.needsFullCount();
  }

  return upstreamCall;
}

auto LimitExecutor::produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  // I think this *should* be the case, because we're passthrough. However,
  // isFull() ignores shadow rows in the passthrough case, which it probably
  // should not.
  // static_assert(Properties::allowsBlockPassthrough == BlockPassthrough::Enable);
  // TRI_ASSERT(input.hasDataRow() == !output.isFull());

  auto const& clientCall = output.getClientCall();
  TRI_ASSERT(clientCall.getOffset() == 0);

  auto stats = LimitStats{};

  auto call = output.getClientCall();
  TRI_ASSERT(call.getOffset() == 0);
  while (inputRange.skippedInFlight() > 0 || inputRange.hasDataRow()) {
    if (remainingOffset() > 0) {
      // First we skip in the input row until we fullfill our local offset.
      auto const didSkip = inputRange.skip(remainingOffset());
      // Need to forward the
      _counter += didSkip;
      // We do not report this to downstream
      // But we report it in fullCount
      if (infos().isFullCountEnabled()) {
        stats.incrFullCountBy(didSkip);
      }
    } else if (!output.isFull()) {
      auto numRowsWritten = size_t{0};

      while (inputRange.hasDataRow()) {
        // This block is passhthrough.
        static_assert(Properties::allowsBlockPassthrough == BlockPassthrough::Enable,
                      "For LIMIT with passthrough to work, there must be "
                      "exactly enough space for all input in the output.");
        // So there will always be enough place for all inputRows within
        // the output.
        TRI_ASSERT(!output.isFull());
        // Also this number can be at most remainingOffset.
        TRI_ASSERT(remainingLimit() > numRowsWritten);
        output.copyRow(inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{}).second);
        output.advanceRow();
        numRowsWritten++;
        _didProduceRows = true;
      }
      _counter += numRowsWritten;
      if (infos().isFullCountEnabled()) {
        stats.incrFullCountBy(numRowsWritten);
      }
    } else if (call.needsFullCount()) {
      // We are done with producing.
      // ExecutionBlockImpl will now call skipSome for the remainder
      // There cannot be a dataRow left, as this block is passthrough!
      TRI_ASSERT(!inputRange.hasDataRow());
      // There are still skippedInflights;
      TRI_ASSERT(inputRange.skippedInFlight() > 0);
      break;
    } else {
      // We are done with producing.
      if (infos().isFullCountEnabled()) {
        // However we need to report the fullCount from above.
        stats.incrFullCountBy(inputRange.skipAll());
      }
      // ExecutionBlockImpl will now call skipSome for the remainder
      // There cannot be a dataRow left, as this block is passthrough!
      TRI_ASSERT(!inputRange.hasDataRow());
      TRI_ASSERT(inputRange.skippedInFlight() == 0);
      break;
    }
  }
  // We're passthrough, we must not have any input left when the limit isfulfilled
  TRI_ASSERT(!limitFulfilled() || !inputRange.hasDataRow());
  return {inputRange.upstreamState(), stats, calculateUpstreamCall(call)};
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
  auto stats = LimitStats{};
  while (inputRange.skippedInFlight() > 0) {
    if (remainingOffset() > 0) {
      // First we skip in the input row until we fullfill our local offset.
      auto const didSkip = inputRange.skip(remainingOffset());
      // Need to forward the
      _counter += didSkip;
      // We do not report this to downstream
      // But we report it in fullCount
      if (infos().isFullCountEnabled()) {
        stats.incrFullCountBy(didSkip);
      }
    } else if (remainingLimit() > 0) {
      // We do only report to downstream if we have a limit to produce
      if (call.getOffset() > 0) {
        // Next we skip as many rows as ordered by the client,
        // but never more then remainingLimit
        auto const didSkip =
            inputRange.skip(std::min(remainingLimit(), call.getOffset()));
        call.didSkip(didSkip);
        _counter += didSkip;
        if (infos().isFullCountEnabled()) {
          stats.incrFullCountBy(didSkip);
        }
      } else if (call.getLimit() > 0) {
        // If we get here we need to break out, and let produce rows be called.
        break;
      } else if (call.needsFullCount()) {
        auto const didSkip = inputRange.skip(remainingLimit());
        call.didSkip(didSkip);
        _counter += didSkip;
        if (infos().isFullCountEnabled()) {
          stats.incrFullCountBy(didSkip);
        }
      } else if (infos().isFullCountEnabled()) {
        // Skip the remainder, it does not matter if we need to produce
        // anything or not. This is only for reporting of the skipped numbers
        auto const didSkip = inputRange.skipAll();
        _counter += didSkip;
        if (infos().isFullCountEnabled()) {
          stats.incrFullCountBy(didSkip);
        }
      }
    } else if (infos().isFullCountEnabled()) {
      // Skip the remainder, it does not matter if we need to produce
      // anything or not. This is only for reporting of the skipped numbers
      auto const didSkip = inputRange.skipAll();
      _counter += didSkip;
      if (infos().isFullCountEnabled()) {
        stats.incrFullCountBy(didSkip);
      }
    } else {
      // We are done.
      // All produced, all skipped, nothing to report
      // Throw away the leftover skipped-rows from upstream
      std::ignore = inputRange.skipAll();
      break;
    }
  }
  return {inputRange.upstreamState(), stats, call.getSkipCount(),
          calculateUpstreamCall(call)};
}
