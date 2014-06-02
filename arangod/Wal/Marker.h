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
#include "BasicsC/tri-strings.h"
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
/// @brief wal attribute marker
////////////////////////////////////////////////////////////////////////////////

    struct attribute_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;

      TRI_shape_aid_t  _attributeId;
      // char* name
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal shape marker
////////////////////////////////////////////////////////////////////////////////

    struct shape_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;
      // char* shape
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal create collection marker
////////////////////////////////////////////////////////////////////////////////

    struct collection_create_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;
      // char* properties
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal drop collection marker
////////////////////////////////////////////////////////////////////////////////

    struct collection_drop_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal rename collection marker
////////////////////////////////////////////////////////////////////////////////

    struct collection_rename_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;
      // char* name
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal change collection marker
////////////////////////////////////////////////////////////////////////////////

    struct collection_change_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;
      // char* properties
    };

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

      // char* key
      // char* shapedJson
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
      
      // char* key
      // char* toKey
      // char* fromKey
      // char* shapedJson
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal remove marker
////////////////////////////////////////////////////////////////////////////////

    struct remove_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_cid_t   _collectionId;

      TRI_voc_rid_t   _rid;   // this is the tick for the deletion
      TRI_voc_tid_t   _tid;
      
      // char* key
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                            Marker
// -----------------------------------------------------------------------------

    class Marker {

      protected:

        Marker& operator= (Marker const&) = delete;

        Marker (Marker&&) = delete; 
        
        Marker (Marker const&) = delete;

        Marker (TRI_df_marker_type_e,
                size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
        
        virtual ~Marker ();

        inline char* steal () {
          char* buffer = _buffer;
          _buffer = nullptr;
          return buffer;
        }

        static inline size_t alignedSize (size_t size) {
          return TRI_DF_ALIGN_BLOCK(size);
        }
        
        inline void* mem () const {
          return static_cast<void*>(_buffer);
        }

        inline char* begin () const {
          return _buffer;
        }
        
        inline char* end () const {
          return _buffer + _size;
        }
     /* 
        inline char* payload () const {
          return begin() + sizeof(TRI_df_marker_t);
        }
*/
////////////////////////////////////////////////////////////////////////////////
/// @brief return the size of the marker
////////////////////////////////////////////////////////////////////////////////

        inline uint32_t size () const {
          return _size;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a hex representation of a marker part
////////////////////////////////////////////////////////////////////////////////
        
        std::string hexifyPart (char const*, size_t) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return a printable string representation of a marker part
////////////////////////////////////////////////////////////////////////////////
  
        std::string stringifyPart (char const*, size_t) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief print the marker in binary form
////////////////////////////////////////////////////////////////////////////////

        void dumpBinary () const;
      

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------
        
      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief store a null-terminated string inside the marker
////////////////////////////////////////////////////////////////////////////////
        
        void storeSizedString (size_t,
                               std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief store a null-terminated string inside the marker
////////////////////////////////////////////////////////////////////////////////

        void storeSizedString (size_t,
                               char const*,
                               size_t);
 
// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to marker data
////////////////////////////////////////////////////////////////////////////////

        char*          _buffer;

////////////////////////////////////////////////////////////////////////////////
/// @brief size of marker data
////////////////////////////////////////////////////////////////////////////////

        uint32_t const _size;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                   AttributeMarker
// -----------------------------------------------------------------------------

    class AttributeMarker : public Marker {

      public:

        AttributeMarker (TRI_voc_tick_t,
                         TRI_voc_cid_t,
                         TRI_shape_aid_t,
                         std::string const&); 

        ~AttributeMarker ();

      public:

        inline char* attributeName () const {
          // pointer to attribute name
          return begin() + sizeof(attribute_marker_t);
        }
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       ShapeMarker
// -----------------------------------------------------------------------------

    class ShapeMarker : public Marker {

      public:

        ShapeMarker (TRI_voc_tick_t,
                     TRI_voc_cid_t,
                     TRI_shape_t const*);

        ~ShapeMarker ();
        
      public:

        inline char* shape () const {
          return begin() + sizeof(shape_marker_t);
        }
        
        inline TRI_shape_sid_t shapeId () const {
          return reinterpret_cast<TRI_shape_t*>(shape())->_sid;
        }
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                            CreateCollectionMarker
// -----------------------------------------------------------------------------

    class CreateCollectionMarker : public Marker {

      public:

        CreateCollectionMarker (TRI_voc_tick_t,
                                TRI_voc_cid_t,
                                std::string const&);

        ~CreateCollectionMarker ();
        
      public:
        
        inline char* properties () const {
          return begin() + sizeof(collection_create_marker_t);
        }
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                              DropCollectionMarker
// -----------------------------------------------------------------------------

    class DropCollectionMarker : public Marker {

      public:

        DropCollectionMarker (TRI_voc_tick_t,
                              TRI_voc_cid_t);

        ~DropCollectionMarker ();
        
      public:
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                            RenameCollectionMarker
// -----------------------------------------------------------------------------

    class RenameCollectionMarker : public Marker {

      public:

        RenameCollectionMarker (TRI_voc_tick_t,
                                TRI_voc_cid_t,
                                std::string const&);

        ~RenameCollectionMarker ();
        
      public:
        
        inline char* name () const {
          return begin() + sizeof(collection_rename_marker_t);
        }
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                            ChangeCollectionMarker
// -----------------------------------------------------------------------------

    class ChangeCollectionMarker : public Marker {

      public:

        ChangeCollectionMarker (TRI_voc_tick_t,
                                TRI_voc_cid_t,
                                std::string const&);

        ~ChangeCollectionMarker ();
        
      public:
        
        inline char* properties () const {
          return begin() + sizeof(collection_change_marker_t);
        }
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                            BeginTransactionMarker
// -----------------------------------------------------------------------------

    class BeginTransactionMarker : public Marker {

      public:

        BeginTransactionMarker (TRI_voc_tick_t,
                                TRI_voc_tid_t); 

        ~BeginTransactionMarker ();

      public:
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                           CommitTransactionMarker
// -----------------------------------------------------------------------------
    
    class CommitTransactionMarker : public Marker {

      public:

        CommitTransactionMarker (TRI_voc_tick_t,
                                 TRI_voc_tid_t);

        ~CommitTransactionMarker ();
      
      public:
        
        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                            AbortTransactionMarker
// -----------------------------------------------------------------------------

    class AbortTransactionMarker : public Marker {

      public:

        AbortTransactionMarker (TRI_voc_tick_t,
                                TRI_voc_tid_t);

        ~AbortTransactionMarker ();
      
      public:
        
        void dump () const;
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
        
        inline TRI_voc_rid_t rid () const {
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return m->_rid;
        }
        
        inline TRI_voc_rid_t tid () const {
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return m->_tid;
        }

        inline char* key () const {
          // pointer to key
          return begin() + sizeof(document_marker_t);
        }

        inline char const* legend () const {
          // pointer to legend
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return begin() + m->_offsetLegend;
        }
        
        inline size_t legendLength () const {
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return static_cast<size_t>(m->_offsetJson - m->_offsetLegend);
        }
        
        inline char const* json () const {
          // pointer to json
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return begin() + m->_offsetJson;
        }
        
        inline size_t jsonLength () const {
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return static_cast<size_t>(size() - m->_offsetJson);
        }
         
        void dump () const;
        
        static DocumentMarker* clone (TRI_df_marker_t const*,
                                      TRI_voc_tick_t,
                                      TRI_voc_cid_t,
                                      TRI_voc_rid_t,
                                      TRI_voc_tid_t,
                                      triagens::basics::JsonLegend&,
                                      TRI_shaped_json_t const*);
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

        inline TRI_voc_rid_t rid () const {
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return m->_rid;
        }
        
        inline TRI_voc_rid_t tid () const {
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return m->_tid;
        }

        inline char const* key () const {
          // pointer to key
          return begin() + sizeof(edge_marker_t);
        }

        inline char const* fromKey () const {
          // pointer to _from key
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return begin() + m->_offsetFromKey;
        }
        
        inline char const* toKey () const {
          // pointer to _to key
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return begin() + m->_offsetToKey;
        }

        inline char const* legend () const {
          // pointer to legend
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return begin() + m->_offsetLegend;
        }
        
        inline size_t legendLength () const {
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return static_cast<size_t>(m->_offsetJson - m->_offsetLegend);
        }
        
        inline char const* json () const {
          // pointer to json
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return begin() + m->_offsetJson;
        }
        
        inline size_t jsonLength () const {
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return static_cast<size_t>(size() - m->_offsetJson);
        }
        
        void dump () const;
        
        static EdgeMarker* clone (TRI_df_marker_t const*,
                                  TRI_voc_tick_t,
                                  TRI_voc_cid_t,
                                  TRI_voc_rid_t,
                                  TRI_voc_tid_t,
                                  triagens::basics::JsonLegend&,
                                  TRI_shaped_json_t const*);
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
          return begin() + sizeof(remove_marker_t);
        }

        inline TRI_voc_rid_t tid () const {
          remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(begin());
          return m->_tid;
        }

        inline TRI_voc_rid_t rid () const {
          remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(begin());
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
