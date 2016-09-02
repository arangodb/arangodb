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

#include "collection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/PageSizeFeature.h"
#include "Aql/QueryCache.h"
#include "Basics/Barrier.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/ThreadPool.h"
#include "Basics/Timers.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterMethods.h"
#include "FulltextIndex/fulltext-index.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/IndexPoolFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Wal/DocumentOperation.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"
#include "Wal/Slots.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBIndex.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#endif

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

int TRI_AddOperationTransaction(TRI_transaction_t*,
                                arangodb::wal::DocumentOperation&, bool&);

TRI_collection_t::TRI_collection_t(TRI_vocbase_t* vocbase)
      : _vocbase(vocbase), 
        _tickMax(0),
        _uncollectedLogfileEntries(0),
        _ditches(this),
        _nextCompactionStartIndex(0),
        _lastCompactionStatus(nullptr),
        _lastCompaction(0.0) {
  setCompactionStatus("compaction not yet started");
}
  
TRI_collection_t::~TRI_collection_t() {
  _ditches.destroy();
}

/// @brief whether or not a collection is fully collected
bool TRI_collection_t::isFullyCollected() {
  int64_t uncollected = _uncollectedLogfileEntries.load();

  return (uncollected == 0);
}

void TRI_collection_t::setNextCompactionStartIndex(size_t index) {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _nextCompactionStartIndex = index;
}

size_t TRI_collection_t::getNextCompactionStartIndex() {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  return _nextCompactionStartIndex;
}

void TRI_collection_t::setCompactionStatus(char const* reason) {
  TRI_ASSERT(reason != nullptr);
  struct tm tb;
  time_t tt = time(nullptr);
  TRI_gmtime(tt, &tb);

  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _lastCompactionStatus = reason;

  strftime(&_lastCompactionStamp[0], sizeof(_lastCompactionStamp),
           "%Y-%m-%dT%H:%M:%SZ", &tb);
}

void TRI_collection_t::getCompactionStatus(char const*& reason,
                                           char* dst, size_t maxSize) {
  memset(dst, 0, maxSize);
  if (maxSize > sizeof(_lastCompactionStamp)) {
    maxSize = sizeof(_lastCompactionStamp);
  }
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  reason = _lastCompactionStatus;
  memcpy(dst, &_lastCompactionStamp[0], maxSize);
}

void TRI_collection_t::figures(std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  builder->add("uncollectedLogfileEntries", VPackValue(_uncollectedLogfileEntries));
  builder->add("lastTick", VPackValue(_tickMax));

  builder->add("documentReferences", VPackValue(_ditches.numDocumentDitches()));
  
  char const* waitingForDitch = _ditches.head();
  builder->add("waitingFor", VPackValue(waitingForDitch == nullptr ? "-" : waitingForDitch));
  
  // fills in compaction status
  char const* lastCompactionStatus = nullptr;
  char lastCompactionStamp[21];
  getCompactionStatus(lastCompactionStatus,
                      &lastCompactionStamp[0],
                      sizeof(lastCompactionStamp));

  if (lastCompactionStatus == nullptr) {
    lastCompactionStatus = "-";
    lastCompactionStamp[0] = '-';
    lastCompactionStamp[1] = '\0';
  }
  
  builder->add("compactionStatus", VPackValue(VPackValueType::Object));
  builder->add("message", VPackValue(lastCompactionStatus));
  builder->add("time", VPackValue(&lastCompactionStamp[0]));
  builder->close(); // compactionStatus
}

/// @brief checks if a collection name is allowed
/// Returns true if the name is allowed and false otherwise
bool TRI_collection_t::IsAllowedName(bool allowSystem, std::string const& name) {
  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (ptr = name.c_str(); *ptr; ++ptr) {
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') ||
             ('A' <= *ptr && *ptr <= 'Z');
      } else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}

/// @brief state during opening of a collection
struct OpenIteratorState {
  LogicalCollection* _collection;
  TRI_collection_t* _document;
  TRI_voc_tid_t _tid;
  TRI_voc_fid_t _fid;
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer*> _stats;
  DatafileStatisticsContainer* _dfi;
  TRI_vocbase_t* _vocbase;
  arangodb::Transaction* _trx;
  uint64_t _deletions;
  uint64_t _documents;
  int64_t _initialCount;

  // NOTE We need to hand in both because in some situation the collection_t  is not
  // yet attached to the LogicalCollection.
  OpenIteratorState(LogicalCollection* collection, TRI_collection_t* document,
                    TRI_vocbase_t* vocbase)
      : _collection(collection),
        _document(document),
        _tid(0),
        _fid(0),
        _stats(),
        _dfi(nullptr),
        _vocbase(vocbase),
        _trx(nullptr),
        _deletions(0),
        _documents(0),
        _initialCount(-1) {
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(document != nullptr);
  }

