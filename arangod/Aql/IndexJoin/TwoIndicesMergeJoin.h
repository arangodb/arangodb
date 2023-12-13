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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <memory>
#include <queue>

#include "Logger/LogMacros.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/IndexJoinStrategy.h"

namespace arangodb::aql {

struct DefaultComparatorTwo {
  template<typename U, typename V>
  auto operator()(U&& left, V&& right) const {
    return left <=> right;
  }
};

template<typename SliceType, typename DocIdType,
         typename KeyCompare = DefaultComparatorTwo>
struct TwoIndicesMergeJoin : IndexJoinStrategy<SliceType, DocIdType> {
  static constexpr size_t FIXED_NON_UNIQUE_INDEX_SIZE_VAR = 2;

  using StreamIteratorType = IndexStreamIterator<SliceType, DocIdType>;
  using Descriptor = IndexDescriptor<SliceType, DocIdType>;

  TwoIndicesMergeJoin(std::vector<Descriptor> descs);

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

  std::vector<IndexStreamData> indexes;

  bool positionAligned = false;

  std::array<DocIdType, 2> documentIds;
  std::vector<SliceType> sliceBuffer;
  std::vector<SliceType> currentKeySet;
  std::span<SliceType> projectionsSpan;

  // This iterator holds the current maximum position
  // of all (in this case exactly two) iterators
  IndexStreamData* maxIter = nullptr;
  // This is basically just the other iterator...
  IndexStreamData* minIter = nullptr;

