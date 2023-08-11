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
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "IndexIterator.h"
#include "Indexes/Index.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Slice.h>

using namespace arangodb;

IndexIterator::IndexIterator(LogicalCollection* collection,
                             transaction::Methods* trx,
                             ReadOwnWrites readOwnWrites)
    : _collection(collection),
      _trx(trx),
      _cacheHits(0),
      _cacheMisses(0),
      _hasMore(true),
      _readOwnWrites(readOwnWrites) {
  TRI_ASSERT(_collection != nullptr);
  TRI_ASSERT(_trx != nullptr);
}

/// @brief returns cache hits (first) and misses (second) statistics, and
/// resets their values to 0
std::pair<std::uint64_t, std::uint64_t>
IndexIterator::getAndResetCacheStats() noexcept {
  std::pair<std::uint64_t, std::uint64_t> result{_cacheHits, _cacheMisses};
  _cacheHits = 0;
  _cacheMisses = 0;
  return result;
}

/// @brief Skip the next toSkip many elements.
///        skipped will be increased by the amount of skipped elements
///        afterwards Check hasMore()==true before using this NOTE: This will
///        throw on OUT_OF_MEMORY
void IndexIterator::skip(uint64_t toSkip, uint64_t& skipped) {
  if (_hasMore) {
    skipImpl(toSkip, skipped);
    if (skipped != toSkip) {
      _hasMore = false;
    }
  }
}

void IndexIterator::skipAll(uint64_t& skipped) {
  while (_hasMore) {
    uint64_t skippedLocal = 0;
    skipImpl(internalBatchSize, skippedLocal);
    if (skippedLocal != internalBatchSize) {
      _hasMore = false;
    }
    skipped += skippedLocal;
  }
}

/// @brief default implementation for rearm
/// specialized index iterators can implement this method with some
/// sensible behavior
bool IndexIterator::rearmImpl(arangodb::aql::AstNode const*,
                              arangodb::aql::Variable const*,
                              IndexIteratorOptions const&) {
  TRI_ASSERT(canRearm());
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, std::string("requested rearming from an index "
                                      "iterator that does not support it (") +
                              std::string{typeName()} + ")");
}

/// @brief default implementation for rearm
/// specialized index iterators can implement this method with some
/// sensible behavior
bool IndexIterator::rearmImpl(velocypack::Slice, IndexIteratorOptions const&) {
  TRI_ASSERT(canRearm());
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, std::string("requested rearming from an index "
                                      "iterator that does not support it (") +
                              std::string{typeName()} + ")");
}

/// @brief default implementation for nextImpl
bool IndexIterator::nextImpl(LocalDocumentIdCallback const&,
                             uint64_t /*limit*/) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "requested next values from an index iterator "
                                 "that does not support it (" +
                                     std::string{typeName()} + ")");
}

bool IndexIterator::nextDocumentImpl(DocumentCallback const& cb,
                                     uint64_t limit) {
  return nextImpl(
      [this, &cb](LocalDocumentId const& token) {
        return _collection->getPhysical()
            ->read(_trx, token, cb, _readOwnWrites)
            .ok();
      },
      limit);
}

/// @brief default implementation for nextCovering
/// specialized index iterators can implement this method with some
/// sensible behavior
bool IndexIterator::nextCoveringImpl(CoveringCallback const&,
                                     uint64_t /*limit*/) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "requested next covering values from an index "
                                 "iterator that does not support it (" +
                                     std::string{typeName()} + ")");
}

/// @brief default implementation for skip
void IndexIterator::skipImpl(uint64_t count, uint64_t& skipped) {
  // Skip the first count-many entries.
  // It is possible that `nextImpl` does not actually move `count` steps
  // forward, however, it will then say so by returning `true` for "there
  // is more to get"! In this case, we need to check, if we have already
  // skipped the requested count, and if not, try again (with a potentially
  // reduced count):
  uint64_t skippedInitial = skipped;
  do {
    TRI_ASSERT(skipped >= skippedInitial && skipped - skippedInitial <= count);
    if (!nextImpl(
            [&skipped](LocalDocumentId const&) {
              ++skipped;
              return true;
            },
            count - (skipped - skippedInitial))) {
      return;
    }
  } while (skipped - skippedInitial < count);
}
