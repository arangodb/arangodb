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
#include "IResearch/IResearchDocument.h"
#include <formats/formats.hpp>
#include <index/index_reader.hpp>
#include <search/boolean_filter.hpp>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
// TODO Make it common!
inline irs::doc_iterator::ptr pkColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.column(arangodb::iresearch::DocumentPrimaryKey::PK());

  return reader ? reader->iterator(irs::ColumnHint::kNormal) : nullptr;
}

} // namespace

template<typename T>
arangodb::IndexIterator::DocumentCallback
MaterializeExecutor<T>::ReadContext::copyDocumentCallback(ReadContext& ctx) {
  typedef std::function<arangodb::IndexIterator::DocumentCallback(ReadContext&)>
      CallbackFactory;
  static CallbackFactory const callbackFactory{[](ReadContext& ctx) {
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      TRI_ASSERT(ctx._outputRow);
      TRI_ASSERT(ctx._inputRow);
      TRI_ASSERT(ctx._inputRow->isInitialized());
      TRI_ASSERT(ctx._infos);
      arangodb::aql::AqlValue a{arangodb::aql::AqlValueHintSliceCopy(doc)};
      bool mustDestroy = true;
      arangodb::aql::AqlValueGuard guard{a, mustDestroy};
      ctx._outputRow->moveValueInto(
          ctx._infos->outputMaterializedDocumentRegId(), *ctx._inputRow, guard);
      return true;
    };
  }};

  return callbackFactory(ctx);
}

template<typename T>
arangodb::aql::MaterializerExecutorInfos<T>::MaterializerExecutorInfos(
    T collectionSource, RegisterId inNmDocId, RegisterId outDocRegId,
    aql::QueryContext& query)
    : _collectionSource(collectionSource),
      _inNonMaterializedDocRegId(inNmDocId),
      _outMaterializedDocumentRegId(outDocRegId),
      _query(query) {}

template<typename T>
arangodb::aql::MaterializeExecutor<T>::MaterializeExecutor(
    MaterializeExecutor<T>::Fetcher& /*fetcher*/, Infos& infos)
    : _trx(infos.query().newTrxContext()),
      _readDocumentContext(infos),
      _infos(infos) {}

template<typename T>
void
arangodb::aql::MaterializeExecutor<T>::fillBuffer(
    AqlItemBlockInputRange& inputRange) {
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
  _bufferedDocs.reserve(numDataRows);
  auto readInputDocs = [numRows, this, &block]<bool HasShadowRows>() {
    auto docRegId = _readDocumentContext._infos->inputNonMaterializedDocRegId();
    for (size_t i = 0, j = 0; i < numRows; ++i) {
      if constexpr (HasShadowRows) {
        if (block->isShadowRow(i)) {
          continue;
        }
      }
      auto const buf = block->getValueReference(i, docRegId).slice().stringView();
      auto searchDoc = iresearch::SearchDoc::decode(buf);
      auto transactionCollection = _trx.state()->collection(
          std::get<0>(*searchDoc.segment()), arangodb::AccessMode::Type::READ);
      TRI_ASSERT(transactionCollection);
      auto collection = transactionCollection->collection().get();
      TRI_ASSERT(collection);
      _bufferedDocs.push_back(
          std::make_tuple(searchDoc, LocalDocumentId{}, collection));
    }
  };

  if (block->hasShadowRows()) {
    readInputDocs.operator()<false>();
  } else {
    readInputDocs.operator()<true>();
  }
  std::vector<size_t> readOrder(_bufferedDocs.size(), 0);
  std::iota(readOrder.begin(), readOrder.end(), 0);
  std::sort(
      std::begin(readOrder), std::end(readOrder),
            [&](auto& lhs, auto& rhs) { return std::get<0>(_bufferedDocs[lhs]) < std::get<0>(_bufferedDocs[rhs]); });
  iresearch::ViewSegment const* lastSegment{nullptr};
  irs::doc_iterator::ptr pkReader;
  irs::payload const* docValue{nullptr};
  LocalDocumentId documentId;
  auto doc = readOrder.begin();
  auto end = readOrder.end();
  while (doc != end) {
    auto& document = _bufferedDocs[*doc];
    if (lastSegment != std::get<0>(document).segment()) {
      lastSegment = std::get<0>(document).segment();
      pkReader = ::pkColumn(*std::get<1>(*lastSegment));
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
    if (std::get<0>(document).doc() == pkReader->seek(std::get<0>(document).doc())) {
      arangodb::iresearch::DocumentPrimaryKey::read(documentId,
                                                    docValue->value);
      std::get<1>(document) = documentId;
    }
    ++doc;
  }

}

template<typename T>
std::tuple<ExecutorState, MaterializeStats, AqlCall>
arangodb::aql::MaterializeExecutor<T>::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  MaterializeStats stats;

  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;


  if constexpr (!std::is_same<T, std::string const&>::value) {
    // buffering all LocalDocumentIds to avoid memory ping-pong
    // between iresearch and storage engine
    fillBuffer(inputRange);
  }
  auto doc = _bufferedDocs.begin();
  auto end = _bufferedDocs.end();
  auto& callback = _readDocumentContext._callback;
  auto docRegId = _readDocumentContext._infos->inputNonMaterializedDocRegId();
  T collectionSource = _readDocumentContext._infos->collectionSource();
  while (inputRange.hasDataRow() && !output.isFull()) {
    bool written = false;
    auto const [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    if constexpr (std::is_same<T, std::string const&>::value) {
      if (_collection == nullptr) {
        _collection = _trx.documentCollection(collectionSource);
      }
      TRI_ASSERT(_collection != nullptr);
    }
    
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

    if constexpr (std::is_same<T, std::string const&>::value) {
      // FIXME(gnusi): use rocksdb::DB::MultiGet(...)
      written =
          _collection->getPhysical()
              ->read(
                  &_trx,
                  LocalDocumentId(input.getValue(docRegId).slice().getUInt()),
                  callback, ReadOwnWrites::no)
              .ok();
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

template<typename T>
std::tuple<ExecutorState, MaterializeStats, size_t, AqlCall>
arangodb::aql::MaterializeExecutor<T>::skipRowsRange(
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

template class ::arangodb::aql::MaterializeExecutor<RegisterId>;
template class ::arangodb::aql::MaterializeExecutor<std::string const&>;

template class ::arangodb::aql::MaterializerExecutorInfos<RegisterId>;
template class ::arangodb::aql::MaterializerExecutorInfos<std::string const&>;
