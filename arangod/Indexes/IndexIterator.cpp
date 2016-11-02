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
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"

using namespace arangodb;
  
IndexIterator::IndexIterator(LogicalCollection* collection, 
                             arangodb::Transaction* trx, 
                             ManagedDocumentResult* mmdr, 
                             arangodb::Index const* index)
      : _collection(collection), 
        _trx(trx), 
        _mmdr(mmdr ? mmdr : new ManagedDocumentResult(trx)), 
        _context(trx, collection, _mmdr, index->fields().size()),
        _responsible(mmdr == nullptr) {
  TRI_ASSERT(_collection != nullptr);
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(_mmdr != nullptr);
}

/// @brief default destructor. Does not free anything
IndexIterator::~IndexIterator() {
  if (_responsible) {
    delete _mmdr;
  }
}

/// @brief default implementation for next
IndexLookupResult IndexIterator::next() { return IndexLookupResult(); }

/// @brief default implementation for nextBabies
void IndexIterator::nextBabies(std::vector<IndexLookupResult>& result, size_t batchSize) {
  result.clear();

  if (batchSize > 0) {
    while (true) {
      IndexLookupResult element = next();
      if (!element) {
        return;
      }
      result.emplace_back(element);
      batchSize--;
      if (batchSize == 0) {
        return;
      }
    }
  }
}

/// @brief default implementation for reset
void IndexIterator::reset() {}

/// @brief default implementation for skip
void IndexIterator::skip(uint64_t count, uint64_t& skipped) {
  // Skip the first count-many entries
  // TODO: Can be improved
  while (count > 0 && next()) {
    --count;
    skipped++;
  }
}

/// @brief Get the next element
///        If one iterator is exhausted, the next one is used.
///        A nullptr indicates that all iterators are exhausted
IndexLookupResult MultiIndexIterator::next() {
  if (_current == nullptr) {
    return IndexLookupResult();
  }
  IndexLookupResult next = _current->next();
  while (!next) {
    _currentIdx++;
    if (_currentIdx >= _iterators.size()) {
      _current = nullptr;
      return IndexLookupResult();
    }
    _current = _iterators.at(_currentIdx);
    next = _current->next();
  }
  return next;
}

/// @brief Get the next limit many elements
///        If one iterator is exhausted, the next one will be used.
///        An empty result vector indicates that all iterators are exhausted
void MultiIndexIterator::nextBabies(std::vector<IndexLookupResult>& result, size_t limit) {
  result.clear();

  if (_current == nullptr) {
    return;
  }
  _current->nextBabies(result, limit);
  while (result.empty()) {
    _currentIdx++;
    if (_currentIdx >= _iterators.size()) {
      _current = nullptr;
      return;
    }
    _current = _iterators.at(_currentIdx);
    _current->nextBabies(result, limit);
  }
}

/// @brief Reset the cursor
///        This will reset ALL internal iterators and start all over again
void MultiIndexIterator::reset() {
  _current = _iterators.at(0);
  _currentIdx = 0;
  for (auto& it : _iterators) {
    it->reset();
  }
}
