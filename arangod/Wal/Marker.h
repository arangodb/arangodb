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
#include "Basics/tri-strings.h"
#include "VocBase/datafile.h"
#include "VocBase/Legends.h"
#include "VocBase/shaped-json.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace wal {

static_assert(sizeof(TRI_df_marker_t) == 24, "invalid base marker size");

////////////////////////////////////////////////////////////////////////////////
/// @brief wal create database marker
////////////////////////////////////////////////////////////////////////////////

struct database_create_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  // char* properties
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal drop database marker
////////////////////////////////////////////////////////////////////////////////

struct database_drop_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal create collection marker
////////////////////////////////////////////////////////////////////////////////

struct collection_create_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
  // char* properties
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal drop collection marker
////////////////////////////////////////////////////////////////////////////////

struct collection_drop_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal rename collection marker
////////////////////////////////////////////////////////////////////////////////

struct collection_rename_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
  // char* name
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal change collection marker
////////////////////////////////////////////////////////////////////////////////

struct collection_change_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
  // char* properties
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal create index marker
////////////////////////////////////////////////////////////////////////////////

struct index_create_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
  TRI_idx_iid_t _indexId;
  // char* properties
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal drop index marker
////////////////////////////////////////////////////////////////////////////////

struct index_drop_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
  TRI_idx_iid_t _indexId;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction begin marker
////////////////////////////////////////////////////////////////////////////////

struct transaction_begin_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_tid_t _transactionId;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction commit marker
////////////////////////////////////////////////////////////////////////////////

struct transaction_commit_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_tid_t _transactionId;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal transaction abort marker
////////////////////////////////////////////////////////////////////////////////

struct transaction_abort_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_tid_t _transactionId;
};

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
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
  TRI_voc_tid_t _transactionId;
  // uint8_t* vpack
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wal vpack remove marker
////////////////////////////////////////////////////////////////////////////////

struct vpack_remove_marker_t : TRI_df_marker_t {
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
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

  static inline size_t alignedSize(size_t size) {
    return TRI_DF_ALIGN_BLOCK(size);
  }

  inline void* mem() const { return static_cast<void*>(_buffer); }

  inline char* begin() const { return _buffer; }

  inline char* end() const { return _buffer + _size; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the size of the marker
  //////////////////////////////////////////////////////////////////////////////

  inline uint32_t size() const { return _size; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a hex representation of a marker part
  //////////////////////////////////////////////////////////////////////////////

  std::string hexifyPart(char const*, size_t) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a printable string representation of a marker part
  //////////////////////////////////////////////////////////////////////////////

  std::string stringifyPart(char const*, size_t) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief print the marker in binary form
  //////////////////////////////////////////////////////////////////////////////

  void dumpBinary() const;

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief store a null-terminated string inside the marker
  //////////////////////////////////////////////////////////////////////////////

  void storeSizedString(size_t, std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief store a null-terminated string inside the marker
  //////////////////////////////////////////////////////////////////////////////

  void storeSizedString(size_t, char const*, size_t);
  
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

  ~MarkerEnvelope();
};

class CreateDatabaseMarker : public Marker {
 public:
  CreateDatabaseMarker(TRI_voc_tick_t, arangodb::velocypack::Slice const&);

  ~CreateDatabaseMarker();

 public:
  inline char* properties() const {
    return begin() + sizeof(database_create_marker_t);
  }

  void dump() const;
};

class DropDatabaseMarker : public Marker {
 public:
  explicit DropDatabaseMarker(TRI_voc_tick_t);

  ~DropDatabaseMarker();

 public:
  void dump() const;
};

class CreateCollectionMarker : public Marker {
 public:
  CreateCollectionMarker(TRI_voc_tick_t, TRI_voc_cid_t, arangodb::velocypack::Slice const&);

  ~CreateCollectionMarker();

 public:
  inline char* properties() const {
    return begin() + sizeof(collection_create_marker_t);
  }

  void dump() const;
};

class DropCollectionMarker : public Marker {
 public:
  DropCollectionMarker(TRI_voc_tick_t, TRI_voc_cid_t);

  ~DropCollectionMarker();

 public:
  void dump() const;
};

class RenameCollectionMarker : public Marker {
 public:
  RenameCollectionMarker(TRI_voc_tick_t, TRI_voc_cid_t, std::string const&);

  ~RenameCollectionMarker();

 public:
  inline char* name() const {
    return begin() + sizeof(collection_rename_marker_t);
  }

  void dump() const;
};

class ChangeCollectionMarker : public Marker {
 public:
  ChangeCollectionMarker(TRI_voc_tick_t, TRI_voc_cid_t, arangodb::velocypack::Slice const&);

  ~ChangeCollectionMarker();

 public:
  inline char* properties() const {
    return begin() + sizeof(collection_change_marker_t);
  }

  void dump() const;
};

class CreateIndexMarker : public Marker {
 public:
  CreateIndexMarker(TRI_voc_tick_t, TRI_voc_cid_t, TRI_idx_iid_t,
                    arangodb::velocypack::Slice const&);

  ~CreateIndexMarker();

 public:
  inline char* properties() const {
    return begin() + sizeof(index_create_marker_t);
  }

  void dump() const;
};

class DropIndexMarker : public Marker {
 public:
  DropIndexMarker(TRI_voc_tick_t, TRI_voc_cid_t, TRI_idx_iid_t);

  ~DropIndexMarker();

 public:
  void dump() const;
};

class BeginTransactionMarker : public Marker {
 public:
  BeginTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t);

  ~BeginTransactionMarker();

 public:
  void dump() const;
};

class CommitTransactionMarker : public Marker {
 public:
  CommitTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t);

  ~CommitTransactionMarker();

 public:
  void dump() const;
};

class AbortTransactionMarker : public Marker {
 public:
  AbortTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t);

