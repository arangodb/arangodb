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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlItemMatrix.h"
#include "Aql/SortedRowsStorageBackend.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/SortExecutor.h"

#include <cstddef>
#include <vector>

namespace arangodb::aql {
class SortExecutorInfos;

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

 private:
  void doSorting();
  size_t currentMemoryUsage() const noexcept;

  SortExecutorInfos& _infos;

  std::vector<SharedAqlItemBlockPtr> _inputBlocks;
  std::vector<AqlItemMatrix::RowIndex> _rowIndexes;

  size_t _returnNext;
  size_t _memoryUsageForInputBlocks;
  bool _sealed;
};

}  // namespace arangodb::aql
