////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

// In order to implement a new IndexIterator the following functions need to be
// implemented.
//
// typeName() returns a string descibing the type of the indexIterator
//
// The next() function of the IndexIterator expects a callback taking
// LocalDocumentIds that are created from RevisionIds. In addition it expects a
// limit. The iterator has to walk through the Index and call the callback with
// at most limit many elements. On the next iteration it has to continue after
// the last returned Token.
//
// reset() resets the iterator
//
// optional - default implementation provided:
//
// skip(trySkip, skipped) tries to skip the next trySkip elements
//
// When finished you need to implement the fuction:
//    virtual IndexIterator* iteratorForCondition(...)
// So a there is a way to create an iterator for the index

#ifndef ARANGOD_INDEXES_INDEX_ITERATOR_H
#define ARANGOD_INDEXES_INDEX_ITERATOR_H 1

#include "Basics/Common.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class Index;
class LogicalCollection;

namespace aql {
struct AstNode;
struct Variable;
}

namespace transaction {
class Methods;
}

struct IndexIteratorOptions;

/// @brief a base class to iterate over the index. An iterator is requested
/// at the index itself
class IndexIterator {
  friend class MultiIndexIterator;

 public:
  typedef std::function<bool(LocalDocumentId const& token)> LocalDocumentIdCallback;
  typedef std::function<bool(LocalDocumentId const& token, velocypack::Slice doc)> DocumentCallback;
  typedef std::function<bool(LocalDocumentId const& token, velocypack::Slice extra)> ExtraCallback;

 public:
  IndexIterator(IndexIterator const&) = delete;
  IndexIterator& operator=(IndexIterator const&) = delete;
  IndexIterator() = delete;

  IndexIterator(LogicalCollection* collection, transaction::Methods* trx);

  virtual ~IndexIterator() = default;

  /// @brief return the underlying collection
  /// this is guaranteed to be a non-nullptr
  LogicalCollection* collection() const noexcept { return _collection; }

  transaction::Methods* transaction() const noexcept { return _trx; }
  
  bool hasMore() const noexcept { return _hasMore; }
  
  void reset();

  /// @brief Calls cb for the next batchSize many elements
  /// returns true if there are more documents (hasMore) and false
  /// if there are none
  bool next(IndexIterator::LocalDocumentIdCallback const& callback, uint64_t batchSize);

  /// @brief Calls cb for the next batchSize many elements
  /// returns true if there are more documents (hasMore) and false
  /// if there are none
  bool nextExtra(IndexIterator::ExtraCallback const& callback, uint64_t batchSize);

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
    while (next(callback, 1000)) { /* intentionally empty */ }
  }

  /// @brief convenience function to retrieve all results with extra
  void allExtra(IndexIterator::ExtraCallback const& callback) {
    while (nextExtra(callback, 1000)) { /* intentionally empty */ }
  }

  /// @brief convenience function to retrieve all results
  void allDocuments(IndexIterator::DocumentCallback const& callback, uint64_t batchSize) {
    while (nextDocument(callback, batchSize)) { /* intentionally empty */ }
  }
  
  /// @brief rearm the iterator with a new condition
  /// requires that canRearm() is true!
  /// if returns true it means that rearming has worked, and the iterator
  /// is ready to be used
  /// if returns false it means that the iterator does not support
  /// the provided condition and would only produce an empty result
  [[nodiscard]] bool rearm(arangodb::aql::AstNode const* node,
                           arangodb::aql::Variable const* variable,
                           IndexIteratorOptions const& opts);

  /// @brief skip the next toSkip many elements.
  ///        skipped will be increased by the amount of skipped elements
  ///        afterwards Check hasMore()==true before using this NOTE: This will
  ///        throw on OUT_OF_MEMORY
  void skip(uint64_t toSkip, uint64_t& skipped);

  /// @brief skip all elements.
  ///        skipped will be increased by the amount of skipped elements
  ///        afterwards Check hasMore()==true before using this NOTE: This will
  ///        throw on OUT_OF_MEMORY
  void skipAll(uint64_t& skipped);
  
  virtual char const* typeName() const = 0;

  /// @brief whether or not the index iterator supports rearming
  virtual bool canRearm() const { return false; }

  /// @brief The default index has no extra information
  virtual bool hasExtra() const { return false; }

  /// @brief default implementation for whether or not an index iterator
  /// provides the "nextCovering" method as a performance optimization
  /// The default index has no covering method information
  virtual bool hasCovering() const { return false; }
 
 protected:
  virtual bool rearmImpl(arangodb::aql::AstNode const* node,
                         arangodb::aql::Variable const* variable,
                         IndexIteratorOptions const& opts);

  virtual bool nextImpl(LocalDocumentIdCallback const& callback, size_t limit);
  virtual bool nextDocumentImpl(DocumentCallback const& callback, size_t limit);
  virtual bool nextExtraImpl(ExtraCallback const& callback, size_t limit);

  // extract index attribute values directly from the index while index scanning
  // must only be called if hasCovering()
  virtual bool nextCoveringImpl(DocumentCallback const& callback, size_t limit);

  virtual void resetImpl() {}

  virtual void skipImpl(uint64_t count, uint64_t& skipped);

  LogicalCollection* _collection;
  transaction::Methods* _trx;
  bool _hasMore;
};

