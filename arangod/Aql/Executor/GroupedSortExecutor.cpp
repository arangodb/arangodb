#include "GroupedSortExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/Executor/GroupedSortExecutorBackend.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"

using namespace arangodb::aql;

GroupedSortExecutorInfos::GroupedSortExecutorInfos(
    std::vector<SortRegister> sortRegisters,
    std::vector<RegisterId> groupedRegisters, bool stable,
    velocypack::Options const* vpackOptions, ResourceMonitor& resourceMonitor)
    : _sortRegisters{std::move(sortRegisters)},
      _groupedRegisters{std::move(groupedRegisters)},
      _stable{stable},
      _vpackOptions{vpackOptions},
      _resourceMonitor{resourceMonitor} {}

GroupedSortExecutor::GroupedSortExecutor(Fetcher&, Infos& infos)
    : _storageBackend{group_sort::StorageBackend(infos)} {}

std::tuple<ExecutorState, NoStats, AqlCall> GroupedSortExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  AqlCall upstreamCall{};

  // fill with rest from before
  while (!output.isFull() && _storageBackend.hasMore()) {
    _storageBackend.produceOutputRow(output);
  }

  while (!output.isFull()) {
    // return if group is finished or input is finished
    _storageBackend.consumeInputRange(inputRange);
    if (!_storageBackend.hasMore()) {
      return {inputRange.upstreamState(), NoStats{}, std::move(upstreamCall)};
    }
    while (!output.isFull() && _storageBackend.hasMore()) {
      _storageBackend.produceOutputRow(output);
    }
  }

  if (_storageBackend.hasMore()) {
    return {ExecutorState::HASMORE, NoStats{}, std::move(upstreamCall)};
  }
  return {inputRange.upstreamState(), NoStats{}, std::move(upstreamCall)};
}

std::tuple<ExecutorState, NoStats, size_t, AqlCall>
GroupedSortExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                   AqlCall& call) {
  AqlCall upstreamCall{};

  // fill with rest from before
  while (call.needSkipMore() && _storageBackend.hasMore()) {
    _storageBackend.skipOutputRow();
    call.didSkip(1);
  }

  while (call.needSkipMore()) {
    // return if group is finished or input is finished
    _storageBackend.consumeInputRange(inputRange);
    if (!_storageBackend.hasMore()) {
      return {inputRange.upstreamState(), NoStats{}, call.getSkipCount(),
              std::move(upstreamCall)};
    }
    while (call.needSkipMore() && _storageBackend.hasMore()) {
      _storageBackend.skipOutputRow();
      call.didSkip(1);
    }
  }

  if (_storageBackend.hasMore()) {
    return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(),
            std::move(upstreamCall)};
  }
  return {inputRange.upstreamState(), NoStats{}, call.getSkipCount(),
          std::move(upstreamCall)};
}

namespace arangodb::aql {
template class ExecutionBlockImpl<GroupedSortExecutor>;
}
