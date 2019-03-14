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

#include "OperationCursor.h"
#include "Basics/Exceptions.h"
#include "Transaction/Methods.h"

using namespace arangodb;
      
OperationCursor::OperationCursor(std::unique_ptr<IndexIterator> iterator)
    : _indexIterator(std::move(iterator)),
      _hasMore(_indexIterator != nullptr) {
  if (_indexIterator == nullptr) {
    // cannot create a cursor without a valid iterator
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

LogicalCollection* OperationCursor::collection() const {
  TRI_ASSERT(_indexIterator != nullptr);
  return _indexIterator->collection();
}

bool OperationCursor::hasExtra() const { return indexIterator()->hasExtra(); }

bool OperationCursor::hasCovering() const {
  return indexIterator()->hasCovering();
}

void OperationCursor::reset() {
  if (_indexIterator != nullptr) {
    _indexIterator->reset();
    _hasMore = true;
  } else {
    _hasMore = false;
  }
}

/// @brief Calls cb for the next batchSize many elements
///        NOTE: This will throw on OUT_OF_MEMORY
bool OperationCursor::next(IndexIterator::LocalDocumentIdCallback const& callback,
                           uint64_t batchSize) {
  if (!hasMore()) {
    return false;
  }

  if (batchSize == UINT64_MAX) {
    batchSize = transaction::Methods::defaultBatchSize();
  }

  size_t atMost = static_cast<size_t>(batchSize);
  _hasMore = _indexIterator->next(callback, atMost);
  return _hasMore;
}

bool OperationCursor::nextDocument(IndexIterator::DocumentCallback const& callback,
                                   uint64_t batchSize) {
  if (!hasMore()) {
    return false;
  }

  if (batchSize == UINT64_MAX) {
    batchSize = transaction::Methods::defaultBatchSize();
  }

  size_t atMost = static_cast<size_t>(batchSize);
  _hasMore = _indexIterator->nextDocument(callback, atMost);
  return _hasMore;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Calls cb for the next batchSize many elements
///        Uses the getExtra feature of indexes. Can only be called on those
///        who support it.
///        NOTE: This will throw on OUT_OF_MEMORY
//////////////////////////////////////////////////////////////////////////////

bool OperationCursor::nextWithExtra(IndexIterator::ExtraCallback const& callback,
                                    uint64_t batchSize) {
  TRI_ASSERT(hasExtra());

  if (!hasMore()) {
    return false;
  }

  if (batchSize == UINT64_MAX) {
    batchSize = transaction::Methods::defaultBatchSize();
  }

  size_t atMost = static_cast<size_t>(batchSize);
  _hasMore = _indexIterator->nextExtra(callback, atMost);
  return _hasMore;
}

bool OperationCursor::nextCovering(IndexIterator::DocumentCallback const& callback,
                                   uint64_t batchSize) {
  TRI_ASSERT(hasCovering());

  if (!hasMore()) {
    return false;
  }

  if (batchSize == UINT64_MAX) {
    batchSize = transaction::Methods::defaultBatchSize();
  }

  size_t atMost = static_cast<size_t>(batchSize);
  _hasMore = _indexIterator->nextCovering(callback, atMost);
  return _hasMore;
}

/// @brief Skip the next toSkip many elements.
///        skipped will be increased by the amount of skipped elements
///        afterwards Check hasMore()==true before using this NOTE: This will
///        throw on OUT_OF_MEMORY
void OperationCursor::skip(uint64_t toSkip, uint64_t& skipped) {
  if (!hasMore()) {
    TRI_ASSERT(false);
    // You requested more even if you should have checked it before.
    return;
  }

  _indexIterator->skip(toSkip, skipped);
  if (skipped != toSkip) {
    _hasMore = false;
  }
}
