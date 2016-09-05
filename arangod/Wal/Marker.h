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
#include "VocBase/DatafileHelper.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace wal {

/// @brief abstract base class for all markers
class Marker {
 private:
  Marker& operator=(Marker const&) = delete;
  Marker(Marker&&) = delete;
  Marker(Marker const&) = delete;
 protected:
  Marker() = default;

 public:
  virtual ~Marker() {}
 
  /// @brief returns the marker type 
  virtual TRI_df_marker_type_t type() const = 0;

  /// @brief returns the datafile id the marker comes from
  /// this should be 0 for new markers, but contain the actual
  /// datafile id for an existing marker during recovery
  virtual TRI_voc_fid_t fid() const = 0;

  /// @brief return the total size of the marker, including the header
  virtual uint32_t size() const = 0;

  /// @brief store the marker in the memory region starting at mem
  /// the region is guaranteed to be big enough to hold size() bytes
  virtual void store(char* mem) const = 0;

  /// @brief a pointer to the beginning of the VPack payload
  virtual void* vpack() const {
    // not implemented for base marker type
    TRI_ASSERT(false);
    return nullptr;
  }
};

/// @brief an envelope that contains a pointer to an existing marker
/// this type is used during recovery only, to represent existing markers
class MarkerEnvelope : public Marker {
 public:
  MarkerEnvelope(TRI_df_marker_t const* other, TRI_voc_fid_t fid) 
      : _other(other),
        _fid(fid),
        _size(other->getSize()) {
    // we must always have a datafile id, and a reasonable marker size
    TRI_ASSERT(_fid > 0);
    TRI_ASSERT(_size >= sizeof(TRI_df_marker_t));
  }

  ~MarkerEnvelope() = default;

  /// @brief returns the marker type 
  TRI_df_marker_type_t type() const override final { 
    // simply return the wrapped marker's type
    return _other->getType(); 
  }
  
  /// @brief returns the datafile id the marker comes from
  TRI_voc_fid_t fid() const override final { return _fid; }
 
  /// @brief a pointer the beginning of the VPack payload
  void* vpack() const override final { 
    return const_cast<void*>(reinterpret_cast<void const*>(reinterpret_cast<uint8_t const*>(_other) + DatafileHelper::VPackOffset(type()))); 
  }

  /// @brief a pointer to the beginning of the wrapped marker
  void const* mem() const {
    return static_cast<void const*>(_other);
  }

  /// @brief return the size of the wrapped marker
  uint32_t size() const override final { return _size; }

  /// @brief store the marker in the memory region starting at mem
  void store(char* mem) const override final {
    // intentionally nothing... should never be called for MarkerEnvelopes,
    // as they represent existing markers from the WAL that do not need to
    // be written again!
    TRI_ASSERT(false); 
  }

 private:
  TRI_df_marker_t const* _other;
  TRI_voc_fid_t _fid;
  uint32_t _size;
};

/// @brief a marker type that is used when inserting new documents,
/// updating/replacing or removing documents
class CrudMarker : public Marker {
 public:
  CrudMarker(TRI_df_marker_type_t type, 
             TRI_voc_tid_t transactionId, 
             arangodb::velocypack::Slice const& data)
    : _transactionId(transactionId),
      _data(data),
      _type(type) {}

  ~CrudMarker() = default;

  /// @brief returns the marker type 
  TRI_df_marker_type_t type() const override final { return _type; }
  
  /// @brief returns the datafile id
  /// this is always 0 for this type of marker, as the marker is not
  /// yet in any datafile
  TRI_voc_fid_t fid() const override final { return 0; }
 
  /// @brief returns the marker size 
  uint32_t size() const override final { 
    return static_cast<uint32_t>(DatafileHelper::VPackOffset(_type) + _data.byteSize());
  }

  /// @brief store the marker in the memory region starting at mem
  void store(char* mem) const override final { 
    // store transaction id
    DatafileHelper::StoreNumber<decltype(_transactionId)>(reinterpret_cast<uint8_t*>(mem) + DatafileHelper::TransactionIdOffset(_type), _transactionId, sizeof(decltype(_transactionId)));
    // store VPack
    memcpy(mem + DatafileHelper::VPackOffset(_type), _data.begin(), static_cast<size_t>(_data.byteSize()));
  }
  
  /// @brief a pointer the beginning of the VPack payload
  void* vpack() const override final { return const_cast<void*>(_data.startAs<void>()); }

 private:
  TRI_voc_tid_t _transactionId;
  arangodb::velocypack::Slice _data;
  TRI_df_marker_type_t _type;
};

/// @brief a marker used for database-related operations
class DatabaseMarker : public Marker {
 public:
  DatabaseMarker(TRI_df_marker_type_t type, 
                 TRI_voc_tick_t databaseId, 
                 arangodb::velocypack::Slice const& data)
    : _databaseId(databaseId),
      _data(data),
      _type(type) {
    TRI_ASSERT(databaseId > 0);
  }

