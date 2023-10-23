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
#include <functional>
#include <span>
#include <memory>

#include "Aql/IndexStreamIterator.h"

namespace arangodb {
namespace velocypack {
class Slice;
}

class LocalDocumentId;

namespace aql {

template<typename SliceType, typename DocIdType>
struct IndexJoinStrategy {
  virtual ~IndexJoinStrategy() = default;
  virtual bool next(std::function<bool(std::span<DocIdType>,
                                       std::span<SliceType>)> const& cb) = 0;

  virtual void reset() = 0;
};

template<typename SliceType, typename DocIdType>
struct IndexDescriptor {
  using StreamIteratorType = IndexStreamIterator<SliceType, DocIdType>;

  IndexDescriptor() = default;
  explicit IndexDescriptor(std::unique_ptr<StreamIteratorType> iter,
                           std::size_t numProjections, bool isUnique)
      : iter(std::move(iter)),
        numProjections(numProjections),
        isUnique(isUnique) {}
  std::unique_ptr<StreamIteratorType> iter;
  std::size_t numProjections{0};
  bool isUnique{false};
};

using AqlIndexJoinStrategy =
    IndexJoinStrategy<velocypack::Slice, LocalDocumentId>;

struct IndexJoinStrategyFactory {
  using Descriptor = IndexDescriptor<velocypack::Slice, LocalDocumentId>;

  std::unique_ptr<AqlIndexJoinStrategy> createStrategy(
      std::vector<Descriptor>, std::size_t numKeyComponents);
};

}  // namespace aql
}  // namespace arangodb
