////////////////////////////////////////////////////////////////////////////////
/// @brief replication dump functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "replication-dump.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"

#include "Utils/CollectionNameResolver.h"
#include "VocBase/collection.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"
#include "Wal/Logfile.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"
#include "Cluster/ServerState.h"

using namespace triagens;

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut function
////////////////////////////////////////////////////////////////////////////////

#define FAIL_IFNOT(func, buffer, val)                                     \
  if (func(buffer, val) != TRI_ERROR_NO_ERROR) {                          \
    return TRI_ERROR_OUT_OF_MEMORY;                                       \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a string-buffer function name
////////////////////////////////////////////////////////////////////////////////

#define APPEND_FUNC(name) TRI_ ## name ## StringBuffer

////////////////////////////////////////////////////////////////////////////////
/// @brief append a character to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_CHAR(buffer, c)      FAIL_IFNOT(APPEND_FUNC(AppendChar), buffer, c)

////////////////////////////////////////////////////////////////////////////////
/// @brief append a string to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_STRING(buffer, str)  FAIL_IFNOT(APPEND_FUNC(AppendString), buffer, str)

////////////////////////////////////////////////////////////////////////////////
/// @brief append uint64 to a string-buffer or fail
////////////////////////////////////////////////////////////////////////////////

#define APPEND_UINT64(buffer, val)  FAIL_IFNOT(APPEND_FUNC(AppendUInt64), buffer, val)

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a datafile descriptor
////////////////////////////////////////////////////////////////////////////////

typedef struct df_entry_s {
  TRI_datafile_t* _data;
  TRI_voc_tick_t  _dataMin;
  TRI_voc_tick_t  _dataMax;
  TRI_voc_tick_t  _tickMax;
  bool            _isJournal;
}
df_entry_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a (local) collection id into a collection name
////////////////////////////////////////////////////////////////////////////////
    
char const* NameFromCid (TRI_replication_dump_t* dump,
                         TRI_voc_cid_t cid) {
  auto it = dump->_collectionNames.find(cid);

  if (it != dump->_collectionNames.end()) {
    // collection name is in cache already
    return (*it).second.c_str();
  }
    
  // collection name not in cache yet
  char* name = TRI_GetCollectionNameByIdVocBase(dump->_vocbase, cid);

  if (name != nullptr) {
    // insert into cache
    try {
      dump->_collectionNames.emplace(std::make_pair(cid, std::string(name)));
    }
    catch (...) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, name);
      return nullptr;
    }

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, name);

    // and look it up again
    return NameFromCid(dump, cid);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a collection name or id to a string buffer
////////////////////////////////////////////////////////////////////////////////

