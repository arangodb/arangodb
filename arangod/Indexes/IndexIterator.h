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

// In order to implement a new IndexIterator the following functions need to be
// implmeneted.
//
// typeName() returns a string descibing the type of the indexIterator
//
// The next() function of the IndexIterator expects a callback taking DocumentIdentifierTokens
// that are created from RevisionIds. In addition it expects a limit.
// The iterator has to walk through the Index and call the callback with at most limit
// many elements. On the next iteration it has to continue after the last returned Token.
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
#include "Indexes/IndexLookupContext.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class Index;
class LogicalCollection;
class ManagedDocumentResult;
namespace transaction {
class Methods;
}

/// @brief a base class to iterate over the index. An iterator is requested
/// at the index itself
class IndexIterator {
 public:
  typedef std::function<void(DocumentIdentifierToken const& token)> TokenCallback;
  typedef std::function<void(DocumentIdentifierToken const& token,
                             velocypack::Slice extra)> DocumentCallback;
  typedef std::function<void(DocumentIdentifierToken const& token,
                             velocypack::Slice extra)> ExtraCallback;

 public:
  IndexIterator(IndexIterator const&) = delete;
  IndexIterator& operator=(IndexIterator const&) = delete;
  IndexIterator() = delete;

  IndexIterator(LogicalCollection*, transaction::Methods*, ManagedDocumentResult*, arangodb::Index const*);

  virtual ~IndexIterator();

  virtual char const* typeName() const = 0;

  LogicalCollection* collection() const { return _collection; }
  transaction::Methods* transaction() const { return _trx; }

  virtual bool hasExtra() const;

  virtual bool next(TokenCallback const& callback, size_t limit) = 0;
  virtual bool nextDocument(DocumentCallback const& callback, size_t limit);
  virtual bool nextExtra(ExtraCallback const& callback, size_t limit);

  virtual void reset();

  virtual void skip(uint64_t count, uint64_t& skipped);

 protected:
  LogicalCollection* _collection;
  transaction::Methods* _trx;
  ManagedDocumentResult* _mmdr;
  IndexLookupContext _context;
  bool _responsible;
};

/// @brief Special iterator if the condition cannot have any result
class EmptyIndexIterator final : public IndexIterator {
 public:
  EmptyIndexIterator(LogicalCollection* collection, transaction::Methods* trx, ManagedDocumentResult* mmdr, arangodb::Index const* index) 
      : IndexIterator(collection, trx, mmdr, index) {}

  ~EmptyIndexIterator() {}

  char const* typeName() const override { return "empty-index-iterator"; }

  bool next(TokenCallback const&, size_t) override {
    return false;
  }

  void reset() override {}

  void skip(uint64_t, uint64_t& skipped) override {
    skipped = 0;
  }
};

/// @brief a wrapper class to iterate over several IndexIterators.
///        Each iterator is requested at the index itself.
///        This iterator does NOT check for uniqueness.
///        Will always start with the first iterator in the vector. Reverse them
///        Outside if necessary.
class MultiIndexIterator final : public IndexIterator {

  public:
   MultiIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                      ManagedDocumentResult* mmdr,
                      arangodb::Index const* index,
                      std::vector<IndexIterator*> const& iterators)
     : IndexIterator(collection, trx, mmdr, index), _iterators(iterators), _currentIdx(0), _current(nullptr) {
       if (!_iterators.empty()) {
         _current = _iterators.at(0);
       }
     }

    ~MultiIndexIterator () {
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
    bool next(TokenCallback const& callback, size_t limit) override;

    /// @brief Reset the cursor
    ///        This will reset ALL internal iterators and start all over again
    void reset() override;

  private:
   std::vector<IndexIterator*> _iterators;
   size_t _currentIdx;
   IndexIterator* _current;
};
}
#endif