/// @brief Special iterator if the condition cannot have any result
class EmptyIndexIterator final : public IndexIterator {
 public:
  EmptyIndexIterator(LogicalCollection* collection, transaction::Methods* trx)
      : IndexIterator(collection, trx) {}

  ~EmptyIndexIterator() = default;

  char const* typeName() const override { return "empty-index-iterator"; }
  
  /// @brief the iterator can easily claim to have extra information, however,
  /// it never produces any results, so this is a cheap trick 
  bool hasExtra() const override { return true; }

  /// @brief the iterator can easily claim to have covering data, however,
  /// it never produces any results, so this is a cheap trick 
  bool hasCovering() const override { return true; }

  bool nextImpl(LocalDocumentIdCallback const&, size_t) override { return false; }
  bool nextDocumentImpl(DocumentCallback const&, size_t) override { return false; }
  bool nextExtraImpl(ExtraCallback const&, size_t) override { return false; }
  bool nextCoveringImpl(DocumentCallback const&, size_t) override { return false; }
  
  void resetImpl() override { _hasMore = false; }

  void skipImpl(uint64_t /*count*/, uint64_t& skipped) override { skipped = 0; }
};

/// @brief a wrapper class to iterate over several IndexIterators.
///        Each iterator is requested at the index itself.
///        This iterator does NOT check for uniqueness.
///        Will always start with the first iterator in the vector. Reverse them
///        outside if necessary.
class MultiIndexIterator final : public IndexIterator {
 public:
  MultiIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                     arangodb::Index const* index, std::vector<std::unique_ptr<IndexIterator>> iterators)
      : IndexIterator(collection, trx),
        _iterators(std::move(iterators)),
        _currentIdx(0),
        _current(nullptr),
        _hasCovering(true) {
    if (!_iterators.empty()) {
      _current = _iterators[0].get();
      for (auto const& it : _iterators) {
        // covering index support only present if all index
        // iterators in this MultiIndexIterator support it
        _hasCovering &= it->hasCovering();
      }
    } else {
      // no iterators => no covering index support
      _hasCovering = false;
    }
  }

  ~MultiIndexIterator() = default;

  char const* typeName() const override { return "multi-index-iterator"; }
  
  /// @brief for whether or not the iterators provide the "nextCovering" method
  /// as a performance optimization
  bool hasCovering() const override { return _hasCovering; }

  /// @brief Get the next elements
  ///        If one iterator is exhausted, the next one is used.
  ///        If callback is called less than limit many times
  ///        all iterators are exhausted
  bool nextImpl(LocalDocumentIdCallback const& callback, size_t limit) override;
  bool nextDocumentImpl(DocumentCallback const& callback, size_t limit) override;
  bool nextExtraImpl(ExtraCallback const& callback, size_t limit) override;
  bool nextCoveringImpl(DocumentCallback const& callback, size_t limit) override;
  
  /// @brief Reset the cursor
  ///        This will reset ALL internal iterators and start all over again
  void resetImpl() override;

 private:
  std::vector<std::unique_ptr<IndexIterator>> _iterators;
  size_t _currentIdx;
  IndexIterator* _current;
  bool _hasCovering;
};

/// Options for creating an index iterator
struct IndexIteratorOptions {
  /// @brief Limit used in a parent LIMIT node (if non-zero)
  size_t limit = 0;
  /// @brief whether the index must sort its results
  bool sorted = true;
  /// @brief the index sort order - this is the same order for all indexes
  bool ascending = true;
  /// @brief Whether FCalls will be evaluated entirely or just it's arguments
  /// Used when creating the condition required to build an iterator
  bool evaluateFCalls = true;
  /// @brief enable caching
  bool enableCache = true;
};
  
/// index estimate map, defined here because it was convenient
typedef std::unordered_map<std::string, double> IndexEstMap;
}  // namespace arangodb
#endif
