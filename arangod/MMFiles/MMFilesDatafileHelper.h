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

#ifndef ARANGOD_MMFILES_MMFILES_DATAFILE_HELPER_H
#define ARANGOD_MMFILES_MMFILES_DATAFILE_HELPER_H 1

#include "Basics/Common.h"
#include "Basics/encoding.h"
#include "MMFiles/MMFilesDatafile.h"
#include "VocBase/Identifiers/FileId.h"

namespace arangodb {
namespace MMFilesDatafileHelper {

////////////////////////////////////////////////////////////////////////////////
/// @brief bit mask for datafile ids (fids) that indicates whether a file is
/// a WAL file (bit set) or a datafile (bit not set)
////////////////////////////////////////////////////////////////////////////////

constexpr inline uint64_t WalFileBitmask() { return 0x8000000000000000ULL; }

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal size of a marker
////////////////////////////////////////////////////////////////////////////////

constexpr inline uint32_t MaximalMarkerSize() {
  return 2UL * 1024UL * 1024UL * 1024UL;  // 2 GB
}

////////////////////////////////////////////////////////////////////////////////
/// @brief journal overhead
////////////////////////////////////////////////////////////////////////////////

constexpr inline uint32_t JournalOverhead() {
  return sizeof(MMFilesDatafileHeaderMarker) + sizeof(MMFilesDatafileFooterMarker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the 8-byte aligned size for the marker
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static inline T AlignedMarkerSize(MMFilesMarker const* marker) {
  size_t value = marker->getSize();
  return static_cast<T>((value + 7) - ((value + 7) & 7));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific offset to the vpack payload
/// note that this function is also used to determine the base length of a
/// marker type
////////////////////////////////////////////////////////////////////////////////

static inline size_t VPackOffset(MMFilesMarkerType type) noexcept {
  if (type == TRI_DF_MARKER_VPACK_DOCUMENT || type == TRI_DF_MARKER_VPACK_REMOVE) {
    // VPack is located after transaction id
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tid_t);
  }
  if (type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION || type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION || type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
      type == TRI_DF_MARKER_VPACK_DROP_INDEX || type == TRI_DF_MARKER_VPACK_CREATE_VIEW ||
      type == TRI_DF_MARKER_VPACK_DROP_VIEW || type == TRI_DF_MARKER_VPACK_CHANGE_VIEW) {
    // VPack is located after database id and collection id
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t);
  }
  if (type == TRI_DF_MARKER_VPACK_CREATE_DATABASE || type == TRI_DF_MARKER_VPACK_DROP_DATABASE) {
    // VPack is located after database id
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t);
  }
  if (type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    // these marker types do not have any VPack
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_tid_t);
  }
  if (type == TRI_DF_MARKER_PROLOGUE) {
    // this type does not have any VPack
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t) + sizeof(TRI_voc_cid_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific database id offset
////////////////////////////////////////////////////////////////////////////////

static inline size_t DatabaseIdOffset(MMFilesMarkerType type) noexcept {
  if (type == TRI_DF_MARKER_PROLOGUE || type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION || type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX || type == TRI_DF_MARKER_VPACK_DROP_INDEX ||
      type == TRI_DF_MARKER_VPACK_CREATE_VIEW || type == TRI_DF_MARKER_VPACK_DROP_VIEW ||
      type == TRI_DF_MARKER_VPACK_CHANGE_VIEW || type == TRI_DF_MARKER_VPACK_CREATE_DATABASE ||
      type == TRI_DF_MARKER_VPACK_DROP_DATABASE || type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return sizeof(MMFilesMarker);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific database id
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t DatabaseId(MMFilesMarker const* marker) noexcept {
  MMFilesMarkerType type = marker->getType();
  if (type == TRI_DF_MARKER_PROLOGUE || type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION || type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX || type == TRI_DF_MARKER_VPACK_DROP_INDEX ||
      type == TRI_DF_MARKER_VPACK_CREATE_VIEW || type == TRI_DF_MARKER_VPACK_DROP_VIEW ||
      type == TRI_DF_MARKER_VPACK_CHANGE_VIEW || type == TRI_DF_MARKER_VPACK_CREATE_DATABASE ||
      type == TRI_DF_MARKER_VPACK_DROP_DATABASE || type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return encoding::readNumber<TRI_voc_tick_t>(reinterpret_cast<uint8_t const*>(marker) +
                                                    DatabaseIdOffset(type),
                                                sizeof(TRI_voc_tick_t));
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific collection id offset
////////////////////////////////////////////////////////////////////////////////

static inline size_t CollectionIdOffset(MMFilesMarkerType type) noexcept {
  if (type == TRI_DF_MARKER_PROLOGUE || type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION || type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX || type == TRI_DF_MARKER_VPACK_DROP_INDEX) {
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific collection id
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t CollectionId(MMFilesMarker const* marker) noexcept {
  MMFilesMarkerType type = marker->getType();
  if (type == TRI_DF_MARKER_PROLOGUE || type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_DROP_COLLECTION || type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
      type == TRI_DF_MARKER_VPACK_CREATE_INDEX || type == TRI_DF_MARKER_VPACK_DROP_INDEX) {
    return encoding::readNumber<TRI_voc_cid_t>(reinterpret_cast<uint8_t const*>(marker) +
                                                   CollectionIdOffset(type),
                                               sizeof(TRI_voc_cid_t));
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific view id offset
////////////////////////////////////////////////////////////////////////////////

static inline size_t ViewIdOffset(MMFilesMarkerType type) noexcept {
  if (type == TRI_DF_MARKER_VPACK_CREATE_VIEW || type == TRI_DF_MARKER_VPACK_DROP_VIEW ||
      type == TRI_DF_MARKER_VPACK_CHANGE_VIEW) {
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific view id
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t ViewId(MMFilesMarker const* marker) noexcept {
  MMFilesMarkerType type = marker->getType();
  if (type == TRI_DF_MARKER_VPACK_CREATE_VIEW || type == TRI_DF_MARKER_VPACK_DROP_VIEW ||
      type == TRI_DF_MARKER_VPACK_CHANGE_VIEW) {
    return encoding::readNumber<TRI_voc_cid_t>(reinterpret_cast<uint8_t const*>(marker) +
                                                   ViewIdOffset(type),
                                               sizeof(TRI_voc_cid_t));
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific transaction id offset
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t TransactionIdOffset(MMFilesMarkerType type) noexcept {
  if (type == TRI_DF_MARKER_VPACK_DOCUMENT || type == TRI_DF_MARKER_VPACK_REMOVE) {
    return sizeof(MMFilesMarker);
  }
  if (type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return sizeof(MMFilesMarker) + sizeof(TRI_voc_tick_t);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the marker-specific transaction id
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_tick_t TransactionId(MMFilesMarker const* marker) noexcept {
  MMFilesMarkerType type = marker->getType();
  if (type == TRI_DF_MARKER_VPACK_DOCUMENT || type == TRI_DF_MARKER_VPACK_REMOVE ||
      type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
      type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
    return encoding::readNumber<TRI_voc_tid_t>(reinterpret_cast<uint8_t const*>(marker) +
                                                   TransactionIdOffset(type),
                                               sizeof(TRI_voc_tid_t));
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a marker, using user-defined tick
////////////////////////////////////////////////////////////////////////////////

static inline void InitMarker(MMFilesMarker* marker, MMFilesMarkerType type,
                              uint32_t size, TRI_voc_tick_t tick) {
  TRI_ASSERT(marker != nullptr);
  TRI_ASSERT(type > TRI_DF_MARKER_MIN && type < TRI_DF_MARKER_MAX);
  TRI_ASSERT(size > 0);

  marker->setSize(size);
  marker->setTypeAndTick(type, tick);
  marker->setCrc(0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a marker, using tick 0
////////////////////////////////////////////////////////////////////////////////

static inline void InitMarker(MMFilesMarker* marker, MMFilesMarkerType type, uint32_t size) {
  InitMarker(marker, type, size, 0);  // always use tick 0
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a header marker
////////////////////////////////////////////////////////////////////////////////

static inline MMFilesDatafileHeaderMarker CreateHeaderMarker(uint32_t maximalSize, FileId fid) {
  // cppcheck-suppress duplicateExpression
  static_assert(sizeof(TRI_voc_tick_t) == sizeof(FileId),
                "invalid tick/fid sizes");

  MMFilesDatafileHeaderMarker header;
  InitMarker(reinterpret_cast<MMFilesMarker*>(&header), TRI_DF_MARKER_HEADER,
             sizeof(MMFilesDatafileHeaderMarker),
             static_cast<TRI_voc_tick_t>(fid.id()));

  header._version = TRI_DF_VERSION;
  header._maximalSize = maximalSize;
  header._fid = static_cast<TRI_voc_tick_t>(fid.id());

  return header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a prologue marker
////////////////////////////////////////////////////////////////////////////////

static inline MMFilesPrologueMarker CreatePrologueMarker(TRI_voc_tick_t databaseId,
                                                         TRI_voc_cid_t collectionId) {
  MMFilesPrologueMarker header;
  InitMarker(reinterpret_cast<MMFilesMarker*>(&header), TRI_DF_MARKER_PROLOGUE,
             sizeof(MMFilesPrologueMarker));

  encoding::storeNumber<decltype(databaseId)>(reinterpret_cast<uint8_t*>(&header) +
                                                  DatabaseIdOffset(TRI_DF_MARKER_PROLOGUE),
                                              databaseId, sizeof(decltype(databaseId)));
  encoding::storeNumber<decltype(collectionId)>(reinterpret_cast<uint8_t*>(&header) +
                                                    CollectionIdOffset(TRI_DF_MARKER_PROLOGUE),
                                                collectionId,
                                                sizeof(decltype(collectionId)));

  return header;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a footer marker, using a user-defined tick
////////////////////////////////////////////////////////////////////////////////

static inline MMFilesDatafileFooterMarker CreateFooterMarker(TRI_voc_tick_t tick) {
  MMFilesDatafileFooterMarker footer;
  InitMarker(reinterpret_cast<MMFilesMarker*>(&footer), TRI_DF_MARKER_FOOTER,
             sizeof(MMFilesDatafileFooterMarker), tick);

  return footer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a footer marker, using tick 0
////////////////////////////////////////////////////////////////////////////////

static inline MMFilesDatafileFooterMarker CreateFooterMarker() {
  return CreateFooterMarker(0);  // always use tick 0
}

}  // namespace MMFilesDatafileHelper
}  // namespace arangodb

#endif
