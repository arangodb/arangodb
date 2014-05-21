////////////////////////////////////////////////////////////////////////////////
/// @brief WAL markers
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_WAL_MARKER_H
#define TRIAGENS_WAL_MARKER_H 1

#include "Basics/Common.h"
#include "ShapedJson/Legends.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/datafile.h"
#include "VocBase/edge-collection.h"

namespace triagens {
  namespace wal {

    static_assert(sizeof(TRI_df_marker_t) == 24, "invalid base marker size");

// -----------------------------------------------------------------------------
// --SECTION--                   low level structs, used for on-disk persistence
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction begin marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_begin_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction commit marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_commit_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction abort marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_abort_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal document marker
////////////////////////////////////////////////////////////////////////////////

    struct document_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_cid_t   _collectionId;

      TRI_voc_rid_t   _rid;        // this is the tick for a create and update
      TRI_voc_tid_t   _tid;

      TRI_shape_sid_t _shape;

      uint16_t        _offsetKey;
      uint16_t        _offsetLegend;
      uint32_t        _offsetJson;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal edge marker
////////////////////////////////////////////////////////////////////////////////

    struct edge_marker_t : document_marker_t {
      TRI_voc_cid_t   _toCid;
      TRI_voc_cid_t   _fromCid;

      uint16_t        _offsetToKey;
      uint16_t        _offsetFromKey;

#ifdef TRI_PADDING_32
      char            _padding_df_marker[4];
#endif
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal remove marker
////////////////////////////////////////////////////////////////////////////////

    struct remove_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_cid_t   _collectionId;

      TRI_voc_rid_t   _rid;   // this is the tick for the deletion
      TRI_voc_tid_t   _tid;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                            Marker
// -----------------------------------------------------------------------------

    class Marker {

      protected:

        Marker (Marker const&) = delete;
        Marker& operator= (Marker const&) = delete;

        Marker (TRI_df_marker_type_e,
                size_t);

        virtual ~Marker ();
        
// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
      
        static inline size_t alignedSize (size_t size) {
          return TRI_DF_ALIGN_BLOCK(size);
        }
        
        inline void* mem () const {
          return static_cast<void*>(_buffer);
        }

        inline char* base () const {
          return _buffer;
        }
      
        inline char* payload () const {
          return base() + sizeof(TRI_df_marker_t);
        }

        inline uint32_t size () const {
          return _size;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------
        
      protected:

        void storeSizedString (size_t,
                               char const*,
                               size_t);
 
// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

        char*          _buffer;
        uint32_t const _size;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                            BeginTransactionMarker
// -----------------------------------------------------------------------------

    class BeginTransactionMarker : public Marker {

      public:

        BeginTransactionMarker (TRI_voc_tick_t,
                                TRI_voc_tid_t); 

        ~BeginTransactionMarker ();
    };

// -----------------------------------------------------------------------------
// --SECTION--                                           CommitTransactionMarker
// -----------------------------------------------------------------------------
    
    class CommitTransactionMarker : public Marker {

      public:

        CommitTransactionMarker (TRI_voc_tick_t,
                                 TRI_voc_tid_t);

        ~CommitTransactionMarker ();
    };

// -----------------------------------------------------------------------------
// --SECTION--                                            AbortTransactionMarker
// -----------------------------------------------------------------------------

    class AbortTransactionMarker : public Marker {

      public:

        AbortTransactionMarker (TRI_voc_tick_t,
                                TRI_voc_tid_t);

        ~AbortTransactionMarker ();
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    DocumentMarker
// -----------------------------------------------------------------------------

    class DocumentMarker : public Marker {

      public:

        DocumentMarker (TRI_voc_tick_t,
                        TRI_voc_cid_t,
                        TRI_voc_rid_t,
                        TRI_voc_tid_t,
                        std::string const&,
                        triagens::basics::JsonLegend&,
                        TRI_shaped_json_t const*);

        ~DocumentMarker ();

      public: 

        inline char const* key () const {
          // pointer to key
          return base() + sizeof(document_marker_t);
        }
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                        EdgeMarker
// -----------------------------------------------------------------------------
    
    class EdgeMarker : public Marker {

      public:

        EdgeMarker (TRI_voc_tick_t,
                    TRI_voc_cid_t,
                    TRI_voc_rid_t,
                    TRI_voc_tid_t,
                    std::string const&,
                    TRI_document_edge_t const*,
                    triagens::basics::JsonLegend&,
                    TRI_shaped_json_t const*);

        ~EdgeMarker ();
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                      RemoveMarker
// -----------------------------------------------------------------------------

    class RemoveMarker : public Marker {

      public:

        RemoveMarker (TRI_voc_tick_t,
                      TRI_voc_cid_t,
                      TRI_voc_rid_t,
                      TRI_voc_tid_t,
                      std::string const&);

        ~RemoveMarker ();

      public:

        inline char const* key () const {
          // pointer to key
          return base() + sizeof(remove_marker_t);
        }

        inline TRI_voc_rid_t rid () const {
          remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(base());
          return m->_rid;
        }

        void dump () const;
    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
