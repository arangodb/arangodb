////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
#define ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H

#include "Aql/ExecutionStats.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"

// We cannot forward-declare nested classes, but we need IResearchView::Snapshot.
#include "IResearch/IResearchView.h"

namespace arangodb {
namespace aql {

template <bool>
class SingleRowFetcher;

class IResearchViewExecutorInfos : public ExecutorInfos {
 public:
  explicit IResearchViewExecutorInfos(ExecutorInfos&& infos,
                                      std::shared_ptr<iresearch::IResearchView::Snapshot const> reader,
                                      RegisterId outputRegister);

 private:
  RegisterId _outputRegister;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> _reader;
};

class IResearchViewStats {
 public:
  IResearchViewStats() noexcept : _scannedIndex(0) {}

  void incrScanned() noexcept { _scannedIndex++; }
  void incrScanned(size_t value) noexcept {
    _scannedIndex = _scannedIndex + value;
  }

  std::size_t getScanned() const noexcept { return _scannedIndex; }

 private:
  std::size_t _scannedIndex;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  IResearchViewStats const& iResearchViewStats) noexcept {
  executionStats.scannedIndex += iResearchViewStats.getScanned();
  return executionStats;
}

template <bool ordered>
class IResearchViewExecutor {
 public:
  struct Properties {
    // even with "ordered = true", this block preserves the order; it just
    // writes scorer information in additional register for a following sort
    // block to use.
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = IResearchViewExecutorInfos;
  using Stats = IResearchViewStats;

  IResearchViewExecutor() = delete;
  IResearchViewExecutor(IResearchViewExecutor&&) noexcept = default;
  IResearchViewExecutor(IResearchViewExecutor const&) = delete;
  IResearchViewExecutor(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