static int AppendCollection (TRI_replication_dump_t* dump,
                             TRI_voc_cid_t cid,
                             bool translateCollectionIds,
                             triagens::arango::CollectionNameResolver* resolver) {
  if (translateCollectionIds) {
    if (cid > 0) {
      std::string name;
      if (triagens::arango::ServerState::instance()->isDBserver()) {
        name = resolver->getCollectionNameCluster(cid);
      }
      else {
        name = resolver->getCollectionName(cid);
      }
      APPEND_STRING(dump->_buffer, name.c_str());
      return TRI_ERROR_NO_ERROR;
    }

    APPEND_STRING(dump->_buffer, "_unknown");
  }
  else {
    APPEND_UINT64(dump->_buffer, (uint64_t) cid);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over a vector of datafiles and pick those with a specific
/// data range
////////////////////////////////////////////////////////////////////////////////

static int IterateDatafiles (TRI_vector_pointer_t const* datafiles,
                             TRI_vector_t* result,
                             TRI_voc_tick_t dataMin,
                             TRI_voc_tick_t dataMax,
                             bool isJournal) {

  int res = TRI_ERROR_NO_ERROR;

  size_t const n = datafiles->_length;

  for (size_t i = 0; i < n; ++i) {
    TRI_datafile_t* df = static_cast<TRI_datafile_t*>(TRI_AtVectorPointer(datafiles, i));

    df_entry_t entry = {
      df,
      df->_dataMin,
      df->_dataMax,
      df->_tickMax,
      isJournal
    };

    LOG_TRACE("checking datafile %llu with data range %llu - %llu, tick max: %llu",
              (unsigned long long) df->_fid,
              (unsigned long long) df->_dataMin,
              (unsigned long long) df->_dataMax,
              (unsigned long long) df->_tickMax);

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

    res = TRI_PushBackVector(result, &entry);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the datafiles of a collection for a specific tick range
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_t GetRangeDatafiles (TRI_document_collection_t* document,
                                       TRI_voc_tick_t dataMin,
                                       TRI_voc_tick_t dataMax) {
  TRI_vector_t datafiles;

  LOG_TRACE("getting datafiles in data range %llu - %llu",
            (unsigned long long) dataMin,
            (unsigned long long) dataMax);

  // determine the datafiles of the collection
  TRI_InitVector(&datafiles, TRI_CORE_MEM_ZONE, sizeof(df_entry_t));

  TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(document);

  IterateDatafiles(&document->_datafiles, &datafiles, dataMin, dataMax, false);
  IterateDatafiles(&document->_journals, &datafiles, dataMin, dataMax, true);

  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(document);

  return datafiles;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append database id plus collection id 
////////////////////////////////////////////////////////////////////////////////
  
static int AppendContext (TRI_replication_dump_t* dump,
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

static int StringifyMarkerDump (TRI_replication_dump_t* dump,
                                TRI_document_collection_t* document,
                                TRI_df_marker_t const* marker,
                                bool withTicks,
                                bool translateCollectionIds,
                                triagens::arango::CollectionNameResolver* resolver) {
  // This covers two cases:
  //   1. document is not nullptr and marker points into a data file
  //   2. document is a nullptr and marker points into a WAL file
  // no other combinations are allowed.

  TRI_string_buffer_t* buffer;
  TRI_replication_operation_e type;
  TRI_voc_key_t key;
  TRI_voc_rid_t rid;
  bool haveData = true;
  bool isWal = false;

  buffer = dump->_buffer;

  if (buffer == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  switch (marker->_type) {
    case TRI_DOC_MARKER_KEY_DELETION: {
      TRI_ASSERT(nullptr != document);
      auto m = reinterpret_cast<TRI_doc_deletion_key_marker_t const*>(marker);
      key = ((char*) m) + m->_offsetKey;
      type = REPLICATION_MARKER_REMOVE;
      rid = m->_rid;
      haveData = false;
      break;
    }

    case TRI_DOC_MARKER_KEY_DOCUMENT: {
      TRI_ASSERT(nullptr != document);
      auto m = reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
      key = ((char*) m) + m->_offsetKey;
      type = REPLICATION_MARKER_DOCUMENT;
      rid = m->_rid;
      break;
    }

    case TRI_DOC_MARKER_KEY_EDGE: {
      TRI_ASSERT(nullptr != document);
      auto m = reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
      key = ((char*) m) + m->_offsetKey;
      type = REPLICATION_MARKER_EDGE;
      rid = m->_rid;
      break;
    }

    case TRI_WAL_MARKER_REMOVE: {
      TRI_ASSERT(nullptr == document);
      auto m = static_cast<wal::remove_marker_t const*>(marker);
      key = ((char*) m) + sizeof(wal::remove_marker_t);
      type = REPLICATION_MARKER_REMOVE;
      rid = m->_revisionId;
      haveData = false;
      isWal = true;
      break;
    }

    case TRI_WAL_MARKER_DOCUMENT: {
      TRI_ASSERT(nullptr == document);
      auto m = static_cast<wal::document_marker_t const*>(marker);
      key = ((char*) m) + m->_offsetKey;
      type = REPLICATION_MARKER_DOCUMENT;
      rid = m->_revisionId;
      isWal = true;
      break;
    }

    case TRI_WAL_MARKER_EDGE: {
      TRI_ASSERT(nullptr == document);
      auto m = static_cast<wal::edge_marker_t const*>(marker);
      key = ((char*) m) + m->_offsetKey;
      type = REPLICATION_MARKER_EDGE;
      rid = m->_revisionId;
      isWal = true;
      break;
    }

    default: {
      return TRI_ERROR_INTERNAL;
    }
  }

  if (withTicks) {
    APPEND_STRING(buffer, "{\"tick\":\"");
    APPEND_UINT64(buffer, (uint64_t) marker->_tick);
    APPEND_STRING(buffer, "\",\"type\":");
  }
  else {
    APPEND_STRING(buffer, "{\"type\":");
  }

  APPEND_UINT64(buffer, (uint64_t) type);
  APPEND_STRING(buffer, ",\"key\":\"");
  // key is user-defined, but does not need escaping
  APPEND_STRING(buffer, key);
  APPEND_STRING(buffer, "\",\"rev\":\"");
  APPEND_UINT64(buffer, (uint64_t) rid);

  // document
  if (haveData) {
    APPEND_STRING(buffer, "\",\"data\":{");

    // common document meta-data
    APPEND_STRING(buffer, "\"" TRI_VOC_ATTRIBUTE_KEY "\":\"");
    APPEND_STRING(buffer, key);
    APPEND_STRING(buffer, "\",\"" TRI_VOC_ATTRIBUTE_REV "\":\"");
    APPEND_UINT64(buffer, (uint64_t) rid);
    APPEND_CHAR(buffer, '"');

    // Is it an edge marker?
    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
        marker->_type == TRI_WAL_MARKER_EDGE) {
      TRI_voc_key_t fromKey;
      TRI_voc_key_t toKey;
      TRI_voc_cid_t fromCid;
      TRI_voc_cid_t toCid;

      if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
        auto e = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);
        fromKey = ((char*) e) + e->_offsetFromKey;
        toKey = ((char*) e) + e->_offsetToKey;
        fromCid = e->_fromCid;
        toCid = e->_toCid;
      }
      else {  // TRI_WAL_MARKER_EDGE
        auto e = reinterpret_cast<wal::edge_marker_t const*>(marker);
        fromKey = ((char*) e) + e->_offsetFromKey;
        toKey = ((char*) e) + e->_offsetToKey;
        fromCid = e->_fromCid;
        toCid = e->_toCid;
      }

      int res;
      APPEND_STRING(buffer, ",\"" TRI_VOC_ATTRIBUTE_FROM "\":\"");
      res = AppendCollection(dump, fromCid, translateCollectionIds, resolver);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      APPEND_STRING(buffer, "\\/");
      APPEND_STRING(buffer, fromKey);
      APPEND_STRING(buffer, "\",\"" TRI_VOC_ATTRIBUTE_TO "\":\"");
      res = AppendCollection(dump, toCid, translateCollectionIds, resolver);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }

      APPEND_STRING(buffer, "\\/");
      APPEND_STRING(buffer, toKey);
      APPEND_CHAR(buffer, '"');
    }

    // the actual document data
    TRI_shaped_json_t shaped;
    if (! isWal) {
      auto m = reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, m);
      TRI_StringifyArrayShapedJson(document->getShaper(), buffer, &shaped, true); // ONLY IN DUMP, PROTECTED by fake trx above
    }
    else {   // isWal == true
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

    APPEND_STRING(buffer, "}}\n");
  }
  else {  // deletion marker, so no data
    APPEND_STRING(buffer, "\"}\n");
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the document attributes of a marker
////////////////////////////////////////////////////////////////////////////////

static int AppendDocument (triagens::wal::document_marker_t const* marker,
                           TRI_replication_dump_t* dump) {
  TRI_shaped_json_t shaped;
  shaped._sid         = marker->_shape;
  shaped._data.length = marker->_size - marker->_offsetJson;
  shaped._data.data   = (char*) marker + marker->_offsetJson;

  // check if the marker contains a legend
  char const* legend = reinterpret_cast<char const*>(marker) + marker->_offsetLegend;
  if (*((uint64_t*) legend) == 0ULL) {
    // marker has no legend

    // create a fake transaction so no assertion fails
    triagens::arango::TransactionBase trx(true);
      
    // try to open the collection and use the shaper
    TRI_vocbase_t* vocbase = TRI_UseDatabaseByIdServer(dump->_vocbase->_server, marker->_databaseId);

    if (vocbase == nullptr) {
      // we'll not return an error in this case but an empty document
      // this is intentional so the replication can still continue with documents of collections
      // that survived
      return TRI_ERROR_NO_ERROR;
    }

    TRI_vocbase_col_status_e status;
    TRI_vocbase_col_t* collection = TRI_UseCollectionByIdVocBase(vocbase, marker->_collectionId, status);

    int res = TRI_ERROR_NO_ERROR;

    if (collection == nullptr) {
      // we'll not return an error in this case but an empty document
      // this is intentional so the replication can still continue with documents of collections
      // that survived
      res = TRI_ERROR_NO_ERROR;
    }
    else {
      TRI_document_collection_t* document = collection->_collection;

      if (! TRI_StringifyArrayShapedJson(document->getShaper(), dump->_buffer, &shaped, true)) {
        res = TRI_ERROR_OUT_OF_MEMORY;
      }
    
      TRI_ReleaseCollectionVocBase(vocbase, collection);
    }

    TRI_ReleaseDatabaseServer(dump->_vocbase->_server, vocbase);

    return res;
  }
  else {
    // marker has a legend, so use it
    if (marker->_offsetJson - marker->_offsetLegend == 8) {
      auto p = reinterpret_cast<int64_t const*>(legend);
      legend += *p;
    }
    triagens::basics::LegendReader lr(legend);
    if (! TRI_StringifyArrayShapedJson(&lr, dump->_buffer, &shaped, true)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  
    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a document marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerDocument (TRI_replication_dump_t* dump,
                                       TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::document_marker_t const*>(marker);
  
  int res = AppendContext(dump, m->_databaseId, m->_collectionId);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  APPEND_STRING(dump->_buffer, "\"tid\":\"");
  APPEND_UINT64(dump->_buffer, m->_transactionId);
  APPEND_STRING(dump->_buffer, "\",\"key\":\"");
  APPEND_STRING(dump->_buffer, (char const*) m + m->_offsetKey);
  APPEND_STRING(dump->_buffer, "\",\"rev\":\"");
  APPEND_UINT64(dump->_buffer, m->_revisionId);
  APPEND_STRING(dump->_buffer, "\",\"data\":{");
    
  // common document meta-data
  APPEND_STRING(dump->_buffer, "\"" TRI_VOC_ATTRIBUTE_KEY "\":\"");
  APPEND_STRING(dump->_buffer, (char const*) m + m->_offsetKey);
  APPEND_STRING(dump->_buffer, "\",\"" TRI_VOC_ATTRIBUTE_REV "\":\"");
  APPEND_UINT64(dump->_buffer, (uint64_t) m->_revisionId);
  APPEND_STRING(dump->_buffer, "\"");

  res = AppendDocument(m, dump);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  APPEND_STRING(dump->_buffer, "}");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify an edge marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerEdge (TRI_replication_dump_t* dump,
                                   TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);
  
  int res = AppendContext(dump, m->_databaseId, m->_collectionId);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  APPEND_STRING(dump->_buffer, "\"tid\":\"");
  APPEND_UINT64(dump->_buffer, m->_transactionId);
  APPEND_STRING(dump->_buffer, "\",\"key\":\"");
  APPEND_STRING(dump->_buffer, (char const*) m + m->_offsetKey);
  APPEND_STRING(dump->_buffer, "\",\"rev\":\"");
  APPEND_UINT64(dump->_buffer, m->_revisionId);
  APPEND_STRING(dump->_buffer, "\",\"data\":{");
    
  // common document meta-data
  APPEND_STRING(dump->_buffer, "\"" TRI_VOC_ATTRIBUTE_KEY "\":\"");
  APPEND_STRING(dump->_buffer, (char const*) m + m->_offsetKey);
  APPEND_STRING(dump->_buffer, "\",\"" TRI_VOC_ATTRIBUTE_REV "\":\"");
  APPEND_UINT64(dump->_buffer, (uint64_t) m->_revisionId);

  // from
  APPEND_STRING(dump->_buffer, "\",\"" TRI_VOC_ATTRIBUTE_FROM "\":\"");
  APPEND_UINT64(dump->_buffer, (uint64_t) m->_fromCid);
  APPEND_STRING(dump->_buffer, "\\/");
  APPEND_STRING(dump->_buffer, (char const*) m + m->_offsetFromKey);

  // to
  APPEND_STRING(dump->_buffer, "\",\"" TRI_VOC_ATTRIBUTE_TO "\":\"");
  APPEND_UINT64(dump->_buffer, (uint64_t) m->_toCid);
  APPEND_STRING(dump->_buffer, "\\/");
  APPEND_STRING(dump->_buffer, (char const*) m + m->_offsetToKey);
  APPEND_STRING(dump->_buffer, "\"");

  res = AppendDocument(reinterpret_cast<triagens::wal::document_marker_t const*>(m), dump);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  APPEND_STRING(dump->_buffer, "}");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a remove marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerRemove (TRI_replication_dump_t* dump,
                                     TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::remove_marker_t const*>(marker);

  int res = AppendContext(dump, m->_databaseId, m->_collectionId);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  APPEND_STRING(dump->_buffer, "\"tid\":\"");
  APPEND_UINT64(dump->_buffer, m->_transactionId);
  APPEND_STRING(dump->_buffer, "\",\"key\":\"");
  APPEND_STRING(dump->_buffer, (char const*) m + sizeof(triagens::wal::remove_marker_t));
  APPEND_STRING(dump->_buffer, "\",\"rev\":\"");
  APPEND_UINT64(dump->_buffer, m->_revisionId);
  APPEND_STRING(dump->_buffer, "\"");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a transaction marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerTransaction (TRI_replication_dump_t* dump,
                                          TRI_df_marker_t const* marker) {
  // note: the data layout of begin / commit / abort markers is identical, so
  // we cast to a begin transaction marker in all cases
  auto m = reinterpret_cast<triagens::wal::transaction_begin_marker_t const*>(marker);
  
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, m->_databaseId);
  APPEND_STRING(dump->_buffer, "\",\"tid\":\"");
  APPEND_UINT64(dump->_buffer, m->_transactionId);
  APPEND_STRING(dump->_buffer, "\"");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a create collection marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerCreateCollection (TRI_replication_dump_t* dump,
                                               TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::collection_create_marker_t const*>(marker);
  
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, m->_databaseId);
  APPEND_STRING(dump->_buffer, "\",\"cid\":\"");
  APPEND_UINT64(dump->_buffer, m->_collectionId);
  APPEND_STRING(dump->_buffer, "\",\"collection\":");
  APPEND_STRING(dump->_buffer, (char const*) m + sizeof(triagens::wal::collection_create_marker_t)); 

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a drop collection marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerDropCollection (TRI_replication_dump_t* dump,
                                             TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::collection_drop_marker_t const*>(marker);
  
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, m->_databaseId);
  APPEND_STRING(dump->_buffer, "\",\"cid\":\"");
  APPEND_UINT64(dump->_buffer, m->_collectionId);
  APPEND_STRING(dump->_buffer, "\"");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a rename collection marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerRenameCollection (TRI_replication_dump_t* dump,
                                               TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::collection_rename_marker_t const*>(marker);
  
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, m->_databaseId);
  APPEND_STRING(dump->_buffer, "\",\"cid\":\"");
  APPEND_UINT64(dump->_buffer, m->_collectionId);
  APPEND_STRING(dump->_buffer, "\",\"collection\":{\"name\":\"");
  APPEND_STRING(dump->_buffer, (char const*) m + sizeof(triagens::wal::collection_rename_marker_t));
  APPEND_STRING(dump->_buffer, "\"}");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a change collection marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerChangeCollection (TRI_replication_dump_t* dump,
                                                TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::collection_change_marker_t const*>(marker);
  
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, m->_databaseId);
  APPEND_STRING(dump->_buffer, "\",\"cid\":\"");
  APPEND_UINT64(dump->_buffer, m->_collectionId);
  APPEND_STRING(dump->_buffer, "\",\"collection\":");
  APPEND_STRING(dump->_buffer, (char const*) m + sizeof(triagens::wal::collection_change_marker_t)); 

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a create index marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerCreateIndex (TRI_replication_dump_t* dump,
                                          TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::index_create_marker_t const*>(marker);
  
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, m->_databaseId);
  APPEND_STRING(dump->_buffer, "\",\"cid\":\"");
  APPEND_UINT64(dump->_buffer, m->_collectionId);
  APPEND_STRING(dump->_buffer, "\",\"id\":\"");
  APPEND_UINT64(dump->_buffer, m->_indexId);
  APPEND_STRING(dump->_buffer, "\",\"index\":");
  APPEND_STRING(dump->_buffer, (char const*) m + sizeof(triagens::wal::index_create_marker_t)); 

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a drop index marker
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarkerDropIndex (TRI_replication_dump_t* dump,
                                        TRI_df_marker_t const* marker) {
  auto m = reinterpret_cast<triagens::wal::index_drop_marker_t const*>(marker);
  
  APPEND_STRING(dump->_buffer, "\"database\":\"");
  APPEND_UINT64(dump->_buffer, m->_databaseId);
  APPEND_STRING(dump->_buffer, "\",\"cid\":\"");
  APPEND_UINT64(dump->_buffer, m->_collectionId);
  APPEND_STRING(dump->_buffer, "\",\"id\":\"");
  APPEND_UINT64(dump->_buffer, m->_indexId);
  APPEND_STRING(dump->_buffer, "\"");

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker should be replicated
////////////////////////////////////////////////////////////////////////////////
     
static inline bool MustReplicateWalMarkerType (TRI_df_marker_t const* marker) { 
  return (marker->_type == TRI_WAL_MARKER_DOCUMENT ||
          marker->_type == TRI_WAL_MARKER_EDGE ||
          marker->_type == TRI_WAL_MARKER_REMOVE ||
          marker->_type == TRI_WAL_MARKER_BEGIN_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_COMMIT_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_ABORT_TRANSACTION ||
          marker->_type == TRI_WAL_MARKER_CREATE_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_DROP_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_RENAME_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_CHANGE_COLLECTION ||
          marker->_type == TRI_WAL_MARKER_CREATE_INDEX ||
          marker->_type == TRI_WAL_MARKER_DROP_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a marker type to a replication type
////////////////////////////////////////////////////////////////////////////////

static TRI_replication_operation_e TranslateType (TRI_df_marker_t const* marker) {
  switch (marker->_type) {
    case TRI_WAL_MARKER_DOCUMENT: 
      return REPLICATION_MARKER_DOCUMENT; 
    case TRI_WAL_MARKER_EDGE:
      return REPLICATION_MARKER_EDGE;
    case TRI_WAL_MARKER_REMOVE:
      return REPLICATION_MARKER_REMOVE;
    case TRI_WAL_MARKER_BEGIN_TRANSACTION: 
      return REPLICATION_TRANSACTION_START;
    case TRI_WAL_MARKER_COMMIT_TRANSACTION: 
      return REPLICATION_TRANSACTION_COMMIT;
    case TRI_WAL_MARKER_ABORT_TRANSACTION: 
      return REPLICATION_TRANSACTION_ABORT;
    case TRI_WAL_MARKER_CREATE_COLLECTION: 
      return REPLICATION_COLLECTION_CREATE;
    case TRI_WAL_MARKER_DROP_COLLECTION: 
      return REPLICATION_COLLECTION_DROP;
    case TRI_WAL_MARKER_RENAME_COLLECTION: 
      return REPLICATION_COLLECTION_RENAME;
    case TRI_WAL_MARKER_CHANGE_COLLECTION: 
      return REPLICATION_COLLECTION_CHANGE;
    case TRI_WAL_MARKER_CREATE_INDEX: 
      return REPLICATION_INDEX_CREATE;
    case TRI_WAL_MARKER_DROP_INDEX: 
      return REPLICATION_INDEX_DROP;
       
    default: 
      return REPLICATION_INVALID;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a raw marker from a WAL logfile for a log dump
////////////////////////////////////////////////////////////////////////////////

static int StringifyWalMarker (TRI_replication_dump_t* dump,
                               TRI_df_marker_t const* marker) {
  TRI_ASSERT_EXPENSIVE(MustReplicateWalMarkerType(marker));
    
  APPEND_STRING(dump->_buffer, "{\"tick\":\"");
  APPEND_UINT64(dump->_buffer, (uint64_t) marker->_tick);
  APPEND_STRING(dump->_buffer, "\",\"type\":");
  APPEND_UINT64(dump->_buffer, (uint64_t) TranslateType(marker)); 
  APPEND_STRING(dump->_buffer, ",");

  int res = TRI_ERROR_INTERNAL;

  switch (marker->_type) {
    case TRI_WAL_MARKER_DOCUMENT: {
      res = StringifyWalMarkerDocument(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_EDGE: {
      res = StringifyWalMarkerEdge(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_REMOVE: {
      res = StringifyWalMarkerRemove(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_BEGIN_TRANSACTION: 
    case TRI_WAL_MARKER_COMMIT_TRANSACTION: 
    case TRI_WAL_MARKER_ABORT_TRANSACTION: {
      res = StringifyWalMarkerTransaction(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_CREATE_COLLECTION: {
      res = StringifyWalMarkerCreateCollection(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_DROP_COLLECTION: {
      res = StringifyWalMarkerDropCollection(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_RENAME_COLLECTION: {
      res = StringifyWalMarkerRenameCollection(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_CHANGE_COLLECTION: {
      res = StringifyWalMarkerChangeCollection(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_CREATE_INDEX: {
      res = StringifyWalMarkerCreateIndex(dump, marker);
      break;
    }

    case TRI_WAL_MARKER_DROP_INDEX: {
      res = StringifyWalMarkerDropIndex(dump, marker);
      break;
    }
    
    case TRI_WAL_MARKER_BEGIN_REMOTE_TRANSACTION:
    case TRI_WAL_MARKER_COMMIT_REMOTE_TRANSACTION:
    case TRI_WAL_MARKER_ABORT_REMOTE_TRANSACTION:
    case TRI_WAL_MARKER_ATTRIBUTE:
    case TRI_WAL_MARKER_SHAPE: {
      TRI_ASSERT(false);
      return TRI_ERROR_INTERNAL;
    }
  }
    
  APPEND_STRING(dump->_buffer, "}\n");
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to extract a database id from a marker
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static TRI_voc_tick_t GetDatabaseId (TRI_df_marker_t const* marker) {
  T const* m = reinterpret_cast<T const*>(marker);
  return m->_databaseId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the database id from a marker
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_tick_t GetDatabaseFromWalMarker (TRI_df_marker_t const* marker) {
  TRI_ASSERT_EXPENSIVE(MustReplicateWalMarkerType(marker));

  switch (marker->_type) {
    case TRI_WAL_MARKER_ATTRIBUTE: 
      return GetDatabaseId<triagens::wal::attribute_marker_t>(marker);
    case TRI_WAL_MARKER_SHAPE: 
      return GetDatabaseId<triagens::wal::shape_marker_t>(marker);
    case TRI_WAL_MARKER_DOCUMENT: 
      return GetDatabaseId<triagens::wal::document_marker_t>(marker);
    case TRI_WAL_MARKER_EDGE: 
      return GetDatabaseId<triagens::wal::edge_marker_t>(marker);
    case TRI_WAL_MARKER_REMOVE: 
      return GetDatabaseId<triagens::wal::remove_marker_t>(marker);
    case TRI_WAL_MARKER_BEGIN_TRANSACTION: 
      return GetDatabaseId<triagens::wal::transaction_begin_marker_t>(marker);
    case TRI_WAL_MARKER_COMMIT_TRANSACTION: 
      return GetDatabaseId<triagens::wal::transaction_commit_marker_t>(marker);
    case TRI_WAL_MARKER_ABORT_TRANSACTION: 
      return GetDatabaseId<triagens::wal::transaction_abort_marker_t>(marker);
    case TRI_WAL_MARKER_CREATE_COLLECTION: 
      return GetDatabaseId<triagens::wal::collection_create_marker_t>(marker);
    case TRI_WAL_MARKER_DROP_COLLECTION: 
      return GetDatabaseId<triagens::wal::collection_drop_marker_t>(marker);
    case TRI_WAL_MARKER_RENAME_COLLECTION: 
      return GetDatabaseId<triagens::wal::collection_rename_marker_t>(marker);
    case TRI_WAL_MARKER_CHANGE_COLLECTION: 
      return GetDatabaseId<triagens::wal::collection_change_marker_t>(marker);
    case TRI_WAL_MARKER_CREATE_INDEX: 
      return GetDatabaseId<triagens::wal::index_create_marker_t>(marker);
    case TRI_WAL_MARKER_DROP_INDEX: 
      return GetDatabaseId<triagens::wal::index_drop_marker_t>(marker);
    case TRI_WAL_MARKER_CREATE_DATABASE: 
      return GetDatabaseId<triagens::wal::database_create_marker_t>(marker);
    case TRI_WAL_MARKER_DROP_DATABASE: 
      return GetDatabaseId<triagens::wal::database_drop_marker_t>(marker);
    default: {
      return 0;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to extract a collection id from a marker
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static TRI_voc_tick_t GetCollectionId (TRI_df_marker_t const* marker) {
  T const* m = reinterpret_cast<T const*>(marker);
  return m->_collectionId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the collection id from a marker
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_tick_t GetCollectionFromWalMarker (TRI_df_marker_t const* marker) {
  switch (marker->_type) {
    case TRI_WAL_MARKER_ATTRIBUTE: 
      return GetCollectionId<triagens::wal::attribute_marker_t>(marker);
    case TRI_WAL_MARKER_SHAPE: 
      return GetCollectionId<triagens::wal::shape_marker_t>(marker);
    case TRI_WAL_MARKER_DOCUMENT: 
      return GetCollectionId<triagens::wal::document_marker_t>(marker);
    case TRI_WAL_MARKER_EDGE: 
      return GetCollectionId<triagens::wal::edge_marker_t>(marker);
    case TRI_WAL_MARKER_REMOVE: 
      return GetCollectionId<triagens::wal::remove_marker_t>(marker);
    case TRI_WAL_MARKER_CREATE_COLLECTION: 
      return GetCollectionId<triagens::wal::collection_create_marker_t>(marker);
    case TRI_WAL_MARKER_DROP_COLLECTION: 
      return GetCollectionId<triagens::wal::collection_drop_marker_t>(marker);
    case TRI_WAL_MARKER_RENAME_COLLECTION: 
      return GetCollectionId<triagens::wal::collection_rename_marker_t>(marker);
    case TRI_WAL_MARKER_CHANGE_COLLECTION: 
      return GetCollectionId<triagens::wal::collection_change_marker_t>(marker);
    case TRI_WAL_MARKER_CREATE_INDEX: 
      return GetCollectionId<triagens::wal::index_create_marker_t>(marker);
    case TRI_WAL_MARKER_DROP_INDEX: 
      return GetCollectionId<triagens::wal::index_drop_marker_t>(marker);
    default: {
      return 0;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker is replicated
////////////////////////////////////////////////////////////////////////////////
     
static bool MustReplicateWalMarker (TRI_replication_dump_t* dump,
                                    TRI_df_marker_t const* marker) { 
  // first check the marker type
  if (! MustReplicateWalMarkerType(marker)) {
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

    if (name != nullptr && TRI_ExcludeCollectionReplication(name)) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a collection
////////////////////////////////////////////////////////////////////////////////

static int DumpCollection (TRI_replication_dump_t* dump,
                           TRI_document_collection_t* document,
                           TRI_voc_tick_t dataMin,
                           TRI_voc_tick_t dataMax,
                           bool withTicks,
                           bool translateCollectionIds,
                           triagens::arango::CollectionNameResolver* resolver) {
  TRI_vector_t datafiles;
  TRI_string_buffer_t* buffer;
  TRI_voc_tick_t lastFoundTick;
  TRI_voc_tid_t lastTid;
  size_t i, n;
  int res;
  bool hasMore;
  bool bufferFull;
  bool ignoreMarkers;

  // The following fake transaction allows us to access data pointers
  // and shapers, essentially disabling the runtime checks. This is OK,
  // since the dump only considers data files (and not WAL files), so
  // the collector has no trouble. Also, the data files of the collection
  // are protected from the compactor by a barrier and the dump only goes
  // until a certain tick.
  triagens::arango::TransactionBase trx(true);

  LOG_TRACE("dumping collection %llu, tick range %llu - %llu",
            (unsigned long long) document->_info._cid,
            (unsigned long long) dataMin,
            (unsigned long long) dataMax);

  buffer         = dump->_buffer;
  datafiles      = GetRangeDatafiles(document, dataMin, dataMax);

  // setup some iteration state
  lastFoundTick  = 0;
  lastTid        = 0;
  res            = TRI_ERROR_NO_ERROR;
  hasMore        = true;
  bufferFull     = false;
  ignoreMarkers  = false;

  n = datafiles._length;

  for (i = 0; i < n; ++i) {
    df_entry_t* e = (df_entry_t*) TRI_AtVector(&datafiles, i);
    TRI_datafile_t* datafile = e->_data;
    char const* ptr;
    char const* end;

    // we are reading from a journal that might be modified in parallel
    // so we must read-lock it
    if (e->_isJournal) {
      TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
    }
    else {
      TRI_ASSERT(datafile->_isSealed);
    }

    ptr = datafile->_data;

    if (res == TRI_ERROR_NO_ERROR) {
      // no error so far. start iterating
      end = ptr + datafile->_currentSize;
    }
    else {
      // some error occurred. don't iterate
      end = ptr;
    }

    while (ptr < end) {
      TRI_df_marker_t* marker = (TRI_df_marker_t*) ptr;
      TRI_voc_tick_t foundTick;
      TRI_voc_tid_t tid;

      if (marker->_size == 0 || marker->_type <= TRI_MARKER_MIN) {
        // end of datafile
        break;
      }

      ptr += TRI_DF_ALIGN_BLOCK(marker->_size);

      if (marker->_type == TRI_DF_MARKER_ATTRIBUTE ||
          marker->_type == TRI_DF_MARKER_SHAPE) {
        // fully ignore these marker types. they don't need to be replicated,
        // but we also cannot stop iteration if we find one of these
        continue;
      }

      // get the marker's tick and check whether we should include it
      foundTick = marker->_tick;

      if (foundTick <= dataMin) {
        // marker too old
        continue;
      }

      if (foundTick > dataMax) {
        // marker too new
        hasMore = false;
        goto NEXT_DF;
      }

      if (marker->_type != TRI_DOC_MARKER_KEY_DOCUMENT &&
          marker->_type != TRI_DOC_MARKER_KEY_EDGE &&
          marker->_type != TRI_DOC_MARKER_KEY_DELETION) {

        // found a non-data marker...

        // check if we can abort searching
        if (foundTick >= dataMax ||
            (foundTick >= e->_tickMax && i == (n - 1))) {
          // fetched the last available marker
          hasMore = false;
          goto NEXT_DF;
        }

        continue;
      }

      // note the last tick we processed
      lastFoundTick = foundTick;


      // handle aborted/unfinished transactions

      if (document->_failedTransactions == nullptr) {
        // there are no failed transactions
        ignoreMarkers = false;
      }
      else {
        // get transaction id of marker
        if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
          tid = ((TRI_doc_deletion_key_marker_t const*) marker)->_tid;
        }
        else {
          tid = ((TRI_doc_document_key_marker_t const*) marker)->_tid;
        }

        // check if marker is from an aborted transaction
        if (tid > 0) {
          if (tid != lastTid) {
            ignoreMarkers = (document->_failedTransactions->find(tid) != document->_failedTransactions->end());
          }

          lastTid = tid;
        }

        if (ignoreMarkers) {
          continue;
        }
      }


      res = StringifyMarkerDump(dump, document, marker, withTicks, translateCollectionIds, resolver);

      if (res != TRI_ERROR_NO_ERROR) {
        break; // will go to NEXT_DF
      }

      if (foundTick >= dataMax ||
          (foundTick >= e->_tickMax && i == (n - 1))) {
        // fetched the last available marker
        hasMore = false;
        goto NEXT_DF;
      }

      if ((uint64_t) TRI_LengthStringBuffer(buffer) > dump->_chunkSize) {
        // abort the iteration
        bufferFull = true;

        goto NEXT_DF;
      }
    }

NEXT_DF:
    if (e->_isJournal) {
      // read-unlock the journal
      TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
    }

    if (res != TRI_ERROR_NO_ERROR || ! hasMore || bufferFull) {
      break;
    }
  }

  TRI_DestroyVector(&datafiles);

  if (res == TRI_ERROR_NO_ERROR) {
    if (lastFoundTick > 0) {
      // data available for requested range
      dump->_lastFoundTick = lastFoundTick;
      dump->_hasMore       = hasMore;
      dump->_bufferFull    = bufferFull;
    }
    else {
      // no data available for requested range
      dump->_lastFoundTick = 0;
      dump->_hasMore       = false;
      dump->_bufferFull    = false;
    }
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpCollectionReplication (TRI_replication_dump_t* dump,
                                   TRI_vocbase_col_t* col,
                                   TRI_voc_tick_t dataMin,
                                   TRI_voc_tick_t dataMax,
                                   bool withTicks,
                                   bool translateCollectionIds) {
  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(col->_collection != nullptr);

  triagens::arango::CollectionNameResolver resolver(col->_vocbase);
  TRI_document_collection_t* document = col->_collection;

  // create a barrier so the underlying collection is not unloaded
  TRI_barrier_t* b = TRI_CreateBarrierReplication(&document->_barrierList);

  if (b == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // block compaction
  TRI_ReadLockReadWriteLock(&document->_compactionLock);

  int res = DumpCollection(dump, document, dataMin, dataMax, withTicks, 
                           translateCollectionIds, &resolver);

  TRI_ReadUnlockReadWriteLock(&document->_compactionLock);

  TRI_FreeBarrier(b);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from the replication log
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpLogReplication (TRI_replication_dump_t* dump,
                            TRI_voc_tick_t tickMin,
                            TRI_voc_tick_t tickMax,
                            bool outputAsArray) {
  LOG_TRACE("dumping log, tick range %llu - %llu",
            (unsigned long long) tickMin,
            (unsigned long long) tickMax);

  // ask the logfile manager which datafiles qualify
  std::vector<triagens::wal::Logfile*> logfiles = triagens::wal::LogfileManager::instance()->getLogfilesForTickRange(tickMin, tickMax);
  size_t const n = logfiles.size();
    
  // setup some iteration state
  int res = TRI_ERROR_NO_ERROR;
  TRI_voc_tick_t lastFoundTick = 0;
  bool hasMore    = true;
  bool bufferFull = false;

  if (outputAsArray) {
    TRI_AppendStringStringBuffer(dump->_buffer, "[\n");
  }

  try {
     bool first = true;

    // iterate over the datafiles found
    for (size_t i = 0; i < n; ++i) {
      triagens::wal::Logfile* logfile = logfiles[i];

      char const* ptr;
      char const* end;
      triagens::wal::LogfileManager::instance()->getActiveLogfileRegion(logfile, ptr, end);

      while (ptr < end) {
        TRI_df_marker_t const* marker = reinterpret_cast<TRI_df_marker_t const*>(ptr);

        if (marker->_size == 0 || marker->_type <= TRI_MARKER_MIN) {
          // end of datafile
          break;
        }

        ptr += TRI_DF_ALIGN_BLOCK(marker->_size);

        // get the marker's tick and check whether we should include it
        TRI_voc_tick_t foundTick = marker->_tick;

        if (foundTick <= tickMin) {
          // marker too old
          continue;
        }

        if (foundTick >= tickMax) {
          hasMore = false;
        }
        
        if (foundTick > tickMax) {
          // marker too new
          break;
        }

        if (! MustReplicateWalMarker(dump, marker)) {
          // check if we can abort searching
          continue;
        }

        // note the last tick we processed
        lastFoundTick = foundTick;
    
        if (outputAsArray) {
          if (! first) {
            TRI_AppendStringStringBuffer(dump->_buffer, ", ");
          }
          else {
            first = false;
          }
        }
  
        res = StringifyWalMarker(dump, marker);

        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }
        
        if ((uint64_t) TRI_LengthStringBuffer(dump->_buffer) >= dump->_chunkSize) {
          // abort the iteration
          bufferFull = true;
          break;
        }
      }

      if (! hasMore || bufferFull) {
        break;
      }
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // always return the logfiles we have used
  triagens::wal::LogfileManager::instance()->returnLogfiles(logfiles);
 
  if (outputAsArray) {
    TRI_AppendStringStringBuffer(dump->_buffer, "\n]");
  }

  if (res == TRI_ERROR_NO_ERROR) {
    if (lastFoundTick > 0) {
      // data available for requested range
      dump->_lastFoundTick = lastFoundTick;
      dump->_hasMore       = hasMore;
      dump->_bufferFull    = bufferFull;
    }
    else {
      // no data available for requested range
      dump->_lastFoundTick = 0;
      dump->_hasMore       = false;
      dump->_bufferFull    = false;
    }
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
