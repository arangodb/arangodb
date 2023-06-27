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
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "IResearch/IResearchReadUtils.h"
#include <formats/formats.hpp>
#include <index/index_reader.hpp>
#include <search/boolean_filter.hpp>

using namespace arangodb;
using namespace arangodb::aql;

template<typename T, bool localDocumentId>
arangodb::IndexIterator::DocumentCallback
MaterializeExecutor<T, localDocumentId>::ReadContext::copyDocumentCallback(
    ReadContext& ctx) {
  typedef std::function<arangodb::IndexIterator::DocumentCallback(ReadContext&)>
      CallbackFactory;
  static CallbackFactory const callbackFactory{[](ReadContext& ctx) {
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      TRI_ASSERT(ctx._outputRow);
      TRI_ASSERT(ctx._inputRow);
      TRI_ASSERT(ctx._inputRow->isInitialized());
      TRI_ASSERT(ctx._infos);
      AqlValue a{AqlValueHintSliceCopy(doc)};
      bool mustDestroy = true;
      AqlValueGuard guard{a, mustDestroy};
      ctx._outputRow->moveValueInto(
          ctx._infos->outputMaterializedDocumentRegId(), *ctx._inputRow, guard);
      return true;
    };
  }};

  return callbackFactory(ctx);
}

template<typename T, bool localDocumentId>
MaterializeExecutor<T, localDocumentId>::MaterializeExecutor(
    MaterializeExecutor<T, localDocumentId>::Fetcher& /*fetcher*/, Infos& infos)
    : _trx(infos.query().newTrxContext(), infos.query().getTrxTypeHint()),
      _readDocumentContext(infos),
      _infos(infos),
      _memoryTracker(_infos.query().resourceMonitor()) {
  if constexpr (isSingleCollection) {
    _collection = nullptr;
  }
}

