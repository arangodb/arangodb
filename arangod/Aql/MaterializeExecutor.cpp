////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "MaterializeExecutor.h"

#include "Aql/QueryContext.h"
#include "Aql/Stats.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "StorageEngine/PhysicalCollection.h"
#include "IResearch/IResearchReadUtils.h"
#include <formats/formats.hpp>
#include <index/index_reader.hpp>
#include <search/boolean_filter.hpp>

namespace arangodb::aql {

template<typename T, bool localDocumentId>
MaterializeExecutor<T, localDocumentId>::ReadContext::ReadContext(Infos& infos)
    : infos{&infos} {
  if constexpr (localDocumentId) {
    callback = [this](LocalDocumentId /*id*/, VPackSlice doc) {
      TRI_ASSERT(this->infos);
      TRI_ASSERT(outputRow);
      TRI_ASSERT(inputRow);
      TRI_ASSERT(inputRow->isInitialized());
      outputRow->moveValueInto(this->infos->outputMaterializedDocumentRegId(),
                               *inputRow, doc);
      return true;
    };
  }
}

template<typename T, bool localDocumentId>
void MaterializeExecutor<T, localDocumentId>::ReadContext::ReadContext::
    moveInto(std::unique_ptr<uint8_t[]> data) {
  TRI_ASSERT(infos);
  TRI_ASSERT(outputRow);
  TRI_ASSERT(inputRow);
  TRI_ASSERT(inputRow->isInitialized());
  AqlValue value{std::move(data)};
  bool mustDestroy = true;
  AqlValueGuard guard{value, mustDestroy};
  // TODO(MBkkt) add moveValueInto overload for std::unique_ptr<uint8_t[]>
  outputRow->moveValueInto(infos->outputMaterializedDocumentRegId(), *inputRow,
                           guard);
}

template<typename T, bool localDocumentId>
MaterializeExecutor<T, localDocumentId>::MaterializeExecutor(
    MaterializeExecutor<T, localDocumentId>::Fetcher& /*fetcher*/, Infos& infos)
    : _buffer{infos.query().resourceMonitor()},
      _trx{infos.query().newTrxContext()},
      _readCtx{infos} {}

template<typename T, bool localDocumentId>
void MaterializeExecutor<T, localDocumentId>::Buffer::fill(
    AqlItemBlockInputRange& inputRange, ReadContext& ctx) {
  TRI_ASSERT(!localDocumentId);
  docs.clear();

  auto const block = inputRange.getBlock();
  if (ADB_UNLIKELY(block == nullptr)) {
    return;
  }

  auto const numRows = block->numRows();
  auto const numDataRows = numRows - block->numShadowRows();
  if (ADB_UNLIKELY(!numDataRows)) {
    return;
  }

  auto const tracked = scope.tracked();
  auto const required = numDataRows * (sizeof(Record) + sizeof(void*));
  if (required > tracked) {
    scope.increase(required - tracked);
  }

  docs.reserve(numDataRows);
  irs::ResolveBool(numRows != numDataRows, [&]<bool HasShadowRows>() {
    auto searchDocRegId = ctx.infos->inputNonMaterializedDocRegId();
    for (size_t i = 0; i != numRows; ++i) {
      if constexpr (HasShadowRows) {
        if (block->isShadowRow(i)) {
          continue;
        }
      }
      auto const buf = block->getValueReference(i, searchDocRegId).slice();
      docs.emplace_back(iresearch::SearchDoc::decode(buf.stringView()));
    }
  });

  order.resize(numDataRows);
  for (auto* data = order.data(); auto& doc : docs) {
    *data++ = &doc;
  }
  std::sort(order.begin(), order.end(), [](const auto* lhs, const auto* rhs) {
    auto* lhs_segment = lhs->segment;
    auto* rhs_segment = rhs->segment;
    if (lhs_segment != rhs_segment) {
      return lhs_segment < rhs_segment;
    }
    return lhs->search < rhs->search;
  });

  iresearch::ViewSegment const* lastSegment{};
  irs::doc_iterator::ptr pkColumn;
  irs::payload const* pkPayload{};
  for (auto* doc : order) {
    auto const search = doc->search;
    doc->storage = {};
    if (lastSegment != doc->segment) {
      pkColumn = iresearch::pkColumn(*doc->segment->segment);
      if (ADB_UNLIKELY(!pkColumn)) {
        continue;  // TODO(MBkkt) assert?
      }
      pkPayload = irs::get<irs::payload>(*pkColumn);
      TRI_ASSERT(pkPayload);
      lastSegment = doc->segment;
    }
    if (ADB_UNLIKELY(pkColumn->seek(search) != search)) {
      continue;  // TODO(MBkkt) assert?
    }
    TRI_ASSERT(pkPayload);
    iresearch::DocumentPrimaryKey::read(doc->storage, pkPayload->value);
  }
}

template<typename T, bool localDocumentId>
std::tuple<ExecutorState, MaterializeStats, AqlCall>
MaterializeExecutor<T, localDocumentId>::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  MaterializeStats stats;

  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;
  if constexpr (localDocumentId) {
    if (!_collection) {
      _collection = _trx.documentCollection(_readCtx.infos->collectionSource())
                        ->getPhysical();
      TRI_ASSERT(_collection);
    }
  } else {
    // buffering all LocalDocumentIds to avoid memory ping-pong
    // between iresearch and storage engine
    _buffer.fill(inputRange, _readCtx);
    auto it = _buffer.order.begin();
    auto end = _buffer.order.end();
    // We cannot call MultiGet from different snapshots
    std::sort(it, end, [](const auto& lhs, const auto& rhs) {
      return lhs->segment->snapshot < rhs->segment->snapshot;
    });
    StorageSnapshot const* lastSnapshot{};
    auto fillKeys = [&] {
      if (it == end) {
        return MultiGetState::kLast;
      }
      auto const* snapshot = (*it)->segment->snapshot;
      if (lastSnapshot != snapshot) {
        lastSnapshot = snapshot;
        return MultiGetState::kWork;
      }
      _getCtx.snapshot = snapshot;

      auto doc = it++;
      auto const storage = (*doc)->storage;
      if (ADB_UNLIKELY(!storage.isSet())) {
        (*doc)->executor = std::numeric_limits<size_t>::max();
        return MultiGetState::kNext;
      }

      auto& logical = *(*doc)->segment->collection;
      (*doc)->executor = _getCtx.serialize(_trx, logical, storage);

      return MultiGetState::kNext;
    };
    _getCtx.multiGet(_buffer.order.size(), fillKeys);
  }
  auto [it, end] = [&] {
    if constexpr (localDocumentId) {
      return std::pair{nullptr, nullptr};
    } else {
      return std::pair{_buffer.docs.begin(), _buffer.docs.end()};
    }
  }();
  auto& callback = _readCtx.callback;
  auto docRegId = _readCtx.infos->inputNonMaterializedDocRegId();
  while (inputRange.hasDataRow() && !output.isFull()) {
    bool written = false;
    auto const [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    _readCtx.inputRow = &input;
    _readCtx.outputRow = &output;

    TRI_IF_FAILURE("MaterializeExecutor::all_fail_and_count") {
      stats.incrFiltered();
      continue;
    }

    TRI_IF_FAILURE("MaterializeExecutor::all_fail") {
      continue;  //
    }

    TRI_IF_FAILURE("MaterializeExecutor::only_one") {
      if (output.numRowsWritten() > 0) {
        continue;
      }
    }

    if constexpr (localDocumentId) {
      static_assert(isSingleCollection);
      LocalDocumentId id{input.getValue(docRegId).slice().getUInt()};
      written = _collection->read(&_trx, id, callback, ReadOwnWrites::no).ok();
    } else if (it != end) {
      if (it->executor != kInvalidRecord) {
        if (auto& value = _getCtx.values[it->executor]; value) {
          _readCtx.moveInto(std::move(value));
          written = true;
        }
      }
      ++it;
    }

    if (written) {
      output.advanceRow();  // document found
    } else {
      stats.incrFiltered();  // document not found
    }
  }

  return {inputRange.upstreamState(), stats, upstreamCall};
}

template<typename T, bool localDocumentId>
std::tuple<ExecutorState, MaterializeStats, size_t, AqlCall>
MaterializeExecutor<T, localDocumentId>::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  size_t skipped = 0;

  // hasDataRow may only occur during fullCount due to previous overfetching
  TRI_ASSERT(!inputRange.hasDataRow() || call.getOffset() == 0);

  if (call.getOffset() > 0) {
    // we can only account for offset
    skipped += inputRange.skip(call.getOffset());
  } else {
    skipped += inputRange.countAndSkipAllRemainingDataRows();

    skipped += inputRange.skipAll();
  }
  call.didSkip(skipped);

  return {inputRange.upstreamState(), MaterializeStats{}, skipped, call};
}

template class MaterializeExecutor<void, false>;
template class MaterializeExecutor<std::string const&, false>;
template class MaterializeExecutor<std::string const&, true>;

template class MaterializerExecutorInfos<void>;
template class MaterializerExecutorInfos<std::string const&>;

}  // namespace arangodb::aql
