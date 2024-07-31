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

#include "IResearchViewMergeExecutor.h"

#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/Executor/IResearchViewExecutorBase.tpp"
#include "IResearch/IResearchReadUtils.h"

#include <search/score.hpp>

namespace arangodb::aql {

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::IResearchViewMergeExecutor(
    Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos}, _heap_it{*infos.sort().first, infos.sort().second} {
  TRI_ASSERT(infos.sort().first);
  TRI_ASSERT(!infos.sort().first->empty());
  TRI_ASSERT(infos.sort().first->size() >= infos.sort().second);
  TRI_ASSERT(infos.sort().second);
  TRI_ASSERT(infos.heapSort().empty());
}

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::Segment::Segment(
    irs::doc_iterator::ptr&& docs, irs::document const& doc,
    irs::score const& score, size_t numScores,
    irs::doc_iterator::ptr&& pkReader, size_t index,
    irs::doc_iterator* sortReaderRef, irs::payload const* sortReaderValue,
    irs::doc_iterator::ptr&& sortReader) noexcept
    : docs(std::move(docs)),
      doc(&doc),
      score(&score),
      numScores(numScores),
      segmentIndex(index),
      sortReaderRef(sortReaderRef),
      sortValue(sortReaderValue),
      sortReader(std::move(sortReader)) {
  TRI_ASSERT(this->docs);
  TRI_ASSERT(this->doc);
  TRI_ASSERT(this->score);
  TRI_ASSERT(this->sortReaderRef);
  TRI_ASSERT(this->sortValue);
  if constexpr (Base::isMaterialized) {
    resetColumn(this->pkReader, std::move(pkReader));
    TRI_ASSERT(this->pkReader.itr);
    TRI_ASSERT(this->pkReader.value);
  }
}

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::MinHeapContext(
    iresearch::IResearchSortBase const& sort, size_t sortBuckets) noexcept
    : _less{sort, sortBuckets} {}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::operator()(
    Value& segment) const {
  while (segment.docs->next()) {
    auto const doc = segment.docs->value();

    if (doc == segment.sortReaderRef->seek(doc)) {
      return true;
    }

    // FIXME read pk as well
  }
  return false;
}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::operator()(
    Value const& lhs, Value const& rhs) const {
  return _less.Compare(lhs.sortValue->value, rhs.sortValue->value) < 0;
}

template<typename ExecutionTraits>
void IResearchViewMergeExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
  Base::reset();

  _segments.clear();
  auto const size = this->_reader->size();
  _segments.reserve(size);

  auto const& columnsFieldsRegs = this->_infos.getOutNonMaterializedViewRegs();
  auto const storedValuesCount = columnsFieldsRegs.size();
  this->_storedValuesReaders.resize(size * storedValuesCount);
  auto isSortReaderUsedInStoredValues =
      storedValuesCount > 0 &&
      iresearch::IResearchViewNode::kSortColumnNumber ==
          columnsFieldsRegs.cbegin()->first;

  for (size_t i = 0; i < size; ++i) {
    auto& segment = (*this->_reader)[i];

    auto it = segment.mask(this->_filter->execute({
        .segment = segment,
        .scorers = this->_scorers,
        .ctx = &this->_filterCtx,
        .wand = {},
    }));
    TRI_ASSERT(it);

    auto const* doc = irs::get<irs::document>(*it);
    TRI_ASSERT(doc);

    auto const* score = &irs::score::kNoScore;
    size_t numScores = 0;

    if constexpr (ExecutionTraits::Ordered) {
      auto* scoreRef = irs::get<irs::score>(*it);

      if (scoreRef) {
        score = scoreRef;
        numScores = this->infos().scorers().size();
      }
    }
    irs::doc_iterator::ptr pkReader;
    if constexpr (Base::isMaterialized) {
      pkReader = iresearch::pkColumn(segment);
      if (ADB_UNLIKELY(!pkReader)) {
        LOG_TOPIC("ee041", WARN, iresearch::TOPIC)
            << "encountered a sub-reader without a primary key column while "
               "executing a query, ignoring";
        continue;
      }
    }

    if constexpr (Base::usesStoredValues) {
      if (ADB_UNLIKELY(!this->getStoredValuesReaders(segment, i))) {
        continue;
      }
    } else {
      TRI_ASSERT(!isSortReaderUsedInStoredValues);
    }

    if (isSortReaderUsedInStoredValues) {
      TRI_ASSERT(i * storedValuesCount < this->_storedValuesReaders.size());
      auto& sortReader = this->_storedValuesReaders[i * storedValuesCount];

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             std::move(pkReader), i, sortReader.itr.get(),
                             sortReader.value, nullptr);
    } else {
      auto itr = iresearch::sortColumn(segment);

      if (ADB_UNLIKELY(!itr)) {
        LOG_TOPIC("af4cd", WARN, iresearch::TOPIC)
            << "encountered a sub-reader without a sort column while "
               "executing a query, ignoring";
        continue;
      }

      irs::payload const* sortValue = irs::get<irs::payload>(*itr);
      if (ADB_UNLIKELY(!sortValue)) {
        sortValue = &iresearch::NoPayload;
      }

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             std::move(pkReader), i, itr.get(), sortValue,
                             std::move(itr));
    }
  }

  _heap_it.Reset(_segments);
}

