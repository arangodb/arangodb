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
#include "ShapedJson/shaped-json.h"
#include "VocBase/datafile.h"

namespace triagens {
  namespace wal {

    static_assert(sizeof(TRI_df_marker_t) == 24, "invalid base marker size");

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction begin marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_begin_marker_t {
      TRI_df_marker_t base;

      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction commit marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_commit_marker_t {
      TRI_df_marker_t base;

      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction abort marker
////////////////////////////////////////////////////////////////////////////////

    struct transaction_abort_marker_t {
      TRI_df_marker_t base;

      TRI_voc_tick_t  _databaseId;
      TRI_voc_tid_t   _transactionId;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal document marker
////////////////////////////////////////////////////////////////////////////////

    struct document_marker_t {
      TRI_df_marker_t base;

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

    struct edge_marker_t {
      document_marker_t base;

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

    struct remove_marker_t {
      TRI_df_marker_t base;

      TRI_voc_tick_t  _databaseId;
      TRI_voc_cid_t   _collectionId;

      TRI_voc_rid_t   _rid;   // this is the tick for the deletion
      TRI_voc_tid_t   _tid;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief wal marker class
////////////////////////////////////////////////////////////////////////////////

    struct Marker {
      Marker (TRI_df_marker_type_e type,
              size_t size)
        : buffer(new char[size]),
          size(size) {

        std::cout << "CREATING MARKER OF TYPE: " << type << "\n";

        // initialise the marker header
        auto h = header();
        h->_type = type;
        h->_size = static_cast<TRI_voc_size_t>(size);
        h->_crc  = 0;
        h->_tick = 0;
      }

      virtual ~Marker () {
        if (buffer != nullptr) {
          delete buffer;
        }
      }
      
      static inline size_t alignedSize (size_t size) {
        return TRI_DF_ALIGN_BLOCK(size);
      }

      inline TRI_df_marker_t* header () const {
        return (TRI_df_marker_t*) buffer;
      }
      
      inline char* base () const {
        return (char*) buffer;
      }
      
      inline char* payload () const {
        return base() + sizeof(TRI_df_marker_t);
      }

      char*          buffer;
      uint32_t const size;
    };

    struct BeginTransactionMarker : public Marker {
      BeginTransactionMarker (TRI_voc_tick_t databaseId,
                              TRI_voc_tid_t transactionId) 
        : Marker(TRI_WAL_MARKER_BEGIN_TRANSACTION, 
                 sizeof(transaction_begin_marker_t)) {
        
        transaction_begin_marker_t* m = reinterpret_cast<transaction_begin_marker_t*>(base());

        m->_databaseId = databaseId;
        m->_transactionId = transactionId; 
      }

      ~BeginTransactionMarker () {
      }

    };
    
    struct CommitTransactionMarker : public Marker {
      CommitTransactionMarker (TRI_voc_tick_t databaseId,
                               TRI_voc_tid_t transactionId) 
        : Marker(TRI_WAL_MARKER_COMMIT_TRANSACTION, 
                 sizeof(transaction_commit_marker_t)) {
        
        transaction_commit_marker_t* m = reinterpret_cast<transaction_commit_marker_t*>(base());
        
        m->_databaseId = databaseId;
        m->_transactionId = transactionId; 
      }

      ~CommitTransactionMarker () {
      }

    };

    struct AbortTransactionMarker : public Marker {
      AbortTransactionMarker (TRI_voc_tick_t databaseId,
                              TRI_voc_tid_t transactionId) 
        : Marker(TRI_WAL_MARKER_ABORT_TRANSACTION, 
                 sizeof(transaction_abort_marker_t)) {
        
        transaction_abort_marker_t* m = reinterpret_cast<transaction_abort_marker_t*>(base());
        
        m->_databaseId = databaseId;
        m->_transactionId = transactionId; 
      }

      ~AbortTransactionMarker () {
      }

    };

    struct DocumentMarker : public Marker {
      DocumentMarker (TRI_voc_tick_t databaseId,
                      TRI_voc_cid_t collectionId,
                      TRI_voc_rid_t revisionId,
                      TRI_voc_tid_t transactionId,
                      std::string const& key,
                      TRI_shaped_json_t const* shapedJson) 
        : Marker(TRI_WAL_MARKER_DOCUMENT, 
                 sizeof(document_marker_t) + alignedSize(key.size() + 2) + shapedJson->_data.length) {

        document_marker_t* m = reinterpret_cast<document_marker_t*>(base());
        m->_databaseId   = databaseId;
        m->_collectionId = collectionId;
        m->_rid          = revisionId;
        m->_tid          = transactionId;
        m->_shape        = shapedJson->_sid;
        m->_offsetKey    = sizeof(document_marker_t); // start position of key
        m->_offsetLegend = m->_offsetKey + alignedSize(key.size() + 2);
        m->_offsetJson   = m->_offsetLegend; // TODO: account for legendSize // + alignedSize(legendSize)
               
        {
          // store key
          size_t const n = key.size();
          char* p = static_cast<char*>(base()) + m->_offsetKey;

          // init key buffer
          memset(p, '\0', (1 + ((n + 1) / 8)) * 8); 
       
          // store length of key 
          *p = (uint8_t) n;
          // store actual key 
          memcpy(p + 1, key.c_str(), n); 
        }

        // store legend // TODO

        // store shapedJson
        {
          char* p = static_cast<char*>(base()) + m->_offsetJson;
          memcpy(p, shapedJson->_data.data, static_cast<size_t>(shapedJson->_data.length));
        }
      }

      ~DocumentMarker () {
      }

    };

   /* 
    struct RemoveMarker : public Marker {
      RemoveMarker (TRI_voc_tick_t databaseId,
                    TRI_voc_cid_t collectionId,
                    TRI_voc_tid_t transactionId,
                    std::string const& key) 
        : Marker(TRI_WAL_MARKER_REMOVE,
                 sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t) + sizeof(TRI_voc_tid_t) + key.size() + 2) {

        char* p = data();
        store<TRI_voc_tick_t>(p, databaseId);
        store<TRI_voc_cid_t>(p, collectionId);
        store<TRI_voc_tid_t>(p, transactionId);

        // store key
        store<uint8_t>(p, (uint8_t) key.size()); 
        store(p, key.c_str(), key.size());
        store<unsigned char>(p, '\0');
      }

      ~RemoveMarker () {
      }

    };
*/
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
