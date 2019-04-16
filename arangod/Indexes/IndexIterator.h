////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
#include "VocBase/LocalDocumentId.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class Index;
class LogicalCollection;

namespace aql {
struct AstNode;
struct Variable;
};

namespace transaction {
class Methods;
}

struct IndexIteratorOptions;

/// @brief a base class to iterate over the index. An iterator is requested
/// at the index itself
class IndexIterator {
 public:
  typedef std::function<void(LocalDocumentId const& token)> LocalDocumentIdCallback;
  typedef std::function<void(LocalDocumentId const& token, velocypack::Slice doc)> DocumentCallback;
  typedef std::function<void(LocalDocumentId const& token, velocypack::Slice extra)> ExtraCallback;

 public:
  IndexIterator(IndexIterator const&) = delete;
  IndexIterator& operator=(IndexIterator const&) = delete;
  IndexIterator() = delete;

  IndexIterator(LogicalCollection*, transaction::Methods*);

  virtual ~IndexIterator() {}

  virtual char const* typeName() const = 0;

  /// @brief return the underlying collection
  /// note: this may return a nullptr in case we are dealing with the EmptyIndexIterator! 
  LogicalCollection* collection() const { return _collection; }

  transaction::Methods* transaction() const { return _trx; }

  /// @brief whether or not the index iterator supports rearming
  virtual bool canRearm() const { return false; }

  /// @brief rearm the iterator with a new condition
  /// requires that canRearm() is true!
  /// if returns true it means that rearming has worked, and the iterator
  /// is ready to be used
  /// if returns false it means that the iterator does not support
  /// the provided condition and would only produce an empty result
  virtual bool rearm(arangodb::aql::AstNode const* node,
                     arangodb::aql::Variable const* variable,
                     IndexIteratorOptions const& opts);

  virtual bool hasExtra() const {
    // The default index has no extra information
    return false;
  }

  /// @brief default implementation for whether or not an index iterator
  /// provides the "nextCovering" method as a performance optimization
  virtual bool hasCovering() const {
    // The default index has no covering method information
    return false;
  }

  virtual bool next(LocalDocumentIdCallback const& callback, size_t limit) = 0;
  virtual bool nextDocument(DocumentCallback const& callback, size_t limit);
  virtual bool nextExtra(ExtraCallback const& callback, size_t limit);

  // extract index attribute values directly from the index while index scanning
  // must only be called if hasCovering()
  virtual bool nextCovering(DocumentCallback const& callback, size_t limit);

  virtual void reset();

  virtual void skip(uint64_t count, uint64_t& skipped);
  
 protected:
  LogicalCollection* _collection;
  transaction::Methods* _trx;
};

/// @brief Special iterator if the condition cannot have any result
class EmptyIndexIterator final : public IndexIterator {
 public:
  EmptyIndexIterator(LogicalCollection* collection, transaction::Methods* trx)
      : IndexIterator(collection, trx) {}

  ~EmptyIndexIterator() {}

  char const* typeName() const override { return "empty-index-iterator"; }

  bool next(LocalDocumentIdCallback const&, size_t) override { return false; }
  bool nextExtra(ExtraCallback const&, size_t) override { return false; }
  bool nextCovering(DocumentCallback const&, size_t) override { return false; }

  void reset() override {}

  void skip(uint64_t, uint64_t& skipped) override { skipped = 0; }
  
  /// @brief the iterator can easily claim to have extra information, however,
  /// it never produces any results, so this is a cheap trick 
  bool hasExtra() const override { return true; }

  /// @brief the iterator can easily claim to have covering data, however,
  /// it never produces any results, so this is a cheap trick 
  bool hasCovering() const override { return true; }
};

/// @brief a wrapper class to iterate over several IndexIterators.
///        Each iterator is requested at the index itself.
///        This iterator does NOT check for uniqueness.
///        Will always start with the first iterator in the vector. Reverse them
///        outside if necessary.
class MultiIndexIterator final : public IndexIterator {
 public:
  MultiIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                     arangodb::Index const* index, std::vector<IndexIterator*> const& iterators)
      : IndexIterator(collection, trx),
        _iterators(iterators),
        _currentIdx(0),
        _current(nullptr),
        _hasCovering(true) {
    if (!_iterators.empty()) {
      _current = _iterators[0];
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

  ~MultiIndexIterator() {
    // Free all iterators
    for (auto& it : _iterators) {
      delete it;
    }
  }

  char const* typeName() const override { return "multi-index-iterator"; }

  /// @brief Get the next elements
  ///        If one iterator is exhausted, the next one is used.
  ///        If callback is called less than limit many times
  ///        all iterators are exhausted
  bool next(LocalDocumentIdCallback const& callback, size_t limit) override;

  bool nextCovering(DocumentCallback const& callback, size_t limit) override;

  /// @brief Reset the cursor
  ///        This will reset ALL internal iterators and start all over again
  void reset() override;

  /// @brief for whether or not the iterators provide the "nextCovering" method
  /// as a performance optimization
  bool hasCovering() const override { return _hasCovering; }

 private:
  std::vector<IndexIterator*> _iterators;
  size_t _currentIdx;
  IndexIterator* _current;
  bool _hasCovering;
};

/// Options for creating an index iterator
struct IndexIteratorOptions {
  /// @brief whether the index must sort it's results
  bool sorted = true;
  /// @brief the index sort order - this is the same order for all indexes
  bool ascending = true;
  /// @brief Whether FCalls will be evaluated entirely or just it's arguments
  /// Used when creating the condition required to build an iterator
  bool evaluateFCalls = true;
  /// @brief Whether to eagerly scan the full range of a condition
  bool fullRange = false;
  /// @brief force covering index access in case this would otherwise be doubtful
  bool forceProjection = false;
  /// @brief Limit used in a parent LIMIT node (if non-zero)
  size_t limit = 0;
};
  
/// index estimate map, defined here because it was convenient
typedef std::unordered_map<std::string, double> IndexEstMap;
}  // namespace arangodb
#endif
