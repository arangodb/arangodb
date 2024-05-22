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

struct DefaultComparator {
  template<typename U, typename V>
  auto operator()(U&& left, V&& right) const {
    return left <=> right;
  }
};

template<typename SliceType, typename DocIdType,
         typename KeyCompare = DefaultComparator>
struct GenericMergeJoin : IndexJoinStrategy<SliceType, DocIdType> {
  using StreamIteratorType = IndexStreamIterator<SliceType, DocIdType>;
  using Descriptor = IndexDescriptor<SliceType, DocIdType>;

  GenericMergeJoin(std::vector<Descriptor> descs);

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
    void seekTo(std::span<SliceType> target);
    bool next();
    void reset(std::span<SliceType> constants);
  };

  struct IndexStreamCompare {
    static std::weak_ordering cmp(IndexStreamData const& left,
                                  IndexStreamData const& right);
    static std::weak_ordering cmp(std::span<SliceType> left,
                                  std::span<SliceType> right);

    bool operator()(IndexStreamData* left, IndexStreamData* right) const;
  };

  struct IndexMinHeap
      : std::priority_queue<IndexStreamData*, std::vector<IndexStreamData*>,
                            IndexStreamCompare> {
    void clear() { this->c.clear(); }
  };

  std::vector<IndexStreamData> indexes;

  bool positionAligned = false;

  std::vector<DocIdType> documentIds;
  std::vector<SliceType> sliceBuffer;
  std::vector<SliceType> currentKeySet;
  std::span<SliceType> projectionsSpan;

  IndexMinHeap minHeap;
  IndexStreamData* maxIter = nullptr;

  void updateHeap();
  std::pair<bool, size_t> advanceIndexes();

  struct ProduceProductResult {
    bool readMore;  // user want to read more data
    bool hasMore;   // iterator has more data
    size_t seeks;   // amount of seeks performed
  };

  ProduceProductResult produceCrossProduct(
      std::function<bool(std::span<DocIdType>, std::span<SliceType>)> const&
          cb);

  void fillInitialMatch();
  std::pair<bool, size_t> findCommonPosition();
};

#define LOG_INDEX_MERGER LOG_DEVEL_IF(false)

