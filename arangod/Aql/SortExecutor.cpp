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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "SortExecutor.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "Basics/ResourceUsage.h"
#include "RocksDBEngine/RocksDBFormat.h"

#include <rocksdb/options.h>

#include <Logger/LogMacros.h>
#include <algorithm>
#include <string>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

std::atomic<uint64_t> keyPrefixCounter = 0;

// custom AqlValue-aware comparator for sorting
class OurLessThan {
 public:
  OurLessThan(velocypack::Options const* options,
              std::vector<SharedAqlItemBlockPtr> const& input,
              std::vector<SortRegister> const& sortRegisters) noexcept
      : _vpackOptions(options), _input(input), _sortRegisters(sortRegisters) {}

  bool operator()(AqlItemMatrix::RowIndex const& a,
                  AqlItemMatrix::RowIndex const& b) const {
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

}  // namespace

SortExecutorInfos::SortExecutorInfos(
    RegisterCount nrInputRegisters, RegisterCount nrOutputRegisters,
    RegIdFlatSet const& registersToClear,
    std::vector<SortRegister> sortRegisters, std::size_t limit,
    AqlItemBlockManager& manager, velocypack::Options const* options,
    arangodb::ResourceMonitor& resourceMonitor, bool stable,
    RocksDBTempStorageFeature& rocksDBfeature)
    : _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToClear(registersToClear.begin(), registersToClear.end()),
      _limit(limit),
      _manager(manager),
      _vpackOptions(options),
      _resourceMonitor(resourceMonitor),
      _sortRegisters(std::move(sortRegisters)),
      _stable(stable),
      _rocksDBfeature(rocksDBfeature) {
  TRI_ASSERT(!_sortRegisters.empty());
}

RegisterCount SortExecutorInfos::numberOfInputRegisters() const {
  return _numInRegs;
}

RegisterCount SortExecutorInfos::numberOfOutputRegisters() const {
  return _numOutRegs;
}

RegIdFlatSet const& SortExecutorInfos::registersToClear() const {
  return _registersToClear;
}

std::vector<SortRegister> const& SortExecutorInfos::sortRegisters()
    const noexcept {
  return _sortRegisters;
}

bool SortExecutorInfos::stable() const { return _stable; }

velocypack::Options const* SortExecutorInfos::vpackOptions() const noexcept {
  return _vpackOptions;
}

arangodb::ResourceMonitor& SortExecutorInfos::getResourceMonitor() const {
  return _resourceMonitor;
}

AqlItemBlockManager& SortExecutorInfos::itemBlockManager() noexcept {
  return _manager;
}

size_t SortExecutorInfos::limit() const noexcept { return _limit; }

SortExecutor::SortExecutor(Fetcher&, SortExecutorInfos& infos)
    : _infos(infos),
      _input(nullptr),
      _currentRow(CreateInvalidInputRowHint{}),
      _returnNext(0),
      _memoryUsageForRowIndexes(0),
      _keyPrefix(++::keyPrefixCounter) {
  rocksutils::uintToPersistentBigEndian<std::uint64_t>(_upperBoundPrefix,
                                                       _keyPrefix + 1);
  rocksutils::uintToPersistentBigEndian<std::uint64_t>(_lowerBoundPrefix,
                                                       _keyPrefix);
  _lowerrBoundSlice = _lowerBoundPrefix;
  _upperBoundSlice = _upperBoundPrefix;
}

SortExecutor::~SortExecutor() {
  _inputReady = false;
  _mustStoreInput = false;
  _rowIndexes.clear();
  _inputBlocks.clear();
  _infos.getResourceMonitor().decreaseMemoryUsage(_memoryUsageForRowIndexes);
}

void SortExecutor::consumeInputForStorage() {
  auto& rocksDBfeature = _infos.getStorageFeature();
  rocksdb::DB* tempDB = rocksDBfeature.tempDB();
  std::string keyWithPrefix;

  size_t const avgSliceSize = 50;
  keyWithPrefix.reserve(sizeof(std::uint64_t) +
                        _infos.sortRegisters().size() * avgSliceSize);
  for (auto const& [first, second] : _rowIndexes) {
    auto& inputBlock = *_curBlock;
    keyWithPrefix.clear();

    rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyWithPrefix,
                                                         _keyPrefix);
    rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyWithPrefix,
                                                         ++_curEntryId);
    for (auto const& reg : _infos.sortRegisters()) {
      auto inputBlockSlice =
          inputBlock.getValueReference(second, reg.reg).slice();
      auto inputBlockSize = inputBlockSlice.byteSize();
      keyWithPrefix.append(inputBlockSlice.startAs<char const>(),
                           inputBlockSize);
      keyWithPrefix.push_back(reg.asc ? '1' : '0');
    }
    InputAqlItemRow newRow(_curBlock, second);
    velocypack::Builder builder;
    builder.openObject();
    newRow.toVelocyPack(_infos.vpackOptions(), builder);
    builder.close();
    _batch.Put(
        rocksDBfeature.cfHandles()[0], keyWithPrefix,
        {builder.slice().startAs<char const>(), builder.slice().byteSize()});
    ++_inputCounterForStorage;
  }
  rocksdb::Status s = tempDB->Write(rocksdb::WriteOptions(), &_batch);
  _batch.Clear();
  _rowIndexes.clear();
  _inputBlocks.clear();
}

