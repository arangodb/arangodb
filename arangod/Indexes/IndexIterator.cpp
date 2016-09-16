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
#include "Utils/CollectionNameResolver.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief context for an index iterator
////////////////////////////////////////////////////////////////////////////////

IndexIteratorContext::IndexIteratorContext(TRI_vocbase_t* vocbase,
                                           CollectionNameResolver const* resolver,
                                           ServerState::RoleEnum serverRole)
    : vocbase(vocbase), resolver(resolver), serverRole(serverRole), ownsResolver(resolver == nullptr) {}

/*
IndexIteratorContext::IndexIteratorContext(TRI_vocbase_t* vocbase)
    : IndexIteratorContext(vocbase, nullptr) {}
*/
IndexIteratorContext::~IndexIteratorContext() {
  if (ownsResolver) {
    delete resolver;
  }
}

CollectionNameResolver const* IndexIteratorContext::getResolver() const {
  if (resolver == nullptr) {
    TRI_ASSERT(ownsResolver);
    resolver = new CollectionNameResolver(vocbase);
  }
  return resolver;
}

bool IndexIteratorContext::isCluster() const {
  return arangodb::ServerState::instance()->isRunningInCluster(serverRole);
}

int IndexIteratorContext::resolveId(char const* handle, size_t length,
                                    TRI_voc_cid_t& cid,
                                    char const*& key,
                                    size_t& outLength) const {
  char const* p = static_cast<char const*>(memchr(handle, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR, length));

  if (p == nullptr || *p == '\0') {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  if (*handle >= '0' && *handle <= '9') {
    cid = arangodb::basics::StringUtils::uint64(handle, p - handle);
  } else {
    std::string const name(handle, p - handle);
    cid = getResolver()->getCollectionIdCluster(name);
  }

  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  key = p + 1;
  outLength = length - (key - handle);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default destructor. Does not free anything
////////////////////////////////////////////////////////////////////////////////

IndexIterator::~IndexIterator() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for next
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* IndexIterator::next() { return nullptr; }

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for nextBabies
////////////////////////////////////////////////////////////////////////////////

void IndexIterator::nextBabies(std::vector<TRI_doc_mptr_t*>& result, size_t batchSize) {
  result.clear();

  if (batchSize > 0) {
    while (true) {
      TRI_doc_mptr_t* mptr = next();
      if (mptr == nullptr) {
        return;
      }
      result.emplace_back(mptr);
      batchSize--;
      if (batchSize == 0) {
        return;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for reset
////////////////////////////////////////////////////////////////////////////////

void IndexIterator::reset() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief default implementation for skip
////////////////////////////////////////////////////////////////////////////////

void IndexIterator::skip(uint64_t count, uint64_t& skipped) {
  // Skip the first count-many entries
  // TODO: Can be improved
  while (count > 0 && next() != nullptr) {
    --count;
    skipped++;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the next element
///        If one iterator is exhausted, the next one is used.
///        A nullptr indicates that all iterators are exhausted
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* MultiIndexIterator::next() {
  if (_current == nullptr) {
    return nullptr;
  }
  TRI_doc_mptr_t* next = _current->next();
  while (next == nullptr) {
    _currentIdx++;
    if (_currentIdx >= _iterators.size()) {
      _current = nullptr;
      return nullptr;
    }
    _current = _iterators.at(_currentIdx);
    next = _current->next();
  }
  return next;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the next limit many elements
///        If one iterator is exhausted, the next one will be used.
///        An empty result vector indicates that all iterators are exhausted
////////////////////////////////////////////////////////////////////////////////

void MultiIndexIterator::nextBabies(std::vector<TRI_doc_mptr_t*>& result, size_t limit) {
  if (_current == nullptr) {
    result.clear();
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

////////////////////////////////////////////////////////////////////////////////
/// @brief Reset the cursor
///        This will reset ALL internal iterators and start all over again
////////////////////////////////////////////////////////////////////////////////

void MultiIndexIterator::reset() {
  _current = _iterators.at(0);
  _currentIdx = 0;
  for (auto& it : _iterators) {
    it->reset();
  }
}
