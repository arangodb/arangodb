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
/// @brief store a vpack slice
////////////////////////////////////////////////////////////////////////////////
  
void Marker::storeSlice(size_t offset, arangodb::velocypack::Slice const& slice) {
  char* p = static_cast<char*>(begin()) + offset;

  memcpy(p, slice.begin(), static_cast<size_t>(slice.byteSize()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a hex representation of a marker part
////////////////////////////////////////////////////////////////////////////////

std::string Marker::hexifyPart(char const* offset, size_t length) const {
  return arangodb::basics::StringUtils::escapeHex(std::string(offset, length), ' ');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a printable string representation of a marker part
////////////////////////////////////////////////////////////////////////////////

std::string Marker::stringifyPart(char const* offset, size_t length) const {
  return arangodb::basics::StringUtils::escapeC(std::string(offset, length));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the marker in binary form
////////////////////////////////////////////////////////////////////////////////

std::string Marker::stringify() const {
  auto const* m = reinterpret_cast<TRI_df_marker_t const*>(begin());
  return std::string("[") + std::string(TRI_NameMarkerDatafile(m)) + 
         ", size " + std::to_string(size()) + 
         ", data " + stringifyPart(begin(), size()) + "]";
}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateDatabaseMarker::~CreateDatabaseMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

DropDatabaseMarker::DropDatabaseMarker(TRI_voc_tick_t databaseId)
    : Marker(TRI_WAL_MARKER_DROP_DATABASE, sizeof(database_drop_marker_t)) {
  database_drop_marker_t* m =
      reinterpret_cast<database_drop_marker_t*>(begin());

  m->_databaseId = databaseId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropDatabaseMarker::~DropDatabaseMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateCollectionMarker::~CreateCollectionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropCollectionMarker::~DropCollectionMarker() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

RenameCollectionMarker::RenameCollectionMarker(TRI_voc_tick_t databaseId,
                                               TRI_voc_cid_t collectionId,
                                               VPackSlice const& properties)
    : Marker(
          TRI_WAL_MARKER_RENAME_COLLECTION,
          sizeof(collection_rename_marker_t) + alignedSize(properties.byteSize())) {
  collection_rename_marker_t* m =
      reinterpret_cast<collection_rename_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_collectionId = collectionId;

  storeSlice(sizeof(collection_rename_marker_t), properties);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

RenameCollectionMarker::~RenameCollectionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

ChangeCollectionMarker::~ChangeCollectionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CreateIndexMarker::~CreateIndexMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

DropIndexMarker::~DropIndexMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

BeginTransactionMarker::~BeginTransactionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CommitTransactionMarker::~CommitTransactionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

AbortTransactionMarker::~AbortTransactionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

BeginRemoteTransactionMarker::~BeginRemoteTransactionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

CommitRemoteTransactionMarker::~CommitRemoteTransactionMarker() {}

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy marker
////////////////////////////////////////////////////////////////////////////////

AbortRemoteTransactionMarker::~AbortRemoteTransactionMarker() {}

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

