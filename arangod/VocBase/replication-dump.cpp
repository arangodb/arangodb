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

#include "replication-dump.h"

#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/json.h"
#include "Basics/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Cluster/ServerState.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/collection.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocShaper.h"
#include "Wal/Logfile.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"

#include <velocypack/Dumper.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut function
////////////////////////////////////////////////////////////////////////////////

#define FAIL_IFNOT(func, buffer, val)            \
  if (func(buffer, val) != TRI_ERROR_NO_ERROR) { \
    return TRI_ERROR_OUT_OF_MEMORY;              \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a string-buffer function name
////////////////////////////////////////////////////////////////////////////////

#define APPEND_FUNC(name) TRI_##name##StringBuffer

////////////////////////////////////////////////////////////////////////////////
/// @brief append a character to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_CHAR(buffer, c) FAIL_IFNOT(APPEND_FUNC(AppendChar), buffer, c)

////////////////////////////////////////////////////////////////////////////////
/// @brief append a string to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_STRING(buffer, str) \
  FAIL_IFNOT(APPEND_FUNC(AppendString), buffer, str)

////////////////////////////////////////////////////////////////////////////////
/// @brief append uint64 to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_UINT64(buffer, val) \
  FAIL_IFNOT(APPEND_FUNC(AppendUInt64), buffer, val)

////////////////////////////////////////////////////////////////////////////////
/// @brief a datafile descriptor
////////////////////////////////////////////////////////////////////////////////

struct df_entry_t {
  TRI_datafile_t const* _data;
  TRI_voc_tick_t _dataMin;
  TRI_voc_tick_t _dataMax;
  TRI_voc_tick_t _tickMax;
  bool _isJournal;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a number slice into its numeric equivalent
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static inline T NumericValue(VPackSlice const& slice, char const* attribute) {
  VPackSlice v = slice.get(attribute);
  if (v.isString()) {
    return static_cast<T>(std::stoull(v.copyString()));
  }
  if (v.isNumber()) {
    return v.getNumber<T>();
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a (local) collection id into a collection name
////////////////////////////////////////////////////////////////////////////////

static char const* NameFromCid(TRI_replication_dump_t* dump,
                               TRI_voc_cid_t cid) {
  auto it = dump->_collectionNames.find(cid);

  if (it != dump->_collectionNames.end()) {
    // collection name is in cache already
    return (*it).second.c_str();
  }

  // collection name not in cache yet
  std::string name(TRI_GetCollectionNameByIdVocBase(dump->_vocbase, cid));

  if (!name.empty()) {
    // insert into cache
    try {
      dump->_collectionNames.emplace(cid, name);
    } catch (...) {
      return nullptr;
    }

    // and look it up again
    return NameFromCid(dump, cid);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a collection name or id to a string buffer
////////////////////////////////////////////////////////////////////////////////

static int AppendCollection(TRI_replication_dump_t* dump, TRI_voc_cid_t cid,
                            bool translateCollectionIds, bool failOnUnknown,
                            arangodb::CollectionNameResolver* resolver) {
  if (translateCollectionIds) {
    if (cid > 0) {
      std::string name;
      if (arangodb::ServerState::instance()->isDBServer()) {
        name = resolver->getCollectionNameCluster(cid);
      } else {
        name = resolver->getCollectionName(cid);
      }
      APPEND_STRING(dump->_buffer, name.c_str());
      if (failOnUnknown && name[0] == '_' && name == "_unknown") {
        return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
      }

      return TRI_ERROR_NO_ERROR;
    }

    APPEND_STRING(dump->_buffer, "_unknown");

    if (failOnUnknown) {
      return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
    }
  } else {
    APPEND_UINT64(dump->_buffer, (uint64_t)cid);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over a vector of datafiles and pick those with a specific
/// data range
////////////////////////////////////////////////////////////////////////////////

static void IterateDatafiles(std::vector<TRI_datafile_t*> const& datafiles,
                             std::vector<df_entry_t>& result,
                             TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                             bool isJournal) {
  for (auto& df : datafiles) {
    df_entry_t entry = {df, df->_dataMin, df->_dataMax, df->_tickMax,
                        isJournal};

    LOG(TRACE) << "checking datafile " << df->_fid << " with data range " << df->_dataMin << " - " << df->_dataMax << ", tick max: " << df->_tickMax;

    if (df->_dataMin == 0 || df->_dataMax == 0) {
      // datafile doesn't have any data
      continue;
    }

    TRI_ASSERT(df->_tickMin <= df->_tickMax);
    TRI_ASSERT(df->_dataMin <= df->_dataMax);

    if (dataMax < df->_dataMin) {
      // datafile is newer than requested range
      continue;
    }

    if (dataMin > df->_dataMax) {
      // datafile is older than requested range
      continue;
    }

    result.emplace_back(entry);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the datafiles of a collection for a specific tick range
////////////////////////////////////////////////////////////////////////////////

static std::vector<df_entry_t> GetRangeDatafiles(
    TRI_document_collection_t* document, TRI_voc_tick_t dataMin,
    TRI_voc_tick_t dataMax) {
  LOG(TRACE) << "getting datafiles in data range " << dataMin << " - " << dataMax;

  std::vector<df_entry_t> datafiles;

  TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(document);

  try {
    IterateDatafiles(document->_datafiles, datafiles, dataMin, dataMax, false);
    IterateDatafiles(document->_journals, datafiles, dataMin, dataMax, true);
  } catch (...) {
    TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(document);
    throw;
  }

  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(document);

  return datafiles;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append database id plus collection id
////////////////////////////////////////////////////////////////////////////////

static int AppendContext(TRI_replication_dump_t* dump,
                         TRI_voc_tick_t databaseId,
                         TRI_voc_cid_t collectionId) {
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, databaseId);
  APPEND_STRING(dump->_buffer, "\",\"cid\":\"");
  APPEND_UINT64(dump->_buffer, collectionId);

  // also include collection name
  char const* cname = NameFromCid(dump, collectionId);
  if (cname != nullptr) {
    APPEND_STRING(dump->_buffer, "\",\"cname\":\"");
    APPEND_STRING(dump->_buffer, cname);
  }

  APPEND_STRING(dump->_buffer, "\",");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a raw marker from a datafile for a collection dump
////////////////////////////////////////////////////////////////////////////////

static int StringifyMarkerDump(TRI_replication_dump_t* dump,
                               TRI_document_collection_t* document,
                               TRI_df_marker_t const* marker, bool withTicks,
                               bool translateCollectionIds, bool failOnUnknown,
                               arangodb::CollectionNameResolver* resolver) {
  // This covers two cases:
  //   1. document is not nullptr and marker points into a data file
  //   2. document is a nullptr and marker points into a WAL file
  // no other combinations are allowed.

  TRI_string_buffer_t* buffer;
  TRI_replication_operation_e type;
  char const* key = nullptr;
  VPackValueLength keyLength = 0;
  TRI_voc_rid_t rid = 0;
  bool haveData = true;
#if 0  
  bool isWal = false;
#endif

  buffer = dump->_buffer;

  if (buffer == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  switch (marker->_type) {
    case TRI_WAL_MARKER_VPACK_DOCUMENT: {
      TRI_ASSERT(nullptr == document);
      auto m = static_cast<wal::vpack_document_marker_t const*>(marker);
      VPackSlice slice(reinterpret_cast<char const*>(m) + DatafileHelper::VPackOffset(TRI_WAL_MARKER_VPACK_DOCUMENT));
      key = slice.get(TRI_VOC_ATTRIBUTE_KEY).getString(keyLength);
      rid = std::stoull(slice.get(TRI_VOC_ATTRIBUTE_REV).copyString());
      type = REPLICATION_MARKER_DOCUMENT;
#if 0      
      isWal = true;
#endif      
      break;
    }

    case TRI_WAL_MARKER_VPACK_REMOVE: {
      TRI_ASSERT(nullptr == document);
      auto m = static_cast<wal::vpack_remove_marker_t const*>(marker);
      VPackSlice slice(reinterpret_cast<char const*>(m) + DatafileHelper::VPackOffset(TRI_WAL_MARKER_VPACK_REMOVE));
      key = slice.get(TRI_VOC_ATTRIBUTE_KEY).getString(keyLength);
      rid = std::stoull(slice.get(TRI_VOC_ATTRIBUTE_REV).copyString());
      type = REPLICATION_MARKER_REMOVE;
      haveData = false;
#if 0      
      isWal = true;
#endif      
      break;
    }

    default: { return TRI_ERROR_INTERNAL; }
  }

  if (withTicks) {
    APPEND_STRING(buffer, "{\"tick\":\"");
    APPEND_UINT64(buffer, (uint64_t)marker->_tick);
    APPEND_STRING(buffer, "\",\"type\":");
  } else {
    APPEND_STRING(buffer, "{\"type\":");
  }

  APPEND_UINT64(buffer, (uint64_t)type);
  APPEND_STRING(buffer, ",\"key\":\"");
  // key is user-defined, but does not need escaping
  TRI_AppendString2StringBuffer(buffer, key, keyLength);
  APPEND_STRING(buffer, "\",\"rev\":\"");
  APPEND_UINT64(buffer, (uint64_t)rid);

  // document
  if (haveData) {
    APPEND_STRING(buffer, "\",\"data\":{");

    // common document meta-data
    APPEND_STRING(buffer, "\"" TRI_VOC_ATTRIBUTE_KEY "\":\"");
    TRI_AppendString2StringBuffer(buffer, key, keyLength);
    APPEND_STRING(buffer, "\",\"" TRI_VOC_ATTRIBUTE_REV "\":\"");
    APPEND_UINT64(buffer, (uint64_t)rid);
    APPEND_CHAR(buffer, '"');

    // Is it an edge marker?
    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_voc_key_t fromKey;
      TRI_voc_key_t toKey;
      TRI_voc_cid_t fromCid;
      TRI_voc_cid_t toCid;

      // TODO vpack
      if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
        auto e = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);
        fromKey = ((char*)e) + e->_offsetFromKey;
        toKey = ((char*)e) + e->_offsetToKey;
        fromCid = e->_fromCid;
        toCid = e->_toCid;
      }

      int res;
      APPEND_STRING(buffer, ",\"" TRI_VOC_ATTRIBUTE_FROM "\":\"");
      res = AppendCollection(dump, fromCid, translateCollectionIds,
                             failOnUnknown, resolver);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      APPEND_STRING(buffer, "\\/");
      APPEND_STRING(buffer, fromKey);
      APPEND_STRING(buffer, "\",\"" TRI_VOC_ATTRIBUTE_TO "\":\"");
      res = AppendCollection(dump, toCid, translateCollectionIds, failOnUnknown,
                             resolver);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      APPEND_STRING(buffer, "\\/");
      APPEND_STRING(buffer, toKey);
      APPEND_CHAR(buffer, '"');
    }

#if 0
    // the actual document data
    TRI_shaped_json_t shaped;
    if (!isWal) {
      auto m = reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);
      TRI_StringifyArrayShapedJson(
          document->getShaper(), buffer, &shaped,
          true);  // ONLY IN DUMP, PROTECTED by fake trx above
    } else {      // isWal == true
      auto m = static_cast<wal::document_marker_t const*>(marker);
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);
      char const* legend = reinterpret_cast<char const*>(m) + m->_offsetLegend;
      if (m->_offsetJson - m->_offsetLegend == 8) {
        auto p = reinterpret_cast<int64_t const*>(legend);
        legend += *p;
      }
      basics::LegendReader legendReader(legend);
      TRI_StringifyArrayShapedJson(&legendReader, buffer, &shaped, true);
    }
#endif    

    APPEND_STRING(buffer, "}}\n");
  } else {  // deletion marker, so no data
    APPEND_STRING(buffer, "\"}\n");
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a document marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerDocument(TRI_replication_dump_t* dump,
                                      TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<arangodb::wal::vpack_document_marker_t const*>(marker);

#if 0
  // TODO
  int res = AppendContext(dump, m->_databaseId, m->_collectionId);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
#endif  
  
  VPackSlice slice(reinterpret_cast<char const*>(DatafileHelper::VPackOffset(TRI_WAL_MARKER_VPACK_DOCUMENT)));

  APPEND_STRING(dump->_buffer, "\"tid\":\"");
  APPEND_UINT64(dump->_buffer, m->_transactionId);
  APPEND_STRING(dump->_buffer, "\",\"key\":\"");
  std::string key(slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString());
  TRI_AppendString2StringBuffer(dump->_buffer, key.c_str(), key.size());
  
  APPEND_STRING(dump->_buffer, "\",\"rev\":\"");
  std::string rev(slice.get(TRI_VOC_ATTRIBUTE_REV).copyString());
  TRI_AppendString2StringBuffer(dump->_buffer, rev.c_str(), rev.size());
  APPEND_STRING(dump->_buffer, "\",\"data\":");

  arangodb::basics::VPackStringBufferAdapter adapter(dump->_buffer); 
  VPackDumper dumper(&adapter); // TODO: need CustomTypeHandler here!
  dumper.dump(slice);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a remove marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerRemove(TRI_replication_dump_t* dump,
                                    TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<arangodb::wal::vpack_remove_marker_t const*>(marker);

#if 0
  // TODO
  int res = AppendContext(dump, m->_databaseId, m->_collectionId);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
#endif  

  APPEND_STRING(dump->_buffer, "\"tid\":\"");
  APPEND_UINT64(dump->_buffer, m->_transactionId);
  APPEND_STRING(dump->_buffer, "\",\"key\":\"");
  
  VPackSlice slice(reinterpret_cast<char const*>(DatafileHelper::VPackOffset(TRI_WAL_MARKER_VPACK_DOCUMENT)));
  std::string key(slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString());
  TRI_AppendString2StringBuffer(dump->_buffer, key.c_str(), key.size());

  APPEND_STRING(dump->_buffer, "\",\"rev\":\"");
  std::string rev(slice.get(TRI_VOC_ATTRIBUTE_REV).copyString());
  TRI_AppendString2StringBuffer(dump->_buffer, rev.c_str(), rev.size());
  APPEND_STRING(dump->_buffer, "\"");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker should be replicated
////////////////////////////////////////////////////////////////////////////////

static inline bool MustReplicateWalMarkerType(TRI_df_marker_t const* marker) {
  return (marker->_type == TRI_WAL_MARKER_VPACK_DOCUMENT ||
          marker->_type == TRI_WAL_MARKER_VPACK_REMOVE ||
          marker->_type == TRI_WAL_MARKER_VPACK_BEGIN_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_COMMIT_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_ABORT_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_CREATE_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_DROP_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_RENAME_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_CHANGE_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_CREATE_INDEX ||
          marker->_type == TRI_WAL_MARKER_VPACK_DROP_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker belongs to a transaction
////////////////////////////////////////////////////////////////////////////////

static inline bool IsTransactionWalMarkerType(TRI_df_marker_t const* marker) {
  return (marker->_type == TRI_WAL_MARKER_VPACK_BEGIN_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_COMMIT_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_VPACK_ABORT_TRANSACTION);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a marker type to a replication type
////////////////////////////////////////////////////////////////////////////////

static TRI_replication_operation_e TranslateType(
    TRI_df_marker_t const* marker) {
  switch (marker->_type) {
    case TRI_WAL_MARKER_VPACK_DOCUMENT:
      return REPLICATION_MARKER_DOCUMENT;
    case TRI_WAL_MARKER_VPACK_REMOVE:
      return REPLICATION_MARKER_REMOVE;
    case TRI_WAL_MARKER_VPACK_BEGIN_TRANSACTION:
      return REPLICATION_TRANSACTION_START;
    case TRI_WAL_MARKER_VPACK_COMMIT_TRANSACTION:
      return REPLICATION_TRANSACTION_COMMIT;
    case TRI_WAL_MARKER_VPACK_ABORT_TRANSACTION:
      return REPLICATION_TRANSACTION_ABORT;
    case TRI_WAL_MARKER_VPACK_CREATE_COLLECTION:
      return REPLICATION_COLLECTION_CREATE;
    case TRI_WAL_MARKER_VPACK_DROP_COLLECTION:
      return REPLICATION_COLLECTION_DROP;
    case TRI_WAL_MARKER_VPACK_RENAME_COLLECTION:
      return REPLICATION_COLLECTION_RENAME;
    case TRI_WAL_MARKER_VPACK_CHANGE_COLLECTION:
      return REPLICATION_COLLECTION_CHANGE;
    case TRI_WAL_MARKER_VPACK_CREATE_INDEX:
      return REPLICATION_INDEX_CREATE;
    case TRI_WAL_MARKER_VPACK_DROP_INDEX:
      return REPLICATION_INDEX_DROP;

    default:
      return REPLICATION_INVALID;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a raw marker from a WAL logfile for a log dump
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarker(TRI_replication_dump_t* dump,
                              TRI_df_marker_t const* marker) {
  TRI_ASSERT(MustReplicateWalMarkerType(marker));

  APPEND_STRING(dump->_buffer, "{\"tick\":\"");
  APPEND_UINT64(dump->_buffer, (uint64_t)marker->_tick);
  APPEND_STRING(dump->_buffer, "\",\"type\":");
  APPEND_UINT64(dump->_buffer, (uint64_t)TranslateType(marker));
  APPEND_STRING(dump->_buffer, ",\"data\":");
      
  char const* p = reinterpret_cast<char const*>(marker);

  int res = TRI_ERROR_INTERNAL;

  switch (marker->_type) {
    case TRI_WAL_MARKER_VPACK_DOCUMENT: {
      res = StringifyWalMarkerDocument(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_VPACK_REMOVE: {
      res = StringifyWalMarkerRemove(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_VPACK_CREATE_DATABASE: 
    case TRI_WAL_MARKER_VPACK_DROP_DATABASE: 
    case TRI_WAL_MARKER_VPACK_CREATE_COLLECTION: 
    case TRI_WAL_MARKER_VPACK_DROP_COLLECTION: 
    case TRI_WAL_MARKER_VPACK_RENAME_COLLECTION: 
    case TRI_WAL_MARKER_VPACK_CHANGE_COLLECTION: 
    case TRI_WAL_MARKER_VPACK_CREATE_INDEX:
    case TRI_WAL_MARKER_VPACK_DROP_INDEX: 
    case TRI_WAL_MARKER_VPACK_BEGIN_TRANSACTION:
    case TRI_WAL_MARKER_VPACK_COMMIT_TRANSACTION:
    case TRI_WAL_MARKER_VPACK_ABORT_TRANSACTION: {
      arangodb::basics::VPackStringBufferAdapter adapter(dump->_buffer); 
      VPackDumper dumper(&adapter); // TODO: need CustomTypeHandler here!
      dumper.dump(VPackSlice(p + sizeof(TRI_df_marker_t)));
      break;
    }

    case TRI_WAL_MARKER_BEGIN_REMOTE_TRANSACTION:
    case TRI_WAL_MARKER_COMMIT_REMOTE_TRANSACTION:
    case TRI_WAL_MARKER_ABORT_REMOTE_TRANSACTION: {
      TRI_ASSERT(false);
      return TRI_ERROR_INTERNAL;
    }
  }

  APPEND_STRING(dump->_buffer, "}\n");
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the database id from a marker
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_tick_t GetDatabaseFromWalMarker(TRI_df_marker_t const* marker) {
  TRI_ASSERT(MustReplicateWalMarkerType(marker));

  char const* p = reinterpret_cast<char const*>(marker);

  VPackSlice const slice(p + sizeof(TRI_df_marker_t));
  if (!slice.isObject()) {
    return 0;
  }
  VPackSlice const database = slice.get("database");
  if (database.isString()) {
    return std::stoull(database.copyString());
  }
  if (database.isNumber()) {
    return database.getNumber<uint64_t>();
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the collection id from a marker
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_cid_t GetCollectionFromWalMarker(TRI_df_marker_t const* marker) {
  switch (marker->_type) {
    case TRI_WAL_MARKER_VPACK_CREATE_COLLECTION:
    case TRI_WAL_MARKER_VPACK_DROP_COLLECTION:
    case TRI_WAL_MARKER_VPACK_RENAME_COLLECTION:
    case TRI_WAL_MARKER_VPACK_CHANGE_COLLECTION:
    case TRI_WAL_MARKER_VPACK_CREATE_INDEX:
    case TRI_WAL_MARKER_VPACK_DROP_INDEX: {
      char const* p = reinterpret_cast<char const*>(marker);
      VPackSlice const slice(p + sizeof(TRI_df_marker_t));
      if (!slice.isObject()) {
        return 0;
      }
      VPackSlice const id = slice.get("cid");
      if (id.isString()) {
        return std::stoull(id.copyString());
      }
      if (id.isNumber()) {
        return id.getNumber<uint64_t>();
      }
      break;
    }
    default: {
      // TODO: handle other markers here!!
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction id from a marker
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_tid_t GetTransactionFromWalMarker(TRI_df_marker_t const* marker) {
  TRI_ASSERT(MustReplicateWalMarkerType(marker));
  
  switch (marker->_type) {
    case TRI_WAL_MARKER_VPACK_DOCUMENT: 
    case TRI_WAL_MARKER_VPACK_REMOVE: 
    case TRI_WAL_MARKER_VPACK_BEGIN_TRANSACTION:
    case TRI_WAL_MARKER_VPACK_COMMIT_TRANSACTION:
    case TRI_WAL_MARKER_VPACK_ABORT_TRANSACTION: {
      char const* p = reinterpret_cast<char const*>(marker);
      VPackSlice const slice(p + sizeof(TRI_df_marker_t));
      if (!slice.isObject()) {
        return 0;
      }
      VPackSlice const tid = slice.get("tid");
      if (tid.isString()) {
        return std::stoull(tid.copyString());
      }
      if (tid.isNumber()) {
        return tid.getNumber<uint64_t>();
      }
      break;
    }
    default: {
      // TODO: handle other markers here!!
    }
  }
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker belongs to a transaction
////////////////////////////////////////////////////////////////////////////////

static bool IsTransactionWalMarker(TRI_replication_dump_t* dump,
                                   TRI_df_marker_t const* marker) {
  // first check the marker type
  if (!IsTransactionWalMarkerType(marker)) {
    return false;
  }

  // then check if the marker belongs to the "correct" database
  if (dump->_vocbase->_id != GetDatabaseFromWalMarker(marker)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker is replicated
////////////////////////////////////////////////////////////////////////////////

static bool MustReplicateWalMarker(
    TRI_replication_dump_t* dump, TRI_df_marker_t const* marker,
    TRI_voc_tick_t firstRegularTick,
    std::unordered_set<TRI_voc_tid_t> const& transactionIds) {
  // first check the marker type
  if (!MustReplicateWalMarkerType(marker)) {
    return false;
  }

  // then check if the marker belongs to the "correct" database
  if (dump->_vocbase->_id != GetDatabaseFromWalMarker(marker)) {
    return false;
  }
  
  // finally check if the marker is for a collection that we want to ignore
  TRI_voc_cid_t cid = GetCollectionFromWalMarker(marker);

  if (cid != 0) {
    char const* name = NameFromCid(dump, cid);

    if (name != nullptr &&
        TRI_ExcludeCollectionReplication(name, dump->_includeSystem)) {
      return false;
    }
  }
  
  if (dump->_restrictCollection > 0 && 
      (cid != dump->_restrictCollection && ! IsTransactionWalMarker(dump, marker))) {
    // restrict output to a single collection, but a different one
    return false;
  }

  
  if (marker->_tick >= firstRegularTick) {
    return true;
  }

  if (!transactionIds.empty()) {
    TRI_voc_tid_t tid = GetTransactionFromWalMarker(marker);
    if (tid == 0 || transactionIds.find(tid) == transactionIds.end()) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a collection
////////////////////////////////////////////////////////////////////////////////

static int DumpCollection(TRI_replication_dump_t* dump,
                          TRI_document_collection_t* document,
                          TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                          bool withTicks, bool translateCollectionIds,
                          bool failOnUnknown,
                          arangodb::CollectionNameResolver* resolver) {
  LOG(TRACE) << "dumping collection " << document->_info.id() << ", tick range " << dataMin << " - " << dataMax;

  TRI_string_buffer_t* buffer = dump->_buffer;

  std::vector<df_entry_t> datafiles;

  try {
    datafiles = GetRangeDatafiles(document, dataMin, dataMax);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // setup some iteration state
  TRI_voc_tick_t lastFoundTick = 0;
  int res = TRI_ERROR_NO_ERROR;
  bool hasMore = true;
  bool bufferFull = false;

  size_t const n = datafiles.size();

  for (size_t i = 0; i < n; ++i) {
    df_entry_t const& e = datafiles[i];
    TRI_datafile_t const* datafile = e._data;

    // we are reading from a journal that might be modified in parallel
    // so we must read-lock it
    if (e._isJournal) {
      TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
    } else {
      TRI_ASSERT(datafile->_isSealed);
    }

    char const* ptr = datafile->_data;
    char const* end;

    if (res == TRI_ERROR_NO_ERROR) {
      // no error so far. start iterating
      end = ptr + datafile->_currentSize;
    } else {
      // some error occurred. don't iterate
      end = ptr;
    }

    while (ptr < end) {
      auto const* marker = reinterpret_cast<TRI_df_marker_t const*>(ptr);

      if (marker->_size == 0 || marker->_type <= TRI_MARKER_MIN) {
        // end of datafile
        break;
      }

      ptr += DatafileHelper::AlignedMarkerSize<size_t>(marker);

      if (marker->_type == TRI_DF_MARKER_BLANK) {
        // fully ignore these marker types. they don't need to be replicated,
        // but we also cannot stop iteration if we find one of these
        continue;
      }

      // TODO: handle prologue markers here

      // get the marker's tick and check whether we should include it
      TRI_voc_tick_t foundTick = marker->_tick;

      if (foundTick <= dataMin) {
        // marker too old
        continue;
      }

      if (foundTick > dataMax) {
        // marker too new
        hasMore = false;
        goto NEXT_DF;
      }

      if (marker->_type != TRI_WAL_MARKER_VPACK_DOCUMENT &&
          marker->_type != TRI_WAL_MARKER_VPACK_REMOVE) {
        // found a non-data marker...

        // check if we can abort searching
        if (foundTick >= dataMax || (foundTick >= e._tickMax && i == (n - 1))) {
          // fetched the last available marker
          hasMore = false;
          goto NEXT_DF;
        }

        continue;
      }

      // note the last tick we processed
      lastFoundTick = foundTick;

      // handle aborted/unfinished transactions

      res =
          StringifyMarkerDump(dump, document, marker, withTicks,
                              translateCollectionIds, failOnUnknown, resolver);

      if (res != TRI_ERROR_NO_ERROR) {
        break;  // will go to NEXT_DF
      }

      if (foundTick >= dataMax || (foundTick >= e._tickMax && i == (n - 1))) {
        // fetched the last available marker
        hasMore = false;
        goto NEXT_DF;
      }

      if ((uint64_t)TRI_LengthStringBuffer(buffer) > dump->_chunkSize) {
        // abort the iteration
        bufferFull = true;

        goto NEXT_DF;
      }
    }

  NEXT_DF:
    if (e._isJournal) {
      // read-unlock the journal
      TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
    }

    if (res != TRI_ERROR_NO_ERROR || !hasMore || bufferFull) {
      break;
    }
  }

  if (res == TRI_ERROR_NO_ERROR) {
    if (lastFoundTick > 0) {
      // data available for requested range
      dump->_lastFoundTick = lastFoundTick;
      dump->_hasMore = hasMore;
      dump->_bufferFull = bufferFull;
    } else {
      // no data available for requested range
      dump->_lastFoundTick = 0;
      dump->_hasMore = false;
      dump->_bufferFull = false;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpCollectionReplication(TRI_replication_dump_t* dump,
                                  TRI_vocbase_col_t* col,
                                  TRI_voc_tick_t dataMin,
                                  TRI_voc_tick_t dataMax, bool withTicks,
                                  bool translateCollectionIds,
                                  bool failOnUnknown) {
  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(col->_collection != nullptr);

  arangodb::CollectionNameResolver resolver(col->_vocbase);
  TRI_document_collection_t* document = col->_collection;

  // create a barrier so the underlying collection is not unloaded
  auto b = document->ditches()->createReplicationDitch(__FILE__, __LINE__);

  if (b == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // block compaction
  TRI_ReadLockReadWriteLock(&document->_compactionLock);

  int res;

  try {
    res = DumpCollection(dump, document, dataMin, dataMax, withTicks,
                         translateCollectionIds, failOnUnknown, &resolver);
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // always execute this
  TRI_ReadUnlockReadWriteLock(&document->_compactionLock);
  document->ditches()->freeDitch(b);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from the replication log
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpLogReplication(
    TRI_replication_dump_t* dump,
    std::unordered_set<TRI_voc_tid_t> const& transactionIds,
    TRI_voc_tick_t firstRegularTick, TRI_voc_tick_t tickMin,
    TRI_voc_tick_t tickMax, bool outputAsArray) {
  LOG(TRACE) << "dumping log, tick range " << tickMin << " - " << tickMax;

  // ask the logfile manager which datafiles qualify
  bool fromTickIncluded = false;
  std::vector<arangodb::wal::Logfile*> logfiles =
      arangodb::wal::LogfileManager::instance()->getLogfilesForTickRange(
          tickMin, tickMax, fromTickIncluded);

  // setup some iteration state
  int res = TRI_ERROR_NO_ERROR;
  TRI_voc_tick_t lastFoundTick = 0;
  bool hasMore = true;
  bool bufferFull = false;

  if (outputAsArray) {
    TRI_AppendStringStringBuffer(dump->_buffer, "[\n");
  }

  try {
    bool first = true;

    // iterate over the datafiles found
    size_t const n = logfiles.size();

    for (size_t i = 0; i < n; ++i) {
      arangodb::wal::Logfile* logfile = logfiles[i];

      char const* ptr;
      char const* end;
      arangodb::wal::LogfileManager::instance()->getActiveLogfileRegion(
          logfile, ptr, end);

      while (ptr < end) {
        TRI_df_marker_t const* marker =
            reinterpret_cast<TRI_df_marker_t const*>(ptr);

        if (marker->_size == 0 || marker->_type <= TRI_MARKER_MIN) {
          // end of datafile
          break;
        }

        ptr += DatafileHelper::AlignedMarkerSize<size_t>(marker);

        // get the marker's tick and check whether we should include it
        TRI_voc_tick_t foundTick = marker->_tick;

        if (foundTick <= tickMin) {
          // marker too old
          continue;
        }

        if (foundTick >= tickMax) {
          hasMore = false;

          if (foundTick > tickMax) {
            // marker too new
            break;
          }
        }

        if (!MustReplicateWalMarker(dump, marker, firstRegularTick,
                                    transactionIds)) {
          continue;
        }

        // note the last tick we processed
        lastFoundTick = foundTick;

        if (outputAsArray) {
          if (!first) {
            TRI_AppendStringStringBuffer(dump->_buffer, ", ");
          } else {
            first = false;
          }
        }

        res = StringifyWalMarker(dump, marker);

        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }

        if ((uint64_t)TRI_LengthStringBuffer(dump->_buffer) >=
            dump->_chunkSize) {
          // abort the iteration
          bufferFull = true;
          break;
        }
      }

      if (!hasMore || bufferFull) {
        break;
      }
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // always return the logfiles we have used
  arangodb::wal::LogfileManager::instance()->returnLogfiles(logfiles);

  if (outputAsArray) {
    TRI_AppendStringStringBuffer(dump->_buffer, "\n]");
  }

  dump->_fromTickIncluded = fromTickIncluded;

  if (res == TRI_ERROR_NO_ERROR) {
    if (lastFoundTick > 0) {
      // data available for requested range
      dump->_lastFoundTick = lastFoundTick;
      dump->_hasMore = hasMore;
      dump->_bufferFull = bufferFull;
    } else {
      // no data available for requested range
      dump->_lastFoundTick = 0;
      dump->_hasMore = false;
      dump->_bufferFull = false;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the transactions that were open at a given point in time
////////////////////////////////////////////////////////////////////////////////

int TRI_DetermineOpenTransactionsReplication(TRI_replication_dump_t* dump,
                                             TRI_voc_tick_t tickMin,
                                             TRI_voc_tick_t tickMax) {
  LOG(TRACE) << "determining transactions, tick range " << tickMin << " - " << tickMax;

  std::unordered_map<TRI_voc_tid_t, TRI_voc_tick_t> transactions;

  // ask the logfile manager which datafiles qualify
  bool fromTickIncluded = false;
  std::vector<arangodb::wal::Logfile*> logfiles =
      arangodb::wal::LogfileManager::instance()->getLogfilesForTickRange(
          tickMin, tickMax, fromTickIncluded);

  // setup some iteration state
  TRI_voc_tick_t lastFoundTick = 0;
  int res = TRI_ERROR_NO_ERROR;

  // LOG(INFO) << "found logfiles: " << logfiles.size();

  try {
    // iterate over the datafiles found
    size_t const n = logfiles.size();
    for (size_t i = 0; i < n; ++i) {
      arangodb::wal::Logfile* logfile = logfiles[i];

      char const* ptr;
      char const* end;
      arangodb::wal::LogfileManager::instance()->getActiveLogfileRegion(
          logfile, ptr, end);

      // LOG(INFO) << "scanning logfile " << i;
      while (ptr < end) {
        TRI_df_marker_t const* marker =
            reinterpret_cast<TRI_df_marker_t const*>(ptr);

        if (marker->_size == 0 || marker->_type <= TRI_MARKER_MIN) {
          // end of datafile
          break;
        }

        ptr += DatafileHelper::AlignedMarkerSize<size_t>(marker);

        // get the marker's tick and check whether we should include it
        TRI_voc_tick_t foundTick = marker->_tick;

        if (foundTick <= tickMin) {
          // marker too old
          continue;
        }

        if (foundTick > tickMax) {
          // marker too new
          // LOG(INFO) << "marker too new. aborting logfile " << i;
          break;
        }

        // note the last tick we processed
        if (foundTick > lastFoundTick) {
          lastFoundTick = foundTick;
        }

        if (!IsTransactionWalMarker(dump, marker)) {
          continue;
        }

        if (marker->_type == TRI_WAL_MARKER_VPACK_BEGIN_TRANSACTION) {
          VPackSlice slice(reinterpret_cast<char const*>(marker) + sizeof(TRI_df_marker_t));
          transactions.emplace(NumericValue<TRI_voc_tid_t>(slice, "tid"), foundTick);
        } else if (marker->_type == TRI_WAL_MARKER_VPACK_COMMIT_TRANSACTION ||
                   marker->_type == TRI_WAL_MARKER_VPACK_ABORT_TRANSACTION) {
          VPackSlice slice(reinterpret_cast<char const*>(marker) + sizeof(TRI_df_marker_t));
          transactions.erase(NumericValue<TRI_voc_tid_t>(slice, "tid"));
        }
      }
    }

    // LOG(INFO) << "found transactions: " << transactions.size();
    // LOG(INFO) << "last tick: " << lastFoundTick;

    if (transactions.empty()) {
      TRI_AppendStringStringBuffer(dump->_buffer, "[]");
    } else {
      bool first = true;
      TRI_AppendStringStringBuffer(dump->_buffer, "[\"");

      for (auto const& it : transactions) {
        if (it.second - 1 < lastFoundTick) {
          lastFoundTick = it.second - 1;
        }

        if (first) {
          first = false;
        } else {
          TRI_AppendStringStringBuffer(dump->_buffer, "\",\"");
        }

        TRI_AppendUInt64StringBuffer(dump->_buffer, it.first);
      }

      TRI_AppendStringStringBuffer(dump->_buffer, "\"]");
    }

    dump->_fromTickIncluded = fromTickIncluded;
    dump->_lastFoundTick = lastFoundTick;
    // LOG(INFO) << "last tick2: " << lastFoundTick;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // always return the logfiles we have used
  arangodb::wal::LogfileManager::instance()->returnLogfiles(logfiles);

  return res;
}
