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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_OPERATION_CURSOR_H
#define ARANGOD_UTILS_OPERATION_CURSOR_H 1

#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"

namespace arangodb {

class LogicalCollection;

struct OperationCursor {
 private:
  std::unique_ptr<IndexIterator> _indexIterator;
  bool _hasMore;

 public:
  /// @brief creates an empty cursor
  OperationCursor()
      : _hasMore(false) {}

  /// @brief creates a cursor using an IndexIterator (which must be valid)
  explicit OperationCursor(std::unique_ptr<IndexIterator> iterator);

  TEST_VIRTUAL ~OperationCursor() {}

  /// @brief return a pointer to the cursor's current index iterator
  /// (note: this may be a nullptr in the beginning)
  IndexIterator* indexIterator() const { return _indexIterator.get(); }
  
  /// @brief inject a new iterator into the cursor. the cursor will
  /// take over the ownership
  void rearm(std::unique_ptr<IndexIterator> iterator) {
    _indexIterator = std::move(iterator); 
    TRI_ASSERT(_indexIterator != nullptr);
    _hasMore = true;
  }

  /// @brief return the logical collection used by the iterator.
  /// note that the collection may be a nullptr in case we are dealing with
  /// an EmptyIndexIterator!
  LogicalCollection* collection() const;

  inline bool hasMore() const { return _hasMore; }

  bool hasExtra() const;
  bool hasCovering() const;

  /// @brief Reset the cursor
  TEST_VIRTUAL void reset();

  /// @brief Calls cb for the next batchSize many elements
  /// returns true if there are more documents (hasMore) and false
  /// if there are none
  bool next(IndexIterator::LocalDocumentIdCallback const& callback, uint64_t batchSize);

  /// @brief Calls cb for the next batchSize many elements
  /// returns true if there are more documents (hasMore) and false
  /// if there are none
  bool nextWithExtra(IndexIterator::ExtraCallback const& callback, uint64_t batchSize);

  /// @brief Calls cb for the next batchSize many elements, complete documents
  /// returns true if there are more documents (hasMore) and false
  /// if there are none
  bool nextDocument(IndexIterator::DocumentCallback const& callback, uint64_t batchSize);

  /// @brief Calls cb for the next batchSize many elements, index-only
  /// projections returns true if there are more documents (hasMore) and false
  /// if there are none
  bool nextCovering(IndexIterator::DocumentCallback const& callback, uint64_t batchSize);

  /// @brief convenience function to retrieve all results
  void all(IndexIterator::LocalDocumentIdCallback const& callback) {
    while (next(callback, 1000)) {
    }
  }

  /// @brief convenience function to retrieve all results with extra
  void allWithExtra(IndexIterator::ExtraCallback const& callback) {
    while (nextWithExtra(callback, 1000)) {
    }
  }

  /// @brief convenience function to retrieve all results
  void allDocuments(IndexIterator::DocumentCallback const& callback, uint64_t batchSize) {
    while (nextDocument(callback, batchSize)) {
    }
  }

  /// @brief skip the next toSkip many elements.
  ///        skipped will be increased by the amount of skipped elements
  ///        afterwards Check hasMore()==true before using this NOTE: This will
  ///        throw on OUT_OF_MEMORY
  void skip(uint64_t toSkip, uint64_t& skipped);
};

}  // namespace arangodb

#endif