template<typename SliceType, typename DocIdType, typename KeyCompare>
void GenericMergeJoin<SliceType, DocIdType, KeyCompare>::reset(
    std::span<SliceType> constants) {
  LOG_INDEX_MERGER << "Calling reset in GenericMergeJoin:";

  size_t constOffset = 0;
  for (auto& idx : indexes) {
    idx.reset(constants.subspan(constOffset, idx._constantSize));
    constOffset += idx._constantSize;
  }

  maxIter = nullptr;
  minHeap.clear();
  for (auto& idx : indexes) {
    if (maxIter == nullptr ||
        IndexStreamCompare{}.cmp(*maxIter, idx) == std::weak_ordering::less) {
      maxIter = &idx;
    }
    minHeap.push(&idx);
  }

  auto isAligned = IndexStreamCompare{}.cmp(*maxIter, *minHeap.top()) ==
                   std::weak_ordering::equivalent;
  if (!maxIter->exhausted && isAligned) {
    fillInitialMatch();
    positionAligned = true;
  }
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::weak_ordering
GenericMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamCompare::cmp(
    IndexStreamData const& left, IndexStreamData const& right) {
  if (!left.exhausted && right.exhausted) {
    return std::weak_ordering::less;
  }

  return cmp(left._position, right._position);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::weak_ordering
GenericMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamCompare::cmp(
    std::span<SliceType> left, std::span<SliceType> right) {
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
bool GenericMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamCompare::
operator()(IndexStreamData* left, IndexStreamData* right) const {
  return cmp(*left, *right) == std::weak_ordering::greater;
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
GenericMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamData::
    IndexStreamData(std::unique_ptr<StreamIteratorType> iter,
                    std::span<SliceType> position,
                    std::span<SliceType> projections, DocIdType& docId,
                    std::size_t constantSize)
    : _iter(std::move(iter)),
      _position(position),
      _projections(projections),
      _docId(docId),
      _constantSize(constantSize) {
  exhausted = !_iter->position(_position);
  LOG_INDEX_MERGER << "iter at " << _position[0];
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void GenericMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamData::
    seekTo(std::span<SliceType> target) {
  LOG_INDEX_MERGER << "iterator seeking to " << target[0];
  std::copy(target.begin(), target.end(), _position.begin());
  exhausted = !_iter->seek(_position);

  LOG_INDEX_MERGER << "iterator seeked to " << _position[0]
                   << " exhausted = " << std::boolalpha << exhausted;
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void GenericMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamData::reset(
    std::span<SliceType> constants) {
  exhausted = !_iter->reset(_position, constants);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
bool GenericMergeJoin<SliceType, DocIdType,
                      KeyCompare>::IndexStreamData::next() {
  return exhausted = !_iter->next(_position, _docId, _projections);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
GenericMergeJoin<SliceType, DocIdType, KeyCompare>::GenericMergeJoin(
    std::vector<Descriptor> descs) {
  indexes.reserve(descs.size());
  documentIds.resize(descs.size());
  currentKeySet.resize(descs[0].numKeyComponents);

  std::size_t bufferSize = 0;
  for (auto const& desc : descs) {
    bufferSize += desc.numProjections + desc.numKeyComponents;
  }

  sliceBuffer.resize(bufferSize);
  auto keySliceIter = sliceBuffer.begin();
  auto projectionsIter =
      sliceBuffer.begin() + descs[0].numKeyComponents * descs.size();
  // Note: Access to descs[0] is safe because we have asserted that descs.size()
  // == 2 and right now we only do support joins on exact one join-key. As soon
  // as we extend this to support joins on multiple keys, we need to change this
  // code here.

  projectionsSpan = {projectionsIter, sliceBuffer.end()};
  auto docIdIter = documentIds.begin();
  for (auto& desc : descs) {
    auto projections = projectionsIter;
    projectionsIter += desc.numProjections;
    auto keyBuffer = keySliceIter;
    keySliceIter += desc.numKeyComponents;
    indexes.emplace_back(std::move(desc.iter),
                         std::span<SliceType>{keyBuffer, keySliceIter},
                         std::span<SliceType>{projections, projectionsIter},
                         *(docIdIter++), desc.numConstants);
  }
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::pair<bool, std::size_t>
GenericMergeJoin<SliceType, DocIdType, KeyCompare>::next(
    const std::function<bool(std::span<DocIdType>, std::span<SliceType>)>& cb) {
  size_t amountOfSeeks = 0;
  while (true) {
    if (positionAligned) {
      auto result = produceCrossProduct(cb);
      amountOfSeeks += result.seeks;
      if (!result.hasMore) {
        positionAligned = false;
        updateHeap();
        if (maxIter->exhausted) {
          return {false, amountOfSeeks};
        }
      }
      if (!result.readMore) {
        return {true, amountOfSeeks};  // potentially more data available
      }
    } else {
      auto [hasMore, amountSeeksLocal] = findCommonPosition();
      amountOfSeeks += amountSeeksLocal;

      if (!hasMore) {
        return {false, amountOfSeeks};
      }
      positionAligned = true;
      fillInitialMatch();
    }
  }
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void GenericMergeJoin<SliceType, DocIdType, KeyCompare>::updateHeap() {
  // clear and rebuild heap
  minHeap.clear();
  for (auto& idx : indexes) {
    if (IndexStreamCompare{}.cmp(*maxIter, idx) == std::weak_ordering::less) {
      maxIter = &idx;
    }
    minHeap.push(&idx);
  }
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::pair<bool, size_t>
GenericMergeJoin<SliceType, DocIdType, KeyCompare>::advanceIndexes() {
  std::size_t where = 0;
  std::size_t amountOfSeeks = 0;

  while (true) {
    if (where == indexes.size()) {
      // reached the end of this streak
      positionAligned = false;
      return {false, amountOfSeeks};
    }

    auto& idx = indexes[where];
    bool exhausted = idx.next();
    if (!exhausted) {
      LOG_INDEX_MERGER << "at position " << where;
      for (auto k : indexes[where]._position) {
        LOG_INDEX_MERGER << k;
      }

      LOG_INDEX_MERGER << "at rbegin ";
      for (auto k : indexes.rbegin()->_position) {
        LOG_INDEX_MERGER << k;
      }

      LOG_INDEX_MERGER << "diff = "
                       << (0 == IndexStreamCompare{}.cmp(
                                    indexes[where]._position, currentKeySet));

      if (0 ==
          IndexStreamCompare{}.cmp(indexes[where]._position, currentKeySet)) {
        break;
      }
    }

    where += 1;
  }

  while (where > 0) {
    where -= 1;
    auto& idx = indexes[where];
    LOG_INDEX_MERGER << "seeking iterator at " << where << " to";
    for (auto k : currentKeySet) {
      LOG_INDEX_MERGER << k;
    }
    idx.seekTo(currentKeySet);
    amountOfSeeks++;
    documentIds[where] = idx._iter->load(idx._projections);
  }

  return {true, amountOfSeeks};
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
auto GenericMergeJoin<SliceType, DocIdType, KeyCompare>::produceCrossProduct(
    const std::function<bool(std::span<DocIdType>, std::span<SliceType>)>& cb)
    -> ProduceProductResult {
  bool readMore = true;
  bool hasMore = true;
  size_t amountOfSeeks = 0;

  while (readMore && hasMore) {
    // produce current position
    readMore = cb(documentIds, projectionsSpan);
    auto [hasMoreLocal, amountOfSeeksLocal] = advanceIndexes();
    hasMore = hasMoreLocal;
    amountOfSeeks += amountOfSeeksLocal;

    LOG_INDEX_MERGER << std::boolalpha << "readMore = " << readMore
                     << " hasMore = " << hasMore;
  }

  return {.readMore = readMore, .hasMore = hasMore, .seeks = amountOfSeeks};
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void GenericMergeJoin<SliceType, DocIdType, KeyCompare>::fillInitialMatch() {
  for (std::size_t i = 0; i < indexes.size(); i++) {
    auto& index = indexes[i];
    documentIds[i] = index._iter->load(index._projections);
  }
  indexes.rbegin()->_iter->cacheCurrentKey(currentKeySet);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::pair<bool, size_t>
GenericMergeJoin<SliceType, DocIdType, KeyCompare>::findCommonPosition() {
  LOG_INDEX_MERGER << "find common position";
  size_t amountOfSeeks = 0;

  while (true) {
    // get the minimum
    TRI_ASSERT(minHeap.size() > 0);
    auto minIndex = minHeap.top();
    LOG_INDEX_MERGER << "Min Heap Size is: " << minHeap.size();
    LOG_INDEX_MERGER << "min is " << minIndex->_position[0]
                     << " exhausted = " << std::boolalpha << minIndex->exhausted
                     << ", address: " << minIndex;
    LOG_INDEX_MERGER << "max is " << maxIter->_position[0]
                     << " exhausted = " << std::boolalpha << maxIter->exhausted
                     << ", address: " << maxIter;

    if (maxIter->exhausted) {
      LOG_INDEX_MERGER << "Max iter is exhausted";
      return {false, amountOfSeeks};
    }

    // check if min == max
    if (IndexStreamCompare::cmp(*minIndex, *maxIter) ==
        std::weak_ordering::equivalent) {
      LOG_INDEX_MERGER << "min and max are equivalent";
      // all iterators are at the same position
      break;
    }

    // remove the minimum, we want to modify it
    minHeap.pop();

    // seek this index to the maximum

    LOG_INDEX_MERGER << "seeking to " << maxIter->_position[0];
    minIndex->seekTo(maxIter->_position);
    amountOfSeeks++;

    if (minIndex->exhausted) {
      LOG_INDEX_MERGER << "iterator exhausted ";
      return {false, amountOfSeeks};  // we are done
    }

    LOG_INDEX_MERGER << "seeked to position " << minIndex->_position[0];
    // check if we have a new maximum
    if (IndexStreamCompare{}.cmp(*maxIter, *minIndex) ==
        std::weak_ordering::less) {
      LOG_INDEX_MERGER << "new max iter ";
      maxIter = minIndex;
    }

    // insert with updated position into heap
    minHeap.push(minIndex);
  }

  LOG_INDEX_MERGER << "common position found: ";
  for (auto const& idx : indexes) {
    LOG_INDEX_MERGER << idx._position[0]
                     << " -> Position: " << idx._position[0];
  }

  return {true, amountOfSeeks};
}

}  // namespace arangodb::aql
