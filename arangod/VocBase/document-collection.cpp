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

#include "document-collection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/Barrier.h"
#include "Basics/conversions.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/Timers.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Basics/StaticStrings.h"
#include "Basics/ThreadPool.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterMethods.h"
#include "FulltextIndex/fulltext-index.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/Ditch.h"
#include "VocBase/IndexPoolFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/ticks.h"
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
   
////////////////////////////////////////////////////////////////////////////////
/// @brief state during opening of a collection
////////////////////////////////////////////////////////////////////////////////

struct open_iterator_state_t {
  TRI_document_collection_t* _document;
  TRI_voc_tid_t _tid;
  TRI_voc_fid_t _fid;
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer*> _stats;
  DatafileStatisticsContainer* _dfi;
  TRI_vocbase_t* _vocbase;
  arangodb::Transaction* _trx;
  uint64_t _deletions;
  uint64_t _documents;
  int64_t _initialCount;

  open_iterator_state_t(TRI_document_collection_t* document,
                        TRI_vocbase_t* vocbase)
      : _document(document),
        _tid(0),
        _fid(0),
        _stats(),
        _dfi(nullptr),
        _vocbase(vocbase),
        _trx(nullptr),
        _deletions(0),
        _documents(0),
        _initialCount(-1) {}

  ~open_iterator_state_t() {
    for (auto& it : _stats) {
      delete it.second;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find a statistics container for a given file id
////////////////////////////////////////////////////////////////////////////////

static DatafileStatisticsContainer* FindDatafileStats(
    open_iterator_state_t* state, TRI_voc_fid_t fid) {
  auto it = state->_stats.find(fid);

  if (it != state->_stats.end()) {
    return (*it).second;
  }

  auto stats = std::make_unique<DatafileStatisticsContainer>();

  state->_stats.emplace(fid, stats.get());
  auto p = stats.release();

  return p;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a document (or edge) marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDocumentMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            open_iterator_state_t* state) {
  auto const fid = datafile->_fid;
  TRI_document_collection_t* document = state->_document;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  document->setLastRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  document->_keyGenerator->track(p, length);

  ++state->_documents;
 
  if (state->_fid != fid) {
    // update the state
    state->_fid = fid; // when we're here, we're looking at a datafile
    state->_dfi = FindDatafileStats(state, fid);
  }

  auto primaryIndex = document->primaryIndex();

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto found = primaryIndex->lookupKey(trx, keySlice);

  // it is a new entry
  if (found == nullptr) {
    TRI_doc_mptr_t* header = document->_masterPointers.request();

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
      document->_masterPointers.release(header);
      LOG(ERR) << "inserting document into primary index failed with error: " << TRI_errno_string(res);

      return res;
    }

    ++document->_numberDocuments;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief process a deletion marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDeletionMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            open_iterator_state_t* state) {
  TRI_document_collection_t* document = state->_document;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_REMOVE));
  
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  document->setLastRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  document->_keyGenerator->track(p, length);

  ++state->_deletions;

  if (state->_fid != datafile->_fid) {
    // update the state
    state->_fid = datafile->_fid;
    state->_dfi = FindDatafileStats(state, datafile->_fid);
  }

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto primaryIndex = document->primaryIndex();
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

    document->deletePrimaryIndex(trx, found);
    --document->_numberDocuments;

    // free the header
    document->_masterPointers.release(found);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIterator(TRI_df_marker_t const* marker, void* data,
                         TRI_datafile_t* datafile) {
  TRI_document_collection_t* document =
      static_cast<open_iterator_state_t*>(data)->_document;
  TRI_voc_tick_t const tick = marker->getTick();
  TRI_df_marker_type_t const type = marker->getType();

  int res;

  if (type == TRI_DF_MARKER_VPACK_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile,
                                           static_cast<open_iterator_state_t*>(data));

    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (tick > datafile->_dataMax) {
      datafile->_dataMax = tick;
    }
  } else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile,
                                           static_cast<open_iterator_state_t*>(data));
  } else {
    if (type == TRI_DF_MARKER_HEADER) {
      // ensure there is a datafile info entry for each datafile of the
      // collection
      FindDatafileStats(static_cast<open_iterator_state_t*>(data), datafile->_fid);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a document collection
////////////////////////////////////////////////////////////////////////////////

static bool InitDocumentCollection(TRI_document_collection_t* document) {
  // create primary index
  std::unique_ptr<arangodb::Index> primaryIndex(
      new arangodb::PrimaryIndex(document));

  try {
    document->addIndex(primaryIndex.get());
    primaryIndex.release();
  } catch (...) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  // create edges index
  if (document->_info.type() == TRI_COL_TYPE_EDGE) {
    TRI_idx_iid_t iid = document->_info.id();
    if (document->_info.planId() > 0) {
      iid = document->_info.planId();
    }

    try {
      std::unique_ptr<arangodb::Index> edgeIndex(
          new arangodb::EdgeIndex(iid, document));

      document->addIndex(edgeIndex.get());
      edgeIndex.release();
    } catch (...) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate all markers of the collection
////////////////////////////////////////////////////////////////////////////////

static int IterateMarkersCollection(arangodb::Transaction* trx,
                                    TRI_collection_t* collection) {
  auto document = reinterpret_cast<TRI_document_collection_t*>(collection);

  // initialize state for iteration
  open_iterator_state_t openState(document, collection->_vocbase);

  if (collection->_info.initialCount() != -1) {
    auto primaryIndex = document->primaryIndex();

    int res = primaryIndex->resize(
        trx, static_cast<size_t>(collection->_info.initialCount() * 1.1));

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    openState._initialCount = collection->_info.initialCount();
  }

  // read all documents and fill primary index
  auto cb = [&openState](TRI_df_marker_t const* marker, TRI_datafile_t* datafile) -> bool {
    return OpenIterator(marker, &openState, datafile); 
  };

  collection->iterateDatafiles(cb);

  LOG(TRACE) << "found " << openState._documents << " document markers, " << openState._deletions << " deletion markers for collection '" << collection->_info.name() << "'";
  
  // update the real statistics for the collection
  try {
    for (auto& it : openState._stats) {
      document->_datafileStatistics.create(it.first, *(it.second));
    }
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_CreateDocumentCollection(
    TRI_vocbase_t* vocbase, VocbaseCollectionInfo& parameters,
    TRI_voc_cid_t cid) {
  if (cid > 0) {
    TRI_UpdateTickServer(cid);
  } else {
    cid = TRI_NewTickServer();
  }

  parameters.setCollectionId(cid);

  // check if we can generate the key generator
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> buffer =
      parameters.keyOptions();

  VPackSlice slice;
  if (buffer != nullptr) {
    slice = VPackSlice(buffer->data());
  }
  
  std::unique_ptr<KeyGenerator> keyGenerator(KeyGenerator::factory(slice));

  if (keyGenerator == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);
    return nullptr;
  }

  auto document = std::make_unique<TRI_document_collection_t>(vocbase); 
  document->_keyGenerator.reset(keyGenerator.release());

  TRI_collection_t* collection =
      TRI_CreateCollection(vocbase, document.get(), parameters);

  if (collection == nullptr) {
    LOG(ERR) << "cannot create document collection";

    return nullptr;
  }

  // create document collection
  if (false == InitDocumentCollection(document.get())) {
    LOG(ERR) << "cannot initialize document collection";
    return nullptr;
  }

  // save the parameters block (within create, no need to lock)
  bool doSync = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database")->forceSyncProperties();
  int res = parameters.saveToFile(collection->path(), doSync);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot save collection parameters in directory '" << collection->path() << "': '" << TRI_last_error() << "'";
    return nullptr;
  }

  // remove the temporary file
  std::string tmpfile = collection->path() + ".tmp";
  TRI_UnlinkFile(tmpfile.c_str());

  return document.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection(TRI_vocbase_t* vocbase,
                                                      TRI_vocbase_col_t* col,
                                                      bool ignoreErrors) {
  // first open the document collection
  auto document = std::make_unique<TRI_document_collection_t>(vocbase);

  TRI_ASSERT(document != nullptr);

  double start = TRI_microtime();
  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "open-document-collection { collection: " << vocbase->name() << "/"
      << col->name() << " }";

  int res = document->open(col->path(), ignoreErrors);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot open document collection from path '" << col->path() << "'";

    return nullptr;
  }

  // create document collection
  if (false == InitDocumentCollection(document.get())) {
    LOG(ERR) << "cannot initialize document collection";
    return nullptr;
  }

  // check if we can generate the key generator
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> buffer = document->_info.keyOptions();

  VPackSlice slice;
  if (buffer.get() != nullptr) {
    slice = VPackSlice(buffer->data());
  }

  std::unique_ptr<KeyGenerator> keyGenerator(KeyGenerator::factory(slice));

  if (keyGenerator == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);

    return nullptr;
  }

  document->_keyGenerator.reset(keyGenerator.release());

  arangodb::SingleCollectionTransaction trx(
      arangodb::StandaloneTransactionContext::Create(vocbase),
      document->_info.id(), TRI_TRANSACTION_WRITE);

  // build the primary index
  res = TRI_ERROR_INTERNAL;

  try {
    double start = TRI_microtime();

    LOG_TOPIC(TRACE, Logger::PERFORMANCE)
        << "iterate-markers { collection: " << vocbase->name() << "/"
        << document->_info.name() << " }";

    // iterate over all markers of the collection
    res = IterateMarkersCollection(&trx, document.get());

    LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::FIXED(TRI_microtime() - start) << " s, iterate-markers { collection: " << vocbase->name() << "/" << document->_info.name() << " }";
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
    auto old = document->useSecondaryIndexes();

    // turn filling of secondary indexes off. we're now only interested in getting
    // the indexes' definition. we'll fill them below ourselves.
    document->useSecondaryIndexes(false);

    try {
      document->detectIndexes(&trx);
      document->useSecondaryIndexes(old);
    } catch (...) {
      document->useSecondaryIndexes(old);
      LOG(ERR) << "cannot initialize collection indexes";
      return nullptr;
    }
  }


  if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
    // build the index structures, and fill the indexes
    document->fillIndexes(&trx, col);
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE)
      << "[timer] " << Logger::FIXED(TRI_microtime() - start)
      << " s, open-document-collection { collection: " << vocbase->name() << "/"
      << document->_info.name() << " }";

  return document.release();
}

/// @brief closes an open collection
int TRI_CloseDocumentCollection(TRI_document_collection_t* document,
                                bool updateStats) {
  auto primaryIndex = document->primaryIndex();
  auto idxSize = primaryIndex->size();

  if (!document->_info.deleted() &&
      document->_info.initialCount() != static_cast<int64_t>(idxSize)) {
    document->_info.updateCount(idxSize);

    bool doSync = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database")->forceSyncProperties();
    // Ignore the error?
    document->_info.saveToFile(document->path(), doSync);
  }

  // closes all open compactors, journals, datafiles
  document->close();
  return TRI_ERROR_NO_ERROR;
}