template<typename ExecutionTraits>
LocalDocumentId IResearchViewMergeExecutor<ExecutionTraits>::readPK(
    Segment& segment) {
  LocalDocumentId documentId;
  TRI_ASSERT(segment.doc);
  commonReadPK(segment.pkReader, segment.doc->value, documentId);
  return documentId;
}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  size_t const atMost =
      Base::isLateMaterialized ? 1 : ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  this->_isMaterialized = false;
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  bool gotData = false;
  while (_heap_it.Next()) {
    auto& segment = _heap_it.Lead();
    ++segment.segmentPos;
    TRI_ASSERT(segment.docs);
    TRI_ASSERT(segment.doc);
    TRI_ASSERT(segment.score);
    // try to read a document PK from iresearch
    LocalDocumentId documentId;

    if constexpr (Base::isMaterialized) {
      documentId = readPK(segment);
      if (!documentId.isSet()) {
        continue;  // No document read, we cannot write it.
      }
    }
    // TODO(MBkkt) cache viewSegment?
    auto& viewSegment = this->_reader->segment(segment.segmentIndex);
    if constexpr (Base::isLateMaterialized) {
      this->_indexReadBuffer.makeValue(PushTag{}, viewSegment,
                                       segment.doc->value);
    } else {
      this->_indexReadBuffer.makeValue(PushTag{}, viewSegment, documentId);
    }
    gotData = true;
    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      // Writes into _scoreBuffer
      this->fillScores(*segment.score);
    }

    if constexpr (Base::usesStoredValues) {
      TRI_ASSERT(segment.doc);
      this->makeStoredValues(PushTag{}, segment.doc->value,
                             segment.segmentIndex);
    }

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid() ==
                 ExecutionTraits::EmitSearchDoc);
      this->_indexReadBuffer.makeSearchDoc(PushTag{}, viewSegment,
                                           segment.doc->value);
    }

    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
  return gotData;
}

template<typename ExecutionTraits>
size_t IResearchViewMergeExecutor<ExecutionTraits>::skip(size_t limit,
                                                         IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter != nullptr);

  auto const toSkip = limit;

  while (limit && _heap_it.Next()) {
    ++_heap_it.Lead().segmentPos;
    --limit;
  }

  return toSkip - limit;
}

template<typename ExecutionTraits>
size_t IResearchViewMergeExecutor<ExecutionTraits>::skipAll(
    IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter != nullptr);

  size_t skipped = 0;
  // 0 || 0 -- _heap_it exhausted, we don't need to skip anything
  // 0 || 1 -- _heap_it not exhausted, we need to skip tail
  // 1 || 0 -- never called _heap_it.Next(), we need to skip all
  // 1 || 1 -- impossible, asserted
  if (!_heap_it.Initilized() || _heap_it.Size() != 0) {
    TRI_ASSERT(_heap_it.Initilized() || _heap_it.Size() == 0);
    auto& infos = this->infos();
    for (auto& segment : _segments) {
      TRI_ASSERT(segment.docs);
      if (infos.filterConditionIsEmpty()) {
        TRI_ASSERT(segment.segmentIndex < this->_reader->size());
        auto const live_docs_count =
            (*this->_reader)[segment.segmentIndex].live_docs_count();
        TRI_ASSERT(segment.segmentPos <= live_docs_count);
        skipped += live_docs_count - segment.segmentPos;
      } else {
        skipped += calculateSkipAllCount(
            infos.countApproximate(), segment.segmentPos, segment.docs.get());
      }
    }
    // Adjusting by count of docs already consumed by heap but not consumed by
    // executor. This count is heap size minus 1 already consumed by executor
    // after heap.next. But we should adjust by the heap size only if the heap
    // was advanced at least once (heap is actually filled on first next) or we
    // have nothing consumed from doc iterators!
    if (infos.countApproximate() == iresearch::CountApproximate::Exact &&
        !infos.filterConditionIsEmpty() && _heap_it.Size() != 0) {
      skipped += (_heap_it.Size() - 1);
    }
  }
  _heap_it.Reset({});  // Make it uninitilized
  return skipped;
}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::writeRow(ReadContext& ctx,
                                                           size_t idx) {
  auto const& value = this->_indexReadBuffer.getValue(idx);
  return Base::writeRowImpl(ctx, idx, value);
}

// needed for tests to link correctly
template class IResearchViewMergeExecutor<ExecutionTraits<
    false, false, false, iresearch::MaterializeType::NotMaterialize>>;

#include "IResearchViewExecutorInstantiations.h"
INSTANTIATE_VIEW_EXECUTOR(IResearchViewMergeExecutor,
                          iresearch::MaterializeType::Materialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewMergeExecutor,
                          iresearch::MaterializeType::NotMaterialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewMergeExecutor,
                          iresearch::MaterializeType::LateMaterialize);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewMergeExecutor,
                          iresearch::MaterializeType::NotMaterialize |
                              iresearch::MaterializeType::UseStoredValues);
INSTANTIATE_VIEW_EXECUTOR(IResearchViewMergeExecutor,
                          iresearch::MaterializeType::LateMaterialize |
                              iresearch::MaterializeType::UseStoredValues);

}  // namespace arangodb::aql
