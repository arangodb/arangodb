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

#include "Marker.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                                            Marker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

Marker::Marker (TRI_df_marker_type_e type, 
                size_t size) 
  : _buffer(new char[size]),
    _size(size) {

  std::cout << "CREATING MARKER OF TYPE: " << type << "\n";

  TRI_df_marker_t* m = reinterpret_cast<TRI_df_marker_t*>(base());
  m->_type = type;
  m->_size = static_cast<TRI_voc_size_t>(size);
  m->_crc  = 0;
  m->_tick = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

Marker::~Marker () {
  if (_buffer != nullptr) {
    delete _buffer;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief store a null-terminated string and length inside the marker
////////////////////////////////////////////////////////////////////////////////
      
void Marker::storeSizedString (size_t offset,
                               char const* value,
                               size_t length) {
  // init key buffer
  char* p = static_cast<char*>(base()) + offset;
  memset(p, '\0', (1 + ((length + 1) / 8)) * 8); 

  // store length of key 
  *p = (uint8_t) length;
  // store actual key 
  memcpy(p + 1, value, length);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            BeginTransactionMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
BeginTransactionMarker::BeginTransactionMarker (TRI_voc_tick_t databaseId,
                                                TRI_voc_tid_t transactionId) 
  : Marker(TRI_WAL_MARKER_BEGIN_TRANSACTION, 
    sizeof(transaction_begin_marker_t)) {

  transaction_begin_marker_t* m = reinterpret_cast<transaction_begin_marker_t*>(base());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

BeginTransactionMarker::~BeginTransactionMarker () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                           CommitTransactionMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
CommitTransactionMarker::CommitTransactionMarker (TRI_voc_tick_t databaseId,
                                                  TRI_voc_tid_t transactionId) 
  : Marker(TRI_WAL_MARKER_COMMIT_TRANSACTION, 
    sizeof(transaction_commit_marker_t)) {

  transaction_commit_marker_t* m = reinterpret_cast<transaction_commit_marker_t*>(base());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CommitTransactionMarker::~CommitTransactionMarker () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                            AbortTransactionMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
AbortTransactionMarker::AbortTransactionMarker (TRI_voc_tick_t databaseId,
                                                TRI_voc_tid_t transactionId) 
  : Marker(TRI_WAL_MARKER_ABORT_TRANSACTION, 
    sizeof(transaction_abort_marker_t)) {

  transaction_abort_marker_t* m = reinterpret_cast<transaction_abort_marker_t*>(base());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

AbortTransactionMarker::~AbortTransactionMarker () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    DocumentMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
        
DocumentMarker::DocumentMarker (TRI_voc_tick_t databaseId,
                                TRI_voc_cid_t collectionId,
                                TRI_voc_rid_t revisionId,
                                TRI_voc_tid_t transactionId,
                                std::string const& key,
                                triagens::basics::JsonLegend& legend,
                                TRI_shaped_json_t const* shapedJson) 
  : Marker(TRI_WAL_MARKER_DOCUMENT, 
    sizeof(document_marker_t) + alignedSize(key.size() + 2) + legend.getSize() + shapedJson->_data.length) {
  document_marker_t* m = reinterpret_cast<document_marker_t*>(base());
  m->_databaseId   = databaseId;
  m->_collectionId = collectionId;
  m->_rid          = revisionId;
  m->_tid          = transactionId;
  m->_shape        = shapedJson->_sid;
  m->_offsetKey    = sizeof(document_marker_t); // start position of key
  m->_offsetLegend = m->_offsetKey + alignedSize(key.size() + 2);
  m->_offsetJson   = m->_offsetLegend + alignedSize(legend.getSize());
          
  storeSizedString(m->_offsetKey, key.c_str(), key.size());

  // store legend 
  {
    char* p = static_cast<char*>(base()) + m->_offsetLegend;
    legend.dump(p);
  }

  // store shapedJson
  {
    char* p = static_cast<char*>(base()) + m->_offsetJson;
    memcpy(p, shapedJson->_data.data, static_cast<size_t>(shapedJson->_data.length));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DocumentMarker::~DocumentMarker () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        EdgeMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
EdgeMarker::EdgeMarker (TRI_voc_tick_t databaseId,
                        TRI_voc_cid_t collectionId,
                        TRI_voc_rid_t revisionId,
                        TRI_voc_tid_t transactionId,
                        std::string const& key,
                        TRI_document_edge_t const* edge,
                        triagens::basics::JsonLegend& legend,
                        TRI_shaped_json_t const* shapedJson) 
  : Marker(TRI_WAL_MARKER_EDGE,
    sizeof(edge_marker_t) + alignedSize(key.size() + 2) + alignedSize(strlen(edge->_fromKey) + 2) + alignedSize(strlen(edge->_toKey) + 2) + legend.getSize() + shapedJson->_data.length) {

  document_marker_t* m = reinterpret_cast<document_marker_t*>(base());
  edge_marker_t* e     = reinterpret_cast<edge_marker_t*>(base());

  m->_databaseId    = databaseId;
  m->_collectionId  = collectionId;
  m->_rid           = revisionId;
  m->_tid           = transactionId;
  m->_shape         = shapedJson->_sid;
  m->_offsetKey     = sizeof(edge_marker_t); // start position of key
  e->_toCid         = edge->_toCid;
  e->_fromCid       = edge->_fromCid;
  e->_offsetToKey   = m->_offsetKey + alignedSize(key.size() + 2);
  e->_offsetFromKey = e->_offsetToKey + alignedSize(strlen(edge->_toKey) + 2);
  m->_offsetLegend  = e->_offsetFromKey + alignedSize(strlen(edge->_fromKey) + 2);
  m->_offsetJson    = m->_offsetLegend + alignedSize(legend.getSize());
          
  // store keys
  storeSizedString(m->_offsetKey, key.c_str(), key.size());
  storeSizedString(e->_offsetFromKey, edge->_fromKey, strlen(edge->_fromKey));
  storeSizedString(e->_offsetToKey, edge->_toKey, strlen(edge->_toKey));

  // store legend 
  {
    char* p = static_cast<char*>(base()) + m->_offsetLegend;
    legend.dump(p);
  }

  // store shapedJson
  {
    char* p = static_cast<char*>(base()) + m->_offsetJson;
    memcpy(p, shapedJson->_data.data, static_cast<size_t>(shapedJson->_data.length));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////
        
EdgeMarker::~EdgeMarker () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      RemoveMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
RemoveMarker::RemoveMarker (TRI_voc_tick_t databaseId,
                            TRI_voc_cid_t collectionId,
                            TRI_voc_rid_t revisionId,
                            TRI_voc_tid_t transactionId,
                            std::string const& key) 
  : Marker(TRI_WAL_MARKER_REMOVE,
    sizeof(remove_marker_t) + alignedSize(key.size() + 2)) {
  remove_marker_t* m = reinterpret_cast<remove_marker_t*>(base());
  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  m->_rid = revisionId;
  m->_tid = transactionId;

  storeSizedString(sizeof(remove_marker_t), key.c_str(), key.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////
      
RemoveMarker::~RemoveMarker () {
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
