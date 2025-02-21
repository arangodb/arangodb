#include "GroupedSortExecutorBackend.h"

#include "Basics/ResourceUsage.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"
#include "Aql/Executor/GroupedSortExecutor.h"

using namespace arangodb::aql::group_sort;
using namespace arangodb::aql;
using namespace arangodb;

// custom AqlValue-aware comparator for sorting
class OurLessThan {
 public:
  OurLessThan(velocypack::Options const* options,
              std::vector<SharedAqlItemBlockPtr> const& input,
              std::vector<SortRegister> const& sortRegisters) noexcept
      : _vpackOptions(options), _input(input), _sortRegisters(sortRegisters) {}

  bool operator()(StorageBackend::RowIndex const& a,
                  StorageBackend::RowIndex const& b) const {
    auto const& left = _input[a.first].get();
    auto const& right = _input[b.first].get();
    for (auto const& reg : _sortRegisters) {
      AqlValue const& lhs = left->getValueReference(a.second, reg.reg);
      AqlValue const& rhs = right->getValueReference(b.second, reg.reg);
      int const cmp = AqlValue::Compare(_vpackOptions, lhs, rhs, true);

      if (cmp < 0) {
        return reg.asc;
      } else if (cmp > 0) {
        return !reg.asc;
      }
    }

    return false;
  }

 private:
  velocypack::Options const* _vpackOptions;
  std::vector<SharedAqlItemBlockPtr> const& _input;
  std::vector<SortRegister> const& _sortRegisters;
};  // OurLessThan

StorageBackend::StorageBackend(GroupedSortExecutorInfos& infos)
    : _infos{infos} {
  _infos.getResourceMonitor().increaseMemoryUsage(currentMemoryUsage());
}

StorageBackend::~StorageBackend() {
  _infos.getResourceMonitor().decreaseMemoryUsage(currentMemoryUsage());
}

size_t StorageBackend::currentMemoryUsage() const noexcept {
  return (_currentGroup.capacity() + _finishedGroup.capacity()) *
         sizeof(RowIndex);
}

void StorageBackend::consumeInputRange(AqlItemBlockInputRange& inputRange) {
  if (inputRange.upstreamState() == ExecutorState::DONE) {
    if (!_currentGroup.empty()) {
      startNewGroup({});
    }
    return;
  }
  auto firstRow = false;

  auto inputBlock = inputRange.getBlock();
  if (inputBlock != nullptr &&
      (_inputBlocks.empty() || _inputBlocks.back() != inputBlock)) {
    if (_inputBlocks.empty()) {
      firstRow = true;
    }
    _inputBlocks.emplace_back(inputBlock);
  }

  ResourceUsageScope guard(_infos.getResourceMonitor());

  size_t numDataRows = inputRange.countDataRows();
  if (_currentGroup.capacity() < _currentGroup.size() + numDataRows) {
    size_t newCapacity = _currentGroup.size() + numDataRows;

    // may throw
    guard.increase((newCapacity - _currentGroup.capacity()) * sizeof(RowIndex));

    _currentGroup.reserve(newCapacity);
  }

  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  // make sure that previous group was already fully consumed
  TRI_ASSERT(!hasMore());

  while (inputRange.hasDataRow()) {
    auto rowId =
        std::make_pair(static_cast<std::uint32_t>(_inputBlocks.size() - 1),
                       static_cast<std::uint32_t>(inputRange.getRowIndex()));

    auto grouped_values = groupedValuesForRow(rowId);

    if (grouped_values != _groupedValuesOfPreviousRow && !firstRow) {
      // finish group and return
      startNewGroup({rowId});
      _groupedValuesOfPreviousRow = grouped_values;
      auto [state, input] =
          inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
      TRI_ASSERT(input.isInitialized());
      guard.steal();
      return;
    }

    firstRow = false;
    _groupedValuesOfPreviousRow = grouped_values;
    _currentGroup.emplace_back(rowId);

    auto [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());

    if (inputRange.upstreamState() == ExecutorState::DONE) {
      startNewGroup({});
      guard.steal();
      return;
    }
  }

  guard.steal();
}
void StorageBackend::produceOutputRow(OutputAqlItemRow& output) {
  TRI_ASSERT(hasMore());
  InputAqlItemRow inRow(_inputBlocks[_finishedGroup[_returnNext].first],
                        _finishedGroup[_returnNext].second);
  output.copyRow(inRow);
  output.advanceRow();
  ++_returnNext;
}
void StorageBackend::skipOutputRow() noexcept {
  TRI_ASSERT(hasMore());
  ++_returnNext;
}
bool StorageBackend::hasMore() const {
  return _returnNext < _finishedGroup.size();
}

void StorageBackend::startNewGroup(std::vector<RowIndex>&& newGroup) {
  ResourceUsageScope guard(_infos.getResourceMonitor());
  // we overwrite finished group, therefore decrease by its memory usage (before
  // updating it)
  _infos.getResourceMonitor().decreaseMemoryUsage(_finishedGroup.capacity() *
                                                  sizeof(RowIndex));
  _finishedGroup = std::move(_currentGroup);
  sortFinishedGroup();

  _infos.getResourceMonitor().increaseMemoryUsage(newGroup.capacity() *
                                                  sizeof(RowIndex));
  _currentGroup = std::move(newGroup);
  _returnNext = 0;
}
StorageBackend::GroupedValues StorageBackend::groupedValuesForRow(
    RowIndex const& rowId) {
  auto row = _inputBlocks[rowId.first].get();
  std::vector<AqlValue> groupedValues;
  for (auto const& registerId : _infos.groupedRegisters()) {
    groupedValues.push_back(row->getValueReference(rowId.second, registerId));
  }
  return GroupedValues{_infos.vpackOptions(), groupedValues};
}
void StorageBackend::sortFinishedGroup() {
  // comparison function
  OurLessThan ourLessThan(_infos.vpackOptions(), _inputBlocks,
                          _infos.sortRegisters());
  if (_infos.stable()) {
    std::stable_sort(_finishedGroup.begin(), _finishedGroup.end(), ourLessThan);
  } else {
    std::sort(_finishedGroup.begin(), _finishedGroup.end(), ourLessThan);
  }
}

bool StorageBackend::GroupedValues::operator==(
    StorageBackend::GroupedValues const& other) const {
  if (values.size() != other.values.size()) {
    return false;
  }
  for (size_t i = 0; i < values.size(); i++) {
    if (AqlValue::Compare(compare_options, values[i], other.values[i], true) !=
        0) {
      return false;
    }
  }
  return true;
}
