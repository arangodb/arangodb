////////////////////////////////////////////////////////////////////////////////
/// @brief cap constraint
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

#ifndef ARANGODB_INDEXES_CAP_CONSTRAINT_H
#define ARANGODB_INDEXES_CAP_CONSTRAINT_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

// -----------------------------------------------------------------------------
// --SECTION--                                               class CapConstraint
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class CapConstraint : public Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        CapConstraint () = delete;

        CapConstraint (TRI_idx_iid_t,
                       struct TRI_document_collection_t*,
                       size_t,
                       int64_t);

        ~CapConstraint ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of documents in the collection
////////////////////////////////////////////////////////////////////////////////
  
        int64_t count () const {
          return _count;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum size of documents in the collection
////////////////////////////////////////////////////////////////////////////////

        int64_t size () const {
          return _size;
        }
        
        IndexType type () const override final {
          return Index::TRI_IDX_TYPE_CAP_CONSTRAINT;
        }

        bool hasSelectivityEstimate () const override final {
          return false;
        }

        double selectivityEstimate () const override final;
        
        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*) const override final;
  
        int insert (struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (struct TRI_doc_mptr_t const*, bool) override final;
        
        int postInsert (struct TRI_transaction_collection_s*, struct TRI_doc_mptr_t const*) override final;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the cap constraint
////////////////////////////////////////////////////////////////////////////////

        int initialize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the cap constraint for the collection
////////////////////////////////////////////////////////////////////////////////

        int apply (TRI_document_collection_t*,
                   struct TRI_transaction_collection_s*);
        
// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying collection
////////////////////////////////////////////////////////////////////////////////

        struct TRI_document_collection_t* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of documents in the collection
////////////////////////////////////////////////////////////////////////////////
  
        int64_t const _count;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum size of documents in the collection
////////////////////////////////////////////////////////////////////////////////

        int64_t const _size;

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