  ~AbortTransactionMarker();

 public:
  void dump() const;
};

class BeginRemoteTransactionMarker : public Marker {
 public:
  BeginRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);

  ~BeginRemoteTransactionMarker();

 public:
  void dump() const;
};

class CommitRemoteTransactionMarker : public Marker {
 public:
  CommitRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);

  ~CommitRemoteTransactionMarker();

 public:
  void dump() const;
};

class AbortRemoteTransactionMarker : public Marker {
 public:
  AbortRemoteTransactionMarker(TRI_voc_tick_t, TRI_voc_tid_t, TRI_voc_tid_t);

  ~AbortRemoteTransactionMarker();

 public:
  void dump() const;
};

class VPackDocumentMarker : public Marker {
 public:
  VPackDocumentMarker(TRI_voc_tick_t, TRI_voc_cid_t, TRI_voc_tid_t,
                      arangodb::velocypack::Slice const&);

  ~VPackDocumentMarker();

 public:
  inline TRI_voc_tid_t transactionId() const {
    auto const* m = reinterpret_cast<vpack_document_marker_t const*>(begin());
    return m->_transactionId;
  }

  inline uint8_t* vpack() const {
    // pointer to vpack
    return reinterpret_cast<uint8_t*>(begin()) +
           sizeof(vpack_document_marker_t);
  }

  inline size_t vpackLength() const {
    VPackSlice slice(vpack());
    return slice.byteSize();
  }
};

class VPackRemoveMarker : public Marker {
 public:
  VPackRemoveMarker(TRI_voc_tick_t, TRI_voc_cid_t, TRI_voc_tid_t,
                    arangodb::velocypack::Slice const&);

  ~VPackRemoveMarker();

 public:
  inline TRI_voc_tid_t transactionId() const {
    auto const* m = reinterpret_cast<vpack_remove_marker_t const*>(begin());
    return m->_transactionId;
  }

  inline uint8_t* vpack() const {
    // pointer to vpack
    return reinterpret_cast<uint8_t*>(begin()) + sizeof(vpack_remove_marker_t);
  }

  inline size_t vpackLength() const {
    VPackSlice slice(vpack());
    return slice.byteSize();
  }
};

}
}

#endif
