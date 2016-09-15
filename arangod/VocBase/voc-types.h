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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_VOC_TYPES_H
#define ARANGOD_VOC_BASE_VOC_TYPES_H 1

#include "Basics/Common.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

/// @brief forward declarations
struct TRI_doc_mptr_t;

/// @brief tick type (56bit)
typedef uint64_t TRI_voc_tick_t;

/// @brief collection identifier type
typedef uint64_t TRI_voc_cid_t;

/// @brief datafile identifier type
typedef uint64_t TRI_voc_fid_t;

/// @brief revision identifier type
typedef uint64_t TRI_voc_rid_t;

/// @brief transaction identifier type
typedef uint64_t TRI_voc_tid_t;

/// @brief size type
typedef uint32_t TRI_voc_size_t;

/// @brief signed size type
typedef int32_t TRI_voc_ssize_t;

/// @brief index identifier
typedef TRI_voc_tick_t TRI_idx_iid_t;

/// @brief crc type
typedef uint32_t TRI_voc_crc_t;

/// @brief server id type
typedef uint64_t TRI_server_id_t;

/// @brief Convert a revision ID to a string
std::string TRI_RidToString(TRI_voc_rid_t rid);

/// @brief Convert a string into a revision ID, no check variant
TRI_voc_rid_t TRI_StringToRid(std::string const& ridStr, bool& isOld);

/// @brief Convert a string into a revision ID, no check variant
TRI_voc_rid_t TRI_StringToRid(char const* p, size_t len);

/// @brief Convert a string into a revision ID, no check variant
TRI_voc_rid_t TRI_StringToRid(char const* p, size_t len, bool& isOld);

/// @brief Convert a string into a revision ID, returns 0 if format invalid
TRI_voc_rid_t TRI_StringToRidWithCheck(std::string const& ridStr, bool& isOld);

/// @brief Convert a string into a revision ID, returns 0 if format invalid
TRI_voc_rid_t TRI_StringToRidWithCheck(char const* p, size_t len, bool& isOld);

/// @brief enum for write operations
enum TRI_voc_document_operation_e : uint8_t {
  TRI_VOC_DOCUMENT_OPERATION_UNKNOWN = 0,
  TRI_VOC_DOCUMENT_OPERATION_INSERT,
  TRI_VOC_DOCUMENT_OPERATION_UPDATE,
  TRI_VOC_DOCUMENT_OPERATION_REPLACE,
  TRI_VOC_DOCUMENT_OPERATION_REMOVE
};

/// @brief server operation modes
enum TRI_vocbase_operationmode_e {
  TRI_VOCBASE_MODE_NORMAL = 1,     // CRUD is allowed
  TRI_VOCBASE_MODE_NO_CREATE = 2,  // C & U not allowed RD allowed
};

/// @brief edge direction
enum TRI_edge_direction_e {
  TRI_EDGE_ANY = 0,  // can only be used for searching
  TRI_EDGE_IN = 1,
  TRI_EDGE_OUT = 2
};

/// @brief velocypack sub-object (for indexes, as part of TRI_index_element_t, 
/// if the last byte in data[] is 0, then it is an offset into the VelocyPack 
/// data in the datafile or WAL file. If the last byte in data[] is 1, then 
/// value.data contains the actual VelocyPack data in place.
struct TRI_vpack_sub_t {
  union {
    uint8_t data[12];
    uint32_t offset;
  } value;
  
  static constexpr size_t maxValueLength() noexcept {
    return sizeof(value.data) - 1;
  }
  
  void setOffset(uint32_t offset) noexcept {
    value.offset = offset;
    value.data[maxValueLength()] = 0; // type = offset
  }
    
  void setValue(uint8_t const* data, size_t length) noexcept {
    memcpy(&value.data[0], data, length);
    value.data[maxValueLength()] = 1; // type = value
  }

  inline bool isOffset() const noexcept {
    return value.data[maxValueLength()] == 0;
  }

  inline bool isValue() const noexcept {
    return value.data[maxValueLength()] == 1;
  }

  VPackSlice slice(TRI_doc_mptr_t const* mptr) const;
};

static_assert(sizeof(TRI_vpack_sub_t) == 12, "invalid size of TRI_vpack_sub_t");

/// @brief fill a TRI_vpack_sub_t structure with a subvalue
void TRI_FillVPackSub(TRI_vpack_sub_t* sub, 
                      VPackSlice const base, VPackSlice const value) noexcept;

/// @brief Hash and Equal comparison for a vector of VPackSlice
namespace std {

template <>
struct hash<std::vector<VPackSlice>> {
  size_t operator()(std::vector<VPackSlice> const& x) const {
    std::hash<VPackSlice> sliceHash;
    size_t res = 0xdeadbeef;
    for (auto& el : x) {
      res ^= sliceHash(el);
    }
    return res;
  }
};

}

/// @brief databases list structure
struct TRI_vocbase_t;

struct DatabasesLists {
  std::unordered_map<std::string, TRI_vocbase_t*> _databases;
  std::unordered_map<std::string, TRI_vocbase_t*> _coordinatorDatabases;
  std::unordered_set<TRI_vocbase_t*> _droppedDatabases;
};

#endif
