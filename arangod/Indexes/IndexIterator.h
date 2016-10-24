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

#ifndef ARANGOD_INDEXES_INDEX_ITERATOR_H
#define ARANGOD_INDEXES_INDEX_ITERATOR_H 1

#include "Basics/Common.h"
#include "Cluster/ServerState.h"
#include "Indexes/IndexElement.h"
#include "Indexes/IndexLookupContext.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class Index;
class LogicalCollection;
class Transaction;

////////////////////////////////////////////////////////////////////////////////
/// @brief a base class to iterate over the index. An iterator is requested
/// at the index itself
////////////////////////////////////////////////////////////////////////////////

class IndexIterator {
 public:
  IndexIterator(IndexIterator const&) = delete;
  IndexIterator& operator=(IndexIterator const&) = delete;
  IndexIterator() = delete;

  IndexIterator(LogicalCollection*, arangodb::Transaction*, ManagedDocumentResult*, arangodb::Index const*);

  virtual ~IndexIterator();

  virtual char const* typeName() const = 0;

  LogicalCollection* collection() const { return _collection; }
  arangodb::Transaction* transaction() const { return _trx; }

  virtual IndexLookupResult next();

  virtual void nextBabies(std::vector<IndexLookupResult>&, size_t);

  virtual void reset();

  virtual void skip(uint64_t count, uint64_t& skipped);

 protected:
  LogicalCollection* _collection;
  arangodb::Transaction* _trx;
  ManagedDocumentResult* _mmdr;
  IndexLookupContext _context;
  bool _responsible;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Special iterator if the condition cannot have any result
////////////////////////////////////////////////////////////////////////////////

class EmptyIndexIterator final : public IndexIterator {
  public:
    EmptyIndexIterator(LogicalCollection* collection, arangodb::Transaction* trx, ManagedDocumentResult* mmdr, arangodb::Index const* index) 
        : IndexIterator(collection, trx, mmdr, index) {}

    ~EmptyIndexIterator() {}

    char const* typeName() const override { return "empty-index-iterator"; }

    IndexLookupResult next() override { return IndexLookupResult(); }

    void nextBabies(std::vector<IndexLookupResult>&, size_t) override {}

    void reset() override {}

    void skip(uint64_t, uint64_t& skipped) override {
      skipped = 0;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a wrapper class to iterate over several IndexIterators.
///        Each iterator is requested at the index itself.
///        This iterator does NOT check for uniqueness.
///        Will always start with the first iterator in the vector. Reverse them
///        Outside if necessary.
////////////////////////////////////////////////////////////////////////////////

class MultiIndexIterator final : public IndexIterator {

  public:
   MultiIndexIterator(LogicalCollection* collection, arangodb::Transaction* trx,
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

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Get the next element
    ///        If one iterator is exhausted, the next one is used.
    ///        A nullptr indicates that all iterators are exhausted
    ////////////////////////////////////////////////////////////////////////////////

    IndexLookupResult next() override;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Get at most the next limit many elements
    ///        If one iterator is exhausted, the next one will be used.
    ///        An empty result vector indicates that all iterators are exhausted
    ////////////////////////////////////////////////////////////////////////////////
    
    void nextBabies(std::vector<IndexLookupResult>&, size_t) override;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Reset the cursor
    ///        This will reset ALL internal iterators and start all over again
    ////////////////////////////////////////////////////////////////////////////////

    void reset() override;

  private:
   std::vector<IndexIterator*> _iterators;
   size_t _currentIdx;
   IndexIterator* _current;
};
}
#endif