template<typename T, bool localDocumentId>
void MaterializeExecutor<T, localDocumentId>::fillBuffer(
    AqlItemBlockInputRange& inputRange) {
  TRI_ASSERT(!localDocumentId);
  _bufferedDocs.clear();
  auto const block = inputRange.getBlock();
  if (block == nullptr) {
    return;
  }
  auto const numRows = block->numRows();
  auto const numDataRows = numRows - block->numShadowRows();
  if (ADB_UNLIKELY(!numDataRows)) {
    return;
  }
  auto const tracked = _memoryTracker.tracked();
  auto const required =
      numDataRows * sizeof(typename decltype(_bufferedDocs)::value_type);
  if (required > tracked) {
    _memoryTracker.increase(required - tracked);
  }
  _bufferedDocs.reserve(numDataRows);
  auto readInputDocs = [numRows, this, &block]<bool HasShadowRows>() {
    auto searchDocRegId =
        _readDocumentContext._infos->inputNonMaterializedDocRegId();
    LogicalCollection const* lastCollection{nullptr};
    if constexpr (isSingleCollection) {
      lastCollection = _collection;
    }
    auto lastSourceId = DataSourceId::none();
    for (size_t i = 0; i < numRows; ++i) {
      if constexpr (HasShadowRows) {
        if (block->isShadowRow(i)) {
          continue;
        }
      }
      auto const buf =
          block->getValueReference(i, searchDocRegId).slice().stringView();
      auto searchDoc = iresearch::SearchDoc::decode(buf);
      if constexpr (!isSingleCollection) {
        auto docSourceId = std::get<0>(*searchDoc.segment());
        if (docSourceId != lastSourceId) {
          lastSourceId = docSourceId;
          auto cachedCollection = _collection.find(docSourceId);
          if (cachedCollection == _collection.end()) {
            auto transactionCollection =
                _trx.state()->collection(std::get<0>(*searchDoc.segment()),
                                         arangodb::AccessMode::Type::READ);
            if (ADB_LIKELY(transactionCollection)) {
              lastCollection =
                  _collection
                      .emplace(docSourceId,
                               transactionCollection->collection().get())
                      .first->second;
            } else {
              lastCollection = nullptr;
            }

          } else {
            lastCollection = cachedCollection->second;
          }
        }
      }
      if (ADB_LIKELY(lastCollection)) {
        _bufferedDocs.push_back(
            std::make_tuple(searchDoc, LocalDocumentId{}, lastCollection));
      }
    }
  };

  if (block->hasShadowRows()) {
    readInputDocs.template operator()<true>();
  } else {
    readInputDocs.template operator()<false>();
  }
  std::vector<size_t> readOrder(_bufferedDocs.size(), 0);
  std::iota(readOrder.begin(), readOrder.end(), 0);
  std::sort(std::begin(readOrder), std::end(readOrder),
            [&](auto& lhs, auto& rhs) {
              return std::get<0>(_bufferedDocs[lhs]) <
                     std::get<0>(_bufferedDocs[rhs]);
            });
  iresearch::ViewSegment const* lastSegment{nullptr};
  irs::doc_iterator::ptr pkReader;
  irs::payload const* docValue{nullptr};
  LocalDocumentId documentId;
  auto doc = readOrder.begin();
  auto end = readOrder.end();
  while (doc != end) {
    TRI_ASSERT(*doc < _bufferedDocs.size());
    auto& document = _bufferedDocs[*doc];
    auto& searchDoc = std::get<0>(document);
    TRI_ASSERT(searchDoc.isValid());
    if (lastSegment != searchDoc.segment()) {
      lastSegment = searchDoc.segment();
      pkReader = iresearch::pkColumn(*std::get<1>(*lastSegment));
      if (ADB_LIKELY(pkReader)) {
        docValue = irs::get<irs::payload>(*pkReader);
      } else {
        // skip segment without PK column
        do {
          ++doc;
        } while (doc != end &&
                 lastSegment == std::get<0>(_bufferedDocs[*doc]).segment());
        continue;
      }
    }
    TRI_ASSERT(docValue);
    if (const auto doc = std::get<0>(document).doc();
        doc == pkReader->seek(doc)) {
      arangodb::iresearch::DocumentPrimaryKey::read(documentId,
                                                    docValue->value);
      std::get<1>(document) = documentId;
    }
    ++doc;
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
    if (_collection == nullptr) {
      _collection = _trx.documentCollection(
          _readDocumentContext._infos->collectionSource());
    }
    TRI_ASSERT(_collection != nullptr);
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

    TRI_IF_FAILURE("MaterializeExecutor::all_fail") { continue; }

    TRI_IF_FAILURE("MaterializeExecutor::only_one") {
      if (output.numRowsWritten() > 0) {
        continue;
      }
    }

    if constexpr (localDocumentId) {
      // FIXME(gnusi): use rocksdb::DB::MultiGet(...)
      TRI_ASSERT(isSingleCollection);
      if constexpr (isSingleCollection) {
        written =
            _collection->getPhysical()
                ->read(
                    &_trx,
                    LocalDocumentId(input.getValue(docRegId).slice().getUInt()),
                    callback, ReadOwnWrites::no)
                .ok();
      }
    } else {
      if (doc != end) {
        auto const& documentId = std::get<1>(*doc);
        if (documentId.isSet()) {
          auto collection = std::get<2>(*doc);
          TRI_ASSERT(collection);
          // FIXME(gnusi): use rocksdb::DB::MultiGet(...)
          written = collection->getPhysical()
                        ->readFromSnapshot(
                            &_trx, documentId, callback, ReadOwnWrites::no,
                            std::get<2>(*std::get<0>(*doc).segment()))
                        .ok();
        }
        ++doc;
      }
    }
    if (written) {
      // document found
      output.advanceRow();
    } else {
      // document not found
      stats.incrFiltered();
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

template class arangodb::aql::MaterializeExecutor<void, false>;
template class arangodb::aql::MaterializeExecutor<std::string const&, true>;
template class arangodb::aql::MaterializeExecutor<std::string const&, false>;

template class arangodb::aql::MaterializerExecutorInfos<void>;
template class arangodb::aql::MaterializerExecutorInfos<std::string const&>;
