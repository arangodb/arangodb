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

#include "IResearchViewExecutorBase.h"

#include <search/score.hpp>

namespace arangodb::aql {

template<typename ExecutionTraits>
class IResearchViewExecutor
    : public IResearchViewExecutorBase<IResearchViewExecutor<ExecutionTraits>,
                                       ExecutionTraits> {
 public:
  using Base = IResearchViewExecutorBase<IResearchViewExecutor<ExecutionTraits>,
                                         ExecutionTraits>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr bool kSorted = false;

  IResearchViewExecutor(IResearchViewExecutor&&) = default;
  IResearchViewExecutor(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutor();

 private:
  friend Base;
  using ReadContext = typename Base::ReadContext;

  struct SegmentReader {
    void finalize() {
      itr.reset();
      doc = nullptr;
      currentSegmentPos = 0;
    }

    // current primary key reader
    ColumnIterator pkReader;
    irs::doc_iterator::ptr itr;
    irs::document const* doc{};
    size_t readerOffset{0};
    // current document iterator position in segment
    size_t currentSegmentPos{0};
    // total position for full snapshot
    size_t totalPos{0};
    size_t numScores{0};
    size_t atMost{0};
    LogicalCollection const* collection{};
    iresearch::ViewSegment const* segment{};
    irs::score const* scr{&irs::score::kNoScore};
  };

  size_t skip(size_t toSkip, IResearchViewStats&);
  size_t skipAll(IResearchViewStats&);

  bool fillBuffer(ReadContext& ctx);

  template<bool parallel>
  bool readSegment(SegmentReader& reader, std::atomic_size_t& bufferIdxGlobal);

  bool writeRow(ReadContext& ctx, size_t idx);

  bool resetIterator(SegmentReader& reader);

  void reset(bool needFullCount);

  // Returns true unless the iterator is exhausted. documentId will always be
  // written. It will always be unset when readPK returns false, but may also be
  // unset if readPK returns true.
  static bool readPK(LocalDocumentId& documentId, SegmentReader& reader);

  std::vector<SegmentReader> _segmentReaders;
  size_t _segmentOffset;
  uint64_t _allocatedThreads{0};
  uint64_t _demandedThreads{0};
};

template<typename ExecutionTraits>
struct IResearchViewExecutorTraits<IResearchViewExecutor<ExecutionTraits>> {
  using IndexBufferValueType =
      std::conditional_t<(ExecutionTraits::MaterializeType &
                          iresearch::MaterializeType::LateMaterialize) ==
                             iresearch::MaterializeType::LateMaterialize,
                         iresearch::SearchDoc, ExecutorValue>;
  static constexpr bool ExplicitScanned = false;
};

}  // namespace arangodb::aql
