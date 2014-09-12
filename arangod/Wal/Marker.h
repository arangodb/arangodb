////////////////////////////////////////////////////////////////////////////////
/// @brief WAL markers
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_WAL_MARKER_H
#define ARANGODB_WAL_MARKER_H 1

#include "Basics/Common.h"
#include "Basics/tri-strings.h"
#include "ShapedJson/Legends.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/datafile.h"
#include "VocBase/voc-types.h"

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
/// @brief wal create database marker
////////////////////////////////////////////////////////////////////////////////

    struct database_create_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      // char* properties
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal drop database marker
////////////////////////////////////////////////////////////////////////////////

    struct database_drop_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
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
/// @brief wal create index marker
////////////////////////////////////////////////////////////////////////////////

    struct index_create_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;
      TRI_idx_iid_t    _indexId;
      // char* properties
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal drop index marker
////////////////////////////////////////////////////////////////////////////////

    struct index_drop_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t   _databaseId;
      TRI_voc_cid_t    _collectionId;
      TRI_idx_iid_t    _indexId;
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
/// @brief wal remote transaction begin marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_remote_begin_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
      TRI_voc_tid_t   _externalId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal remote transaction commit marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_remote_commit_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
      TRI_voc_tid_t   _externalId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal remote transaction abort marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_remote_abort_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
      TRI_voc_tid_t   _externalId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal document marker
////////////////////////////////////////////////////////////////////////////////

    struct document_marker_t : TRI_df_marker_t {
      TRI_voc_tick_t  _databaseId;
      TRI_voc_cid_t   _collectionId;

      TRI_voc_rid_t   _revisionId;        // this is the tick for a create and update
      TRI_voc_tid_t   _transactionId;

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

      TRI_voc_rid_t   _revisionId;   // this is the tick for the deletion
      TRI_voc_tid_t   _transactionId;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief create a marker from a marker existing in memory
////////////////////////////////////////////////////////////////////////////////

        Marker (TRI_df_marker_t const*,
                TRI_voc_fid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a marker that manages its own memory
////////////////////////////////////////////////////////////////////////////////

        Marker (TRI_df_marker_type_e,
                size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

        virtual ~Marker ();
        
        inline void freeBuffer () {
          if (_buffer != nullptr && _mustFree) {
            delete[] _buffer;
            
            _buffer = nullptr;
            _mustFree = false;
          }
        }

        inline char* steal () {
          char* buffer = _buffer;
          _buffer = nullptr;
          _mustFree = false;
          return buffer;
        }

        inline TRI_voc_fid_t fid () const {
          return _fid;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the destructor must free the memory
////////////////////////////////////////////////////////////////////////////////

        bool           _mustFree;

////////////////////////////////////////////////////////////////////////////////
/// @brief id of the logfile the marker is stored in
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_fid_t  _fid;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    EnvelopeMarker
// -----------------------------------------------------------------------------

    class EnvelopeMarker : public Marker {

      public:

        explicit EnvelopeMarker (TRI_df_marker_t const*, 
                                 TRI_voc_fid_t);

        ~EnvelopeMarker ();
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

        void setType (TRI_df_marker_type_t);

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
// --SECTION--                                              CreateDatabaseMarker
// -----------------------------------------------------------------------------

    class CreateDatabaseMarker : public Marker {

      public:

        CreateDatabaseMarker (TRI_voc_tick_t,
                              std::string const&);

        ~CreateDatabaseMarker ();

      public:

        inline char* properties () const {
          return begin() + sizeof(database_create_marker_t);
        }

        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                DropDatabaseMarker
// -----------------------------------------------------------------------------

    class DropDatabaseMarker : public Marker {

      public:

        DropDatabaseMarker (TRI_voc_tick_t);

        ~DropDatabaseMarker ();

      public:

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
// --SECTION--                                                 CreateIndexMarker
// -----------------------------------------------------------------------------

    class CreateIndexMarker : public Marker {

      public:

        CreateIndexMarker (TRI_voc_tick_t,
                           TRI_voc_cid_t,
                           TRI_idx_iid_t,
                           std::string const&);

        ~CreateIndexMarker ();

      public:

        inline char* properties () const {
          return begin() + sizeof(index_create_marker_t);
        }

        void dump () const;
    };


// -----------------------------------------------------------------------------
// --SECTION--                                                   DropIndexMarker
// -----------------------------------------------------------------------------

    class DropIndexMarker : public Marker {

      public:

        DropIndexMarker (TRI_voc_tick_t,
                         TRI_voc_cid_t,
                         TRI_idx_iid_t);

        ~DropIndexMarker ();

      public:

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
// --SECTION--                                      BeginRemoteTransactionMarker
// -----------------------------------------------------------------------------

    class BeginRemoteTransactionMarker : public Marker {

      public:

        BeginRemoteTransactionMarker (TRI_voc_tick_t,
                                      TRI_voc_tid_t,
                                      TRI_voc_tid_t);

        ~BeginRemoteTransactionMarker ();

      public:

        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                     CommitRemoteTransactionMarker
// -----------------------------------------------------------------------------

    class CommitRemoteTransactionMarker : public Marker {

      public:

        CommitRemoteTransactionMarker (TRI_voc_tick_t,
                                       TRI_voc_tid_t,
                                       TRI_voc_tid_t);

        ~CommitRemoteTransactionMarker ();

      public:

        void dump () const;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                      AbortRemoteTransactionMarker
// -----------------------------------------------------------------------------

    class AbortRemoteTransactionMarker : public Marker {

      public:

        AbortRemoteTransactionMarker (TRI_voc_tick_t,
                                      TRI_voc_tid_t,
                                      TRI_voc_tid_t);

        ~AbortRemoteTransactionMarker ();

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
                        size_t,
                        TRI_shaped_json_t const*);
        
        ~DocumentMarker ();

      public:

        inline TRI_voc_rid_t revisionId () const {
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return m->_revisionId;
        }

        inline TRI_voc_tid_t transactionId () const {
          document_marker_t const* m = reinterpret_cast<document_marker_t const*>(begin());
          return m->_transactionId;
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

        void storeLegend (triagens::basics::JsonLegend&);

        void dump () const;

        static DocumentMarker* clone (TRI_df_marker_t const*,
                                      TRI_voc_tick_t,
                                      TRI_voc_cid_t,
                                      TRI_voc_rid_t,
                                      TRI_voc_tid_t,
                                      size_t,
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
                    size_t,
                    TRI_shaped_json_t const*);
        
        ~EdgeMarker ();

        inline TRI_voc_rid_t revisionId () const {
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return m->_revisionId;
        }

        inline TRI_voc_rid_t transactionId () const {
          edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(begin());
          return m->_transactionId;
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
        
        void storeLegend (triagens::basics::JsonLegend&);

        void dump () const;

        static EdgeMarker* clone (TRI_df_marker_t const*,
                                  TRI_voc_tick_t,
                                  TRI_voc_cid_t,
                                  TRI_voc_rid_t,
                                  TRI_voc_tid_t,
                                  size_t,
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

        inline TRI_voc_tid_t transactionId () const {
          remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(begin());
          return m->_transactionId;
        }

        inline TRI_voc_rid_t revisionId () const {
          remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(begin());
          return m->_revisionId;
        }

        void dump () const;
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
