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

#ifndef ARANGOD_WAL_MARKER_H
#define ARANGOD_WAL_MARKER_H 1

#include "Basics/Common.h"
#include "VocBase/datafile.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace wal {

////////////////////////////////////////////////////////////////////////////////
/// @brief wal remote transaction begin marker
////////////////////////////////////////////////////////////////////////////////

struct transaction_remote_begin_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_tid_t _transactionId;
  TRI_voc_tid_t _externalId;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal remote transaction commit marker
////////////////////////////////////////////////////////////////////////////////

struct transaction_remote_commit_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_tid_t _transactionId;
  TRI_voc_tid_t _externalId;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal remote transaction abort marker
////////////////////////////////////////////////////////////////////////////////

struct transaction_remote_abort_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_tid_t _transactionId;
  TRI_voc_tid_t _externalId;
};

class Marker {
  Marker& operator=(Marker const&) = delete;
  Marker(Marker&&) = delete;
  Marker(Marker const&) = delete;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a marker from a marker existing in memory
  //////////////////////////////////////////////////////////////////////////////

  Marker(TRI_df_marker_t const*, TRI_voc_fid_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a marker that manages its own memory
  //////////////////////////////////////////////////////////////////////////////

  Marker(TRI_df_marker_type_t, size_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create marker from a VPackSlice
  //////////////////////////////////////////////////////////////////////////////
  
  Marker(TRI_df_marker_type_t, arangodb::velocypack::Slice const&);

 public:
  virtual ~Marker();

  inline void freeBuffer() {
    if (_buffer != nullptr && _mustFree) {
      delete[] _buffer;

      _buffer = nullptr;
      _mustFree = false;
    }
  }

  inline TRI_voc_fid_t fid() const { return _fid; }

  inline void* mem() const { return static_cast<void*>(_buffer); }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the size of the marker
  //////////////////////////////////////////////////////////////////////////////

  inline uint32_t size() const { return _size; }

 protected:

  inline char* begin() const { return _buffer; }

  inline char* end() const { return _buffer + _size; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief store a vpack slice
  //////////////////////////////////////////////////////////////////////////////
  
  void storeSlice(size_t, arangodb::velocypack::Slice const&);

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pointer to marker data
  //////////////////////////////////////////////////////////////////////////////

  char* _buffer;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief size of marker data
  //////////////////////////////////////////////////////////////////////////////

  uint32_t const _size;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the destructor must free the memory
  //////////////////////////////////////////////////////////////////////////////

  bool _mustFree;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief id of the logfile the marker is stored in
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_fid_t _fid;
};

class MarkerEnvelope : public Marker {
 public:
  MarkerEnvelope(TRI_df_marker_t const*, TRI_voc_fid_t);
  ~MarkerEnvelope() = default;
};

class CrudMarker : public Marker {
 public:
  CrudMarker(TRI_df_marker_type_t, TRI_voc_tid_t, arangodb::velocypack::Slice const&);
  ~CrudMarker() = default;
};

class DatabaseMarker : public Marker {
 public:
  DatabaseMarker(TRI_df_marker_type_t, TRI_voc_tick_t, arangodb::velocypack::Slice const&);
  ~DatabaseMarker() = default;
};

class CollectionMarker : public Marker {
 public:
  CollectionMarker(TRI_df_marker_type_t, TRI_voc_tick_t, TRI_voc_cid_t, arangodb::velocypack::Slice const&);
  ~CollectionMarker() = default;
};

class TransactionMarker : public Marker {
 public:
  TransactionMarker(TRI_df_marker_type_t, TRI_voc_tick_t, TRI_voc_tid_t);
  ~TransactionMarker() = default;
};

class BeginRemoteTransactionMarker : public Marker {
 public:
  BeginRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);
  ~BeginRemoteTransactionMarker() = default;
};

class CommitRemoteTransactionMarker : public Marker {
 public:
  CommitRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);
  ~CommitRemoteTransactionMarker() = default;
};

class AbortRemoteTransactionMarker : public Marker {
 public:
  AbortRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);
  ~AbortRemoteTransactionMarker() = default;
};

}
}

#endif