void SortExecutor::consumeInput(AqlItemBlockInputRange& inputRange,
                                ExecutorState& state) {
  size_t memoryUsageForRowIndexes =
      inputRange.countDataRows() * sizeof(AqlItemMatrix::RowIndex);

  ResourceUsageScope guard(_infos.getResourceMonitor(),
                           memoryUsageForRowIndexes);

  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (inputRange.hasDataRow()) {
    // This executor is passthrough. it has enough place to write.
    _rowIndexes.emplace_back(
        std::make_pair(_inputBlocks.size() - 1, inputRange.getRowIndex()));
    if (_memoryUsage >= kMemoryLowerBound) {
      //  if (_rowIndexes.size() >= 3) {  // THIS IS JUST FOR DEBUGGING,
      // MUST BE AROUND 100k OR
      // A THRESHOLD IN BYTES USED IN RAM
      _mustStoreInput = true;
      consumeInputForStorage();
    }
    std::tie(state, input) =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
  }
  if (_mustStoreInput) {
    // consume remainder in _rowIndexes
    consumeInputForStorage();
  }
  guard.steal();
  _memoryUsageForRowIndexes += memoryUsageForRowIndexes;
}

rocksdb::Status SortExecutor::deleteRangeInStorage() {
  return _infos.getStorageFeature().tempDB()->DeleteRange(
      rocksdb::WriteOptions(), _infos.getStorageFeature().cfHandles()[0],
      _lowerrBoundSlice, _upperBoundSlice);
}

std::tuple<ExecutorState, NoStats, AqlCall> SortExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  AqlCall upstreamCall{};

  if (!_inputReady) {
    _curBlock = inputRange.getBlock();
    if (_curBlock != nullptr) {
      _memoryUsage += _curBlock->getMemoryUsage();
      if (!_mustStoreInput) {
        _inputBlocks.emplace_back(_curBlock);
      }
    }
  }

  if ((!_mustStoreInput &&
       (_returnNext >= _rowIndexes.size() && !_rowIndexes.empty())) ||
      (_mustStoreInput &&
       (_returnNext >= _curEntryId &&
        inputRange.upstreamState() == ExecutorState::DONE))) {
    // Bail out if called too often,
    // Bail out on no elements
    if (_mustStoreInput) {
      auto status = deleteRangeInStorage();
      if (!status.ok()) {
        LOG_TOPIC("20e7f", FATAL, arangodb::Logger::STARTUP)
            << "unable to initialize RocksDB engine: " << status.ToString();
      }
    }
    return {ExecutorState::DONE, NoStats{}, upstreamCall};
  }

  ExecutorState state = ExecutorState::HASMORE;

  if (!_inputReady) {
    consumeInput(inputRange, state);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, upstreamCall};
    }
    if (!_mustStoreInput && (_returnNext < _rowIndexes.size())) {
      doSorting();
    }
    _inputReady = true;
    if (_mustStoreInput) {
      rocksdb::ReadOptions readOptions;

      std::string keyPrefix;
      rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyPrefix,
                                                           _keyPrefix);

      readOptions.iterate_upper_bound = &_upperBoundSlice;

      readOptions.prefix_same_as_start = true;

      _curIt = std::unique_ptr<rocksdb::Iterator>(
          _infos.getStorageFeature().tempDB()->NewIterator(
              readOptions, _infos.getStorageFeature().cfHandles()[0]));
      _curIt->Seek(keyPrefix);
    }
  }

  if (!_mustStoreInput) {
    while (_returnNext < _rowIndexes.size() && !output.isFull()) {
      InputAqlItemRow inRow(_inputBlocks[_rowIndexes[_returnNext].first],
                            _rowIndexes[_returnNext].second);
      output.copyRow(inRow);
      output.advanceRow();
      _returnNext++;
    }
  } else {
    for (; _curIt->Valid() && !output.isFull(); _curIt->Next()) {
      std::string key = _curIt->key().ToString();
      auto p1 = _curIt->value().data();
      arangodb::velocypack::Slice slice1(reinterpret_cast<uint8_t const*>(p1));
      auto curBlock = _infos.itemBlockManager().requestAndInitBlock(slice1);
      TRI_ASSERT(curBlock->getRefCount() == 1);
      {
        InputAqlItemRow inRow(curBlock, 0);
        TRI_ASSERT(curBlock->getRefCount() == 2);

        output.copyRow(inRow);
        TRI_ASSERT(curBlock->getRefCount() == 3);
      }
      TRI_ASSERT(curBlock->getRefCount() == 2);

      output.advanceRow();

      _returnNext++;
    }
  }

  if ((!_mustStoreInput && (_returnNext >= _rowIndexes.size())) ||
      (_mustStoreInput && (_returnNext >= _curEntryId))) {
    if (_mustStoreInput) {
      auto status = deleteRangeInStorage();
      if (!status.ok()) {
        LOG_TOPIC("5a64a", FATAL, arangodb::Logger::STARTUP)
            << "unable to initialize RocksDB engine: " << status.ToString();
      }
    }
    state = ExecutorState::DONE;
  }

  return {state, NoStats{}, upstreamCall};
}

