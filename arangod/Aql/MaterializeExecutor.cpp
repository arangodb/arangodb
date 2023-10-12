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

MaterializeExecutorBase::MaterializeExecutorBase(Infos& infos)
    : _trx{infos.query().newTrxContext()}, _infos(infos) {}

MaterializeRocksDBExecutor::MaterializeRocksDBExecutor(Fetcher&, Infos& infos)
    : MaterializeExecutorBase(infos),
      _collection(infos.collection()->getCollection()->getPhysical()) {
  TRI_ASSERT(_collection != nullptr);
}

std::tuple<ExecutorState, MaterializeStats, AqlCall>
MaterializeRocksDBExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                        OutputAqlItemRow& output) {
  MaterializeStats stats;

  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  auto docRegId = _infos.inputNonMaterializedDocRegId();
  auto docOutReg = _infos.outputMaterializedDocumentRegId();
  while (inputRange.hasDataRow() && !output.isFull()) {
    auto const [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});

    LocalDocumentId id{input.getValue(docRegId).slice().getUInt()};
    auto result = _collection->lookup(
        &_trx, id,
        [&, &inputRef = input](LocalDocumentId, aql::DocumentData&& data,
                               VPackSlice doc) {
          TRI_ASSERT(inputRef.isInitialized());
          if (data) {
            output.moveValueInto(docOutReg, inputRef, &data);
          } else {
            output.moveValueInto(docOutReg, inputRef, doc);
          }
          return true;
        },
        {});

    if (result.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          result.errorNumber(),
          basics::StringUtils::concatT(
              "failed to materialize document ", id.id(), " for collection ",
              _infos.collection()->name(), ": ", result.errorMessage()));
    }

    output.advanceRow();
  }

  return {inputRange.upstreamState(), stats, output.getClientCall()};
}

MaterializeSearchExecutor::MaterializeSearchExecutor(Fetcher&, Infos& infos)
    : MaterializeExecutorBase(infos),
      _buffer(infos.query().resourceMonitor()) {}

void MaterializeSearchExecutor::Buffer::fill(AqlItemBlockInputRange& inputRange,
                                             RegisterId searchDocRegId) {
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

std::tuple<ExecutorState, MaterializeStats, AqlCall>
MaterializeSearchExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                       OutputAqlItemRow& output) {
  MaterializeStats stats;

  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;
  {
    // buffering all LocalDocumentIds to avoid memory ping-pong
    // between iresearch and storage engine
    _buffer.fill(inputRange, _infos.inputNonMaterializedDocRegId());
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
  auto it = _buffer.docs.begin();
  auto end = _buffer.docs.end();
  while (inputRange.hasDataRow() && !output.isFull()) {
    bool written = false;
    auto const [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});

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

    if (it != end) {
      if (it->executor != kInvalidRecord) {
        if (auto& value = _getCtx.values[it->executor]; value) {
          TRI_ASSERT(input.isInitialized());
          output.moveValueInto(_infos.outputMaterializedDocumentRegId(), input,
                               &value);
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

std::tuple<ExecutorState, MaterializeStats, size_t, AqlCall>
MaterializeExecutorBase::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                       AqlCall& call) {
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

}  // namespace arangodb::aql