  void updateMinMaxIterators();
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

#define LOG_IDX_TWO_NONU_MERGER LOG_DEVEL_IF(false)

template<typename SliceType, typename DocIdType, typename KeyCompare>
void TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::reset(
    std::span<SliceType> constants) {
  size_t constOffset = 0;
  for (auto& idx : indexes) {
    idx.reset(constants.subspan(constOffset, idx._constantSize));
    constOffset += idx._constantSize;
  }

  maxIter = nullptr;
  minIter = nullptr;
  for (auto& idx : indexes) {
    if (maxIter == nullptr ||
        IndexStreamCompare{}.cmp(*maxIter, idx) == std::weak_ordering::less) {
      if (maxIter != nullptr) {
        minIter = maxIter;
      }
      maxIter = &idx;
    } else {
      minIter = &idx;
    }
  }
  TRI_ASSERT(minIter != nullptr);
  TRI_ASSERT(maxIter != nullptr);
  TRI_ASSERT(minIter != maxIter);
  auto isAligned = IndexStreamCompare{}.cmp(*maxIter, *minIter) ==
                   std::weak_ordering::equivalent;
  if (!maxIter->exhausted && isAligned) {
    fillInitialMatch();
    positionAligned = true;
  }
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::weak_ordering
TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamCompare::cmp(
    IndexStreamData const& left, IndexStreamData const& right) {
  if (!left.exhausted && right.exhausted) {
    return std::weak_ordering::less;
  }

  return cmp(left._position, right._position);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::weak_ordering
TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamCompare::cmp(
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
bool TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamCompare::
operator()(IndexStreamData* left, IndexStreamData* right) const {
  return cmp(*left, *right) == std::weak_ordering::greater;
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamData::
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
  LOG_IDX_TWO_NONU_MERGER << "iter at " << _position[0];
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamData::
    seekTo(std::span<SliceType> target) {
  LOG_IDX_TWO_NONU_MERGER << "iterator seeking to " << target[0];
  std::copy(target.begin(), target.end(), _position.begin());
  exhausted = !_iter->seek(_position);

  LOG_IDX_TWO_NONU_MERGER << "iterator seeked to " << _position[0]
                          << " exhausted = " << exhausted;
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::IndexStreamData::
    reset(std::span<SliceType> constants) {
  exhausted = !_iter->reset(_position, constants);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
bool TwoIndicesMergeJoin<SliceType, DocIdType,
                         KeyCompare>::IndexStreamData::next() {
  return exhausted = !_iter->next(_position, _docId, _projections);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::TwoIndicesMergeJoin(
    std::vector<Descriptor> descs) {
  TRI_ASSERT(descs.size() == FIXED_NON_UNIQUE_INDEX_SIZE_VAR);
  indexes.reserve(FIXED_NON_UNIQUE_INDEX_SIZE_VAR);
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
TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::next(
    const std::function<bool(std::span<DocIdType>, std::span<SliceType>)>& cb) {
  size_t amountOfSeeks = 0;
  while (true) {
    if (positionAligned) {
      auto result = produceCrossProduct(cb);
      amountOfSeeks += result.seeks;
      if (!result.hasMore) {
        positionAligned = false;
        updateMinMaxIterators();
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
void TwoIndicesMergeJoin<SliceType, DocIdType,
                         KeyCompare>::updateMinMaxIterators() {
  TRI_ASSERT(minIter != nullptr);
  TRI_ASSERT(maxIter != nullptr);

  // clear and re-coordinate min and max iterators (if required)
  if (IndexStreamCompare{}.cmp(*maxIter, *minIter) ==
      std::weak_ordering::less) {
    std::swap(maxIter, minIter);
  }
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::pair<bool, size_t>
TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::advanceIndexes() {
  std::size_t amountOfSeeks = 0;

  auto incrementAndCheckMatch = [&](IndexStreamData& idx) -> bool {
    bool exhausted = idx.next();
    if (!exhausted) {
      LOG_IDX_TWO_NONU_MERGER << "next call on idx: " << &idx;
      for (auto k : idx._position) {
        LOG_IDX_TWO_NONU_MERGER << k;
      }

      LOG_IDX_TWO_NONU_MERGER
          << "diff = "
          << (0 == IndexStreamCompare{}.cmp(idx._position, currentKeySet));

      if (std::weak_ordering::equivalent ==
          IndexStreamCompare{}.cmp(idx._position, currentKeySet)) {
        return true;
      }
    }
    return false;
  };

  auto seekIndexAndCacheProjections = [&](IndexStreamData& idx,
                                          size_t position) {
    LOG_IDX_TWO_NONU_MERGER << "seeking iterator at position: " << position
                            << " to";
    for (auto k : currentKeySet) {
      LOG_IDX_TWO_NONU_MERGER << k;
    }
    idx.seekTo(currentKeySet);
    amountOfSeeks++;
    documentIds[position] = idx._iter->load(idx._projections);
  };

  bool foundMatch = incrementAndCheckMatch(indexes[0]);
  if (!foundMatch) {
    foundMatch = incrementAndCheckMatch(indexes[1]);
    if (!foundMatch) {
      // reached the end of this streak
      positionAligned = false;
      return {false, amountOfSeeks};
    }
    seekIndexAndCacheProjections(indexes[0], 0);
    return {true, amountOfSeeks};
  }

  return {false, amountOfSeeks};
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
auto TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::produceCrossProduct(
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

    LOG_IDX_TWO_NONU_MERGER << std::boolalpha << "readMore = " << readMore
                            << " hasMore = " << hasMore;
  }

  return {.readMore = readMore, .hasMore = hasMore, .seeks = amountOfSeeks};
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
void TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::fillInitialMatch() {
  for (std::size_t i = 0; i < indexes.size(); i++) {
    auto& index = indexes[i];
    documentIds[i] = index._iter->load(index._projections);
  }
  indexes.rbegin()->_iter->cacheCurrentKey(currentKeySet);
}

template<typename SliceType, typename DocIdType, typename KeyCompare>
std::pair<bool, size_t>
TwoIndicesMergeJoin<SliceType, DocIdType, KeyCompare>::findCommonPosition() {
  LOG_IDX_TWO_NONU_MERGER << "find common position";
  size_t amountOfSeeks = 0;

  while (true) {
    // get the minimum
    auto minIndex = minIter;
    LOG_IDX_TWO_NONU_MERGER << "min is " << minIndex->_position[0]
                            << " exhausted = " << std::boolalpha
                            << minIndex->exhausted;
    LOG_IDX_TWO_NONU_MERGER << "max is " << maxIter->_position[0]
                            << " exhausted = " << std::boolalpha
                            << maxIter->exhausted;

    if (maxIter->exhausted) {
      return {false, amountOfSeeks};
    }

    // check if min == max
    if (IndexStreamCompare::cmp(*minIndex, *maxIter) ==
        std::weak_ordering::equivalent) {
      // all iterators are at the same position
      break;
    }

    // seek this index to the maximum

    LOG_IDX_TWO_NONU_MERGER << "seeking to " << maxIter->_position[0];
    minIndex->seekTo(maxIter->_position);
    amountOfSeeks++;

    if (minIndex->exhausted) {
      LOG_IDX_TWO_NONU_MERGER << "iterator exhausted ";
      return {false, amountOfSeeks};  // we are done
    }

    LOG_IDX_TWO_NONU_MERGER << "seeked to position " << minIndex->_position[0];
    // check if we have a new maximum
    if (IndexStreamCompare{}.cmp(*maxIter, *minIndex) ==
        std::weak_ordering::less) {
      LOG_IDX_TWO_NONU_MERGER << "new max iter ";
      std::swap(minIter, maxIter);
    }
  }

  LOG_IDX_TWO_NONU_MERGER << "common position found: ";
  for (auto const& idx : indexes) {
    LOG_IDX_TWO_NONU_MERGER << idx._position[0];
  }

  return {true, amountOfSeeks};
}

}  // namespace arangodb::aql
