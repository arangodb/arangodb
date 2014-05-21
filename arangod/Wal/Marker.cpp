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
  char* p = static_cast<char*>(base()) + offset;

  // store actual key 
  memcpy(p, value, length);
  // append NUL byte
  p[length] = '\0';
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
    sizeof(document_marker_t) + alignedSize(key.size() + 1) + legend.getSize() + shapedJson->_data.length) {
  document_marker_t* m = reinterpret_cast<document_marker_t*>(base());
  m->_databaseId   = databaseId;
  m->_collectionId = collectionId;
  m->_rid          = revisionId;
  m->_tid          = transactionId;
  m->_shape        = shapedJson->_sid;
  m->_offsetKey    = sizeof(document_marker_t); // start position of key
  m->_offsetLegend = m->_offsetKey + alignedSize(key.size() + 1);
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
  
  dump();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DocumentMarker::~DocumentMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

void DocumentMarker::dump () const {
  document_marker_t* m = reinterpret_cast<document_marker_t*>(base());

  std::cout << "WAL DOCUMENT MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", REV: " << m->_rid 
            << ", TRX: " << m->_tid 
            << ", KEY: " << ((char*) base() + m->_offsetKey)  
            << "\n";
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
    sizeof(edge_marker_t) + alignedSize(key.size() + 1) + alignedSize(strlen(edge->_fromKey) + 1) + alignedSize(strlen(edge->_toKey) + 1) + legend.getSize() + shapedJson->_data.length) {

//  document_marker_t* m = reinterpret_cast<document_marker_t*>(base());
  edge_marker_t* m = reinterpret_cast<edge_marker_t*>(base());

  m->_databaseId    = databaseId;
  m->_collectionId  = collectionId;
  m->_rid           = revisionId;
  m->_tid           = transactionId;
  m->_shape         = shapedJson->_sid;
  m->_offsetKey     = sizeof(edge_marker_t); // start position of key
  m->_toCid         = edge->_toCid;
  m->_fromCid       = edge->_fromCid;
  m->_offsetToKey   = m->_offsetKey + alignedSize(key.size() + 1);
  m->_offsetFromKey = m->_offsetToKey + alignedSize(strlen(edge->_toKey) + 1);
  m->_offsetLegend  = m->_offsetFromKey + alignedSize(strlen(edge->_fromKey) + 1);
  m->_offsetJson    = m->_offsetLegend + alignedSize(legend.getSize());
          
  // store keys
  storeSizedString(m->_offsetKey, key.c_str(), key.size());
  storeSizedString(m->_offsetFromKey, edge->_fromKey, strlen(edge->_fromKey));
  storeSizedString(m->_offsetToKey, edge->_toKey, strlen(edge->_toKey));

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
  
  dump();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////
        
EdgeMarker::~EdgeMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

void EdgeMarker::dump () const {
  edge_marker_t* m = reinterpret_cast<edge_marker_t*>(base());

  std::cout << "WAL EDGE MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", REV: " << m->_rid 
            << ", TRX: " << m->_tid 
            << ", KEY: " << ((char*) base() + m->_offsetKey)
            << ", FROMCID " << m->_fromCid 
            << ", TOCID " << m->_toCid 
            << ", FROMKEY: " << ((char*) base() + m->_offsetFromKey) 
            << ", TOKEY: " << ((char*) base() + m->_offsetFromKey)
            << "\n";
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
    sizeof(remove_marker_t) + alignedSize(key.size() + 1)) {
  remove_marker_t* m = reinterpret_cast<remove_marker_t*>(base());
  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  m->_rid = revisionId;
  m->_tid = transactionId;

  storeSizedString(sizeof(remove_marker_t), key.c_str(), key.size());

  dump();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////
      
RemoveMarker::~RemoveMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

void RemoveMarker::dump () const {
  remove_marker_t* m = reinterpret_cast<remove_marker_t*>(base());

  std::cout << "WAL REMOVE MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", REV: " << m->_rid 
            << ", TRX: " << m->_tid 
            << ", KEY: " << (((char*) base()) + sizeof(remove_marker_t)) 
            << "\n";
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
