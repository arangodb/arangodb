////////////////////////////////////////////////////////////////////////////////
/// @brief primary index
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INDEXES_PRIMARY_INDEX_H
#define ARANGODB_INDEXES_PRIMARY_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                class PrimaryIndex
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class PrimaryIndex : public Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        PrimaryIndex () = delete;

        explicit PrimaryIndex (struct TRI_document_collection_t*);

        ~PrimaryIndex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------
        
      public:

        struct PrimaryIndexType {
          uint64_t  _nrAlloc;     // the size of the table
          uint64_t  _nrUsed;      // the number of used entries
          void**    _table;       // the table itself
        };

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
        
        IndexType type () const override final {
          return Index::TRI_IDX_TYPE_PRIMARY_INDEX;
        }

        bool hasSelectivityEstimate () const override final {
          return true;
        }

        double selectivityEstimate () const override final {
          return 1.0;
        }

        bool dumpFields () const override final {
          return true;
        }
        
        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*) const override final;
  
        int insert (struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (struct TRI_doc_mptr_t const*, bool) override final;

        void* lookupKey (char const*) const;
        int insertKey (struct TRI_doc_mptr_t const*, void const**);
        void insertKey (struct TRI_doc_mptr_t const*);
        void* removeKey (char const*);

        int resize (size_t);
        int resize ();

        static uint64_t calculateHash (char const*); 
        
        static uint64_t calculateHash (char const*, size_t);
        
        PrimaryIndexType* internals () {
          return &_primaryIndex;
        } 

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

        bool shouldResize () const;

        bool resize (uint64_t, bool);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        struct TRI_document_collection_t* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual index
////////////////////////////////////////////////////////////////////////////////

        PrimaryIndexType _primaryIndex;

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array of pointers
////////////////////////////////////////////////////////////////////////////////

        static uint64_t const InitialSize;
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
