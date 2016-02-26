////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Marker.h"
#include "VocBase/document-collection.h"

#undef DEBUG_WAL
#undef DEBUG_WAL_DETAIL

using namespace arangodb::wal;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a marker from a marker existing in memory
////////////////////////////////////////////////////////////////////////////////

Marker::Marker(TRI_df_marker_t const* existing, TRI_voc_fid_t fid)
    : _buffer(reinterpret_cast<char*>(const_cast<TRI_df_marker_t*>(existing))),
      _size(existing->_size),
      _mustFree(false),
      _fid(fid) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker with a sized buffer
////////////////////////////////////////////////////////////////////////////////

Marker::Marker(TRI_df_marker_type_e type, size_t size)
    : _buffer(new char[size]),
      _size(static_cast<uint32_t>(size)),
      _mustFree(true),
      _fid(0) {
  TRI_df_marker_t* m = reinterpret_cast<TRI_df_marker_t*>(begin());
  memset(m, 0, size);

  m->_type = type;
  m->_size = static_cast<TRI_voc_size_t>(size);
  m->_crc = 0;
  m->_tick = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

Marker::~Marker() {
  if (_buffer != nullptr && _mustFree) {
    delete[] _buffer;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a null-terminated string inside the marker
////////////////////////////////////////////////////////////////////////////////

void Marker::storeSizedString(size_t offset, std::string const& value) {
  return storeSizedString(offset, value.c_str(), value.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a null-terminated string inside the marker
////////////////////////////////////////////////////////////////////////////////

void Marker::storeSizedString(size_t offset, char const* value, size_t length) {
  char* p = static_cast<char*>(begin()) + offset;

  // store actual key
  memcpy(p, value, length);
  // append NUL byte
  p[length] = '\0';
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief store a vpack slice
////////////////////////////////////////////////////////////////////////////////
  
void Marker::storeSlice(size_t offset, arangodb::velocypack::Slice const& slice) {
  char* p = static_cast<char*>(begin()) + offset;

  memcpy(p, slice.begin(), static_cast<size_t>(slice.byteSize()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a hex representation of a marker part
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
std::string Marker::hexifyPart(char const* offset, size_t length) const {
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
std::string Marker::stringifyPart(char const* offset, size_t length) const {
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
void Marker::dumpBinary() const {
  std::cout << "BINARY:     '" << stringifyPart(begin(), size()) << "'\n\n";
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

MarkerEnvelope::MarkerEnvelope(TRI_df_marker_t const* existing,
                               TRI_voc_fid_t fid)
    : Marker(existing, fid) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

MarkerEnvelope::~MarkerEnvelope() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

CreateDatabaseMarker::CreateDatabaseMarker(TRI_voc_tick_t databaseId,
                                           VPackSlice const& properties)
    : Marker(TRI_WAL_MARKER_CREATE_DATABASE,
             sizeof(database_create_marker_t) +
                 alignedSize(properties.byteSize())) {
  database_create_marker_t* m =
      reinterpret_cast<database_create_marker_t*>(begin());

  m->_databaseId = databaseId;

  storeSlice(sizeof(database_create_marker_t), properties);

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateDatabaseMarker::~CreateDatabaseMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CreateDatabaseMarker::dump() const {
  database_create_marker_t* m =
      reinterpret_cast<database_create_marker_t*>(begin());

  std::cout << "WAL CREATE DATABASE MARKER FOR DB " << m->_databaseId
            << ", PROPERTIES " << properties() << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

DropDatabaseMarker::DropDatabaseMarker(TRI_voc_tick_t databaseId)
    : Marker(TRI_WAL_MARKER_DROP_DATABASE, sizeof(database_drop_marker_t)) {
  database_drop_marker_t* m =
      reinterpret_cast<database_drop_marker_t*>(begin());

  m->_databaseId = databaseId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropDatabaseMarker::~DropDatabaseMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void DropDatabaseMarker::dump() const {
  database_drop_marker_t* m =
      reinterpret_cast<database_drop_marker_t*>(begin());

  std::cout << "WAL DROP DATABASE MARKER FOR DB " << m->_databaseId
            << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

CreateCollectionMarker::CreateCollectionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t collectionId,
                                               VPackSlice const& properties) 
    : Marker(TRI_WAL_MARKER_CREATE_COLLECTION,
             sizeof(collection_create_marker_t) +
                 alignedSize(properties.byteSize())) {
  collection_create_marker_t* m =
      reinterpret_cast<collection_create_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;

  storeSlice(sizeof(collection_create_marker_t), properties);

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateCollectionMarker::~CreateCollectionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CreateCollectionMarker::dump() const {
  collection_create_marker_t* m =
      reinterpret_cast<collection_create_marker_t*>(begin());

  std::cout << "WAL CREATE COLLECTION MARKER FOR DB " << m->_databaseId
            << ", COLLECTION " << m->_collectionId << ", PROPERTIES "
            << properties() << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

DropCollectionMarker::DropCollectionMarker(TRI_voc_tick_t databaseId,
                                           TRI_voc_cid_t collectionId)
    : Marker(TRI_WAL_MARKER_DROP_COLLECTION, sizeof(collection_drop_marker_t)) {
  collection_drop_marker_t* m =
      reinterpret_cast<collection_drop_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropCollectionMarker::~DropCollectionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void DropCollectionMarker::dump() const {
  collection_drop_marker_t* m =
      reinterpret_cast<collection_drop_marker_t*>(begin());

  std::cout << "WAL DROP COLLECTION MARKER FOR DB " << m->_databaseId
            << ", COLLECTION " << m->_collectionId << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

RenameCollectionMarker::RenameCollectionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t collectionId,
                                               std::string const& name)
    : Marker(
          TRI_WAL_MARKER_RENAME_COLLECTION,
          sizeof(collection_rename_marker_t) + alignedSize(name.size() + 1)) {
  collection_rename_marker_t* m =
      reinterpret_cast<collection_rename_marker_t*>(begin());

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

RenameCollectionMarker::~RenameCollectionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void RenameCollectionMarker::dump() const {
  collection_rename_marker_t* m =
      reinterpret_cast<collection_rename_marker_t*>(begin());

  std::cout << "WAL RENAME COLLECTION MARKER FOR DB " << m->_databaseId
            << ", COLLECTION " << m->_collectionId << ", NAME " << name()
            << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

ChangeCollectionMarker::ChangeCollectionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t collectionId,
                                               VPackSlice const& properties)
    : Marker(TRI_WAL_MARKER_CHANGE_COLLECTION,
             sizeof(collection_change_marker_t) +
                 alignedSize(properties.byteSize())) {
  collection_change_marker_t* m =
      reinterpret_cast<collection_change_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;

  storeSlice(sizeof(collection_change_marker_t), properties);

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

ChangeCollectionMarker::~ChangeCollectionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void ChangeCollectionMarker::dump() const {
  collection_change_marker_t* m =
      reinterpret_cast<collection_change_marker_t*>(begin());

  std::cout << "WAL CHANGE COLLECTION MARKER FOR DB " << m->_databaseId
            << ", COLLECTION " << m->_collectionId << ", PROPERTIES "
            << properties() << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

CreateIndexMarker::CreateIndexMarker(TRI_voc_tick_t databaseId,
                                     TRI_voc_cid_t collectionId,
                                     TRI_idx_iid_t indexId,
                                     VPackSlice const& properties)
    : Marker(
          TRI_WAL_MARKER_CREATE_INDEX,
          sizeof(index_create_marker_t) + alignedSize(properties.byteSize())) {
  index_create_marker_t* m = reinterpret_cast<index_create_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  m->_indexId = indexId;

  storeSlice(sizeof(index_create_marker_t), properties);

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateIndexMarker::~CreateIndexMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CreateIndexMarker::dump() const {
  index_create_marker_t* m = reinterpret_cast<index_create_marker_t*>(begin());

  std::cout << "WAL CREATE INDEX MARKER FOR DB " << m->_databaseId
            << ", COLLECTION " << m->_collectionId << ", INDEX " << m->_indexId
            << ", PROPERTIES " << properties() << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

DropIndexMarker::DropIndexMarker(TRI_voc_tick_t databaseId,
                                 TRI_voc_cid_t collectionId,
                                 TRI_idx_iid_t indexId)
    : Marker(TRI_WAL_MARKER_DROP_INDEX, sizeof(index_drop_marker_t)) {
  index_drop_marker_t* m = reinterpret_cast<index_drop_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  m->_indexId = indexId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropIndexMarker::~DropIndexMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void DropIndexMarker::dump() const {
  index_drop_marker_t* m = reinterpret_cast<index_drop_marker_t*>(begin());

  std::cout << "WAL DROP INDEX MARKER FOR DB " << m->_databaseId
            << ", COLLECTION " << m->_collectionId << ", INDEX " << m->_indexId
            << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

BeginTransactionMarker::BeginTransactionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_tid_t transactionId)
    : Marker(TRI_WAL_MARKER_VPACK_BEGIN_TRANSACTION,
             sizeof(transaction_begin_marker_t)) {
  transaction_begin_marker_t* m =
      reinterpret_cast<transaction_begin_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

BeginTransactionMarker::~BeginTransactionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void BeginTransactionMarker::dump() const {
  transaction_begin_marker_t* m =
      reinterpret_cast<transaction_begin_marker_t*>(begin());

  std::cout << "WAL TRANSACTION BEGIN MARKER FOR DB " << m->_databaseId
            << ", TRANSACTION " << m->_transactionId << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

CommitTransactionMarker::CommitTransactionMarker(TRI_voc_tick_t databaseId,
                                                 TRI_voc_tid_t transactionId)
    : Marker(TRI_WAL_MARKER_VPACK_COMMIT_TRANSACTION,
             sizeof(transaction_commit_marker_t)) {
  transaction_commit_marker_t* m =
      reinterpret_cast<transaction_commit_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CommitTransactionMarker::~CommitTransactionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CommitTransactionMarker::dump() const {
  transaction_commit_marker_t* m =
      reinterpret_cast<transaction_commit_marker_t*>(begin());

  std::cout << "WAL TRANSACTION COMMIT MARKER FOR DB " << m->_databaseId
            << ", TRANSACTION " << m->_transactionId << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

AbortTransactionMarker::AbortTransactionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_tid_t transactionId)
    : Marker(TRI_WAL_MARKER_VPACK_ABORT_TRANSACTION,
             sizeof(transaction_abort_marker_t)) {
  transaction_abort_marker_t* m =
      reinterpret_cast<transaction_abort_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

AbortTransactionMarker::~AbortTransactionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void AbortTransactionMarker::dump() const {
  transaction_abort_marker_t* m =
      reinterpret_cast<transaction_abort_marker_t*>(begin());

  std::cout << "WAL TRANSACTION ABORT MARKER FOR DB " << m->_databaseId
            << ", TRANSACTION " << m->_transactionId << ", SIZE: " << size()
            << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

BeginRemoteTransactionMarker::BeginRemoteTransactionMarker(
    TRI_voc_tick_t databaseId, TRI_voc_tid_t transactionId,
    TRI_voc_tid_t externalId)
    : Marker(TRI_WAL_MARKER_BEGIN_REMOTE_TRANSACTION,
             sizeof(transaction_remote_begin_marker_t)) {
  transaction_remote_begin_marker_t* m =
      reinterpret_cast<transaction_remote_begin_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;
  m->_externalId = externalId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

BeginRemoteTransactionMarker::~BeginRemoteTransactionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void BeginRemoteTransactionMarker::dump() const {
  transaction_remote_begin_marker_t* m =
      reinterpret_cast<transaction_remote_begin_marker_t*>(begin());

  std::cout << "WAL REMOTE TRANSACTION BEGIN MARKER FOR DB " << m->_databaseId
            << ", TRANSACTION " << m->_transactionId << ", EXTERNAL ID "
            << m->_externalId << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

CommitRemoteTransactionMarker::CommitRemoteTransactionMarker(
    TRI_voc_tick_t databaseId, TRI_voc_tid_t transactionId,
    TRI_voc_tid_t externalId)
    : Marker(TRI_WAL_MARKER_COMMIT_REMOTE_TRANSACTION,
             sizeof(transaction_remote_commit_marker_t)) {
  transaction_remote_commit_marker_t* m =
      reinterpret_cast<transaction_remote_commit_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;
  m->_externalId = externalId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CommitRemoteTransactionMarker::~CommitRemoteTransactionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void CommitRemoteTransactionMarker::dump() const {
  transaction_remote_commit_marker_t* m =
      reinterpret_cast<transaction_remote_commit_marker_t*>(begin());

  std::cout << "WAL REMOTE_TRANSACTION COMMIT MARKER FOR DB " << m->_databaseId
            << ", TRANSACTION " << m->_transactionId << ", EXTERNAL ID "
            << m->_externalId << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

AbortRemoteTransactionMarker::AbortRemoteTransactionMarker(
    TRI_voc_tick_t databaseId, TRI_voc_tid_t transactionId,
    TRI_voc_tid_t externalId)
    : Marker(TRI_WAL_MARKER_ABORT_REMOTE_TRANSACTION,
             sizeof(transaction_remote_abort_marker_t)) {
  transaction_remote_abort_marker_t* m =
      reinterpret_cast<transaction_remote_abort_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;
  m->_externalId = externalId;

#ifdef DEBUG_WAL
  dump();
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

AbortRemoteTransactionMarker::~AbortRemoteTransactionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump marker
////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_WAL
void AbortRemoteTransactionMarker::dump() const {
  transaction_remote_abort_marker_t* m =
      reinterpret_cast<transaction_remote_abort_marker_t*>(begin());

  std::cout << "WAL REMOTE TRANSACTION ABORT MARKER FOR DB " << m->_databaseId
            << ", TRANSACTION " << m->_transactionId << ", EXTERNAL ID "
            << m->_externalId << ", SIZE: " << size() << "\n";

#ifdef DEBUG_WAL_DETAIL
  dumpBinary();
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

VPackDocumentMarker::VPackDocumentMarker(TRI_voc_tick_t databaseId,
                                         TRI_voc_cid_t collectionId,
                                         TRI_voc_tid_t transactionId,
                                         VPackSlice const& slice)
    : Marker(TRI_WAL_MARKER_VPACK_DOCUMENT,
             sizeof(vpack_document_marker_t) + slice.byteSize()) {
  auto* m = reinterpret_cast<vpack_document_marker_t*>(begin());
  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  m->_transactionId = transactionId;

  // store vpack
  memcpy(vpack(), slice.begin(), slice.byteSize());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

VPackDocumentMarker::~VPackDocumentMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

VPackRemoveMarker::VPackRemoveMarker(TRI_voc_tick_t databaseId,
                                     TRI_voc_cid_t collectionId,
                                     TRI_voc_tid_t transactionId,
                                     VPackSlice const& slice)
    : Marker(TRI_WAL_MARKER_VPACK_REMOVE,
             sizeof(vpack_remove_marker_t) + slice.byteSize()) {
  auto* m = reinterpret_cast<vpack_remove_marker_t*>(begin());
  m->_databaseId = databaseId;
  m->_collectionId = collectionId;
  m->_transactionId = transactionId;

  // store vpack
  memcpy(vpack(), slice.begin(), slice.byteSize());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

VPackRemoveMarker::~VPackRemoveMarker() {}

