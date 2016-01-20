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
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class CollectionNameResolver;

////////////////////////////////////////////////////////////////////////////////
/// @brief context for an index iterator
////////////////////////////////////////////////////////////////////////////////

struct IndexIteratorContext {
  IndexIteratorContext(TRI_vocbase_t*, CollectionNameResolver*);

  explicit IndexIteratorContext(TRI_vocbase_t*);

  ~IndexIteratorContext();

  CollectionNameResolver const* getResolver() const;

  bool isCluster() const;

  int resolveId(char const*, TRI_voc_cid_t&, char const*&) const;

  TRI_vocbase_t* vocbase;
  mutable CollectionNameResolver const* resolver;
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

  virtual void reset();
};
}

#endif