  ~OpenIteratorState() {
    for (auto& it : _stats) {
      delete it.second;
    }
  }
};

/// @brief find a statistics container for a given file id
static DatafileStatisticsContainer* FindDatafileStats(
    OpenIteratorState* state, TRI_voc_fid_t fid) {
  auto it = state->_stats.find(fid);

  if (it != state->_stats.end()) {
    return (*it).second;
  }

  auto stats = std::make_unique<DatafileStatisticsContainer>();

  state->_stats.emplace(fid, stats.get());
  auto p = stats.release();

  return p;
}

/// @brief process a document (or edge) marker when opening a collection
static int OpenIteratorHandleDocumentMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            OpenIteratorState* state) {
  auto const fid = datafile->fid();
  LogicalCollection* collection = state->_collection;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  collection->setRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  collection->keyGenerator()->track(p, length);

  ++state->_documents;
 
  if (state->_fid != fid) {
    // update the state
    state->_fid = fid; // when we're here, we're looking at a datafile
    state->_dfi = FindDatafileStats(state, fid);
  }

  auto primaryIndex = collection->primaryIndex();

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto found = primaryIndex->lookupKey(trx, keySlice);

  // it is a new entry
  if (found == nullptr) {
    TRI_doc_mptr_t* header = collection->_masterPointers.request();

    if (header == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    header->setFid(fid, false);
    header->setHash(primaryIndex->calculateHash(trx, keySlice));
    header->setVPackFromMarker(marker);  

    // insert into primary index
    void const* result = nullptr;
    int res = primaryIndex->insertKey(trx, header, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      collection->_masterPointers.release(header);
      LOG(ERR) << "inserting document into primary index failed with error: " << TRI_errno_string(res);

      return res;
    }
    ++collection->_numberDocuments;

    // update the datafile info
    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += DatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  // it is an update, but only if found has a smaller revision identifier
  else {
    // save the old data
    TRI_doc_mptr_t oldData = *found;

    // update the header info
    found->setFid(fid, false); // when we're here, we're looking at a datafile
    found->setVPackFromMarker(marker);

    // update the datafile info
    DatafileStatisticsContainer* dfi;
    if (oldData.getFid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, oldData.getFid());
    }

    if (oldData.vpack() != nullptr) { 
      int64_t size = static_cast<int64_t>(oldData.markerSize());

      dfi->numberAlive--;
      dfi->sizeAlive -= DatafileHelper::AlignedSize<int64_t>(size);
      dfi->numberDead++;
      dfi->sizeDead += DatafileHelper::AlignedSize<int64_t>(size);
    }

    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += DatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief process a deletion marker when opening a collection
static int OpenIteratorHandleDeletionMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            OpenIteratorState* state) {
  LogicalCollection* collection = state->_collection;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_REMOVE));
  
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  collection->setRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  collection->keyGenerator()->track(p, length);

  ++state->_deletions;

  if (state->_fid != datafile->fid()) {
    // update the state
    state->_fid = datafile->fid();
    state->_dfi = FindDatafileStats(state, datafile->fid());
  }

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto primaryIndex = collection->primaryIndex();
  TRI_doc_mptr_t* found = primaryIndex->lookupKey(trx, keySlice);

  // it is a new entry, so we missed the create
  if (found == nullptr) {
    // update the datafile info
    state->_dfi->numberDeletions++;
  }

  // it is a real delete
  else {
    // update the datafile info
    DatafileStatisticsContainer* dfi;

    if (found->getFid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, found->getFid());
    }

    TRI_ASSERT(found->vpack() != nullptr);

    int64_t size = DatafileHelper::AlignedSize<int64_t>(found->markerSize());

    dfi->numberAlive--;
    dfi->sizeAlive -= DatafileHelper::AlignedSize<int64_t>(size);
    dfi->numberDead++;
    dfi->sizeDead += DatafileHelper::AlignedSize<int64_t>(size);
    state->_dfi->numberDeletions++;

    collection->deletePrimaryIndex(trx, found);

    --collection->_numberDocuments;

    // free the header
    collection->_masterPointers.release(found);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief iterator for open
static bool OpenIterator(TRI_df_marker_t const* marker, OpenIteratorState* data,
                         TRI_datafile_t* datafile) {
  TRI_collection_t* document = data->_document;
  TRI_voc_tick_t const tick = marker->getTick();
  TRI_df_marker_type_t const type = marker->getType();

  int res;

  if (type == TRI_DF_MARKER_VPACK_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile, data);

    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (tick > datafile->_dataMax) {
      datafile->_dataMax = tick;
    }
  } else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile, data);
  } else {
    if (type == TRI_DF_MARKER_HEADER) {
      // ensure there is a datafile info entry for each datafile of the
      // collection
      FindDatafileStats(data, datafile->fid());
    }

    LOG(TRACE) << "skipping marker type " << TRI_NameMarkerDatafile(marker);
    res = TRI_ERROR_NO_ERROR;
  }

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }

  if (tick > datafile->_tickMax) {
    datafile->_tickMax = tick;
  }

  if (tick > document->_tickMax) {
    if (type != TRI_DF_MARKER_HEADER &&
        type != TRI_DF_MARKER_FOOTER &&
        type != TRI_DF_MARKER_COL_HEADER &&
        type != TRI_DF_MARKER_PROLOGUE) {
      document->_tickMax = tick;
    }
  }

  return (res == TRI_ERROR_NO_ERROR);
}

