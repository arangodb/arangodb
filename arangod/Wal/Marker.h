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

static_assert(sizeof(TRI_df_marker_t) == 24, "invalid base marker size");

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

////////////////////////////////////////////////////////////////////////////////
/// @brief wal document marker
////////////////////////////////////////////////////////////////////////////////

struct document_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;

  TRI_voc_rid_t _revisionId;  // this is the tick for a create and update
  TRI_voc_tid_t _transactionId;

  TRI_shape_sid_t _shape;

  uint16_t _offsetKey;
  uint16_t _offsetLegend;
  uint32_t _offsetJson;

  // char* key
  // char* shapedJson
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal vpack document marker
////////////////////////////////////////////////////////////////////////////////

struct vpack_document_marker_t : TRI_df_marker_t {
  TRI_voc_tid_t _transactionId;
  // uint8_t* vpack
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal vpack remove marker
////////////////////////////////////////////////////////////////////////////////

struct vpack_remove_marker_t : TRI_df_marker_t {
  TRI_voc_tid_t _transactionId;
  // uint8_t* vpack
};

class Marker {
 protected:
  Marker& operator=(Marker const&) = delete;
  Marker(Marker&&) = delete;
  Marker(Marker const&) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a marker from a marker existing in memory
  //////////////////////////////////////////////////////////////////////////////

  Marker(TRI_df_marker_t const*, TRI_voc_fid_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create marker from a VPackSlice
  //////////////////////////////////////////////////////////////////////////////
  
  Marker(TRI_df_marker_type_e, arangodb::velocypack::Slice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a marker that manages its own memory
  //////////////////////////////////////////////////////////////////////////////

  Marker(TRI_df_marker_type_e, size_t);

 public:
  virtual ~Marker();

  inline void freeBuffer() {
    if (_buffer != nullptr && _mustFree) {
      delete[] _buffer;

      _buffer = nullptr;
      _mustFree = false;
    }
  }

  inline char* steal() {
    char* buffer = _buffer;
    _buffer = nullptr;
    _mustFree = false;
    return buffer;
  }

  inline TRI_voc_fid_t fid() const { return _fid; }

  inline void* mem() const { return static_cast<void*>(_buffer); }

  inline char* begin() const { return _buffer; }

  inline char* end() const { return _buffer + _size; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the size of the marker
  //////////////////////////////////////////////////////////////////////////////

  inline uint32_t size() const { return _size; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a printable representation of the marker
  //////////////////////////////////////////////////////////////////////////////

  std::string stringify() const;

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a hex representation of a marker part
  //////////////////////////////////////////////////////////////////////////////

  std::string hexifyPart(char const*, size_t) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a printable string representation of a marker part
  //////////////////////////////////////////////////////////////////////////////

  std::string stringifyPart(char const*, size_t) const;

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

class CreateDatabaseMarker : public Marker {
 public:
  explicit CreateDatabaseMarker(arangodb::velocypack::Slice const&);
  ~CreateDatabaseMarker() = default;
};

class DropDatabaseMarker : public Marker {
 public:
  explicit DropDatabaseMarker(arangodb::velocypack::Slice const&);
  ~DropDatabaseMarker() = default;
};

class CreateCollectionMarker : public Marker {
 public:
  explicit CreateCollectionMarker(arangodb::velocypack::Slice const&);
  ~CreateCollectionMarker() = default;
};

class DropCollectionMarker : public Marker {
 public:
  explicit DropCollectionMarker(arangodb::velocypack::Slice const&);
  ~DropCollectionMarker() = default;
};

class RenameCollectionMarker : public Marker {
 public:
  explicit RenameCollectionMarker(arangodb::velocypack::Slice const&);
  ~RenameCollectionMarker() = default;
};

class ChangeCollectionMarker : public Marker {
 public:
  explicit ChangeCollectionMarker(arangodb::velocypack::Slice const&);
  ~ChangeCollectionMarker() = default;
};

class CreateIndexMarker : public Marker {
 public:
  explicit CreateIndexMarker(arangodb::velocypack::Slice const&);
  ~CreateIndexMarker() = default;
};

class DropIndexMarker : public Marker {
 public:
  explicit DropIndexMarker(arangodb::velocypack::Slice const&);
  ~DropIndexMarker() = default;
};

class BeginTransactionMarker : public Marker {
 public:
  explicit BeginTransactionMarker(arangodb::velocypack::Slice const&);
  ~BeginTransactionMarker() = default;
};

class CommitTransactionMarker : public Marker {
 public:
  explicit CommitTransactionMarker(arangodb::velocypack::Slice const&);
  ~CommitTransactionMarker() = default;
};

class AbortTransactionMarker : public Marker {
 public:
  explicit AbortTransactionMarker(arangodb::velocypack::Slice const&);
  ~AbortTransactionMarker() = default;
};

class BeginRemoteTransactionMarker : public Marker {
 public:
  BeginRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);

  ~BeginRemoteTransactionMarker();
};

class CommitRemoteTransactionMarker : public Marker {
 public:
  CommitRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);

  ~CommitRemoteTransactionMarker();
};

class AbortRemoteTransactionMarker : public Marker {
 public:
  AbortRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);

  ~AbortRemoteTransactionMarker();
};

class VPackDocumentMarker : public Marker {
 public:
  VPackDocumentMarker(TRI_voc_tid_t, arangodb::velocypack::Slice const&);
  ~VPackDocumentMarker() = default;
};

class VPackRemoveMarker : public Marker {
 public:
  VPackRemoveMarker(TRI_voc_tid_t, arangodb::velocypack::Slice const&);
  ~VPackRemoveMarker() = default;
};

}
}

#endif
