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
      _size(existing->getSize()),
      _mustFree(false),
      _fid(fid) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker from a VPackSlice
////////////////////////////////////////////////////////////////////////////////
  
Marker::Marker(TRI_df_marker_type_t type, VPackSlice const& properties)
    : Marker(type, sizeof(TRI_df_marker_t) + properties.byteSize()) {
  
  storeSlice(sizeof(TRI_df_marker_t), properties);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker with a sized buffer
////////////////////////////////////////////////////////////////////////////////

Marker::Marker(TRI_df_marker_type_t type, size_t size)
    : _buffer(new char[size]),
      _size(static_cast<uint32_t>(size)),
      _mustFree(true),
      _fid(0) {
  DatafileHelper::InitMarker(reinterpret_cast<TRI_df_marker_t*>(begin()), type, _size);
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
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

MarkerEnvelope::MarkerEnvelope(TRI_df_marker_t const* existing,
                               TRI_voc_fid_t fid)
    : Marker(existing, fid) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

VPackDocumentMarker::VPackDocumentMarker(TRI_voc_tid_t transactionId,
                                         VPackSlice const& properties)
    : Marker(TRI_DF_MARKER_VPACK_DOCUMENT,
             sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tid_t) + properties.byteSize()) {
  *reinterpret_cast<TRI_voc_tid_t*>(begin() + sizeof(TRI_df_marker_t)) = transactionId;

  // store vpack
  storeSlice(sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tid_t), properties);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

VPackRemoveMarker::VPackRemoveMarker(TRI_voc_tid_t transactionId,
                                     VPackSlice const& properties)
    : Marker(TRI_DF_MARKER_VPACK_REMOVE,
             sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tid_t) + properties.byteSize()) {
  
  *reinterpret_cast<TRI_voc_tid_t*>(begin() + sizeof(TRI_df_marker_t)) = transactionId;

  // store vpack
  storeSlice(sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tid_t), properties);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

BeginRemoteTransactionMarker::BeginRemoteTransactionMarker(
    TRI_voc_tick_t databaseId, TRI_voc_tid_t transactionId,
    TRI_voc_tid_t externalId)
    : Marker(TRI_DF_MARKER_BEGIN_REMOTE_TRANSACTION,
             sizeof(transaction_remote_begin_marker_t)) {
  transaction_remote_begin_marker_t* m =
      reinterpret_cast<transaction_remote_begin_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;
  m->_externalId = externalId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

CommitRemoteTransactionMarker::CommitRemoteTransactionMarker(
    TRI_voc_tick_t databaseId, TRI_voc_tid_t transactionId,
    TRI_voc_tid_t externalId)
    : Marker(TRI_DF_MARKER_COMMIT_REMOTE_TRANSACTION,
             sizeof(transaction_remote_commit_marker_t)) {
  transaction_remote_commit_marker_t* m =
      reinterpret_cast<transaction_remote_commit_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;
  m->_externalId = externalId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create marker
////////////////////////////////////////////////////////////////////////////////

AbortRemoteTransactionMarker::AbortRemoteTransactionMarker(
    TRI_voc_tick_t databaseId, TRI_voc_tid_t transactionId,
    TRI_voc_tid_t externalId)
    : Marker(TRI_DF_MARKER_ABORT_REMOTE_TRANSACTION,
             sizeof(transaction_remote_abort_marker_t)) {
  transaction_remote_abort_marker_t* m =
      reinterpret_cast<transaction_remote_abort_marker_t*>(begin());

  m->_databaseId = databaseId;
  m->_transactionId = transactionId;
  m->_externalId = externalId;
}

