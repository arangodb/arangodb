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
#include "VocBase/document-collection.h"

#undef DEBUG_WAL  
#undef DEBUG_WAL_DETAIL  

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                                            Marker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker with a sized buffer
////////////////////////////////////////////////////////////////////////////////

Marker::Marker (TRI_df_marker_type_e type, 
                size_t size) 
  : _buffer(new char[size]),
    _size(static_cast<uint32_t>(size)) {

  TRI_df_marker_t* m = reinterpret_cast<TRI_df_marker_t*>(begin());
  memset(m, 0, size);

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
    delete[] _buffer;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief store a null-terminated string inside the marker
////////////////////////////////////////////////////////////////////////////////
      
void Marker::storeSizedString (size_t offset,
                               std::string const& value) {
  return storeSizedString(offset, value.c_str(), value.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a null-terminated string inside the marker
////////////////////////////////////////////////////////////////////////////////
      
void Marker::storeSizedString (size_t offset,
                               char const* value,
                               size_t length) {
  char* p = static_cast<char*>(begin()) + offset;

  // store actual key 
  memcpy(p, value, length);
  // append NUL byte
  p[length] = '\0';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a hex representation of a marker part
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
std::string Marker::hexifyPart (char const* offset, size_t length) const {
  size_t destLength;
  char* s = TRI_EncodeHexString(offset, length, &destLength);

  if (s != nullptr) {
    std::string result(s);
    TRI_Free(TRI_CORE_MEM_ZONE, s);
    return result;
  }

  return "ERROR";
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return a printable string representation of a marker part
////////////////////////////////////////////////////////////////////////////////
  
#ifdef DEBUG_WAL
std::string Marker::stringifyPart (char const* offset, size_t length) const {
  char* s = TRI_PrintableString(offset, length);

  if (s != nullptr) {
    std::string result(s);
    TRI_Free(TRI_CORE_MEM_ZONE, s);
    return result;
  }

  return "ERROR";
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief print the marker in binary form
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void Marker::dumpBinary () const {
  std::cout << "BINARY:     '" << stringifyPart(begin(), size()) << "'\n\n";
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   AttributeMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
AttributeMarker::AttributeMarker (TRI_voc_tick_t databaseId,
                                  TRI_voc_cid_t collectionId,
                                  TRI_shape_aid_t attributeId,
                                  std::string const& attributeName) 
  : Marker(TRI_WAL_MARKER_ATTRIBUTE, sizeof(attribute_marker_t) + alignedSize(attributeName.size() + 1)) {

  attribute_marker_t* m = reinterpret_cast<attribute_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  m->_attributeId = attributeId;

  storeSizedString(sizeof(attribute_marker_t), attributeName);

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

AttributeMarker::~AttributeMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void AttributeMarker::dump () const {
  attribute_marker_t* m = reinterpret_cast<attribute_marker_t*>(begin());

  std::cout << "WAL ATTRIBUTE MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", ATTRIBUTE ID: " << m->_attributeId
            << ", ATTRIBUTE: " << attributeName() 
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif            

// -----------------------------------------------------------------------------
// --SECTION--                                                       ShapeMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
ShapeMarker::ShapeMarker (TRI_voc_tick_t databaseId,
                          TRI_voc_cid_t collectionId,
                          TRI_shape_t const* shape) 
  : Marker(TRI_WAL_MARKER_SHAPE, sizeof(shape_marker_t) + shape->_size) {

  shape_marker_t* m = reinterpret_cast<shape_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;

  memcpy(this->shape(), shape, shape->_size); 

#ifdef DEBUG_WAL
  dump();
#endif  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

ShapeMarker::~ShapeMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void ShapeMarker::dump () const {
  shape_marker_t* m = reinterpret_cast<shape_marker_t*>(begin());

  std::cout << "WAL SHAPE MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", SHAPE ID: " << shapeId()
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              CreateDatabaseMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
CreateDatabaseMarker::CreateDatabaseMarker (TRI_voc_tick_t databaseId,
                                            std::string const& properties) 
  : Marker(TRI_WAL_MARKER_CREATE_DATABASE, sizeof(database_create_marker_t) + alignedSize(properties.size() + 1)) {

  database_create_marker_t* m = reinterpret_cast<database_create_marker_t*>(begin());

  m->_databaseId = databaseId;
  
  storeSizedString(sizeof(database_create_marker_t), properties);
  
#ifdef DEBUG_WAL
  dump();
#endif  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateDatabaseMarker::~CreateDatabaseMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CreateDatabaseMarker::dump () const {
  database_create_marker_t* m = reinterpret_cast<database_create_marker_t*>(begin());

  std::cout << "WAL CREATE DATABASE MARKER FOR DB " << m->_databaseId 
            << ", PROPERTIES " << properties()
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                DropDatabaseMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
DropDatabaseMarker::DropDatabaseMarker (TRI_voc_tick_t databaseId)
  : Marker(TRI_WAL_MARKER_DROP_DATABASE, sizeof(database_drop_marker_t)) {

  database_drop_marker_t* m = reinterpret_cast<database_drop_marker_t*>(begin());

  m->_databaseId = databaseId;
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropDatabaseMarker::~DropDatabaseMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void DropDatabaseMarker::dump () const {
  database_drop_marker_t* m = reinterpret_cast<database_drop_marker_t*>(begin());

  std::cout << "WAL DROP DATABASE MARKER FOR DB " << m->_databaseId 
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                            CreateCollectionMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
CreateCollectionMarker::CreateCollectionMarker (TRI_voc_tick_t databaseId,
                                                TRI_voc_cid_t collectionId,
                                                string const& properties) 
  : Marker(TRI_WAL_MARKER_CREATE_COLLECTION, sizeof(collection_create_marker_t) + alignedSize(properties.size() + 1)) {

  collection_create_marker_t* m = reinterpret_cast<collection_create_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  
  storeSizedString(sizeof(collection_create_marker_t), properties);
  
#ifdef DEBUG_WAL
  dump();
#endif  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateCollectionMarker::~CreateCollectionMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CreateCollectionMarker::dump () const {
  collection_create_marker_t* m = reinterpret_cast<collection_create_marker_t*>(begin());

  std::cout << "WAL CREATE COLLECTION MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId
            << ", PROPERTIES " << properties()
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              DropCollectionMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
DropCollectionMarker::DropCollectionMarker (TRI_voc_tick_t databaseId,
                                            TRI_voc_cid_t collectionId) 
  : Marker(TRI_WAL_MARKER_DROP_COLLECTION, sizeof(collection_drop_marker_t)) {

  collection_drop_marker_t* m = reinterpret_cast<collection_drop_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropCollectionMarker::~DropCollectionMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void DropCollectionMarker::dump () const {
  collection_drop_marker_t* m = reinterpret_cast<collection_drop_marker_t*>(begin());

  std::cout << "WAL DROP COLLECTION MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                            RenameCollectionMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
RenameCollectionMarker::RenameCollectionMarker (TRI_voc_tick_t databaseId,
                                                TRI_voc_cid_t collectionId,
                                                string const& name)
  : Marker(TRI_WAL_MARKER_RENAME_COLLECTION, sizeof(collection_rename_marker_t) + alignedSize(name.size() + 1)) {

  collection_rename_marker_t* m = reinterpret_cast<collection_rename_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  
  storeSizedString(sizeof(collection_rename_marker_t), name);
  
#ifdef DEBUG_WAL
  dump();
#endif  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

RenameCollectionMarker::~RenameCollectionMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void RenameCollectionMarker::dump () const {
  collection_rename_marker_t* m = reinterpret_cast<collection_rename_marker_t*>(begin());

  std::cout << "WAL RENAME COLLECTION MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId
            << ", NAME " << name() 
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                            ChangeCollectionMarker
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////
      
ChangeCollectionMarker::ChangeCollectionMarker (TRI_voc_tick_t databaseId,
                                                TRI_voc_cid_t collectionId,
                                                string const& properties)
  : Marker(TRI_WAL_MARKER_CHANGE_COLLECTION, sizeof(collection_change_marker_t) + alignedSize(properties.size() + 1)) {

  collection_change_marker_t* m = reinterpret_cast<collection_change_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  
  storeSizedString(sizeof(collection_change_marker_t), properties);
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

ChangeCollectionMarker::~ChangeCollectionMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void ChangeCollectionMarker::dump () const {
  collection_change_marker_t* m = reinterpret_cast<collection_change_marker_t*>(begin());

  std::cout << "WAL CHANGE COLLECTION MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId
            << ", PROPERTIES " << properties()
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

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
  : Marker(TRI_WAL_MARKER_BEGIN_TRANSACTION, sizeof(transaction_begin_marker_t)) {

  transaction_begin_marker_t* m = reinterpret_cast<transaction_begin_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId; 
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

BeginTransactionMarker::~BeginTransactionMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void BeginTransactionMarker::dump () const {
  transaction_begin_marker_t* m = reinterpret_cast<transaction_begin_marker_t*>(begin());

  std::cout << "WAL TRANSACTION BEGIN MARKER FOR DB " << m->_databaseId 
            << ", TRANSACTION " << m->_transactionId
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

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
  : Marker(TRI_WAL_MARKER_COMMIT_TRANSACTION, sizeof(transaction_commit_marker_t)) {

  transaction_commit_marker_t* m = reinterpret_cast<transaction_commit_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId; 
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CommitTransactionMarker::~CommitTransactionMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CommitTransactionMarker::dump () const {
  transaction_commit_marker_t* m = reinterpret_cast<transaction_commit_marker_t*>(begin());

  std::cout << "WAL TRANSACTION COMMIT MARKER FOR DB " << m->_databaseId 
            << ", TRANSACTION " << m->_transactionId
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

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
  : Marker(TRI_WAL_MARKER_ABORT_TRANSACTION, sizeof(transaction_abort_marker_t)) {

  transaction_abort_marker_t* m = reinterpret_cast<transaction_abort_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId; 
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

AbortTransactionMarker::~AbortTransactionMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void AbortTransactionMarker::dump () const {
  transaction_commit_marker_t* m = reinterpret_cast<transaction_commit_marker_t*>(begin());

  std::cout << "WAL TRANSACTION ABORT MARKER FOR DB " << m->_databaseId 
            << ", TRANSACTION " << m->_transactionId
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

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
  document_marker_t* m = reinterpret_cast<document_marker_t*>(begin());
  m->_databaseId    = databaseId;
  m->_collectionId  = collectionId;
  m->_revisionId    = revisionId;
  m->_transactionId = transactionId;
  m->_shape         = shapedJson->_sid;

  m->_offsetKey     = sizeof(document_marker_t); // start position of key
  m->_offsetLegend  = m->_offsetKey + alignedSize(key.size() + 1);
  m->_offsetJson    = m->_offsetLegend + alignedSize(legend.getSize());
          
  storeSizedString(m->_offsetKey, key);

  // store legend 
  {
    char* p = static_cast<char*>(begin()) + m->_offsetLegend;
    legend.dump(p);
  }

  // store shapedJson
  {
    char* p = static_cast<char*>(begin()) + m->_offsetJson;
    memcpy(p, shapedJson->_data.data, static_cast<size_t>(shapedJson->_data.length));
  }
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DocumentMarker::~DocumentMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void DocumentMarker::dump () const {
  document_marker_t* m = reinterpret_cast<document_marker_t*>(begin());

  std::cout << "WAL DOCUMENT MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", REV: " << m->_rid 
            << ", TRX: " << m->_transactionId
            << ", KEY: " << key()
            << ", OFFSETKEY: " << m->_offsetKey 
            << ", OFFSETLEGEND: " << m->_offsetLegend 
            << ", OFFSETJSON: " << m->_offsetJson 
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL 
  std::cout << "LEGEND:     '" << stringifyPart(legend(), legendLength()) << "'\n";
  std::cout << "LEGEND HEX: '" << hexifyPart(legend(), legendLength()) << "'\n";
  std::cout << "JSON:       '" << stringifyPart(json(), jsonLength()) << "'\n";
  std::cout << "JSON HEX:   '" << hexifyPart(json(), jsonLength()) << "'\n";
  
  dumpBinary();
#endif  
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a marker from another marker
////////////////////////////////////////////////////////////////////////////////

DocumentMarker* DocumentMarker::clone (TRI_df_marker_t const* other,
                                       TRI_voc_tick_t databaseId,
                                       TRI_voc_cid_t collectionId,
                                       TRI_voc_rid_t revisionId,
                                       TRI_voc_tid_t transactionId,
                                       triagens::basics::JsonLegend& legend,
                                       TRI_shaped_json_t const* shapedJson) {
  char const* base = reinterpret_cast<char const*>(other);

  if (other->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    TRI_doc_document_key_marker_t const* original = reinterpret_cast<TRI_doc_document_key_marker_t const*>(other);
    
    return new DocumentMarker(databaseId,
                              collectionId,
                              revisionId,
                              transactionId,
                              std::string(base + original->_offsetKey),
                              legend,
                              shapedJson);
  }
  else {
    TRI_ASSERT(other->_type == TRI_WAL_MARKER_DOCUMENT);

    document_marker_t const* original = reinterpret_cast<document_marker_t const*>(other);

    TRI_ASSERT(original->_databaseId == databaseId);
    TRI_ASSERT(original->_collectionId == collectionId);

    return new DocumentMarker(original->_databaseId,
                              original->_collectionId,
                              revisionId,
                              transactionId,
                              std::string(base + original->_offsetKey),
                              legend,
                              shapedJson);
  }
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

  edge_marker_t* m = reinterpret_cast<edge_marker_t*>(begin());

  m->_databaseId    = databaseId;
  m->_collectionId  = collectionId;
  m->_revisionId    = revisionId;
  m->_transactionId = transactionId;
  m->_shape         = shapedJson->_sid;
  m->_offsetKey     = sizeof(edge_marker_t); // start position of key
  m->_toCid         = edge->_toCid;
  m->_fromCid       = edge->_fromCid;
  m->_offsetToKey   = m->_offsetKey + alignedSize(key.size() + 1);
  m->_offsetFromKey = m->_offsetToKey + alignedSize(strlen(edge->_toKey) + 1);
  m->_offsetLegend  = m->_offsetFromKey + alignedSize(strlen(edge->_fromKey) + 1);
  m->_offsetJson    = m->_offsetLegend + alignedSize(legend.getSize());
          
  // store keys
  storeSizedString(m->_offsetKey, key.c_str());
  storeSizedString(m->_offsetFromKey, edge->_fromKey, strlen(edge->_fromKey));
  storeSizedString(m->_offsetToKey, edge->_toKey, strlen(edge->_toKey));

  // store legend 
  {
    char* p = static_cast<char*>(begin()) + m->_offsetLegend;
    legend.dump(p);
  }

  // store shapedJson
  {
    char* p = static_cast<char*>(begin()) + m->_offsetJson;
    memcpy(p, shapedJson->_data.data, static_cast<size_t>(shapedJson->_data.length));
  }
  
#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////
        
EdgeMarker::~EdgeMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void EdgeMarker::dump () const {
  edge_marker_t* m = reinterpret_cast<edge_marker_t*>(begin());

  std::cout << "WAL EDGE MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", REV: " << rid()
            << ", TRX: " << tid() 
            << ", KEY: " << key() 
            << ", FROMCID " << m->_fromCid 
            << ", TOCID " << m->_toCid 
            << ", FROMKEY: " << fromKey() 
            << ", TOKEY: " << toKey()
            << ", OFFSETKEY: " << m->_offsetKey 
            << ", OFFSETFROM: " << m->_offsetFromKey 
            << ", OFFSETTO: " << m->_offsetToKey 
            << ", OFFSETLEGEND: " << m->_offsetLegend 
            << ", OFFSETJSON: " << m->_offsetJson 
            << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL 
  std::cout << "LEGEND:     '" << stringifyPart(legend(), legendLength()) << "'\n";
  std::cout << "LEGEND HEX: '" << hexifyPart(legend(), legendLength()) << "'\n";
  std::cout << "JSON:       '" << stringifyPart(json(), jsonLength()) << "'\n";
  std::cout << "JSON HEX:   '" << hexifyPart(json(), jsonLength()) << "'\n";

  dumpBinary();
#endif  
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a marker from another marker
////////////////////////////////////////////////////////////////////////////////

EdgeMarker* EdgeMarker::clone (TRI_df_marker_t const* other,
                               TRI_voc_tick_t databaseId,
                               TRI_voc_cid_t collectionId,
                               TRI_voc_rid_t revisionId,
                               TRI_voc_tid_t transactionId,
                               triagens::basics::JsonLegend& legend,
                               TRI_shaped_json_t const* shapedJson) {
  char const* base = reinterpret_cast<char const*>(other);

  if (other->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* original = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(other);
    
    TRI_document_edge_t edge;
    edge._fromCid = original->_fromCid;
    edge._toCid   = original->_toCid;
    edge._toKey   = (TRI_voc_key_t) base + original->_offsetToKey;
    edge._fromKey = (TRI_voc_key_t) base + original->_offsetFromKey;
    
    return new EdgeMarker(databaseId,
                          collectionId,
                          revisionId,
                          transactionId,
                          std::string(base + original->base._offsetKey),
                          &edge,
                          legend,
                          shapedJson);
  }
  else {
    TRI_ASSERT(other->_type == TRI_WAL_MARKER_EDGE);

    edge_marker_t const* original = reinterpret_cast<edge_marker_t const*>(other);

    TRI_ASSERT(original->_databaseId == databaseId);
    TRI_ASSERT(original->_collectionId == collectionId);

    TRI_document_edge_t edge;
    edge._fromCid = original->_fromCid;
    edge._toCid   = original->_toCid;
    edge._toKey   = (TRI_voc_key_t) base + original->_offsetToKey;
    edge._fromKey = (TRI_voc_key_t) base + original->_offsetFromKey;

    return new EdgeMarker(original->_databaseId,
                          original->_collectionId,
                          revisionId,
                          transactionId,
                          std::string(base + original->_offsetKey),
                          &edge,
                          legend,
                          shapedJson);
  }
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
  : Marker(TRI_WAL_MARKER_REMOVE, sizeof(remove_marker_t) + alignedSize(key.size() + 1)) {
  remove_marker_t* m = reinterpret_cast<remove_marker_t*>(begin());
  m->_databaseId    = databaseId;
  m->_collectionId  = collectionId;
  m->_revisionId    = revisionId;
  m->_transactionId = transactionId;

  storeSizedString(sizeof(remove_marker_t), key);

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////
      
RemoveMarker::~RemoveMarker () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void RemoveMarker::dump () const {
  remove_marker_t* m = reinterpret_cast<remove_marker_t*>(begin());

  std::cout << "WAL REMOVE MARKER FOR DB " << m->_databaseId 
            << ", COLLECTION " << m->_collectionId 
            << ", REV: " << m->_rid 
            << ", TRX: " << m->_transactionId 
            << ", KEY: " << key()
            << "\n";

#ifdef DEBUG_WAL_DETAIL 
  dumpBinary();
#endif
}
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
