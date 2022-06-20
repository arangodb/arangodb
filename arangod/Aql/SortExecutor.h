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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include <rocksdb/comparator.h>
#include <rocksdb/db.h>
#include <rocksdb/iterator.h>

#include <cstddef>
#include <memory>

#include <Logger/LogMacros.h>

namespace arangodb {
struct ResourceMonitor;

namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class AqlItemBlockManager;
class RegisterInfos;
class NoStats;
class OutputAqlItemRow;
template<BlockPassthrough>
class SingleRowFetcher;
struct SortRegister;

class TwoPartComparator : public rocksdb::Comparator {
 public:
  TwoPartComparator(arangodb::velocypack::Options const* options)
      : _options(options) {}
  int Compare(const rocksdb::Slice& lhs,
              const rocksdb::Slice& rhs) const override {
    /*
    velocypack::Builder builder1;
    builder1.add(lhs.data());
    velocypack::Builder builder2;
    builder2.add(rhs.data());
    VPackSlice const lSlice = VPackSlice(builder1.slice());
    VPackSlice const rSlice = VPackSlice(builder2.slice());
     */
    AqlValue aqlValue1(lhs.ToString());
    AqlValue aqlValue2(lhs.ToString());
    int const cmp =
        arangodb::aql::AqlValue::Compare(_options, aqlValue1, aqlValue2, true);
    LOG_DEVEL << "comparing " << lhs.ToString() << " " << rhs.ToString();
    if (cmp < 0) {
      return true;  // actually whether it's ascending or descending
    } else if (cmp > 0) {
      return false;  // actually the opposite from above
    }
    return false;
  }

  // Ignore the following methods for now:
  const char* Name() const override { return "TwoPartComparator"; }
  void FindShortestSeparator(std::string*,
                             const rocksdb::Slice&) const override {}
  void FindShortSuccessor(std::string*) const override {}

 private:
  arangodb::velocypack::Options const* _options;
};

class SortExecutorInfos {
 public:
  SortExecutorInfos(RegisterCount nrInputRegisters,
                    RegisterCount nrOutputRegisters,
                    RegIdFlatSet const& registersToClear,
                    std::vector<SortRegister> sortRegisters, std::size_t limit,
                    AqlItemBlockManager& manager,
                    velocypack::Options const* options,
                    arangodb::ResourceMonitor& resourceMonitor, bool stable);

  SortExecutorInfos() = delete;
  SortExecutorInfos(SortExecutorInfos&&) = default;
  SortExecutorInfos(SortExecutorInfos const&) = delete;
  ~SortExecutorInfos() = default;

  [[nodiscard]] RegisterCount numberOfInputRegisters() const;

  [[nodiscard]] RegisterCount numberOfOutputRegisters() const;

  [[nodiscard]] RegIdFlatSet const& registersToClear() const;

  [[nodiscard]] velocypack::Options const* vpackOptions() const noexcept;

  [[nodiscard]] std::vector<SortRegister> const& sortRegisters() const noexcept;

  [[nodiscard]] arangodb::ResourceMonitor& getResourceMonitor() const;

  [[nodiscard]] bool stable() const;

  [[nodiscard]] size_t limit() const noexcept;

  [[nodiscard]] AqlItemBlockManager& itemBlockManager() noexcept;

 private:
  RegisterCount _numInRegs;
  RegisterCount _numOutRegs;
  RegIdFlatSet _registersToClear;
  std::size_t _limit;
  AqlItemBlockManager& _manager;
  velocypack::Options const* _vpackOptions;
  arangodb::ResourceMonitor& _resourceMonitor;
  std::vector<SortRegister> _sortRegisters;
  bool _stable;
};

/**
 * @brief Implementation of Sort Node
 */
class SortExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SortExecutorInfos;
  using Stats = NoStats;

  SortExecutor(Fetcher&, Infos& infos);
  ~SortExecutor();

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output,
      ExecutionEngine* engine = nullptr);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */

  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call,
      ExecutionEngine* engine);

  /*
  [[nodiscard]] auto expectedNumberOfRowsNew(
      AqlItemBlockInputMatrix const& input, AqlCall const& call) const noexcept
      -> size_t;
      */

 private:
  void doSorting();
  void consumeInput(AqlItemBlockInputRange& inputRange, ExecutorState& state,
                    ExecutionEngine* engine);

 private:
  bool _inputReady = false;
  bool _mustStoreInput = false;
  Infos& _infos;

  AqlItemMatrix const* _input;
  InputAqlItemRow _currentRow;

  std::vector<AqlItemMatrix::RowIndex> _sortedIndexes;

  size_t _returnNext;

  size_t _memoryUsageForRowIndexes;
  std::vector<SharedAqlItemBlockPtr> _inputBlocks;
  std::vector<AqlItemMatrix::RowIndex> _rowIndexes;
  std::unique_ptr<TwoPartComparator> _comp;
  rocksdb::Iterator* _curIt = nullptr;
  SharedAqlItemBlockPtr _curBlock = nullptr;
  rocksdb::ColumnFamilyHandle* _cfHandle = nullptr;
};
}  // namespace aql
}  // namespace arangodb
