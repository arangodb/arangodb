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

#include "SortedRowsStorageBackendRocksDB.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutionState.h"
#include "Aql/Executor/SortExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/SortRegister.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBSortedRowsStorageContext.h"
#include "RocksDBEngine/RocksDBTempStorage.h"

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>

namespace arangodb {

SortedRowsStorageBackendRocksDB::SortedRowsStorageBackendRocksDB(
    RocksDBTempStorage& storage, aql::SortExecutorInfos& infos)
    : _tempStorage(storage),
      _infos(infos),
      _rowNumberForInsert(0),
      _memoryTracker(nullptr, nullptr,
                     RocksDBMethodsMemoryTracker::kDefaultGranularity) {
  _memoryTracker.beginQuery(_infos.getQuery().resourceMonitorAsSharedPtr());
}

SortedRowsStorageBackendRocksDB::~SortedRowsStorageBackendRocksDB() {
  _memoryTracker.endQuery();

  try {
    cleanup();
  } catch (...) {
  }
}

aql::ExecutorState SortedRowsStorageBackendRocksDB::consumeInputRange(
    aql::AqlItemBlockInputRange& inputRange) {
  if (_context == nullptr) {
    // create context on the fly
    _context = _tempStorage.getSortedRowsStorageContext(_memoryTracker);
  }

  TRI_ASSERT(_iterator == nullptr);

  size_t const avgSliceSize = 50;
  // string that will be recycled for every key we build
  std::string keyWithPrefix;
  keyWithPrefix.reserve(sizeof(std::uint64_t) +
                        _infos.sortRegisters().size() * avgSliceSize);

  // RocksDBKey instance that will be recycled for every key
  // we build
  RocksDBKey rocksDBKey;

  aql::ExecutorState state = aql::ExecutorState::HASMORE;
  aql::InputAqlItemRow input{aql::CreateInvalidInputRowHint{}};
  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);

  auto inputBlock = inputRange.getBlock();

  while (inputRange.hasDataRow()) {
    // build key for insertion
    keyWithPrefix.clear();
    // append our own 8 byte prefix
    rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyWithPrefix,
                                                         _context->keyPrefix());
    // append per-row number (used only for stable sorts)
    rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyWithPrefix,
                                                         ++_rowNumberForInsert);

    for (auto const& reg : _infos.sortRegisters()) {
      auto inputBlockSlice =
          inputBlock->getValueReference(inputRange.getRowIndex(), reg.reg)
              .slice();
      auto inputBlockSize = inputBlockSlice.byteSize();
      keyWithPrefix.append(inputBlockSlice.startAs<char const>(),
                           inputBlockSize);
      keyWithPrefix.push_back(reg.asc ? '1' : '0');
    }

    rocksDBKey.constructFromBuffer(keyWithPrefix);

    // build value for insertion
    builder.clear();

    // TODO: this is very inefficient. improve it
    builder.openObject();
    inputBlock->toVelocyPack(inputRange.getRowIndex(),
                             inputRange.getRowIndex() + 1,
                             _infos.vpackOptions(), builder);
    builder.close();

    auto res = _context->storeRow(rocksDBKey, builder.slice());

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    std::tie(state, input) =
        inputRange.nextDataRow(aql::AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
  }

  return state;
}

bool SortedRowsStorageBackendRocksDB::hasReachedCapacityLimit() const noexcept {
  return _context->hasReachedMaxCapacity();
  // TODO: honor capacity setting from TemporaryStorageFeature
}

bool SortedRowsStorageBackendRocksDB::hasMore() const {
  TRI_ASSERT(_iterator != nullptr);
  return _iterator->Valid();
}

void SortedRowsStorageBackendRocksDB::produceOutputRow(
    aql::OutputAqlItemRow& output) {
  TRI_ASSERT(hasMore());

  velocypack::Slice slice(
      reinterpret_cast<uint8_t const*>(_iterator->value().data()));

  // TODO: improve the efficiency of this
  auto curBlock = _infos.itemBlockManager().requestAndInitBlock(slice);
  {
    aql::InputAqlItemRow inRow(curBlock, 0);
    output.copyRow(inRow);
  }

  output.advanceRow();

  _iterator->Next();
}

void SortedRowsStorageBackendRocksDB::skipOutputRow() noexcept {
  TRI_ASSERT(hasMore());
  _iterator->Next();
}

void SortedRowsStorageBackendRocksDB::seal() {
  TRI_ASSERT(_iterator == nullptr);

  _context->ingestAll();

  _iterator = _context->getIterator();
}

void SortedRowsStorageBackendRocksDB::spillOver(
    SortedRowsStorageBackend& other) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "unexpected call to SortedRowsStorageBackendRocksDB::spillOver");
}

void SortedRowsStorageBackendRocksDB::cleanup() {
  _iterator.reset();
  if (_context == nullptr) {
    return;
  }

  _context->cleanup();
}

}  // namespace arangodb
