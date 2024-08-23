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
/// @author Tobias Gödderz
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewHeapSortExecutor.h"

#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/Executor/IResearchViewExecutorBase.tpp"
#include "IResearch/IResearchReadUtils.h"

namespace arangodb::aql {

template<typename ExecutionTraits>
IResearchViewHeapSortExecutor<ExecutionTraits>::IResearchViewHeapSortExecutor(
    Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos} {
  this->_indexReadBuffer.setHeapSort(
      this->_infos.heapSort(), this->_infos.getOutNonMaterializedViewRegs());
}

template<typename ExecutionTraits>
size_t IResearchViewHeapSortExecutor<ExecutionTraits>::skipAll(
    IResearchViewStats& stats) {
  TRI_ASSERT(this->_filter);
  size_t totalSkipped{0};
  if (_bufferFilled) {
    // we already know exact full count after buffer filling
    TRI_ASSERT(_bufferedCount >= this->_indexReadBuffer.size());
    totalSkipped =
        _totalCount - (_bufferedCount - this->_indexReadBuffer.size());
  } else {
    // Looks like this is currently unreachable.
    // If this assert is ever triggered, tests for such case should be added.
    // But implementations should work.
    TRI_ASSERT(false);
    size_t const count = this->_reader->size();
    for (size_t readerOffset = 0; readerOffset < count; ++readerOffset) {
      auto& segmentReader = (*this->_reader)[readerOffset];
      auto itr = this->_filter->execute({
          .segment = segmentReader,
          .scorers = this->_scorers,
          .ctx = &this->_filterCtx,
          .wand = {},
      });
      TRI_ASSERT(itr);
      if (!itr) {
        continue;
      }
      itr = segmentReader.mask(std::move(itr));
      if (this->infos().filterConditionIsEmpty()) {
        totalSkipped += this->_reader->live_docs_count();
      } else {
        totalSkipped += calculateSkipAllCount(this->infos().countApproximate(),
                                              0, itr.get());
      }
    }
    stats.incrScanned(totalSkipped);
  }

  // seal the executor anyway
  _bufferFilled = true;
  this->_isMaterialized = false;
  this->_indexReadBuffer.clear();
  _totalCount = 0;
  _bufferedCount = 0;
  return totalSkipped;
}

template<typename ExecutionTraits>
size_t IResearchViewHeapSortExecutor<ExecutionTraits>::skip(
    size_t limit, IResearchViewStats& stats) {
  if (fillBufferInternal(limit)) {
    stats.incrScanned(getScanned());
  }
  auto const size = this->_indexReadBuffer.size();
  if (limit < size) {
    this->_indexReadBuffer.skip(limit);
    return limit;
  }
  this->_indexReadBuffer.clear();
  return size;
}

template<typename ExecutionTraits>
void IResearchViewHeapSortExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
  Base::reset();
#ifdef USE_ENTERPRISE
  if (!needFullCount) {
    this->_wand = this->_infos.optimizeTopK().makeWandContext(
        this->_infos.heapSort(), this->_scorers);
  }
#endif
  _totalCount = 0;
  _bufferedCount = 0;
  _bufferFilled = false;
  this->_storedValuesReaders.resize(
      this->_reader->size() *
      this->_infos.getOutNonMaterializedViewRegs().size());
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::writeRow(ReadContext& ctx,
                                                              size_t idx) {
  static_assert(!Base::isLateMaterialized,
                "HeapSort superseeds LateMaterialization");
  auto const& value = this->_indexReadBuffer.getValue(idx);
  return Base::writeRowImpl(ctx, idx, value);
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::fillBuffer(ReadContext&) {
  fillBufferInternal(0);
  // FIXME: Use additional flag from fillBufferInternal
  return !this->_indexReadBuffer.empty();
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::fillBufferInternal(
    size_t skip) {
  if (_bufferFilled) {
    return false;
  }
  _bufferFilled = true;
  TRI_ASSERT(!this->_infos.heapSort().empty());
  TRI_ASSERT(this->_filter != nullptr);
  size_t const atMost = this->_infos.heapSortLimit();
  TRI_ASSERT(atMost);
  this->_isMaterialized = false;
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  auto const count = this->_reader->size();

  for (auto const& cmp : this->_infos.heapSort()) {
    if (!cmp.isScore()) {
      this->_storedColumnsMask.insert(cmp.source);
    }
  }

  containers::SmallVector<irs::score_t, 4> scores;
  if constexpr (ExecutionTraits::Ordered) {
    scores.resize(this->infos().scorers().size());
  }

  irs::doc_iterator::ptr itr;
  irs::document const* doc{};
  irs::score* scr = const_cast<irs::score*>(&irs::score::kNoScore);
  size_t numScores{0};
  irs::score_t threshold = 0.f;
  for (size_t readerOffset = 0; readerOffset < count;) {
    if (!itr) {
      auto& segmentReader = (*this->_reader)[readerOffset];
      itr = this->_filter->execute({
          .segment = segmentReader,
          .scorers = this->_scorers,
          .ctx = &this->_filterCtx,
          .wand = this->_wand,
      });
      TRI_ASSERT(itr);
      doc = irs::get<irs::document>(*itr);
      TRI_ASSERT(doc);
      if constexpr (ExecutionTraits::Ordered) {
        auto* score = irs::get_mutable<irs::score>(itr.get());
        if (score != nullptr) {
          scr = score;
          numScores = scores.size();
          scr->Min(threshold);
        }
      }
      itr = segmentReader.mask(std::move(itr));
      TRI_ASSERT(itr);
    }
    if (!itr->next()) {
      ++readerOffset;
      itr.reset();
      doc = nullptr;
      scr = const_cast<irs::score*>(&irs::score::kNoScore);
      numScores = 0;
      continue;
    }
    ++_totalCount;

    (*scr)(scores.data());
    auto provider = [this, readerOffset](ptrdiff_t column) {
      auto& segmentReader = (*this->_reader)[readerOffset];
      ColumnIterator it;
      if (iresearch::IResearchViewNode::kSortColumnNumber == column) {
        auto sortReader = iresearch::sortColumn(segmentReader);
        if (ADB_LIKELY(sortReader)) {
          resetColumn(it, std::move(sortReader));
        }
      } else {
        auto const& columns = this->_infos.storedValues().columns();
        auto const* storedValuesReader =
            segmentReader.column(columns[column].name);
        if (ADB_LIKELY(storedValuesReader)) {
          resetColumn(it,
                      storedValuesReader->iterator(irs::ColumnHint::kNormal));
        }
      }
      return it;
    };
    this->_indexReadBuffer.pushSortedValue(
        provider, HeapSortExecutorValue{readerOffset, doc->value},
        std::span{scores.data(), numScores}, *scr, threshold);
  }
  this->_indexReadBuffer.finalizeHeapSort();
  _bufferedCount = this->_indexReadBuffer.size();
  if (skip >= _bufferedCount) {
    return true;
  }
  auto pkReadingOrder = this->_indexReadBuffer.getMaterializeRange(skip);
  std::sort(pkReadingOrder.begin(), pkReadingOrder.end(),
            [buffer = &this->_indexReadBuffer](size_t lhs, size_t rhs) {
              auto const& lhs_val = buffer->getValue(lhs);
              auto const& rhs_val = buffer->getValue(rhs);
              auto const lhs_offset = lhs_val.readerOffset();
              auto const rhs_offset = rhs_val.readerOffset();
              if (lhs_offset != rhs_offset) {
                return lhs_offset < rhs_offset;
              }
              return lhs_val.docId() < rhs_val.docId();
            });

  size_t lastSegmentIdx = count;
  ColumnIterator pkReader;
  iresearch::ViewSegment const* segment{};
  auto orderIt = pkReadingOrder.begin();
  while (orderIt != pkReadingOrder.end()) {
    auto& value = this->_indexReadBuffer.getValue(*orderIt);
    auto const docId = value.docId();
    auto const segmentIdx = value.readerOffset();
    if (lastSegmentIdx != segmentIdx) {
      auto& segmentReader = (*this->_reader)[segmentIdx];
      auto pkIt = iresearch::pkColumn(segmentReader);
      pkReader.itr.reset();
      segment = &this->_reader->segment(segmentIdx);
      if (ADB_UNLIKELY(!pkIt)) {
        LOG_TOPIC("bd02b", WARN, iresearch::TOPIC)
            << "encountered a sub-reader without a primary key column "
               "while executing a query, ignoring";
        continue;
      }
      resetColumn(pkReader, std::move(pkIt));
      if constexpr (Base::contains(
                        iresearch::MaterializeType::UseStoredValues)) {
        if (ADB_UNLIKELY(
                !this->getStoredValuesReaders(segmentReader, segmentIdx))) {
          LOG_TOPIC("bd02c", WARN, iresearch::TOPIC)
              << "encountered a sub-reader without stored values column "
                 "while executing a query, ignoring";
          continue;
        }
      }
      lastSegmentIdx = segmentIdx;
    }

    LocalDocumentId documentId;
    commonReadPK(pkReader, docId, documentId);

    TRI_ASSERT(segment);
    value.translate(*segment, documentId);

    if constexpr (Base::usesStoredValues) {
      auto const& columnsFieldsRegs =
          this->infos().getOutNonMaterializedViewRegs();
      TRI_ASSERT(!columnsFieldsRegs.empty());
      auto readerIndex = segmentIdx * columnsFieldsRegs.size();
      size_t valueIndex = *orderIt * columnsFieldsRegs.size();
      for (auto it = columnsFieldsRegs.begin(), end = columnsFieldsRegs.end();
           it != end; ++it) {
        if (this->_storedColumnsMask.contains(it->first)) {
          valueIndex++;
          continue;
        }
        TRI_ASSERT(readerIndex < this->_storedValuesReaders.size());
        auto const& reader = this->_storedValuesReaders[readerIndex++];
        TRI_ASSERT(reader.itr);
        TRI_ASSERT(reader.value);
        auto const& payload = reader.value->value;
        bool const found = docId == reader.itr->seek(docId);
        if (found && !payload.empty()) {
          this->_indexReadBuffer.makeStoredValue(valueIndex++, payload);
        } else {
          this->_indexReadBuffer.makeStoredValue(valueIndex++, kNullSlice);
        }
      }
    }

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
      this->_indexReadBuffer.makeSearchDoc(PushTag{}, *segment, docId);
    }

    this->_indexReadBuffer.assertSizeCoherence();
    ++orderIt;
  }
  return true;
}

#include "IResearchViewExecutorInstantiations.h"
INSTANTIATE_VIEW_EXECUTOR(IResearchViewHeapSortExecutor,
                          iresearch::MaterializeType::Materialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewHeapSortExecutor,
                          iresearch::MaterializeType::NotMaterialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewHeapSortExecutor,
                          iresearch::MaterializeType::NotMaterialize |
                              iresearch::MaterializeType::UseStoredValues);

}  // namespace arangodb::aql
