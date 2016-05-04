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

#ifndef ARANGOD_VOC_BASE_DATAFILE_HELPER_H
#define ARANGOD_VOC_BASE_DATAFILE_HELPER_H 1

#include "Basics/Common.h"
#include "VocBase/datafile.h"

namespace arangodb {
namespace DatafileHelper {

////////////////////////////////////////////////////////////////////////////////
/// @brief bit mask for datafile ids (fids) that indicates whether a file is
/// a WAL file (bit set) or a datafile (bit not set)
////////////////////////////////////////////////////////////////////////////////
  
constexpr inline uint64_t WalFileBitmask() { return 0x8000000000000000ULL; }

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal size of a marker
////////////////////////////////////////////////////////////////////////////////

constexpr inline TRI_voc_size_t MaximalMarkerSize() {
  static_assert(sizeof(TRI_voc_size_t) >= 4, "TRI_voc_size_t is too small");

  return 2UL * 1024UL * 1024UL * 1024UL; // 2 GB
}

////////////////////////////////////////////////////////////////////////////////
/// @brief journal overhead
////////////////////////////////////////////////////////////////////////////////

constexpr inline TRI_voc_size_t JournalOverhead() {
  return sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the 8-byte aligned size for the value
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static constexpr inline T AlignedSize(T value) {
  return (value + 7) - ((value + 7) & 7);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the 8-byte aligned size for the marker
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static inline T AlignedMarkerSize(TRI_df_marker_t const* marker) {
  size_t value = marker->getSize();
  return static_cast<T>((value + 7) - ((value + 7) & 7));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief portably and safely reads a number
////////////////////////////////////////////////////////////////////////////////
  
template <typename T>
static inline T ReadNumber(uint8_t const* source, uint32_t length) {
  T value = 0;
  uint64_t x = 0;
  uint8_t const* end = source + length;
  do {
    value += static_cast<T>(*source++) << x;
    x += 8;
  } while (source < end);
  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief portably and safely stores a number
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static inline void StoreNumber(uint8_t* dest, T value, uint32_t length) {
  uint8_t* end = dest + length;
  do {
    *dest++ = static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  } while (dest < end);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific offset to the vpack payload
/// note that this function is also used to determine the base length of a
/// marker type
////////////////////////////////////////////////////////////////////////////////

static inline size_t VPackOffset(TRI_df_marker_type_t type) noexcept {
  if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
      type == TRI_DF_MARKER_VPACK_REMOVE) {
    // VPack is located after transaction id
    return sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tid_t); 
  }
  if (type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
      type == TRI_DF_MARKER_VPACK_DROP_INDEX) {
    // VPack is located after database id and collection id
    return sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t);
  }
  if (type == TRI_DF_MARKER_VPACK_CREATE_DATABASE ||
      type == TRI_DF_MARKER_VPACK_DROP_DATABASE) {
    // VPack is located after database id
    return sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tick_t);
  }
  if (type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    // these marker types do not have any VPack
    return sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_tid_t);
  }
  if (type == TRI_DF_MARKER_PROLOGUE) {
    // this type does not have any VPack
    return sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific database id offset
////////////////////////////////////////////////////////////////////////////////

static inline size_t DatabaseIdOffset(TRI_df_marker_type_t type) noexcept {
  if (type == TRI_DF_MARKER_PROLOGUE ||
      type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
      type == TRI_DF_MARKER_VPACK_DROP_INDEX ||
      type == TRI_DF_MARKER_VPACK_CREATE_DATABASE ||
      type == TRI_DF_MARKER_VPACK_DROP_DATABASE ||
      type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return sizeof(TRI_df_marker_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific database id
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t DatabaseId(TRI_df_marker_t const* marker) noexcept {
  TRI_df_marker_type_t type = marker->getType();
  if (type == TRI_DF_MARKER_PROLOGUE ||
      type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
      type == TRI_DF_MARKER_VPACK_DROP_INDEX ||
      type == TRI_DF_MARKER_VPACK_CREATE_DATABASE ||
      type == TRI_DF_MARKER_VPACK_DROP_DATABASE ||
      type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return ReadNumber<TRI_voc_tick_t>(reinterpret_cast<uint8_t const*>(marker) + DatabaseIdOffset(type), sizeof(TRI_voc_tick_t));
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific collection id offset
////////////////////////////////////////////////////////////////////////////////

static inline size_t CollectionIdOffset(TRI_df_marker_type_t type) noexcept {
  if (type == TRI_DF_MARKER_PROLOGUE ||
      type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
      type == TRI_DF_MARKER_VPACK_DROP_INDEX) {
    return sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tick_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific collection id
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t CollectionId(TRI_df_marker_t const* marker) noexcept {
  TRI_df_marker_type_t type = marker->getType();
  if (type == TRI_DF_MARKER_PROLOGUE ||
      type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
      type == TRI_DF_MARKER_VPACK_DROP_INDEX) {
    return ReadNumber<TRI_voc_cid_t>(reinterpret_cast<uint8_t const*>(marker) + CollectionIdOffset(type), sizeof(TRI_voc_cid_t));
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific transaction id offset
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t TransactionIdOffset(TRI_df_marker_type_t type) noexcept {
  if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
      type == TRI_DF_MARKER_VPACK_REMOVE) {
    return sizeof(TRI_df_marker_t);
  }
  if (type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return sizeof(TRI_df_marker_t) + sizeof(TRI_voc_tick_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific transaction id
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t TransactionId(TRI_df_marker_t const* marker) noexcept {
  TRI_df_marker_type_t type = marker->getType();
  if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
      type == TRI_DF_MARKER_VPACK_REMOVE ||
      type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return ReadNumber<TRI_voc_tid_t>(reinterpret_cast<uint8_t const*>(marker) + TransactionIdOffset(type), sizeof(TRI_voc_tid_t));
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a marker, using user-defined tick
////////////////////////////////////////////////////////////////////////////////

static inline void InitMarker(TRI_df_marker_t* marker, 
                   TRI_df_marker_type_t type, uint32_t size, TRI_voc_tick_t tick) {
  TRI_ASSERT(marker != nullptr);
  TRI_ASSERT(type > TRI_DF_MARKER_MIN && type < TRI_DF_MARKER_MAX);
  TRI_ASSERT(size > 0);

  marker->setSize(size);
  marker->setType(type);
  marker->setCrc(0);
  marker->setTick(tick);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a marker, using tick 0
////////////////////////////////////////////////////////////////////////////////

static inline void InitMarker(TRI_df_marker_t* marker, 
                              TRI_df_marker_type_t type, uint32_t size) {
  InitMarker(marker, type, size, 0); // always use tick 0
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a header marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_df_header_marker_t CreateHeaderMarker(TRI_voc_size_t maximalSize, TRI_voc_tick_t fid) {
  static_assert(sizeof(TRI_voc_tick_t) == sizeof(TRI_voc_fid_t), "invalid tick/fid sizes");

  TRI_df_header_marker_t header;
  InitMarker(reinterpret_cast<TRI_df_marker_t*>(&header), TRI_DF_MARKER_HEADER, sizeof(TRI_df_header_marker_t), fid);

  header._version = TRI_DF_VERSION;
  header._maximalSize = maximalSize; 
  header._fid = fid;

  return header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a prologue marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_df_prologue_marker_t CreatePrologueMarker(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
  TRI_df_prologue_marker_t header;
  InitMarker(reinterpret_cast<TRI_df_marker_t*>(&header), TRI_DF_MARKER_PROLOGUE, sizeof(TRI_df_prologue_marker_t));

  StoreNumber<decltype(databaseId)>(reinterpret_cast<uint8_t*>(&header) + DatabaseIdOffset(TRI_DF_MARKER_PROLOGUE), databaseId, sizeof(decltype(databaseId))); 
  StoreNumber<decltype(collectionId)>(reinterpret_cast<uint8_t*>(&header) + CollectionIdOffset(TRI_DF_MARKER_PROLOGUE), collectionId, sizeof(decltype(collectionId))); 

  return header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a footer marker, using a user-defined tick
////////////////////////////////////////////////////////////////////////////////

static inline TRI_df_footer_marker_t CreateFooterMarker(TRI_voc_tick_t tick) {
  TRI_df_footer_marker_t footer;
  InitMarker(reinterpret_cast<TRI_df_marker_t*>(&footer), TRI_DF_MARKER_FOOTER, sizeof(TRI_df_footer_marker_t), tick);

  return footer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a footer marker, using tick 0
////////////////////////////////////////////////////////////////////////////////

static inline TRI_df_footer_marker_t CreateFooterMarker() {
  return CreateFooterMarker(0); // always use tick 0
}

}
} // namespace

#endif
