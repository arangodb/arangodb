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
/// @author Heiko Kernbach
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <memory>
#include <queue>

#include "Logger/LogMacros.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/IndexJoinStrategy.h"

namespace arangodb::aql {

constexpr size_t FIXED_INDEX_SIZE_VAR = 2;

struct DefaultCmp {
  template<typename U, typename V>
  auto operator()(U&& left, V&& right) const {
    return left <=> right;
  }
};

template<typename SliceType, typename DocIdType,
         typename KeyCompare = DefaultCmp>
struct TwoIndicesUniqueMergeJoin : IndexJoinStrategy<SliceType, DocIdType> {
  using StreamIteratorType = IndexStreamIterator<SliceType, DocIdType>;
  using Descriptor = IndexDescriptor<SliceType, DocIdType>;

  TwoIndicesUniqueMergeJoin(std::vector<Descriptor> descs);

  std::pair<bool, size_t> next(
      std::function<bool(std::span<DocIdType>, std::span<SliceType>)> const& cb)
      override;

  void reset(std::span<SliceType> constants) override;

 private:
  struct IndexStreamData {
    std::unique_ptr<StreamIteratorType> _iter;
    std::span<SliceType> _position;
    std::span<SliceType> _projections;
    DocIdType& _docId;
    size_t _constantSize{0};
    bool exhausted{false};

    IndexStreamData(std::unique_ptr<StreamIteratorType> iter,
                    std::span<SliceType> position,
                    std::span<SliceType> projections, DocIdType& docId,
                    size_t constantSize);
    bool next();
    void reset(std::span<SliceType>);
  };

  struct IndexStreamCompare {
    static std::weak_ordering cmp(std::span<SliceType> left,
                                  std::span<SliceType> right);

    bool operator()(IndexStreamData* left, IndexStreamData* right) const;
  };

  std::array<SliceType, 2> keyCache;
  std::array<DocIdType, 2> documentCache;

  std::vector<SliceType> sliceBuffer;
  std::span<SliceType> projectionsSpan;

