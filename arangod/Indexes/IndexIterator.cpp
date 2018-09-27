////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "VocBase/LogicalCollection.h"

#include <velocypack/Slice.h>

using namespace arangodb;
  
IndexIterator::IndexIterator(LogicalCollection* collection, 
                             transaction::Methods* trx)
      : _collection(collection), 
        _trx(trx) { 
  TRI_ASSERT(_collection != nullptr);
  TRI_ASSERT(_trx != nullptr);
}

bool IndexIterator::nextDocument(DocumentCallback const& cb, size_t limit) {
  return next([this, &cb](LocalDocumentId const& token) {
    _collection->readDocumentWithCallback(_trx, token, cb);
  }, limit);
}

/// @brief default implementation for nextCovering
/// specialized index iterators can implement this method with some
/// sensible behavior
bool IndexIterator::nextCovering(DocumentCallback const&, size_t) {
  TRI_ASSERT(!hasCovering());
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "Requested covering values from an index that "
                                 "does not support it. This seems to be a bug "
                                 "in ArangoDB. Please report the query you are "
                                 "using + the indexes you have defined on the "
                                 "relevant collections to arangodb.com");
}

/// @brief default implementation for nextExtra
bool IndexIterator::nextExtra(ExtraCallback const&, size_t) {
  TRI_ASSERT(!hasExtra());
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "Requested extra values from an index that "
                                 "does not support it. This seems to be a bug "
                                 "in ArangoDB. Please report the query you are "
                                 "using + the indexes you have defined on the "
                                 "relevant collections to arangodb.com");
}

/// @brief default implementation for reset
void IndexIterator::reset() {}

/// @brief default implementation for skip
void IndexIterator::skip(uint64_t count, uint64_t& skipped) {
  // Skip the first count-many entries
  auto cb = [&skipped] (LocalDocumentId const& ) {
    ++skipped;
  };
  // TODO: Can be improved
  next(cb, count);
}

/// @brief Get the next elements
///        If one iterator is exhausted, the next one is used.
///        If callback is called less than limit many times
///        all iterators are exhausted
bool MultiIndexIterator::next(LocalDocumentIdCallback const& callback, size_t limit) {
  auto cb = [&limit, &callback] (LocalDocumentId const& token) {
    --limit;
    callback(token);
  };
  while (limit > 0) {
    if (_current == nullptr) {
      return false;
    }
    if (!_current->next(cb, limit)) {
      _currentIdx++;
      if (_currentIdx >= _iterators.size()) {
        _current = nullptr;
        return false;
      } else {
        _current = _iterators.at(_currentIdx);
      }
    }
  }
  return true;
}

/// @brief Get the next elements
///        If one iterator is exhausted, the next one is used.
///        If callback is called less than limit many times
///        all iterators are exhausted
bool MultiIndexIterator::nextCovering(DocumentCallback const& callback, size_t limit) {
  TRI_ASSERT(hasCovering());
  auto cb = [&limit, &callback] (LocalDocumentId const& token, arangodb::velocypack::Slice slice) {
    --limit;
    callback(token, slice);
  };
  while (limit > 0) {
    if (_current == nullptr) {
      return false;
    }
    if (!_current->nextCovering(cb, limit)) {
      _currentIdx++;
      if (_currentIdx >= _iterators.size()) {
        _current = nullptr;
        return false;
      } else {
        _current = _iterators.at(_currentIdx);
      }
    }
  }
  return true;
}

/// @brief Reset the cursor
///        This will reset ALL internal iterators and start all over again
void MultiIndexIterator::reset() {
  _current = _iterators[0];
  _currentIdx = 0;
  for (auto& it : _iterators) {
    it->reset();
  }
}
