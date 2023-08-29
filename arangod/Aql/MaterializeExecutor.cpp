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
#include "StorageEngine/PhysicalCollection.h"
#include "IResearch/IResearchReadUtils.h"
#include <formats/formats.hpp>
#include <index/index_reader.hpp>
#include <search/boolean_filter.hpp>

namespace arangodb::aql {

template<typename T, bool localDocumentId>
MaterializeExecutor<T, localDocumentId>::ReadContext::ReadContext(Infos& infos)
    : _infos{&infos}, _callback{[this](LocalDocumentId /*id*/, VPackSlice doc) {
        TRI_ASSERT(_outputRow);
        TRI_ASSERT(_inputRow);
        TRI_ASSERT(_inputRow->isInitialized());
        TRI_ASSERT(_infos);
        AqlValue a{AqlValueHintSliceCopy(doc)};
        bool mustDestroy = true;
        AqlValueGuard guard{a, mustDestroy};
        _outputRow->moveValueInto(_infos->outputMaterializedDocumentRegId(),
                                  *_inputRow, guard);
        return true;
      }} {}

template<typename T, bool localDocumentId>
MaterializeExecutor<T, localDocumentId>::MaterializeExecutor(
    MaterializeExecutor<T, localDocumentId>::Fetcher& /*fetcher*/, Infos& infos)
    : _trx{infos.query().newTrxContext()},
      _readDocumentContext{infos},
      _memoryTracker{infos.query().resourceMonitor()} {}

template<typename T, bool localDocumentId>
void MaterializeExecutor<T, localDocumentId>::fillBuffer(
    AqlItemBlockInputRange& inputRange) {
  TRI_ASSERT(!localDocumentId);
  _bufferedDocs.clear();

  auto const block = inputRange.getBlock();
  if (ADB_UNLIKELY(block == nullptr)) {
    return;
  }

  auto const numRows = block->numRows();
  auto const numDataRows = numRows - block->numShadowRows();
  if (ADB_UNLIKELY(!numDataRows)) {
    return;
  }

  auto const tracked = _memoryTracker.tracked();
  auto const required = numDataRows * (sizeof(BufferRecord) + sizeof(void*));
  if (required > tracked) {
    _memoryTracker.increase(required - tracked);
  }

  _bufferedDocs.reserve(numDataRows);
  irs::ResolveBool(numRows != numDataRows, [&]<bool HasShadowRows>() {
    auto searchDocRegId =
        _readDocumentContext._infos->inputNonMaterializedDocRegId();
    for (size_t i = 0; i != numRows; ++i) {
      if constexpr (HasShadowRows) {
        if (block->isShadowRow(i)) {
          continue;
        }
      }
      auto const buf =
          block->getValueReference(i, searchDocRegId).slice().stringView();
      _bufferedDocs.emplace_back(iresearch::SearchDoc::decode(buf));
    }
  });

  _readOrder.resize(numDataRows);
  for (auto* data = _readOrder.data(); auto& doc : _bufferedDocs) {
    *data++ = &doc;
  }
  std::sort(_readOrder.begin(), _readOrder.end(),
            [](const auto* lhs, const auto* rhs) {
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
  for (auto* doc : _readOrder) {
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

  if constexpr (isSingleCollection) {
    if (!_collection) {
      _collection = _trx.documentCollection(
                            _readDocumentContext._infos->collectionSource())
                        ->getPhysical();
      TRI_ASSERT(_collection);
    }
  }
  if constexpr (!localDocumentId) {
    // buffering all LocalDocumentIds to avoid memory ping-pong
    // between iresearch and storage engine
    fillBuffer(inputRange);
  }
  auto doc = _bufferedDocs.begin();
  auto end = _bufferedDocs.end();
  auto& callback = _readDocumentContext._callback;
  auto docRegId = _readDocumentContext._infos->inputNonMaterializedDocRegId();
  while (inputRange.hasDataRow() && !output.isFull()) {
    bool written = false;
    auto const [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    _readDocumentContext._inputRow = &input;
    _readDocumentContext._outputRow = &output;

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

    // FIXME(gnusi): use rocksdb::DB::MultiGet(...)
    if constexpr (localDocumentId) {
      static_assert(isSingleCollection);
      written =
          _collection
              ->read(
                  &_trx,
                  LocalDocumentId{input.getValue(docRegId).slice().getUInt()},
                  callback, ReadOwnWrites::no)
              .ok();
    } else if (doc != end) {
      auto const storage = doc->storage;
      if (storage.isSet()) {
        written =
            doc->segment->collection->getPhysical()
                ->readFromSnapshot(&_trx, storage, callback, ReadOwnWrites::no,
                                   *doc->segment->snapshot)
                .ok();
      }
      ++doc;
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
