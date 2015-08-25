////////////////////////////////////////////////////////////////////////////////
/// @brief index base class
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

#ifndef ARANGODB_INDEXES_INDEX_H
#define ARANGODB_INDEXES_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_t;
struct TRI_document_collection_t;
struct TRI_shaped_json_s;
struct TRI_transaction_collection_s;

////////////////////////////////////////////////////////////////////////////////
/// @brief index query parameter
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_index_search_value_s {
  size_t _length;
  struct TRI_shaped_json_s* _values;
}
TRI_index_search_value_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Index
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        Index () = delete;
        Index (Index const&) = delete;
        Index& operator= (Index const&) = delete;

        Index (TRI_idx_iid_t,
               struct TRI_document_collection_t*,
               std::vector<std::string> const&);

        virtual ~Index ();

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief index types
////////////////////////////////////////////////////////////////////////////////

        enum IndexType {
          TRI_IDX_TYPE_UNKNOWN = 0,
          TRI_IDX_TYPE_PRIMARY_INDEX,
          TRI_IDX_TYPE_GEO1_INDEX,
          TRI_IDX_TYPE_GEO2_INDEX,
          TRI_IDX_TYPE_HASH_INDEX,
          TRI_IDX_TYPE_EDGE_INDEX,
          TRI_IDX_TYPE_FULLTEXT_INDEX,
          TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX, // DEPRECATED and not functional anymore
          TRI_IDX_TYPE_SKIPLIST_INDEX,
          TRI_IDX_TYPE_BITARRAY_INDEX,       // DEPRECATED and not functional anymore
          TRI_IDX_TYPE_CAP_CONSTRAINT
        };

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index id
////////////////////////////////////////////////////////////////////////////////
        
        inline TRI_idx_iid_t id () const {
          return _iid;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index fields
////////////////////////////////////////////////////////////////////////////////

        inline std::vector<std::string> const& fields () const {
          return _fields;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a contextual string for logging
////////////////////////////////////////////////////////////////////////////////

        std::string context () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of the index
////////////////////////////////////////////////////////////////////////////////
        
        char const* typeName () const {
          return typeName(type());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type based on a type name
////////////////////////////////////////////////////////////////////////////////

        static IndexType type (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of an index type
////////////////////////////////////////////////////////////////////////////////

        static char const* typeName (IndexType);

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an index id
////////////////////////////////////////////////////////////////////////////////

        static bool validateId (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an index handle (collection name + / + index id)
////////////////////////////////////////////////////////////////////////////////

        static bool validateHandle (char const*,
                                    size_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new index id
////////////////////////////////////////////////////////////////////////////////

        static TRI_idx_iid_t generateId ();

////////////////////////////////////////////////////////////////////////////////
/// @brief index comparator, used by the coordinator to detect if two index
/// contents are the same
////////////////////////////////////////////////////////////////////////////////

        static bool Compare (TRI_json_t const* lhs,
                             TRI_json_t const* rhs);

        virtual IndexType type () const = 0;
        virtual bool hasSelectivityEstimate () const = 0;
        virtual double selectivityEstimate () const;
        virtual size_t memory () const = 0;
        virtual triagens::basics::Json toJson (TRI_memory_zone_t*) const;
        virtual bool dumpFields () const = 0;
  
        virtual int insert (struct TRI_doc_mptr_t const*, bool) = 0;
        virtual int remove (struct TRI_doc_mptr_t const*, bool) = 0;
        virtual int postInsert (struct TRI_transaction_collection_s*, struct TRI_doc_mptr_t const*);

        // a garbage collection function for the index
        virtual int cleanup ();

        // give index a hint about the expected size
        virtual int sizeHint (size_t);

        // give index a hint, that initial loading is done
        virtual int sizeHint ();

        friend std::ostream& operator<< (std::ostream&, Index const*);
        friend std::ostream& operator<< (std::ostream&, Index const&);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

        TRI_idx_iid_t const                      _iid;

        struct TRI_document_collection_t*        _collection;

        std::vector<std::string> const           _fields;
               
    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
