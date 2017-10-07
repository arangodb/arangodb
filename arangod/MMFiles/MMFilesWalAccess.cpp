////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "MMFiles/MMFilesWalAccess.h"
#include "MMFiles/mmfiles-replication-common.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesLogfileManager.h" 
#include "MMFiles/MMFilesCompactionLocker.h"
#include "MMFiles/MMFilesDitch.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::mmfilesutils;

/// @brief translate a (local) collection id into a collection name
/*static char const* UUIDFromCid(MMFilesReplicationDumpContext* dump,
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
}*/

Result MMFilesWalAccess::tickRange(std::pair<TRI_voc_tick_t,
                                             TRI_voc_tick_t>& minMax) const {
  auto const ranges = MMFilesLogfileManager::instance()->ranges();
  if (!ranges.empty()) {
    minMax.first = ranges[0].tickMin;
    minMax.second = ranges[0].tickMax;
  } else {
    return Result(TRI_ERROR_INTERNAL, "could not load tick ranges");
  }
  for (auto const& range : ranges) {
    if (range.tickMin < minMax.first) {
      minMax.first = range.tickMin;
    }
    if (range.tickMax > minMax.second) {
      minMax.second = range.tickMax;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

/// {"lastTick":"123",
///  "version":"3.2",
///  "serverId":"abc",
///  "clients": {
///    "serverId": "ass", "lastTick":"123", ...
///  }}
///
TRI_voc_tick_t MMFilesWalAccess::lastTick() const {
  TRI_voc_tick_t maxTick = 0;
  auto const& ranges = MMFilesLogfileManager::instance()->ranges();
  for (auto& it : ranges) {
    if (maxTick < it.tickMax) {
      maxTick = it.tickMax;
    }
  }
  return maxTick;
}

/// @brief whether or not a marker belongs to a transaction
static bool IsTransactionWalMarker(WalAccess::Filter const& filter,
                                   MMFilesMarker const* marker) {
  // first check the marker type
  if (!IsTransactionWalMarkerType(marker)) {
    return false;
  }
  
  if (filter.vocbase != 0) {
    TRI_voc_tick_t dbId = MMFilesDatafileHelper::DatabaseId(marker);
    // then check if the marker belongs to the "correct" database
    return filter.vocbase == dbId;
  }
  return true;
}

/// should return the list of transactions started, but not committed in that
/// range (range can be adjusted)
WalAccessResult MMFilesWalAccess::openTransactions(uint64_t tickStart, uint64_t tickEnd,
                                                   WalAccess::Filter const& filter,
                                                   TransactionCallback const& cb) const {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "determining transactions, tick range "
    << tickStart << " - " << tickEnd;
  
  std::unordered_map<TRI_voc_tid_t, TRI_voc_tick_t> transactions;
  
  // ask the logfile manager which datafiles qualify
  bool fromTickIncluded = false;
  std::vector<arangodb::MMFilesWalLogfile*> logfiles =
  MMFilesLogfileManager::instance()->getLogfilesForTickRange(tickStart, tickEnd, fromTickIncluded);
  
  // setup some iteration state
  TRI_voc_tick_t lastFoundTick = 0;
  WalAccessResult res;
  
  // LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "found logfiles: " << logfiles.size();
  
  try {
    // iterate over the datafiles found
    size_t const n = logfiles.size();
    for (size_t i = 0; i < n; ++i) {
      arangodb::MMFilesWalLogfile* logfile = logfiles[i];
      
      char const* ptr;
      char const* end;
      MMFilesLogfileManager::instance()->getActiveLogfileRegion(
                                                                logfile, ptr, end);
      
      // LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "scanning logfile " << i;
      while (ptr < end) {
        auto const* marker = reinterpret_cast<MMFilesMarker const*>(ptr);
        
        if (marker->getSize() == 0) {
          // end of datafile
          break;
        }
        
        MMFilesMarkerType const type = marker->getType();
        
        if (type <= TRI_DF_MARKER_MIN || type >= TRI_DF_MARKER_MAX) {
          // somehow invalid
          break;
        }
        
        ptr += MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker);
        
        // get the marker's tick and check whether we should include it
        TRI_voc_tick_t const foundTick = marker->getTick();
        
        if (foundTick <= tickStart) {
          // marker too old
          continue;
        }
        
        if (foundTick > tickEnd) {
          // marker too new
          break;
        }
        
        // note the last tick we processed
        if (foundTick > lastFoundTick) {
          lastFoundTick = foundTick;
        }
        
        if (!IsTransactionWalMarker(filter, marker)) {
          continue;
        }
        
        TRI_voc_tid_t tid = MMFilesDatafileHelper::TransactionId(marker);
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
    
    // LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "found transactions: " << transactions.size();
    // LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "last tick: " << lastFoundTick;
    
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    if (transactions.empty()) {
      builder.add(VPackSlice::emptyArraySlice());
    } else {
      builder.openArray();
      for (auto const& it : transactions) {
        if (it.second - 1 < lastFoundTick) {
          lastFoundTick = it.second - 1;
        }
        cb(it.first, it.second);
      }
      builder.close();
    }
    
    res.reset(TRI_ERROR_NO_ERROR, fromTickIncluded, lastFoundTick);
    
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception while determining open transactions: " << ex.what();
    res.reset(ex.code(), false, 0);
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception while determining open transactions: " << ex.what();
    res.reset(TRI_ERROR_INTERNAL, false, 0);
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception while determining open transactions";
    res.reset(TRI_ERROR_INTERNAL, false, 0);
  }
  
  // always return the logfiles we have used
  MMFilesLogfileManager::instance()->returnLogfiles(logfiles);
  
  return res;
}

/// Tails the wall, this will already sanitize the
WalAccessResult MMFilesWalAccess::tail(std::unordered_set<TRI_voc_tid_t> const& transactionIds,
                                       TRI_voc_tick_t firstRegularTick,
                                       uint64_t tickStart, uint64_t tickEnd, size_t chunkSize,
                                       bool includeSystem, WalAccess::Filter const& filter,
                                       MarkerCallback const&) const {
  
  return WalAccessResult(TRI_ERROR_INTERNAL, false, 0);
}
/*
static int SliceifyMarker(MMFilesReplicationDumpContext* dump,
                          TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                          MMFilesMarker const* marker, bool isDump,
                          bool withTicks, bool isEdgeCollection) {
  TRI_ASSERT(MustReplicateWalMarkerType(marker));
  MMFilesMarkerType const type = marker->getType();

  VPackBuffer<uint8_t> buffer;
  std::shared_ptr<VPackBuffer<uint8_t>> bufferPtr;
  bufferPtr.reset(&buffer, arangodb::velocypack::BufferNonDeleter<uint8_t>());

  VPackBuilder builder(bufferPtr, &dump->_vpackOptions);
  builder.openObject();

  if (!isDump) {
    // logger-follow command
    builder.add("tick", VPackValue(static_cast<uint64_t>(marker->getTick())));
    builder.add("type",
                VPackValue(static_cast<uint64_t>(TranslateType(marker))));

    if (type == TRI_DF_MARKER_VPACK_DOCUMENT ||
        type == TRI_DF_MARKER_VPACK_REMOVE ||
        type == TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION ||
        type == TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION ||
        type == TRI_DF_MARKER_VPACK_ABORT_TRANSACTION) {
      // transaction id
      builder.add("tid", VPackValue(MMFilesDatafileHelper::TransactionId(marker)));
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
    builder.add("type",
                VPackValue(static_cast<uint64_t>(TranslateType(marker))));
  }

  switch (type) {
    case TRI_DF_MARKER_VPACK_DOCUMENT:
    case TRI_DF_MARKER_VPACK_REMOVE:
    case TRI_DF_MARKER_VPACK_CREATE_DATABASE:
    case TRI_DF_MARKER_VPACK_CREATE_COLLECTION:
    case TRI_DF_MARKER_VPACK_CREATE_INDEX:
    case TRI_DF_MARKER_VPACK_CREATE_VIEW:
    case TRI_DF_MARKER_VPACK_RENAME_COLLECTION:
    case TRI_DF_MARKER_VPACK_CHANGE_COLLECTION:
    case TRI_DF_MARKER_VPACK_CHANGE_VIEW:
    case TRI_DF_MARKER_VPACK_DROP_DATABASE:
    case TRI_DF_MARKER_VPACK_DROP_COLLECTION:
    case TRI_DF_MARKER_VPACK_DROP_INDEX: 
    case TRI_DF_MARKER_VPACK_DROP_VIEW: { 
      VPackSlice slice(reinterpret_cast<char const*>(marker) +
                       MMFilesDatafileHelper::VPackOffset(type));
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
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "got invalid marker of type " << static_cast<int>(type); 
      return TRI_ERROR_INTERNAL;
    }
  }

  builder.close();

  dump->_slices.push_back(std::move(buffer));

  return TRI_ERROR_NO_ERROR;
}
/// @brief whether or not a marker belongs to a transaction
static bool IsTransactionWalMarker(MMFilesReplicationDumpContext* dump,
                                   MMFilesMarker const* marker) {
  // first check the marker type
  if (!IsTransactionWalMarkerType(marker)) {
    return false;
  }

  // then check if the marker belongs to the "correct" database
  if (dump->_vocbase->id() != MMFilesDatafileHelper::DatabaseId(marker)) {
    return false;
  }

  return true;
}

/// @brief whether or not a marker is replicated
static bool MustReplicateWalMarker(
    MMFilesReplicationDumpContext* dump, MMFilesMarker const* marker,
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

  // after first regular tick, dump all transactions normally
  if (marker->getTick() >= firstRegularTick) {
    return true;
  }

  if (!transactionIds.empty()) {
    TRI_voc_tid_t tid = MMFilesDatafileHelper::TransactionId(marker);
    if (tid == 0 || transactionIds.find(tid) == transactionIds.end()) {
      return false;
    }
  }

  return true;
}

/// @brief dump data from the replication log
int MMFilesDumpLogReplication(
    MMFilesReplicationDumpContext* dump,
    std::unordered_set<TRI_voc_tid_t> const& transactionIds,
    TRI_voc_tick_t firstRegularTick, TRI_voc_tick_t tickMin,
    TRI_voc_tick_t tickMax, bool outputAsArray) {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "dumping log, tick range " << tickMin << " - " << tickMax;

  // get a custom type handler
  auto customTypeHandler = dump->_transactionContext->orderCustomTypeHandler();
  dump->_vpackOptions.customTypeHandler = customTypeHandler.get();

  // ask the logfile manager which datafiles qualify
  bool fromTickIncluded = false;
  std::vector<arangodb::MMFilesWalLogfile*> logfiles =
      MMFilesLogfileManager::instance()->getLogfilesForTickRange(
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
      arangodb::MMFilesWalLogfile* logfile = logfiles[i];

      char const* ptr;
      char const* end;
      MMFilesLogfileManager::instance()->getActiveLogfileRegion(
          logfile, ptr, end);

      while (ptr < end) {
        auto const* marker = reinterpret_cast<MMFilesMarker const*>(ptr);

        if (marker->getSize() == 0) {
          // end of datafile
          break;
        }

        MMFilesMarkerType type = marker->getType();

        if (type <= TRI_DF_MARKER_MIN || type >= TRI_DF_MARKER_MAX) {
          break;
        }

        // handle special markers
        if (type == TRI_DF_MARKER_PROLOGUE) {
          lastDatabaseId = MMFilesDatafileHelper::DatabaseId(marker);
          lastCollectionId = MMFilesDatafileHelper::CollectionId(marker);
        } else if (type == TRI_DF_MARKER_HEADER ||
                   type == TRI_DF_MARKER_FOOTER) {
          lastDatabaseId = 0;
          lastCollectionId = 0;
        } else if (type == TRI_DF_MARKER_VPACK_CREATE_COLLECTION) {
          // fill collection name cache
          TRI_voc_tick_t databaseId = MMFilesDatafileHelper::DatabaseId(marker);
          TRI_ASSERT(databaseId != 0);
          TRI_voc_cid_t collectionId = MMFilesDatafileHelper::CollectionId(marker);
          TRI_ASSERT(collectionId != 0);

          if (dump->_vocbase->id() == databaseId) {
            VPackSlice slice(reinterpret_cast<char const*>(marker) +
                             MMFilesDatafileHelper::VPackOffset(type));
            VPackSlice name = slice.get("name");
            if (name.isString()) {
              dump->_collectionNames[collectionId] = name.copyString();
            }
          }
        } else if (type == TRI_DF_MARKER_VPACK_RENAME_COLLECTION) {
          // invalidate collection name cache because this is a
          // rename operation
          dump->_collectionNames.clear();
        }

        ptr += MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker);

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
          databaseId = MMFilesDatafileHelper::DatabaseId(marker);
          collectionId = MMFilesDatafileHelper::CollectionId(marker);
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

        if (dump->_useVst) {
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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception while dumping replication log: " << ex.what();
    res = ex.code();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception while dumping replication log: " << ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception while dumping replication log";
    res = TRI_ERROR_INTERNAL;
  }

  // always return the logfiles we have used
  MMFilesLogfileManager::instance()->returnLogfiles(logfiles);

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

/// @brief determine the transactions that were open at a given point in time
int MMFilesDetermineOpenTransactionsReplication(MMFilesReplicationDumpContext* dump,
                                             TRI_voc_tick_t tickMin,
                                             TRI_voc_tick_t tickMax,
                                             bool useVst) {
 
}
*/