  // We do have exactly two IndexStreams
  std::unique_ptr<IndexStreamData> leftIndex;
  std::unique_ptr<IndexStreamData> rightIndex;
};

#define LOG_INDEX_UNIQUE_MERGER LOG_DEVEL_IF(false)

template<typename SliceType, typename DocIdType, typename KeyCompare>
void TwoIndicesUniqueMergeJoin<SliceType, DocIdType, KeyCompare>::reset(
    std::span<SliceType> constants) {
  TRI_ASSERT(leftIndex != nullptr);
  leftIndex->reset(constants.subspan(0, leftIndex->_constantSize));
  TRI_ASSERT(rightIndex != nullptr);
  rightIndex->reset(constants.subspan(leftIndex->_constantSize));
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::weak_ordering TwoIndicesUniqueMergeJoin<SliceType, DocIdType, KeyCompare>::
    IndexStreamCompare::cmp(std::span<SliceType> left,
                            std::span<SliceType> right) {
  for (std::size_t k = 0; k < left.size(); k++) {
    auto v = KeyCompare{}(left[k], right[k]);
    if (v > 0) {
      return std::weak_ordering::greater;
    } else if (v < 0) {
      return std::weak_ordering::less;
    }
  }

  return std::weak_ordering::equivalent;
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
bool TwoIndicesUniqueMergeJoin<SliceType, DocIdType, KeyCompare>::
    IndexStreamCompare::operator()(IndexStreamData* left,
                                   IndexStreamData* right) const {
  return cmp(*left, *right) == std::weak_ordering::greater;
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
TwoIndicesUniqueMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamData::
    IndexStreamData(std::unique_ptr<StreamIteratorType> iter,
                    std::span<SliceType> position,
                    std::span<SliceType> projections, DocIdType& docId,
                    size_t constantSize)
    : _iter(std::move(iter)),
      _position(position),
      _projections(projections),
      _docId(docId),
      _constantSize(constantSize) {
  exhausted = !_iter->position(_position);
  LOG_INDEX_UNIQUE_MERGER << "iterator pointing to " << _position[0] << " ("
                          << this << ")";
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void TwoIndicesUniqueMergeJoin<SliceType, DocIdType, KeyCompare>::
    IndexStreamData::reset(std::span<SliceType> constants) {
  exhausted = !_iter->reset(_position, constants);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
bool TwoIndicesUniqueMergeJoin<SliceType, DocIdType,
                               KeyCompare>::IndexStreamData::next() {
  return exhausted = !_iter->next(_position, _docId, _projections);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
TwoIndicesUniqueMergeJoin<SliceType, DocIdType, KeyCompare>::
    TwoIndicesUniqueMergeJoin(std::vector<Descriptor> descs) {
  TRI_ASSERT(descs.size() == FIXED_INDEX_SIZE_VAR);
  std::size_t bufferSize = 0;
  for (auto const& desc : descs) {
    bufferSize += desc.numProjections + desc.numKeyComponents;
  }

  sliceBuffer.resize(bufferSize);

  auto keySliceIter = sliceBuffer.begin();
  auto projectionsIter =
      sliceBuffer.begin() + descs[0].numKeyComponents * FIXED_INDEX_SIZE_VAR;
  // Note: Access to descs[0] is safe because we have asserted that descs.size()
  // == 2 and right now we only do support joins on exact one join-key. As soon
  // as we extend this to support joins on multiple keys, we need to change this
  // code here.
  projectionsSpan = {projectionsIter, sliceBuffer.end()};
  auto docIdIter = documentCache.begin();
  for (auto& desc : descs) {
    auto projections = projectionsIter;
    projectionsIter += desc.numProjections;
    auto keyBuffer = keySliceIter;
    keySliceIter += desc.numKeyComponents;
    if (leftIndex == nullptr) {
      leftIndex = std::make_unique<IndexStreamData>(
          std::move(desc.iter), std::span<SliceType>{keyBuffer, keySliceIter},
          std::span<SliceType>{projections, projectionsIter}, *(docIdIter++),
          desc.numConstants);
      LOG_INDEX_UNIQUE_MERGER << "Set left iterator";
    } else {
      rightIndex = std::make_unique<IndexStreamData>(
          std::move(desc.iter), std::span<SliceType>{keyBuffer, keySliceIter},
          std::span<SliceType>{projections, projectionsIter}, *(docIdIter++),
          desc.numConstants);
      LOG_INDEX_UNIQUE_MERGER << "Set right iterator";
    }
  }
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::pair<bool, size_t>
TwoIndicesUniqueMergeJoin<SliceType, DocIdType, KeyCompare>::next(
    const std::function<bool(std::span<DocIdType>, std::span<SliceType>)>& cb) {
  size_t amountOfSeeks = 0;

  LOG_INDEX_UNIQUE_MERGER << "Calling main next() method";
  bool leftIteratorHasMore =
      leftIndex->_iter->position({keyCache.begin(), keyCache.begin() + 1});
  bool rightIteratorHasMore =
      rightIndex->_iter->position({keyCache.begin() + 1, keyCache.begin() + 2});
  LOG_INDEX_UNIQUE_MERGER << "Initial Left iterator has more: "
                          << std::boolalpha << leftIteratorHasMore
                          << ", Initial Right iterator has more: "
                          << std::boolalpha << rightIteratorHasMore;

  bool isDocsPopulated = false;

  while (leftIteratorHasMore && rightIteratorHasMore) {
    LOG_INDEX_UNIQUE_MERGER << "In the while loop";
    LOG_INDEX_UNIQUE_MERGER << "Left iterator has more: " << std::boolalpha
                            << leftIteratorHasMore
                            << ", Right iterator has more: " << std::boolalpha
                            << rightIteratorHasMore;
    LOG_INDEX_UNIQUE_MERGER << "Key cache left: " << keyCache[0]
                            << ", Key cache right: " << keyCache[1];

    auto leftKeySpan =
        std::span<SliceType>{keyCache.begin(), keyCache.begin() + 1};
    auto rightKeySpan =
        std::span<SliceType>{keyCache.begin() + 1, keyCache.begin() + 2};
    auto cmpResult = IndexStreamCompare{}.cmp(leftKeySpan, rightKeySpan);

    if (cmpResult == std::weak_ordering::less /*keyCache[0] < keyCache[1]*/) {
      LOG_INDEX_UNIQUE_MERGER << "Case: less";
      keyCache[0] = keyCache[1];
      leftIteratorHasMore =
          leftIndex->_iter->seek({keyCache.begin(), keyCache.begin() + 1});
      amountOfSeeks++;
      isDocsPopulated = false;
    } else if (cmpResult ==
               std::weak_ordering::greater /*keyCache[0] > keyCache[1]*/) {
      LOG_INDEX_UNIQUE_MERGER << "Case: greater";
      keyCache[1] = keyCache[0];
      rightIteratorHasMore =
          rightIndex->_iter->seek({keyCache.begin() + 1, keyCache.begin() + 2});
      amountOfSeeks++;
      isDocsPopulated = false;
    } else {
      LOG_INDEX_UNIQUE_MERGER << "Case: equal";
      TRI_ASSERT(cmpResult == std::weak_ordering::equivalent);
      if (!isDocsPopulated) {
        documentCache[0] = leftIndex->_iter->load(leftIndex->_projections);
        documentCache[1] = rightIndex->_iter->load(rightIndex->_projections);
      }

      bool readMore = cb(documentCache, projectionsSpan);

      LOG_INDEX_UNIQUE_MERGER << "Value Left: " << keyCache[0]
                              << ", Value Right: " << keyCache[1];
      leftIteratorHasMore =
          leftIndex->_iter->next({keyCache.begin(), keyCache.begin() + 1},
                                 documentCache[0], leftIndex->_projections);
      rightIteratorHasMore =
          rightIndex->_iter->next({keyCache.begin() + 1, keyCache.begin() + 2},
                                  documentCache[1], rightIndex->_projections);
      isDocsPopulated = true;

      if (!readMore) {
        LOG_INDEX_UNIQUE_MERGER << "next() returned true";
        return {true, amountOfSeeks};  // potentially more data available
      }
    }
  }

  // either left or right iterators are exhausted
  LOG_INDEX_UNIQUE_MERGER << "next() returned false";
  return {false, amountOfSeeks};
}

}  // namespace arangodb::aql
