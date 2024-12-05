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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Executor/SortExecutor.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/SortedRowsStorageBackend.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace arangodb::aql {
class SortExecutorInfos;

/**
   Values of all grouped registers in a row
 */
struct GroupedValues {
  velocypack::Options const* compare_options;
  std::vector<AqlValue> values;
  bool operator==(GroupedValues const& other) const {
    if (values.size() != other.values.size()) {
      return false;
    }
    for (size_t i = 0; i < values.size(); i++) {
      if (AqlValue::Compare(compare_options, values[i], other.values[i],
                            true) != 0) {
        return false;
      }
    }
    return true;
  }
};

class SortedRowsStorageBackendMemory final : public SortedRowsStorageBackend {
 public:
  explicit SortedRowsStorageBackendMemory(SortExecutorInfos& infos);
  ~SortedRowsStorageBackendMemory();

  ExecutorState consumeInputRange(AqlItemBlockInputRange& inputRange) final;

  bool hasReachedCapacityLimit() const noexcept final;

  bool hasMore() const final;
  void produceOutputRow(OutputAqlItemRow& output) final;
  void skipOutputRow() noexcept final;
  void seal() final;
  void spillOver(SortedRowsStorageBackend& other) final;

  // uint32_t in this vector is a reasonable trade-off between performance and
  // amount of data. With this values we can sort up to ~ 4.000.000.000 times
  // 1000 elements in memory. Anything beyond that has a questionable runtime on
  // nowadays hardware anyways.
  using RowIndex = std::pair<uint32_t, uint32_t>;

 private:
  void sortFinishedGroup();
  size_t currentMemoryUsage() const noexcept;
  GroupedValues groupedValuesForRow(RowIndex const& rowId);
  void updateFinishedGroup();

  SortExecutorInfos& _infos;

  std::vector<SharedAqlItemBlockPtr> _inputBlocks;
  std::vector<RowIndex> _currentGroup = {};
  std::vector<RowIndex> _finishedGroup = {};
  GroupedValues _groupedValuesOfPreviousRow = {};

  size_t _returnNext = 0;
  size_t _memoryUsageForInputBlocks;
};

}  // namespace arangodb::aql