void SortExecutor::doSorting() {
  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // comparison function
  OurLessThan ourLessThan(_infos.vpackOptions(), _inputBlocks,
                          _infos.sortRegisters());
  if (_infos.stable()) {
    std::stable_sort(_rowIndexes.begin(), _rowIndexes.end(), ourLessThan);
  } else {
    std::sort(_rowIndexes.begin(), _rowIndexes.end(), ourLessThan);
  }
}

std::tuple<ExecutorState, NoStats, size_t, AqlCall> SortExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  AqlCall upstreamCall{};

  ExecutorState state = ExecutorState::HASMORE;

  if (!_inputReady) {
    _curBlock = inputRange.getBlock();
    if (_curBlock != nullptr) {
      _memoryUsage += _curBlock->getMemoryUsage();
      if (!_mustStoreInput) {
        _inputBlocks.emplace_back(_curBlock);
      }
    }
  }

  if ((!_mustStoreInput &&
       (_returnNext >= _rowIndexes.size() && !_rowIndexes.empty())) ||
      (_mustStoreInput &&
       (_returnNext >= _curEntryId &&
        inputRange.upstreamState() == ExecutorState::DONE))) {
    // Bail out if called too often,
    // Bail out on no elements
    if (_mustStoreInput) {
      auto status = deleteRangeInStorage();
      if (!status.ok()) {
        LOG_TOPIC("c7490", FATAL, arangodb::Logger::STARTUP)
            << "unable to initialize RocksDB engine: " << status.ToString();
      }
    }
    return {ExecutorState::DONE, NoStats{}, 0, upstreamCall};
  }

  if (!_inputReady) {
    consumeInput(inputRange, state);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, 0, upstreamCall};
    }
    if (!_mustStoreInput && (_returnNext < _rowIndexes.size())) {
      doSorting();
    }
    _inputReady = true;
    if (_mustStoreInput) {
      rocksdb::ReadOptions readOptions;

      readOptions.iterate_upper_bound = &_upperBoundSlice;

      std::string keyPrefix;
      rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyPrefix,
                                                           _keyPrefix);
      readOptions.prefix_same_as_start = true;

      _curIt = std::unique_ptr<rocksdb::Iterator>(
          _infos.getStorageFeature().tempDB()->NewIterator(
              readOptions, _infos.getStorageFeature().cfHandles()[0]));
      _curIt->Seek(keyPrefix);
    }
  }

  if (state == ExecutorState::HASMORE) {
    return {state, NoStats{}, 0, upstreamCall};
  }

  if (!_mustStoreInput) {
    while (_returnNext < _rowIndexes.size() && call.shouldSkip()) {
      InputAqlItemRow inRow(_inputBlocks[_rowIndexes[_returnNext].first],
                            _rowIndexes[_returnNext].second);
      _returnNext++;
      call.didSkip(1);
    }
  } else {
    for (; _curIt->Valid() && call.shouldSkip(); _curIt->Next()) {
      _returnNext++;
      call.didSkip(1);
    }
  }

  if ((!_mustStoreInput && (_returnNext >= _rowIndexes.size())) ||
      (_mustStoreInput && (_returnNext >= _curEntryId))) {
    if (_mustStoreInput) {
      auto status = deleteRangeInStorage();
      if (!status.ok()) {
        LOG_TOPIC("43bf7", FATAL, arangodb::Logger::STARTUP)
            << "unable to initialize RocksDB engine: " << status.ToString();
      }
    }
    return {ExecutorState::DONE, NoStats{}, call.getSkipCount(), upstreamCall};
  }
  return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), upstreamCall};
}

/*
[[nodiscard]] auto SortExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputMatrix const& input, AqlCall const& call) const noexcept
    -> size_t {
  size_t rowsAvailable = input.countDataRows();
  if (_input != nullptr) {
    if (_returnNext < _sortedIndexes.size()) {
      TRI_ASSERT(_returnNext <= rowsAvailable);
      // if we have input, we are enumerating rows
      // In a block within the given matrix.
      // Unfortunately there could be more than
      // one full block in the matrix and we do not know
      // in which block we are.
      // So if we are in the first block this will be accurate
      rowsAvailable -= _returnNext;
      // If we are in a later block, we will allocate space
      // again for the first block.
      // Nevertheless this is highly unlikely and
      // only is bad if we sort few elements within highly nested
      // subqueries.
    }
    // else we are in DONE state and not yet reset.
    // We do not exactly now how many rows will be there
  }
  if (input.countShadowRows() == 0) {
    return std::min(call.getLimit(), rowsAvailable);
  }
  return rowsAvailable;
}
 */
