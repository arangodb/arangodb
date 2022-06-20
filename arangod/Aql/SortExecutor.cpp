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
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "Basics/ResourceUsage.h"
#include "RestServer/RocksDBTempStorageFeature.h"

#include <rocksdb/options.h>

#include <Logger/LogMacros.h>
#include <algorithm>
#include <string>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
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
      LOG_DEVEL << "reg id " << reg.reg.toUInt32();
      AqlValue const& lhs = left->getValueReference(a.second, reg.reg);
      AqlValue const& rhs = right->getValueReference(b.second, reg.reg);
      int const cmp = AqlValue::Compare(_vpackOptions, lhs, rhs, true);
      LOG_DEVEL << "comparing " << lhs.slice().toString() << " "
                << rhs.slice().toString();
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
    arangodb::ResourceMonitor& resourceMonitor, bool stable)
    : _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToClear(registersToClear.begin(), registersToClear.end()),
      _limit(limit),
      _manager(manager),
      _vpackOptions(options),
      _resourceMonitor(resourceMonitor),
      _sortRegisters(std::move(sortRegisters)),
      _stable(stable) {
  TRI_ASSERT(!_sortRegisters.empty());
  for (auto const& reg : _sortRegisters) {
    LOG_DEVEL << "ASC " << reg.asc;
  }
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
      _comp(std::make_unique<TwoPartComparator>(_infos.vpackOptions())) {}

SortExecutor::~SortExecutor() {
  _inputReady = false;
  _mustStoreInput = false;
  _rowIndexes.clear();
  _inputBlocks.clear();
  _infos.getResourceMonitor().decreaseMemoryUsage(_memoryUsageForRowIndexes);
}

void SortExecutor::consumeInput(AqlItemBlockInputRange& inputRange,
                                ExecutorState& state, ExecutionEngine* engine) {
  size_t memoryUsageForRowIndexes =
      inputRange.countDataRows() * sizeof(AqlItemMatrix::RowIndex);

  ResourceUsageScope guard(_infos.getResourceMonitor(),
                           memoryUsageForRowIndexes);

  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (inputRange.hasDataRow()) {
    // This executor is passthrough. it has enough place to write.

    if (_rowIndexes.size() >= 3) {  // THIS IS JUST FOR DEBUGGING,
                                    // MUST BE AROUND 100k OR
                                    // A THRESHOLD IN BYTES USED IN RAM
      _mustStoreInput = true;

      auto& rocksDBfeature = engine->getQuery()
                                 .vocbase()
                                 .server()
                                 .getFeature<RocksDBTempStorageFeature>();
      if (!_rowIndexes.empty()) {
        rocksdb::DB* tempDB = rocksDBfeature.tempDB();
        rocksdb::Options options = rocksDBfeature.tempDBOptions();
        rocksdb::WriteOptions writeOptions;
        rocksdb::ColumnFamilyOptions cfOptions;
        cfOptions.comparator = _comp.get();
        tempDB->CreateColumnFamily(cfOptions, "SortCF", &_cfHandle);
        //   writeOptions.comparator = &_comp;
        _curIt = tempDB->NewIterator(rocksdb::ReadOptions());
        for (auto const& [first, second] : _rowIndexes) {
          for (auto const& reg : _infos.sortRegisters()) {
            InputAqlItemRow newRow(_curBlock, second);
            velocypack::Builder builder;
            newRow.toVelocyPack(_infos.vpackOptions(), builder);
            tempDB->Put(writeOptions, _cfHandle,
                        _inputBlocks[first]
                            .get()
                            ->getValueReference(second, reg.reg)
                            .slice()
                            .toString(),
                        builder.toString());
          }
        }
        _rowIndexes.clear();
      }
    } else {
      _rowIndexes.emplace_back(
          std::make_pair(_inputBlocks.size() - 1, inputRange.getRowIndex()));
    }
    std::tie(state, input) =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
  }
  guard.steal();
  _memoryUsageForRowIndexes += memoryUsageForRowIndexes;
}

std::tuple<ExecutorState, NoStats, AqlCall> SortExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output,
    ExecutionEngine* engine) {
  AqlCall upstreamCall{};
  if (!_inputReady) {
    _curBlock = inputRange.getBlock();
    if (_curBlock != nullptr && !_mustStoreInput) {
      _inputBlocks.emplace_back(_curBlock);
    }
  }

  if ((!_mustStoreInput &&
       (_returnNext >= _rowIndexes.size() && !_rowIndexes.empty())) ||
      (_curIt != nullptr && !_curIt->Valid())) {
    // Bail out if called too often,
    // Bail out on no elements
    return {ExecutorState::DONE, NoStats{}, upstreamCall};
  }

  ExecutorState state = ExecutorState::HASMORE;

  if (!_inputReady) {
    consumeInput(inputRange, state, engine);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, upstreamCall};
    }
    if (!_mustStoreInput && (_returnNext < _rowIndexes.size())) {
      doSorting();
      //  _inputReady = true;
    }
    _inputReady = true;
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
    // iterate through rocksdb storage, but how to build a row from key and
    // value
  }

  if ((!_mustStoreInput && (_returnNext >= _rowIndexes.size())) ||
      !_curIt->Valid()) {
    //  _inputReady = false;
    // _inputBlocks.clear();
    // _rowIndexes.clear();
    state = ExecutorState::DONE;
  } else {
    state = ExecutorState::HASMORE;
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
    AqlItemBlockInputRange& inputRange, AqlCall& call,
    ExecutionEngine* engine) {
  AqlCall upstreamCall{};

  ExecutorState state = ExecutorState::HASMORE;

  if (!_inputReady) {
    auto inputBlock = inputRange.getBlock();
    if (inputBlock != nullptr) {
      _inputBlocks.emplace_back(inputBlock);
    }
    consumeInput(inputRange, state, engine);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, 0, upstreamCall};
    }
    if (_returnNext < _rowIndexes.size()) {
      doSorting();
      _inputReady = true;
    }
  }

  if (state == ExecutorState::HASMORE) {
    return {state, NoStats{}, 0, upstreamCall};
  }

  if (_returnNext >= _rowIndexes.size() && !_rowIndexes.empty()) {
    // Bail out if called too often,
    // Bail out on no elements
    return {ExecutorState::DONE, NoStats{}, 0, upstreamCall};
  }

  while (_returnNext < _rowIndexes.size() && call.shouldSkip()) {
    InputAqlItemRow inRow(_inputBlocks[_rowIndexes[_returnNext].first],
                          _rowIndexes[_returnNext].second);
    _returnNext++;
    call.didSkip(1);
  }

  if (_returnNext >= _rowIndexes.size()) {
    //  _inputReady = false;
    //  _rowIndexes.clear();
    //  _inputBlocks.clear();
    return {ExecutorState::DONE, NoStats{}, call.getSkipCount(), upstreamCall};
  }
  return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), upstreamCall};

  //  throw std::exception();
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
