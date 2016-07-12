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
#include "VocBase/vocbase.h"

namespace arangodb {
class CollectionNameResolver;

////////////////////////////////////////////////////////////////////////////////
/// @brief context for an index iterator
////////////////////////////////////////////////////////////////////////////////

struct IndexIteratorContext {
  IndexIteratorContext(IndexIteratorContext const&) = delete;
  IndexIteratorContext& operator=(IndexIteratorContext const&) = delete;
  
  IndexIteratorContext() = delete;
  IndexIteratorContext(TRI_vocbase_t*, CollectionNameResolver const*, ServerState::RoleEnum);
  //explicit IndexIteratorContext(TRI_vocbase_t*);

  ~IndexIteratorContext();

  CollectionNameResolver const* getResolver() const;

  bool isCluster() const;

  int resolveId(char const*, size_t, TRI_voc_cid_t&, char const*&, size_t&) const;

  TRI_vocbase_t* vocbase;
  mutable CollectionNameResolver const* resolver;
  ServerState::RoleEnum serverRole;
  bool ownsResolver;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a base class to iterate over the index. An iterator is requested
/// at the index itself
////////////////////////////////////////////////////////////////////////////////

class IndexIterator {
 public:
  IndexIterator(IndexIterator const&) = delete;
  IndexIterator& operator=(IndexIterator const&) = delete;

  IndexIterator() {}
  virtual ~IndexIterator();

  virtual TRI_doc_mptr_t* next();

  virtual void nextBabies(std::vector<TRI_doc_mptr_t*>&, size_t);

  virtual void reset();

  virtual void skip(uint64_t count, uint64_t& skipped);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Special iterator if the condition cannot have any result
////////////////////////////////////////////////////////////////////////////////

class EmptyIndexIterator : public IndexIterator {
  public:
    EmptyIndexIterator() {}
    ~EmptyIndexIterator() {}

    TRI_doc_mptr_t* next() override {
      return nullptr;
    }

    void nextBabies(std::vector<TRI_doc_mptr_t*>&, size_t) override {}

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

class MultiIndexIterator : public IndexIterator {

  public:
   explicit MultiIndexIterator(std::vector<IndexIterator*> const& iterators)
     : _iterators(iterators), _currentIdx(0), _current(nullptr) {
       if (!_iterators.empty()) {
         _current = _iterators.at(0);
       }
     };

    ~MultiIndexIterator () {
      // Free all iterators
      for (auto& it : _iterators) {
        delete it;
      }
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Get the next element
    ///        If one iterator is exhausted, the next one is used.
    ///        A nullptr indicates that all iterators are exhausted
    ////////////////////////////////////////////////////////////////////////////////

    TRI_doc_mptr_t* next() override;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief Get at most the next limit many elements
    ///        If one iterator is exhausted, the next one will be used.
    ///        An empty result vector indicates that all iterators are exhausted
    ////////////////////////////////////////////////////////////////////////////////
    
    void nextBabies(std::vector<TRI_doc_mptr_t*>&, size_t) override;

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
