////////////////////////////////////////////////////////////////////////////////
/// @brief index iterator base class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INDEXES_INDEX_ITERATOR_H
#define ARANGODB_INDEXES_INDEX_ITERATOR_H 1

#include "Basics/Common.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace arango {
    class CollectionNameResolver;

// -----------------------------------------------------------------------------
// --SECTION--                                               class IndexIterator
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief context for an index iterator
////////////////////////////////////////////////////////////////////////////////

    struct IndexIteratorContext {
      IndexIteratorContext (TRI_vocbase_t*, CollectionNameResolver*);
      
      ~IndexIteratorContext ();

      CollectionNameResolver const* getResolver ();

      TRI_vocbase_t* vocbase;
      CollectionNameResolver const* resolver;
      bool ownsResolver;
      bool isRunningInCluster;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief a base class to iterate over the index. An iterator is requested 
/// at the index itself
////////////////////////////////////////////////////////////////////////////////

    class IndexIterator {

      public:

        IndexIterator (IndexIterator const&) = delete;
        IndexIterator& operator= (IndexIterator const&) = delete;
        
        IndexIterator () {
        }
        
        virtual ~IndexIterator ();

        virtual TRI_doc_mptr_t* next ();

        virtual void reset ();
    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
