////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/StringHeap.h"
#include "VocBase/ManagedDocumentResult.h"

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
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
struct AqlValue;
class QueryContext;
}  // namespace aql

namespace graph {

struct EdgeDocumentToken;

/// Small wrapper around the actual datastore in
/// which edges and vertices are stored. The cluster can overwrite this
/// with an implementation which caches entire documents,
/// the single server / db server can just work with raw
/// document tokens and retrieve documents as needed
struct BaseOptions;

class TraverserCache {
 public:
  explicit TraverserCache(aql::QueryContext& query, BaseOptions* opts);

  virtual ~TraverserCache();

  /// @brief clears all allocated memory in the underlying StringHeap
  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document stored within the token
  ///        into the given builder.
  //////////////////////////////////////////////////////////////////////////////
  virtual void insertEdgeIntoResult(graph::EdgeDocumentToken const& etkn,
                                    velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return AQL value containing the result
  ///        The document will be looked up in the StorageEngine
  //////////////////////////////////////////////////////////////////////////////
  virtual aql::AqlValue fetchEdgeAqlResult(graph::EdgeDocumentToken const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Append the vertex for the given id
  ///        The document will be looked up in the StorageEngine
  //////////////////////////////////////////////////////////////////////////////
  virtual bool appendVertex(std::string_view idString,
                            arangodb::velocypack::Builder& result);
  virtual bool appendVertex(std::string_view idString,
                            arangodb::aql::AqlValue& result);

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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Persist the given id string. The return value is guaranteed to
  ///        stay valid as long as this cache is valid
  //////////////////////////////////////////////////////////////////////////////
  std::string_view persistString(std::string_view idString);

  arangodb::velocypack::HashedStringRef persistString(
      arangodb::velocypack::HashedStringRef idString);

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

  /// Only valid until the next call to this class
  virtual velocypack::Slice lookupToken(EdgeDocumentToken const& token);

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reusable ManagedDocumentResult that temporarily takes
  ///        responsibility for one document.
  //////////////////////////////////////////////////////////////////////////////
  ManagedDocumentResult _mmdr;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Query used to register warnings to.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::aql::QueryContext& _query;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transaction to access data, This class is NOT responsible for it.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::transaction::Methods* _trx;

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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Stringheap to take care of _id strings, s.t. they stay valid
  ///        during the entire traversal.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::StringHeap _stringHeap;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set of all strings persisted in the stringHeap. So we can save some
  ///        memory by not storing them twice.
  //////////////////////////////////////////////////////////////////////////////
  std::unordered_set<arangodb::velocypack::HashedStringRef> _persistedStrings;

  BaseOptions const* _baseOptions;

  /// @brief whether or not to allow adding of previously unknown collections
  /// during the traversal
  bool const _allowImplicitCollections;
};

}  // namespace graph
}  // namespace arangodb
