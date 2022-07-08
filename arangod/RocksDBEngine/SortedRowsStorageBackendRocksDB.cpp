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

#include "SortedRowsStorageBackendRocksDB.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBSortedRowsStorageContext.h"
#include "RocksDBEngine/RocksDBTempStorage.h"

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>

namespace arangodb {

SortedRowsStorageBackendRocksDB::SortedRowsStorageBackendRocksDB(
    RocksDBTempStorage& storage, aql::SortExecutorInfos& infos)
    : _tempStorage(storage), _infos(infos), _rowNumberForInsert(0) {}

SortedRowsStorageBackendRocksDB::~SortedRowsStorageBackendRocksDB() {
  try {
    cleanup();
  } catch (...) {
  }
}

aql::ExecutorState SortedRowsStorageBackendRocksDB::consumeInputRange(
    aql::AqlItemBlockInputRange& inputRange) {
  if (_context == nullptr) {
    // create context on the fly
    _context = _tempStorage.getSortedRowsStorageContext();
  }

  TRI_ASSERT(_iterator == nullptr);
  std::string keyWithPrefix;

  size_t const avgSliceSize = 50;
  keyWithPrefix.reserve(sizeof(std::uint64_t) +
                        _infos.sortRegisters().size() * avgSliceSize);

  aql::ExecutorState state = aql::ExecutorState::HASMORE;
  aql::InputAqlItemRow input{aql::CreateInvalidInputRowHint{}};

  auto inputBlock = inputRange.getBlock();

  while (inputRange.hasDataRow()) {
    keyWithPrefix.clear();
    rocksutils::uintToPersistentBigEndian<std::uint64_t>(keyWithPrefix,
                                                         _context->keyPrefix());
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

    aql::InputAqlItemRow newRow(inputBlock, inputRange.getRowIndex());
    velocypack::Builder builder;
    builder.openObject();
    newRow.toVelocyPack(_infos.vpackOptions(), builder);
    builder.close();

    rocksdb::Slice keySlice = keyWithPrefix;
    RocksDBKey rocksDBKey(keySlice);

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

bool SortedRowsStorageBackendRocksDB::hasMore() const {
  TRI_ASSERT(_iterator != nullptr);
  return _iterator->Valid();
}

void SortedRowsStorageBackendRocksDB::produceOutputRow(
    aql::OutputAqlItemRow& output) {
  TRI_ASSERT(hasMore());

  std::string key = _iterator->key().ToString();
  auto p1 = _iterator->value().data();
  velocypack::Slice slice1(reinterpret_cast<uint8_t const*>(p1));

  auto curBlock = _infos.itemBlockManager().requestAndInitBlock(slice1);
  TRI_ASSERT(curBlock->getRefCount() == 1);
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

void SortedRowsStorageBackendRocksDB::cleanup() {
  _iterator.reset();
  if (_context == nullptr) {
    return;
  }

  _context->cleanup();
}

}  // namespace arangodb