/// @brief iterate all markers of the collection
static int IterateMarkersCollection(arangodb::Transaction* trx,
                                    LogicalCollection* collection,
                                    TRI_collection_t* document) {

  // FIXME All still reference collection()->_info.
  // initialize state for iteration
  OpenIteratorState openState(collection, document, collection->vocbase());

  if (collection->getPhysical()->initialCount() != -1) {
    auto primaryIndex = collection->primaryIndex();

    int res = primaryIndex->resize(
        trx, static_cast<size_t>(collection->getPhysical()->initialCount() * 1.1));

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    openState._initialCount = collection->getPhysical()->initialCount();
  }

  // read all documents and fill primary index
  auto cb = [&openState](TRI_df_marker_t const* marker, TRI_datafile_t* datafile) -> bool {
    return OpenIterator(marker, &openState, datafile); 
  };

  collection->iterateDatafiles(cb);

  LOG(TRACE) << "found " << openState._documents << " document markers, " 
             << openState._deletions << " deletion markers for collection '" << collection->name() << "'";
  
  // update the real statistics for the collection
  try {
    for (auto& it : openState._stats) {
      collection->createStats(it.first, *(it.second));
    }
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief creates a new collection
TRI_collection_t* TRI_collection_t::create(
    TRI_vocbase_t* vocbase,
    TRI_voc_cid_t cid) {
  TRI_ASSERT(cid != 0);
  auto collection = std::make_unique<TRI_collection_t>(vocbase); 
  return collection.release();
}

/// @brief opens an existing collection
TRI_collection_t* TRI_collection_t::open(TRI_vocbase_t* vocbase,
                                         arangodb::LogicalCollection* col,
                                         bool ignoreErrors) {
  VPackBuilder builder;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->getCollectionInfo(vocbase, col->cid(), builder, false, 0);

  // open the collection
  auto collection = std::make_unique<TRI_collection_t>(vocbase);

  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "open-document-collection { collection: " << vocbase->name() << "/"
      << col->name() << " }";

  collection->_path = col->path();
  int res = col->open(ignoreErrors);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot open document collection from path '" << col->path() << "'";

    return nullptr;
  }

  res = col->createInitialIndexes();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot initialize document collection: " << TRI_errno_string(res);
    return nullptr;
  }
  
  arangodb::SingleCollectionTransaction trx(
      arangodb::StandaloneTransactionContext::Create(vocbase),
      col->cid(), TRI_TRANSACTION_WRITE);

  // build the primary index
  res = TRI_ERROR_INTERNAL;

  try {
    double start = TRI_microtime();

    LOG_TOPIC(TRACE, Logger::PERFORMANCE)
        << "iterate-markers { collection: " << vocbase->name() << "/"
        << col->name() << " }";

    // iterate over all markers of the collection
    res = IterateMarkersCollection(&trx, col, collection.get());

    LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::FIXED(TRI_microtime() - start) << " s, iterate-markers { collection: " << vocbase->name() << "/" << col->name() << " }";
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (std::bad_alloc const&) {
    res = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot iterate data of document collection";
    TRI_set_errno(res);

    return nullptr;
  }

  // build the indexes meta-data, but do not fill the indexes yet
  {
    auto old = col->useSecondaryIndexes();

    // turn filling of secondary indexes off. we're now only interested in getting
    // the indexes' definition. we'll fill them below ourselves.
    col->useSecondaryIndexes(false);

    try {
      col->detectIndexes(&trx);
      col->useSecondaryIndexes(old);
    } catch (std::exception const& ex) {
      col->useSecondaryIndexes(old);
      LOG(ERR) << "cannot initialize collection indexes: " << ex.what();
      return nullptr;
    } catch (...) {
      col->useSecondaryIndexes(old);
      LOG(ERR) << "cannot initialize collection indexes: unknown exception";
      return nullptr;
    }
  }


  if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
    // build the index structures, and fill the indexes
    col->fillIndexes(&trx);
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "[timer] " << Logger::FIXED(TRI_microtime() - start)
      << " s, open-document-collection { collection: " << vocbase->name() << "/"
      << col->name() << " }";

  return collection.release();
}
