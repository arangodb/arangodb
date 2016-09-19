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
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Logger/Logger.h"
#include "VocBase/CompactionLocker.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/Ditch.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/datafile.h"
#include "VocBase/vocbase.h"
#include "Wal/Logfile.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"

#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief append values to a string buffer
////////////////////////////////////////////////////////////////////////////////

static void Append(TRI_replication_dump_t* dump, uint64_t value) {
  int res = TRI_AppendUInt64StringBuffer(dump->_buffer, value);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

static void Append(TRI_replication_dump_t* dump, char const* value) {
  int res = TRI_AppendStringStringBuffer(dump->_buffer, value);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
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
  std::string name(dump->_vocbase->collectionName(cid));

  if (!name.empty()) {
    // insert into cache
    try {
      dump->_collectionNames.emplace(cid, std::move(name));
    } catch (...) {
      return nullptr;
    }

    // and look it up again
    return NameFromCid(dump, cid);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker should be replicated
////////////////////////////////////////////////////////////////////////////////

static inline bool MustReplicateWalMarkerType(TRI_df_marker_t const* marker) {
  TRI_df_marker_type_t type = marker->getType();
  return (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
          type == TRI_DF_MARKER_VPACK_REMOVE ||
          type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_DROP_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_CHANGE_COLLECTION ||
          type == TRI_DF_MARKER_VPACK_CREATE_INDEX ||
          type == TRI_DF_MARKER_VPACK_DROP_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker belongs to a transaction
////////////////////////////////////////////////////////////////////////////////

static inline bool IsTransactionWalMarkerType(TRI_df_marker_t const* marker) {
  TRI_df_marker_type_t type = marker->getType();
  return (type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
          type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a marker type to a replication type
////////////////////////////////////////////////////////////////////////////////

static TRI_replication_operation_e TranslateType(
    TRI_df_marker_t const* marker) {
  switch (marker->getType()) {
    case TRI_DF_MARKER_VPACK_DOCUMENT:
      return REPLICATION_MARKER_DOCUMENT;
    case TRI_DF_MARKER_VPACK_REMOVE:
      return REPLICATION_MARKER_REMOVE;
    case TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION:
      return REPLICATION_TRANSACTION_START;
    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION:
      return REPLICATION_TRANSACTION_COMMIT;
    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION:
      return REPLICATION_TRANSACTION_ABORT;
    case TRI_DF_MARKER_VPACK_CREATE_COLLECTION:
      return REPLICATION_COLLECTION_CREATE;
    case TRI_DF_MARKER_VPACK_DROP_COLLECTION:
      return REPLICATION_COLLECTION_DROP;
    case TRI_DF_MARKER_VPACK_RENAME_COLLECTION:
      return REPLICATION_COLLECTION_RENAME;
    case TRI_DF_MARKER_VPACK_CHANGE_COLLECTION:
      return REPLICATION_COLLECTION_CHANGE;
    case TRI_DF_MARKER_VPACK_CREATE_INDEX:
      return REPLICATION_INDEX_CREATE;
    case TRI_DF_MARKER_VPACK_DROP_INDEX:
      return REPLICATION_INDEX_DROP;

    default:
      return REPLICATION_INVALID;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a raw marker from a logfile for a log dump or logger
/// follow command
////////////////////////////////////////////////////////////////////////////////

static int StringifyMarker(TRI_replication_dump_t* dump,
                           TRI_voc_tick_t databaseId,
                           TRI_voc_cid_t collectionId,
                           TRI_df_marker_t const* marker, bool isDump,
                           bool withTicks, bool isEdgeCollection) {
  TRI_ASSERT(MustReplicateWalMarkerType(marker));
  TRI_df_marker_type_t const type = marker->getType();

  if (!isDump) {
    // logger-follow command
    Append(dump, "{\"tick\":\"");
    Append(dump, static_cast<uint64_t>(marker->getTick()));
    // for debugging use the following
    // Append(dump, "\",\"typeName\":\"");
    // Append(dump, TRI_NameMarkerDatafile(marker));

    if (dump->_compat28 && (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
                            type == TRI_DF_MARKER_VPACK_REMOVE)) {
      // 2.8-compatible format
      VPackSlice slice(reinterpret_cast<char const*>(marker) +
                       DatafileHelper::VPackOffset(type));
      arangodb::basics::VPackStringBufferAdapter adapter(dump->_buffer);
      VPackDumper dumper(
          &adapter,
          &dump->_vpackOptions);  // note: we need the CustomTypeHandler here

      // additionally dump "key" and "rev" attributes on the top-level
      Append(dump, "\",\"key\":");
      dumper.dump(slice.get(StaticStrings::KeyString));
      if (slice.hasKey(StaticStrings::RevString)) {
        Append(dump, ",\"rev\":");
        dumper.dump(slice.get(StaticStrings::RevString));
      }
      // convert 2300 markers to 2301 markers for edges
      Append(dump, ",\"type\":");
      if (type == TRI_DF_MARKER_VPACK_DOCUMENT && isEdgeCollection) {
        Append(dump, 2301);
      } else {
        Append(dump, static_cast<uint64_t>(TranslateType(marker)));
      }
    } else {
      // 3.0 dump
      Append(dump, "\",\"type\":");
      Append(dump, static_cast<uint64_t>(TranslateType(marker)));
    }

    if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
        type == TRI_DF_MARKER_VPACK_REMOVE ||
        type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
        type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
        type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
      // transaction id
      Append(dump, ",\"tid\":\"");
      Append(dump, DatafileHelper::TransactionId(marker));
      Append(dump, "\"");
    }

    if (databaseId > 0) {
      Append(dump, ",\"database\":\"");
      Append(dump, databaseId);
      Append(dump, "\"");

      if (collectionId > 0) {
        Append(dump, ",\"cid\":\"");
        Append(dump, collectionId);
        Append(dump, "\"");
        // also include collection name
        char const* cname = NameFromCid(dump, collectionId);

        if (cname != nullptr) {
          Append(dump, ",\"cname\":\"");
          Append(dump, cname);
          Append(dump, "\"");
        }
      }
    }
  } else {
    // collection dump
    if (withTicks) {
      Append(dump, "{\"tick\":\"");
      Append(dump, static_cast<uint64_t>(marker->getTick()));
      Append(dump, "\",");
    } else {
      Append(dump, "{");
    }

    if (dump->_compat28 && (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
                            type == TRI_DF_MARKER_VPACK_REMOVE)) {
      // 2.8-compatible format
      VPackSlice slice(reinterpret_cast<char const*>(marker) +
                       DatafileHelper::VPackOffset(type));
      arangodb::basics::VPackStringBufferAdapter adapter(dump->_buffer);
      VPackDumper dumper(
          &adapter,
          &dump->_vpackOptions);  // note: we need the CustomTypeHandler here

      // additionally dump "key" and "rev" attributes on the top-level
      Append(dump, "\"key\":");
      dumper.dump(slice.get(StaticStrings::KeyString));
      if (slice.hasKey(StaticStrings::RevString)) {
        Append(dump, ",\"rev\":");
        dumper.dump(slice.get(StaticStrings::RevString));
      }
      // convert 2300 markers to 2301 markers for edges
      Append(dump, ",\"type\":");
      if (type == TRI_DF_MARKER_VPACK_DOCUMENT && isEdgeCollection) {
        Append(dump, 2301);
      } else {
        Append(dump, static_cast<uint64_t>(TranslateType(marker)));
      }
    } else {
      Append(dump, "\"type\":");
      Append(dump, static_cast<uint64_t>(TranslateType(marker)));
    }
  }

  switch (type) {
    case TRI_DF_MARKER_VPACK_DOCUMENT:
    case TRI_DF_MARKER_VPACK_REMOVE:
    case TRI_DF_MARKER_VPACK_CREATE_DATABASE:
    case TRI_DF_MARKER_VPACK_CREATE_COLLECTION:
    case TRI_DF_MARKER_VPACK_CREATE_INDEX:
    case TRI_DF_MARKER_VPACK_RENAME_COLLECTION:
    case TRI_DF_MARKER_VPACK_CHANGE_COLLECTION:
    case TRI_DF_MARKER_VPACK_DROP_DATABASE:
    case TRI_DF_MARKER_VPACK_DROP_COLLECTION:
    case TRI_DF_MARKER_VPACK_DROP_INDEX: {
      Append(dump, ",\"data\":");

      VPackSlice slice(reinterpret_cast<char const*>(marker) +
                       DatafileHelper::VPackOffset(type));
      arangodb::basics::VPackStringBufferAdapter adapter(dump->_buffer);
      VPackDumper dumper(
          &adapter,
          &dump->_vpackOptions);  // note: we need the CustomTypeHandler here
      dumper.dump(slice);
      break;
    }

    case TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION:
    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION:
    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION: {
      // nothing to do
      break;
    }

    default: {
      TRI_ASSERT(false);
      return TRI_ERROR_INTERNAL;
    }
  }

  Append(dump, "}\n");
  return TRI_ERROR_NO_ERROR;
}

static int SliceifyMarker(TRI_replication_dump_t* dump,
                          TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                          TRI_df_marker_t const* marker, bool isDump,
                          bool withTicks, bool isEdgeCollection) {
  TRI_ASSERT(MustReplicateWalMarkerType(marker));
  TRI_df_marker_type_t const type = marker->getType();

  VPackBuffer<uint8_t> buffer;
  std::shared_ptr<VPackBuffer<uint8_t>> bufferPtr;
  bufferPtr.reset(&buffer, arangodb::velocypack::BufferNonDeleter<uint8_t>());

  VPackBuilder builder(bufferPtr, &dump->_vpackOptions);
  builder.openObject();

  if (!isDump) {
    // logger-follow command
    builder.add("tick", VPackValue(static_cast<uint64_t>(marker->getTick())));

    if (dump->_compat28 && (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
                            type == TRI_DF_MARKER_VPACK_REMOVE)) {
      // 2.8-compatible format
      VPackSlice slice(reinterpret_cast<char const*>(marker) +
                       DatafileHelper::VPackOffset(type));
      // additionally dump "key" and "rev" attributes on the top-level
      builder.add("key", slice.get(StaticStrings::KeyString));
      if (slice.hasKey(StaticStrings::RevString)) {
        builder.add("rev", slice.get(StaticStrings::RevString));
      }
      // convert 2300 markers to 2301 markers for edges
      if (type == TRI_DF_MARKER_VPACK_DOCUMENT && isEdgeCollection) {
        builder.add("type", VPackValue(2301));
      } else {
        builder.add("type",
                    VPackValue(static_cast<uint64_t>(TranslateType(marker))));
      }
    } else {
      // 3.0 dump
      builder.add("type",
                  VPackValue(static_cast<uint64_t>(TranslateType(marker))));
    }

    if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
        type == TRI_DF_MARKER_VPACK_REMOVE ||
        type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
        type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
        type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
      // transaction id
      builder.add("tid", VPackValue(DatafileHelper::TransactionId(marker)));
    }
    if (databaseId > 0) {
      builder.add("database", VPackValue(databaseId));
      if (collectionId > 0) {
        builder.add("cid", VPackValue(collectionId));
        // also include collection name
        char const* cname = NameFromCid(dump, collectionId);
        if (cname != nullptr) {
          builder.add("cname", VPackValue(cname));
        }
      }
    }
  } else {
    // collection dump
    if (withTicks) {
      builder.add("tick", VPackValue(static_cast<uint64_t>(marker->getTick())));
    }

    if (dump->_compat28 && (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
                            type == TRI_DF_MARKER_VPACK_REMOVE)) {
      // 2.8-compatible format
      VPackSlice slice(reinterpret_cast<char const*>(marker) +
                       DatafileHelper::VPackOffset(type));
      builder.add("key", slice.get(StaticStrings::KeyString));
      if (slice.hasKey(StaticStrings::RevString)) {
        builder.add("rev", slice.get(StaticStrings::RevString));
      }
      // convert 2300 markers to 2301 markers for edges
      Append(dump, ",\"type\":");
      if (type == TRI_DF_MARKER_VPACK_DOCUMENT && isEdgeCollection) {
        builder.add("type", VPackValue(2301));
      } else {
        builder.add("type",
                    VPackValue(static_cast<uint64_t>(TranslateType(marker))));
      }
    } else {
      builder.add("type",
                  VPackValue(static_cast<uint64_t>(TranslateType(marker))));
    }
  }

  switch (type) {
    case TRI_DF_MARKER_VPACK_DOCUMENT:
    case TRI_DF_MARKER_VPACK_REMOVE:
    case TRI_DF_MARKER_VPACK_CREATE_DATABASE:
    case TRI_DF_MARKER_VPACK_CREATE_COLLECTION:
    case TRI_DF_MARKER_VPACK_CREATE_INDEX:
    case TRI_DF_MARKER_VPACK_RENAME_COLLECTION:
    case TRI_DF_MARKER_VPACK_CHANGE_COLLECTION:
    case TRI_DF_MARKER_VPACK_DROP_DATABASE:
    case TRI_DF_MARKER_VPACK_DROP_COLLECTION:
    case TRI_DF_MARKER_VPACK_DROP_INDEX: {
      VPackSlice slice(reinterpret_cast<char const*>(marker) +
                       DatafileHelper::VPackOffset(type));
      builder.add("data", slice);
      break;
    }

    case TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION:
    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION:
    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION: {
      // nothing to do
      break;
    }

    default: {
      TRI_ASSERT(false);
      return TRI_ERROR_INTERNAL;
    }
  }

  builder.close();

  dump->_slices.push_back(std::move(buffer));

  return TRI_ERROR_NO_ERROR;
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
  if (dump->_vocbase->id() != DatafileHelper::DatabaseId(marker)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker is replicated
////////////////////////////////////////////////////////////////////////////////

static bool MustReplicateWalMarker(
    TRI_replication_dump_t* dump, TRI_df_marker_t const* marker,
    TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
    TRI_voc_tick_t firstRegularTick,
    std::unordered_set<TRI_voc_tid_t> const& transactionIds) {
  // first check the marker type
  if (!MustReplicateWalMarkerType(marker)) {
    return false;
  }

  // then check if the marker belongs to the "correct" database
  if (dump->_vocbase->id() != databaseId) {
    return false;
  }

  // finally check if the marker is for a collection that we want to ignore
  TRI_voc_cid_t cid = collectionId;

  if (cid != 0) {
    char const* name = NameFromCid(dump, cid);

    if (name != nullptr &&
        TRI_ExcludeCollectionReplication(name, dump->_includeSystem)) {
      return false;
    }
  }

  if (dump->_restrictCollection > 0 &&
      (cid != dump->_restrictCollection &&
       !IsTransactionWalMarker(dump, marker))) {
    // restrict output to a single collection, but a different one
    return false;
  }

  if (marker->getTick() >= firstRegularTick) {
    return true;
  }

  if (!transactionIds.empty()) {
    TRI_voc_tid_t tid = DatafileHelper::TransactionId(marker);
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
                          LogicalCollection* collection,
                          TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                          TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                          bool withTicks, bool useVpp = false) {
  LOG(TRACE) << "dumping collection " << collection->cid() << ", tick range "
             << dataMin << " - " << dataMax;

  bool const isEdgeCollection = (collection->type() == TRI_COL_TYPE_EDGE);

  // setup some iteration state
  TRI_voc_tick_t lastFoundTick = 0;
  bool bufferFull = false;

  auto callback = [&dump, &lastFoundTick, &databaseId, &collectionId,
                   &withTicks, &isEdgeCollection, &bufferFull, &useVpp](
      TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker) {
    // note the last tick we processed
    lastFoundTick = foundTick;

    int res;
    if (useVpp) {
      res = SliceifyMarker(dump, databaseId, collectionId, marker, true,
                           withTicks, isEdgeCollection);
    } else {
      res = StringifyMarker(dump, databaseId, collectionId, marker, true,
                            withTicks, isEdgeCollection);
    }

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    // TODO if vppcase find out slice lenght of _slices.back()
    if (static_cast<uint64_t>(TRI_LengthStringBuffer(dump->_buffer)) >
        dump->_chunkSize) {
      // abort the iteration
      bufferFull = true;
      return false;  // stop iterating
    }

    return true;  // continue iterating
  };

  try {
    bool hasMore = collection->applyForTickRange(dataMin, dataMax, callback);

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

    return TRI_ERROR_NO_ERROR;
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_DumpCollectionReplication(TRI_replication_dump_t* dump,
                                  arangodb::LogicalCollection* collection,
                                  TRI_voc_tick_t dataMin,
                                  TRI_voc_tick_t dataMax, bool withTicks) {
  TRI_ASSERT(collection != nullptr);

  // get a custom type handler
  auto customTypeHandler = dump->_transactionContext->orderCustomTypeHandler();
  dump->_vpackOptions.customTypeHandler = customTypeHandler.get();

  // create a barrier so the underlying collection is not unloaded
  auto b = collection->ditches()->createReplicationDitch(__FILE__, __LINE__);

  if (b == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // block compaction
  int res;
  {
    CompactionPreventer compactionPreventer(collection);

    try {
      res = DumpCollection(dump, collection, collection->vocbase()->id(),
                           collection->cid(), dataMin, dataMax, withTicks);
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }
  }

  // always execute this
  collection->ditches()->freeDitch(b);

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

  // get a custom type handler
  auto customTypeHandler = dump->_transactionContext->orderCustomTypeHandler();
  dump->_vpackOptions.customTypeHandler = customTypeHandler.get();

  // ask the logfile manager which datafiles qualify
  bool fromTickIncluded = false;
  std::vector<arangodb::wal::Logfile*> logfiles =
      arangodb::wal::LogfileManager::instance()->getLogfilesForTickRange(
          tickMin, tickMax, fromTickIncluded);

  // setup some iteration state
  int res = TRI_ERROR_NO_ERROR;
  TRI_voc_tick_t lastFoundTick = 0;
  TRI_voc_tick_t lastDatabaseId = 0;
  TRI_voc_cid_t lastCollectionId = 0;
  bool hasMore = true;
  bool bufferFull = false;

  try {
    if (outputAsArray) {
      Append(dump, "[");
    }
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
        auto const* marker = reinterpret_cast<TRI_df_marker_t const*>(ptr);

        if (marker->getSize() == 0) {
          // end of datafile
          break;
        }

        TRI_df_marker_type_t type = marker->getType();

        if (type <= TRI_DF_MARKER_MIN || type >= TRI_DF_MARKER_MAX) {
          break;
        }

        // handle special markers
        if (type == TRI_DF_MARKER_PROLOGUE) {
          lastDatabaseId = DatafileHelper::DatabaseId(marker);
          lastCollectionId = DatafileHelper::CollectionId(marker);
        } else if (type == TRI_DF_MARKER_HEADER ||
                   type == TRI_DF_MARKER_FOOTER) {
          lastDatabaseId = 0;
          lastCollectionId = 0;
        } else if (type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION) {
          // fill collection name cache
          TRI_voc_tick_t databaseId = DatafileHelper::DatabaseId(marker);
          TRI_ASSERT(databaseId != 0);
          TRI_voc_cid_t collectionId = DatafileHelper::CollectionId(marker);
          TRI_ASSERT(collectionId != 0);

          if (dump->_vocbase->id() == databaseId) {
            VPackSlice slice(reinterpret_cast<char const*>(marker) +
                             DatafileHelper::VPackOffset(type));
            VPackSlice name = slice.get("name");
            if (name.isString()) {
              dump->_collectionNames[collectionId] = name.copyString();
            }
          }
        }

        ptr += DatafileHelper::AlignedMarkerSize<size_t>(marker);

        // get the marker's tick and check whether we should include it
        TRI_voc_tick_t foundTick = marker->getTick();

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

        TRI_voc_tick_t databaseId;
        TRI_voc_cid_t collectionId;

        if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
            type == TRI_DF_MARKER_VPACK_REMOVE) {
          databaseId = lastDatabaseId;
          collectionId = lastCollectionId;
        } else {
          databaseId = DatafileHelper::DatabaseId(marker);
          collectionId = DatafileHelper::CollectionId(marker);
        }

        if (!MustReplicateWalMarker(dump, marker, databaseId, collectionId,
                                    firstRegularTick, transactionIds)) {
          continue;
        }

        // note the last tick we processed
        lastFoundTick = foundTick;

        if (outputAsArray) {
          if (!first) {
            Append(dump, ",");
          } else {
            first = false;
          }
        }

        if (dump->_useVpp) {
          res = SliceifyMarker(dump, databaseId, collectionId, marker, false,
                               true, false);
        } else {
          res = StringifyMarker(dump, databaseId, collectionId, marker, false,
                                true, false);
        }

        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }

        if (static_cast<uint64_t>(TRI_LengthStringBuffer(dump->_buffer)) >=
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

    if (outputAsArray) {
      Append(dump, "]");
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // always return the logfiles we have used
  arangodb::wal::LogfileManager::instance()->returnLogfiles(logfiles);

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
                                             TRI_voc_tick_t tickMax,
                                             bool useVpp) {
  LOG(TRACE) << "determining transactions, tick range " << tickMin << " - "
             << tickMax;

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
        auto const* marker = reinterpret_cast<TRI_df_marker_t const*>(ptr);

        if (marker->getSize() == 0) {
          // end of datafile
          break;
        }

        TRI_df_marker_type_t const type = marker->getType();

        if (type <= TRI_DF_MARKER_MIN || type >= TRI_DF_MARKER_MAX) {
          // somehow invalid
          break;
        }

        ptr += DatafileHelper::AlignedMarkerSize<size_t>(marker);

        // get the marker's tick and check whether we should include it
        TRI_voc_tick_t const foundTick = marker->getTick();

        if (foundTick <= tickMin) {
          // marker too old
          continue;
        }

        if (foundTick > tickMax) {
          // marker too new
          break;
        }

        // note the last tick we processed
        if (foundTick > lastFoundTick) {
          lastFoundTick = foundTick;
        }

        if (!IsTransactionWalMarker(dump, marker)) {
          continue;
        }

        TRI_voc_tid_t tid = DatafileHelper::TransactionId(marker);
        TRI_ASSERT(tid > 0);

        if (type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION) {
          transactions.emplace(tid, foundTick);
        } else if (type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
                   type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
          transactions.erase(tid);
        } else {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "found invalid marker type");
        }
      }
    }

    // LOG(INFO) << "found transactions: " << transactions.size();
    // LOG(INFO) << "last tick: " << lastFoundTick;

    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    if (useVpp) {
      if (transactions.empty()) {
        builder.add(VPackSlice::emptyArraySlice());
      } else {
        builder.openArray();
        for (auto const& it : transactions) {
          if (it.second - 1 < lastFoundTick) {
            lastFoundTick = it.second - 1;
          }
          builder.add(VPackValue(it.first));
        }
        builder.close();
      }

    } else {
      if (transactions.empty()) {
        Append(dump, "[]");
      } else {
        bool first = true;
        Append(dump, "[\"");

        for (auto const& it : transactions) {
          if (it.second - 1 < lastFoundTick) {
            lastFoundTick = it.second - 1;
          }

          if (first) {
            first = false;
          } else {
            Append(dump, "\",\"");
          }

          Append(dump, it.first);
        }

        Append(dump, "\"]");
      }
    }

    dump->_fromTickIncluded = fromTickIncluded;
    dump->_lastFoundTick = lastFoundTick;
    // LOG(INFO) << "last tick2: " << lastFoundTick;

    dump->_slices.push_back(std::move(buffer));

  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // always return the logfiles we have used
  arangodb::wal::LogfileManager::instance()->returnLogfiles(logfiles);

  return res;
}