  ~DatabaseMarker() = default;

  /// @brief returns the marker type 
  TRI_df_marker_type_t type() const override final { return _type; }

  /// @brief returns the datafile id
  /// this is always 0 for this type of marker, as the marker is not
  /// yet in any datafile
  TRI_voc_fid_t fid() const override final { return 0; }
  
  /// @brief returns the marker size 
  uint32_t size() const override final { 
    return static_cast<uint32_t>(DatafileHelper::VPackOffset(_type) + _data.byteSize());
  }

  /// @brief store the marker in the memory region starting at mem
  void store(char* mem) const override final { 
    // store database id
    DatafileHelper::StoreNumber<decltype(_databaseId)>(reinterpret_cast<uint8_t*>(mem) + DatafileHelper::DatabaseIdOffset(_type), _databaseId, sizeof(decltype(_databaseId)));
    // store VPack
    memcpy(mem + DatafileHelper::VPackOffset(_type), _data.begin(), static_cast<size_t>(_data.byteSize()));
  }

 private:
  TRI_voc_tick_t _databaseId;
  arangodb::velocypack::Slice _data;
  TRI_df_marker_type_t _type;
};

/// @brief a marker used for collection-related operations
class CollectionMarker : public Marker {
 public:
  CollectionMarker(TRI_df_marker_type_t type, 
                   TRI_voc_tick_t databaseId, 
                   TRI_voc_cid_t collectionId, 
                   arangodb::velocypack::Slice const& data)
    : _databaseId(databaseId),
      _collectionId(collectionId),
      _data(data.begin()),
      _type(type) {

    TRI_ASSERT(databaseId > 0);
    TRI_ASSERT(collectionId > 0);
  }

  ~CollectionMarker() = default;

  /// @brief returns the marker type 
  TRI_df_marker_type_t type() const override final { return _type; }
  
  /// @brief returns the datafile id
  /// this is always 0 for this type of marker, as the marker is not
  /// yet in any datafile
  TRI_voc_fid_t fid() const override final { return 0; }
  
  /// @brief returns the marker size 
  uint32_t size() const override final { 
    return static_cast<uint32_t>(DatafileHelper::VPackOffset(_type) + _data.byteSize());
  }

  /// @brief store the marker in the memory region starting at mem
  void store(char* mem) const override final { 
    // store database id
    DatafileHelper::StoreNumber<decltype(_databaseId)>(reinterpret_cast<uint8_t*>(mem) + DatafileHelper::DatabaseIdOffset(_type), _databaseId, sizeof(decltype(_databaseId)));
    // store collection id
    DatafileHelper::StoreNumber<decltype(_collectionId)>(reinterpret_cast<uint8_t*>(mem) + DatafileHelper::CollectionIdOffset(_type), _collectionId, sizeof(decltype(_collectionId)));
    // store VPack
    memcpy(mem + DatafileHelper::VPackOffset(_type), _data.begin(), static_cast<size_t>(_data.byteSize()));
  }

 private:
  TRI_voc_tick_t _databaseId;
  TRI_voc_cid_t _collectionId;
  arangodb::velocypack::Slice _data;
  TRI_df_marker_type_t _type;
};

/// @brief a marker used for transaction-related operations
class TransactionMarker : public Marker {
 public:
  TransactionMarker(TRI_df_marker_type_t type, 
                    TRI_voc_tick_t databaseId, 
                    TRI_voc_tid_t transactionId)
    : _databaseId(databaseId),
      _transactionId(transactionId),
      _type(type) {
    TRI_ASSERT(databaseId > 0);
    TRI_ASSERT(transactionId > 0);
  }

  ~TransactionMarker() = default;

  /// @brief returns the marker type 
  TRI_df_marker_type_t type() const override final { return _type; }
  
  /// @brief returns the datafile id
  /// this is always 0 for this type of marker, as the marker is not
  /// yet in any datafile
  TRI_voc_fid_t fid() const override final { return 0; }
  
  /// @brief returns the marker size 
  uint32_t size() const override final {
    // these markers do not have any VPack payload 
    return static_cast<uint32_t>(DatafileHelper::VPackOffset(_type));
  }

  /// @brief store the marker in the memory region starting at mem
  void store(char* mem) const override final { 
    // store database id
    DatafileHelper::StoreNumber<decltype(_databaseId)>(reinterpret_cast<uint8_t*>(mem) + DatafileHelper::DatabaseIdOffset(_type), _databaseId, sizeof(decltype(_databaseId)));
    // store transaction id
    DatafileHelper::StoreNumber<decltype(_transactionId)>(reinterpret_cast<uint8_t*>(mem) + DatafileHelper::TransactionIdOffset(_type), _transactionId, sizeof(decltype(_transactionId)));
  }

 private:
  TRI_voc_tick_t _databaseId;
  TRI_voc_tid_t _transactionId;
  TRI_df_marker_type_t _type;
};

}
}

#endif
