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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Graph/BaseOptions.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>

#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace arangodb {

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}  // namespace velocypack

namespace aql {
struct AqlValue;
class QueryContext;
}  // namespace aql

namespace graph {

/// Small wrapper around the actual datastore in
/// which edges and vertices are stored. The cluster can overwrite this
/// with an implementation which caches entire documents,
/// the single server / db server can just work with raw
/// document tokens and retrieve documents as needed

class TraverserCache {
 public:
  explicit TraverserCache()
      : _insertedDocuments(0),
        _filtered(0),
        _cursorsCreated(0),
        _cursorsRearmed(0),
        _cacheHits(0),
        _cacheMisses(0) {}

  virtual ~TraverserCache() = default;

  [[nodiscard]] std::uint64_t getAndResetInsertedDocuments() {
    return std::exchange(_insertedDocuments, 0);
  }

  [[nodiscard]] std::uint64_t getAndResetFiltered() {
    return std::exchange(_filtered, 0);
  }

  [[nodiscard]] std::uint64_t getAndResetCursorsCreated() {
    return std::exchange(_cursorsCreated, 0);
  }

  [[nodiscard]] std::uint64_t getAndResetCursorsRearmed() {
    return std::exchange(_cursorsRearmed, 0);
  }

  [[nodiscard]] std::uint64_t getAndResetCacheHits() {
    return std::exchange(_cacheHits, 0);
  }

  [[nodiscard]] std::uint64_t getAndResetCacheMisses() {
    return std::exchange(_cacheMisses, 0);
  }

  void incrDocuments(std::uint64_t value = 1) noexcept {
    _insertedDocuments += value;
  }
  void incrFiltered(std::uint64_t value = 1) noexcept { _filtered += value; }
  void incrCursorsCreated(std::uint64_t value = 1) noexcept {
    _cursorsCreated += value;
  }
  void incrCursorsRearmed(std::uint64_t value = 1) noexcept {
    _cursorsRearmed += value;
  }
  void incrCacheHits(std::uint64_t value = 1) noexcept { _cacheHits += value; }
  void incrCacheMisses(std::uint64_t value = 1) noexcept {
    _cacheMisses += value;
  }

 protected:
  /// @brief Documents inserted in this cache
  std::uint64_t _insertedDocuments;
  /// @brief Documents filtered
  std::uint64_t _filtered;
  /// @brief number of cursor objects created
  std::uint64_t _cursorsCreated;
  /// @brief number of existing cursor objects that were rearmed
  std::uint64_t _cursorsRearmed;
  /// @brief number of cache lookup hits
  std::uint64_t _cacheHits;
  /// @brief number of cache lookup misses
  std::uint64_t _cacheMisses;
};

}  // namespace graph
}  // namespace arangodb
