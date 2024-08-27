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

#pragma once

#include "Assertions/Assert.h"
#include "Aql/Executor/IResearchViewExecutorBase.h"

#include <index/heap_iterator.hpp>

namespace arangodb::aql {

template<typename ExecutionTraits>
class IResearchViewMergeExecutor
    : public IResearchViewExecutorBase<
          IResearchViewMergeExecutor<ExecutionTraits>, ExecutionTraits> {
 public:
  using Base =
      IResearchViewExecutorBase<IResearchViewMergeExecutor<ExecutionTraits>,
                                ExecutionTraits>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr bool Ordered = ExecutionTraits::Ordered;
  static constexpr bool kSorted = true;

  IResearchViewMergeExecutor(IResearchViewMergeExecutor&&) = default;
  IResearchViewMergeExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;
  using ReadContext = typename Base::ReadContext;

  struct Segment {
    Segment(irs::doc_iterator::ptr&& docs, irs::document const& doc,
            irs::score const& score, size_t numScores,
            irs::doc_iterator::ptr&& pkReader, size_t index,
            irs::doc_iterator* sortReaderRef,
            irs::payload const* sortReaderValue,
            irs::doc_iterator::ptr&& sortReader) noexcept;
    Segment(Segment const&) = delete;
    Segment(Segment&&) = default;
    Segment& operator=(Segment const&) = delete;
    Segment& operator=(Segment&&) = delete;

    irs::doc_iterator::ptr docs;
    irs::document const* doc{};
    irs::score const* score{};
    size_t numScores{};
    ColumnIterator pkReader;
    size_t segmentIndex;  // first stored values index
    irs::doc_iterator* sortReaderRef;
    irs::payload const* sortValue;
    irs::doc_iterator::ptr sortReader;
    size_t segmentPos{0};
  };

  class MinHeapContext {
   public:
    using Value = Segment;

    MinHeapContext(iresearch::IResearchSortBase const& sort,
                   size_t sortBuckets) noexcept;

    // advance
    bool operator()(Value& segment) const;

    // compare
    bool operator()(const Value& lhs, const Value& rhs) const;

   private:
    iresearch::VPackComparer<iresearch::IResearchSortBase> _less;
  };

  // reads local document id from a specified segment
  LocalDocumentId readPK(Segment& segment);

  bool fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, size_t idx);

  void reset(bool needFullCount);
  size_t skip(size_t toSkip, IResearchViewStats&);
  size_t skipAll(IResearchViewStats&);

  std::vector<Segment> _segments;
  irs::ExternalMergeIterator<MinHeapContext> _heap_it;
};

template<typename ExecutionTraits>
struct IResearchViewExecutorTraits<
    IResearchViewMergeExecutor<ExecutionTraits>> {
  using IndexBufferValueType =
      std::conditional_t<(ExecutionTraits::MaterializeType &
                          iresearch::MaterializeType::LateMaterialize) ==
                             iresearch::MaterializeType::LateMaterialize,
                         iresearch::SearchDoc, ExecutorValue>;
  static constexpr bool ExplicitScanned = false;
};

}  // namespace arangodb::aql
