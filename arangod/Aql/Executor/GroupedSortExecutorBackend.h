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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class AqlItemBlockInputRange;
class OutputAqlItemRow;
struct GroupedSortExecutorInfos;

namespace group_sort {

struct StorageBackend {
 public:
  StorageBackend(GroupedSortExecutorInfos& infos);
  ~StorageBackend();
  void consumeInputRange(AqlItemBlockInputRange& inputRange);
  void produceOutputRow(OutputAqlItemRow& output);
  void skipOutputRow() noexcept;
  bool hasMore() const;
  using RowIndex = std::pair<uint32_t, uint32_t>;

 private:
  /**
     Values of all grouped registers in a row
  */
  struct GroupedValues {
    velocypack::Options const* compare_options;
    std::vector<AqlValue> values;
    bool operator==(GroupedValues const& other) const;
  };

  void sortFinishedGroup();
  GroupedValues groupedValuesForRow(RowIndex const& rowId);
  void startNewGroup(std::vector<RowIndex>&& newGroup);
  size_t currentMemoryUsage() const noexcept;

  GroupedSortExecutorInfos& _infos;
  std::vector<SharedAqlItemBlockPtr> _inputBlocks = {};
  std::vector<RowIndex> _currentGroup = {};
  std::vector<RowIndex> _finishedGroup = {};
  GroupedValues _groupedValuesOfPreviousRow = {};
  size_t _returnNext = 0;
};

}  // namespace group_sort
}  // namespace arangodb::aql
