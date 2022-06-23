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
#include "RestServer/RocksDBTempStorageFeature.h"
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

class SortExecutorInfos {
 public:
  SortExecutorInfos(RegisterCount nrInputRegisters,
                    RegisterCount nrOutputRegisters,
                    RegIdFlatSet const& registersToClear,
                    std::vector<SortRegister> sortRegisters, std::size_t limit,
                    AqlItemBlockManager& manager,
                    velocypack::Options const* options,
                    arangodb::ResourceMonitor& resourceMonitor, bool stable,
                    RocksDBTempStorageFeature& rocksDBfeature);

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

  [[nodiscard]] RocksDBTempStorageFeature& getStorageFeature() const {
    return _rocksDBfeature;
  }

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
  RocksDBTempStorageFeature& _rocksDBfeature;
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
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */

  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

  /*
  [[nodiscard]] auto expectedNumberOfRowsNew(
      AqlItemBlockInputMatrix const& input, AqlCall const& call) const noexcept
      -> size_t;
      */

 private:
  void doSorting();
  void consumeInput(AqlItemBlockInputRange& inputRange, ExecutorState& state);
  void consumeInputForStorage();
  rocksdb::Status deleteRangeInStorage();

 private:
  static constexpr size_t kMemoryLowerBound = 1024 * 1024;
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
  std::unique_ptr<rocksdb::Iterator> _curIt;
  SharedAqlItemBlockPtr _curBlock = nullptr;
  rocksdb::ColumnFamilyHandle* _cfHandle = nullptr;
  std::uint64_t const _keyPrefix;
  std::string _lowerBoundPrefix;
  rocksdb::WriteBatch _batch;
  std::uint64_t _inputCounterForStorage;
  std::string _upperBoundPrefix;
  std::uint64_t _curEntryId = 0;
  rocksdb::Slice _lowerrBoundSlice;
  rocksdb::Slice _upperBoundSlice;
  std::uint64_t _memoryUsage = 0;
};
}  // namespace aql
}  // namespace arangodb

/*
 * = 0 in comparator
 *  delete in range here also remove files in storage feature
 *  hardcode memory threshold to 100k
 *  more complex queries with arrays and strings >= 16 bytes
 *  test more
 *  see memory usage in top
 */