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
//
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <span>
#include <memory>
#include <queue>
#include <functional>

namespace arangodb {

struct IndexStreamOptions {
  std::vector<std::size_t> usedKeyFields;
  std::vector<std::size_t> projectedFields;
};

template<typename SliceType, typename DocIdType>
struct IndexStreamIterator {
  virtual ~IndexStreamIterator() = default;
  // load the current position into the span.
  // returns false if the end of the index range was reached.
  virtual bool position(std::span<SliceType>) const = 0;
  // seek to the given position. The span is updated with the actual new
  // position. returns false if the end of the index range was reached.
  virtual bool seek(std::span<SliceType>) = 0;

  // loads the document id and fills the projections (if any)
  virtual DocIdType load(std::span<SliceType> projections) const = 0;

  // load the current position into the span.
  // returns false if either the index is exhausted or the next element has a
  // different key set. In that case `key` is updated with the found key.
  // returns true if a entry with key was found. LocalDocumentId and
  // projections are loaded.

  enum class NextResult {
    kHasMore,
    kRangeExhausted,
    kIteratorExhausted,
  };
  virtual NextResult next(std::span<SliceType> key, DocIdType&,
                          std::span<SliceType> projections) = 0;
};

struct DefaultComparator {
  template<typename U, typename V>
  auto operator()(U&& left, V&& right) const {
    return left <=> right;
  }
};

template<typename SliceType, typename DocIdType,
         typename KeyCompare = DefaultComparator>
struct IndexMerger {
  using StreamIteratorType = IndexStreamIterator<SliceType, DocIdType>;

  struct IndexDescriptor {
    explicit IndexDescriptor(std::unique_ptr<StreamIteratorType> iter,
                             std::size_t numProjections)
        : iter(std::move(iter)), numProjections(numProjections) {}
    std::unique_ptr<StreamIteratorType> iter;
    std::size_t numProjections;
  };

  IndexMerger(std::vector<IndexDescriptor> descs, std::size_t numKeyComponents);

  bool next(std::function<bool(std::span<DocIdType>,
                               std::span<SliceType>)> const& cb);

 private:
  struct IndexStreamData {
    std::unique_ptr<StreamIteratorType> _iter;
    std::span<SliceType> _position;
    std::span<SliceType> _projections;
    DocIdType& _docId;
    bool exhausted{false};

    IndexStreamData(std::unique_ptr<StreamIteratorType> iter,
                    std::span<SliceType> position,
                    std::span<SliceType> projections, DocIdType& docId);
    void seekTo(std::span<SliceType> target);
    bool next();
  };

  struct IndexStreamCompare {
    static std::weak_ordering cmp(IndexStreamData const& left,
                                  IndexStreamData const& right);

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

  IndexMinHeap minHeap;
  IndexStreamData* maxIter = nullptr;

  void updateHeap();
  bool advanceIndexes();

  struct ProduceProductResult {
    bool readMore;  // user want to read more data
    bool hasMore;   // iterator has more data
  };

  ProduceProductResult produceCrossProduct(
      std::function<bool(std::span<DocIdType>, std::span<SliceType>)> const&
          cb);

  void fillInitialMatch();
  bool findCommonPosition();
};

namespace velocypack {
class Slice;
}

struct VPackSliceComparator;
class LocalDocumentId;

extern template struct IndexMerger<velocypack::Slice, LocalDocumentId,
                                   VPackSliceComparator>;
using AqlIndexMerger =
    IndexMerger<velocypack::Slice, LocalDocumentId, VPackSliceComparator>;
struct AqlIndexStreamInterface : AqlIndexMerger::StreamIteratorType {};

}  // namespace arangodb
