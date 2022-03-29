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

void IndexIterator::reset() {
  // intentionally do not reset cache statistics here.
  _hasMore = true;
  resetImpl();
}

/// @brief Calls cb for the next batchSize many elements
///        NOTE: This will throw on OUT_OF_MEMORY
bool IndexIterator::next(LocalDocumentIdCallback const& callback,
                         uint64_t batchSize) {
  if (_hasMore) {
    TRI_ASSERT(batchSize != UINT64_MAX);
    _hasMore = nextImpl(callback, static_cast<size_t>(batchSize));
  }
  return _hasMore;
}

bool IndexIterator::nextDocument(DocumentCallback const& callback,
                                 uint64_t batchSize) {
  if (_hasMore) {
    TRI_ASSERT(batchSize != UINT64_MAX);
    _hasMore = nextDocumentImpl(callback, static_cast<size_t>(batchSize));
  }
  return _hasMore;
}

bool IndexIterator::nextCovering(CoveringCallback const& callback,
                                 uint64_t batchSize) {
  if (_hasMore) {
    TRI_ASSERT(batchSize != UINT64_MAX);
    _hasMore = nextCoveringImpl(callback, static_cast<size_t>(batchSize));
  }
  return _hasMore;
}

bool IndexIterator::rearm(arangodb::aql::AstNode const* node,
                          arangodb::aql::Variable const* variable,
                          IndexIteratorOptions const& opts) {
  // intentionally do not reset cache statistics here.
  _hasMore = true;
  if (rearmImpl(node, variable, opts)) {
    reset();
    return true;
  }
  return false;
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

/// @brief Skip all elements.
///        skipped will be increased by the amount of skipped elements
///        afterwards Check hasMore()==true before using this NOTE: This will
///        throw on OUT_OF_MEMORY
void IndexIterator::skipAll(uint64_t& skipped) {
  size_t toSkip = 1000;

  while (_hasMore) {
    uint64_t skippedLocal = 0;
    skipImpl(toSkip, skippedLocal);
    if (skippedLocal != toSkip) {
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
      TRI_ERROR_INTERNAL,
      "requested rearming of an index iterator that does not support it");
}

/// @brief default implementation for nextImpl
bool IndexIterator::nextImpl(LocalDocumentIdCallback const&, size_t /*limit*/) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "requested next values from an index iterator that does not support it");
}

bool IndexIterator::nextDocumentImpl(DocumentCallback const& cb, size_t limit) {
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
                                     size_t /*limit*/) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string("requested next covering values from an index "
                  "iterator that does not support it (") +
          typeName() + ")");
}

/// @brief default implementation for skip
void IndexIterator::skipImpl(uint64_t count, uint64_t& skipped) {
  // Skip the first count-many entries
  nextImpl(
      [&skipped](LocalDocumentId const&) {
        ++skipped;
        return true;
      },
      count);
}

/// @brief Get the next elements
///        If one iterator is exhausted, the next one is used.
///        If callback is called less than limit many times
///        all iterators are exhausted
bool MultiIndexIterator::nextImpl(LocalDocumentIdCallback const& callback,
                                  size_t limit) {
  auto cb = [&limit, &callback](LocalDocumentId const& token) {
    if (callback(token)) {
      --limit;
      return true;
    }
    return false;
  };
  while (limit > 0) {
    if (_current == nullptr) {
      return false;
    }
    if (!_current->nextImpl(cb, limit)) {
      _currentIdx++;
      if (_currentIdx >= _iterators.size()) {
        _current = nullptr;
        return false;
      }
      _current = _iterators.at(_currentIdx).get();
    }
  }
  return true;
}

/// @brief Get the next elements
///        If one iterator is exhausted, the next one is used.
///        If callback is called less than limit many times
///        all iterators are exhausted
bool MultiIndexIterator::nextDocumentImpl(DocumentCallback const& callback,
                                          size_t limit) {
  auto cb = [&limit, &callback](LocalDocumentId const& token,
                                arangodb::velocypack::Slice slice) {
    if (callback(token, slice)) {
      --limit;
      return true;
    }
    return false;
  };
  while (limit > 0) {
    if (_current == nullptr) {
      return false;
    }
    if (!_current->nextDocumentImpl(cb, limit)) {
      _currentIdx++;
      if (_currentIdx >= _iterators.size()) {
        _current = nullptr;
        return false;
      }
      _current = _iterators[_currentIdx].get();
    }
  }
  return true;
}

/// @brief Get the next elements
///        If one iterator is exhausted, the next one is used.
///        If callback is called less than limit many times
///        all iterators are exhausted
bool MultiIndexIterator::nextCoveringImpl(CoveringCallback const& callback,
                                          size_t limit) {
  auto cb = [&limit, &callback](LocalDocumentId const& token,
                                IndexIteratorCoveringData& data) {
    if (callback(token, data)) {
      --limit;
      return true;
    }
    return false;
  };
  while (limit > 0) {
    if (_current == nullptr) {
      return false;
    }
    if (!_current->nextCoveringImpl(cb, limit)) {
      _currentIdx++;
      if (_currentIdx >= _iterators.size()) {
        _current = nullptr;
        return false;
      }
      _current = _iterators[_currentIdx].get();
    }
  }
  return true;
}

/// @brief Reset the cursor
///        This will reset ALL internal iterators and start all over again
void MultiIndexIterator::resetImpl() {
  _current = _iterators[0].get();
  _currentIdx = 0;
  for (auto& it : _iterators) {
    it->resetImpl();
  }
}
