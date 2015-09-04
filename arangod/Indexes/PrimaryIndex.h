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
#include "Basics/AssocUnique.h"
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
        
      private:

        typedef triagens::basics::AssocUnique<char const,
                TRI_doc_mptr_t> TRI_PrimaryIndex_t;

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
        
        size_t size () const;

        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*, bool) const override final;
        triagens::basics::Json toJsonFigures (TRI_memory_zone_t*) const override final;
  
        int insert (TRI_doc_mptr_t const*, bool) override final;

        int remove (TRI_doc_mptr_t const*, bool) override final;

        TRI_doc_mptr_t* lookupKey (char const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
/// returns the index position into which a key would belong in the second
/// parameter. sets position to UINT64_MAX if the position cannot be determined
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t* lookupKey (char const*, uint64_t&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a random order. 
///        Returns nullptr if all documents have been returned.
///        Convention: step === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t* lookupRandom (triagens::basics::BucketPosition& initialPosition,
                                      triagens::basics::BucketPosition& position,
                                      uint64_t& step,
                                      uint64_t& total);

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a sequential order. 
///        Returns nullptr if all documents have been returned.
///        Convention: position === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t* lookupSequential (triagens::basics::BucketPosition& position,
                                          uint64_t& total);

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        reversed sequential order. 
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_mptr_t* lookupSequentialReverse (triagens::basics::BucketPosition& position);

        int insertKey (TRI_doc_mptr_t*, void const**);
        void insertKey (TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key/element to the index
/// this is a special, optimized version that receives the target slot index
/// from a previous lookupKey call
////////////////////////////////////////////////////////////////////////////////

        void insertKey (struct TRI_doc_mptr_t*, uint64_t);

        TRI_doc_mptr_t* removeKey (char const*);

        int resize (size_t);

        static uint64_t calculateHash (char const*); 
        
        static uint64_t calculateHash (char const*, size_t);

        void invokeOnAllElements (std::function<void(TRI_doc_mptr_t*)>);
        
// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual index
////////////////////////////////////////////////////////////////////////////////

        TRI_PrimaryIndex_t* _primaryIndex;

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
