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

#include "Aql/QueryCache.h"
#include "Basics/Barrier.h"
#include "Basics/conversions.h"
#include "Basics/Exceptions.h"
#include "Basics/files.h"
#include "Basics/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/ThreadPool.h"
#include "Cluster/ServerState.h"
#include "FulltextIndex/fulltext-index.h"
#include "Indexes/CapConstraint.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "RestServer/ArangoServer.h"
#include "Utils/transactions.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
#include "VocBase/Ditch.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"
#include "VocBase/headers.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/server.h"
#include "VocBase/shape-accessor.h"
#include "VocBase/update-policy.h"
#include "VocBase/VocShaper.h"
#include "Wal/DocumentOperation.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"
#include "Wal/Slots.h"

#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t::TRI_document_collection_t()
    : _lock(),
      _shaper(nullptr),
      _nextCompactionStartIndex(0),
      _lastCompactionStatus(nullptr),
      _useSecondaryIndexes(true),
      _capConstraint(nullptr),
      _ditches(this),
      _headersPtr(nullptr),
      _keyGenerator(nullptr),
      _uncollectedLogfileEntries(0),
      _cleanupIndexes(0) {
  _tickMax = 0;

  setCompactionStatus("compaction not yet started");
  if (ServerState::instance()->isDBServer()) {
    _followers.reset(new FollowerInfo(this));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a document collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t::~TRI_document_collection_t() {
  delete _keyGenerator;
}

std::string TRI_document_collection_t::label() const {
  return std::string(_vocbase->_name) + " / " + _info.name();
}

void TRI_document_collection_t::setNextCompactionStartIndex(size_t index) {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _nextCompactionStartIndex = index;
}

size_t TRI_document_collection_t::getNextCompactionStartIndex() {
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  return _nextCompactionStartIndex;
}

void TRI_document_collection_t::setCompactionStatus(char const* reason) {
  TRI_ASSERT(reason != nullptr);
  struct tm tb;
  time_t tt = time(nullptr);
  TRI_gmtime(tt, &tb);

  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  _lastCompactionStatus = reason;

  strftime(&_lastCompactionStamp[0], sizeof(_lastCompactionStamp),
           "%Y-%m-%dT%H:%M:%SZ", &tb);
}

void TRI_document_collection_t::getCompactionStatus(char const*& reason,
                                                    char* dst, size_t maxSize) {
  memset(dst, 0, maxSize);
  if (maxSize > sizeof(_lastCompactionStamp)) {
    maxSize = sizeof(_lastCompactionStamp);
  }
  MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
  reason = _lastCompactionStatus;
  memcpy(dst, &_lastCompactionStamp[0], maxSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::beginRead() {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_info.name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginRead blocked: " << document->_info._name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  // LOCKING-DEBUG
  // std::cout << "BeginRead: " << document->_info._name << std::endl;
  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);
  
  try { 
    _vocbase->_deadlockDetector.addReader(this, false);
  }
  catch (...) {
    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::endRead() {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_info.name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndRead blocked: " << document->_info._name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  
  try { 
    _vocbase->_deadlockDetector.unsetReader(this);
  }
  catch (...) {
  }
  
  // LOCKING-DEBUG
  // std::cout << "EndRead: " << document->_info._name << std::endl;
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::beginWrite() {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_info.name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginWrite blocked: " << document->_info._name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  // LOCKING_DEBUG
  // std::cout << "BeginWrite: " << document->_info._name << std::endl;
  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);
  
  // register writer
  try { 
    _vocbase->_deadlockDetector.addWriter(this, false);
  }
  catch (...) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::endWrite() {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_info.name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndWrite blocked: " << document->_info._name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  
  // unregister writer
  try {
    _vocbase->_deadlockDetector.unsetWriter(this);
  }
  catch (...) {
    // must go on here to unlock the lock
  }

  // LOCKING-DEBUG
  // std::cout << "EndWrite: " << document->_info._name << std::endl;
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection, with a timeout (in µseconds)
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::beginReadTimed(uint64_t timeout,
                                              uint64_t sleepPeriod) {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_info.name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginReadTimed blocked: " << document->_info._name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  uint64_t waited = 0;
  if (timeout == 0) {
    // we don't allow looping forever. limit waiting to 15 minutes max.
    timeout = 15 * 60 * 1000 * 1000;
  }

  // LOCKING-DEBUG
  // std::cout << "BeginReadTimed: " << document->_info._name << std::endl;
  int iterations = 0;
  bool wasBlocked = false;

  while (!TRI_TRY_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this)) {
    try {
      if (!wasBlocked) {
        // insert reader
        if (_vocbase->_deadlockDetector.setReaderBlocked(this) == TRI_ERROR_DEADLOCK) {
          // deadlock
          LOG(TRACE) << "deadlock detected while trying to acquire read-lock on collection '" << _info.namec_str() << "'";
          return TRI_ERROR_DEADLOCK;
        }
        wasBlocked = true;
        LOG(TRACE) << "waiting for read-lock on collection '" << _info.namec_str() << "'";
      } else if (++iterations >= 5) {
        // periodically check for deadlocks
        TRI_ASSERT(wasBlocked);
        iterations = 0;
        if (_vocbase->_deadlockDetector.detectDeadlock(this, false) == TRI_ERROR_DEADLOCK) {
          // deadlock
          _vocbase->_deadlockDetector.unsetReaderBlocked(this);
          LOG(TRACE) << "deadlock detected while trying to acquire read-lock on collection '" << _info.namec_str() << "'";
          return TRI_ERROR_DEADLOCK;
        }
      }
    } catch (...) {
      // clean up!
      if (wasBlocked) {
        _vocbase->_deadlockDetector.unsetReaderBlocked(this);
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

#ifdef _WIN32
    usleep((unsigned long)sleepPeriod);
#else
    usleep((useconds_t)sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      _vocbase->_deadlockDetector.unsetReaderBlocked(this);
      LOG(TRACE) << "timed out waiting for read-lock on collection '" << _info.namec_str() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }
  
  try { 
    // when we are here, we've got the read lock
    _vocbase->_deadlockDetector.addReader(this, wasBlocked);
  }
  catch (...) {
    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection, with a timeout
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::beginWriteTimed(uint64_t timeout,
                                               uint64_t sleepPeriod) {
  if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(_info.name());
    auto it = arangodb::Transaction::_makeNolockHeaders->find(collName);
    if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginWriteTimed blocked: " << document->_info._name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  uint64_t waited = 0;
  if (timeout == 0) {
    // we don't allow looping forever. limit waiting to 15 minutes max.
    timeout = 15 * 60 * 1000 * 1000;
  }

  // LOCKING-DEBUG
  // std::cout << "BeginWriteTimed: " << document->_info._name << std::endl;
  int iterations = 0;
  bool wasBlocked = false;

  while (!TRI_TRY_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this)) {
    try {
      if (!wasBlocked) {
        // insert writer
        if (_vocbase->_deadlockDetector.setWriterBlocked(this) == TRI_ERROR_DEADLOCK) {
          // deadlock
          LOG(TRACE) << "deadlock detected while trying to acquire write-lock on collection '" << _info.namec_str() << "'";
          return TRI_ERROR_DEADLOCK;
        }
        wasBlocked = true;
        LOG(TRACE) << "waiting for write-lock on collection '" << _info.namec_str() << "'";
      } else if (++iterations >= 5) {
        // periodically check for deadlocks
        TRI_ASSERT(wasBlocked);
        iterations = 0;
        if (_vocbase->_deadlockDetector.detectDeadlock(this, true) == TRI_ERROR_DEADLOCK) {
          // deadlock
          _vocbase->_deadlockDetector.unsetWriterBlocked(this);
          LOG(TRACE) << "deadlock detected while trying to acquire write-lock on collection '" << _info.namec_str() << "'";
          return TRI_ERROR_DEADLOCK;
        }
      }
    } catch (...) {
      // clean up!
      if (wasBlocked) {
        _vocbase->_deadlockDetector.unsetWriterBlocked(this);
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

#ifdef _WIN32
    usleep((unsigned long)sleepPeriod);
#else
    usleep((useconds_t)sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      _vocbase->_deadlockDetector.unsetWriterBlocked(this);
      LOG(TRACE) << "timed out waiting for write-lock on collection '" << _info.namec_str() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  try { 
    // register writer 
    _vocbase->_deadlockDetector.addWriter(this, wasBlocked);
  }
  catch (...) {
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(this);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of documents in collection
///
/// the caller must have read-locked the collection!
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_document_collection_t::size() {
  return static_cast<uint64_t>(_numberDocuments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection
/// note: the collection lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

TRI_doc_collection_info_t* TRI_document_collection_t::figures() {
  // prefill with 0's to init counters
  TRI_doc_collection_info_t* info =
      static_cast<TRI_doc_collection_info_t*>(TRI_Allocate(
          TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_collection_info_t), true));

  if (info == nullptr) {
    return nullptr;
  }

  DatafileStatisticsContainer dfi = _datafileStatistics.all();
  info->_numberAlive += static_cast<TRI_voc_ssize_t>(dfi.numberAlive);
  info->_numberDead += static_cast<TRI_voc_ssize_t>(dfi.numberDead);
  info->_numberDeletions += static_cast<TRI_voc_ssize_t>(dfi.numberDeletions);
  info->_numberShapes += static_cast<TRI_voc_ssize_t>(dfi.numberShapes);
  info->_numberAttributes += static_cast<TRI_voc_ssize_t>(dfi.numberAttributes);

  info->_sizeAlive += dfi.sizeAlive;
  info->_sizeDead += dfi.sizeDead;
  info->_sizeShapes += dfi.sizeShapes;
  info->_sizeAttributes += dfi.sizeAttributes;

  // add the file sizes for datafiles and journals
  TRI_collection_t* base = this;

  for (size_t i = 0; i < base->_datafiles._length; ++i) {
    auto df = static_cast<TRI_datafile_t const*>(base->_datafiles._buffer[i]);

    info->_datafileSize += (int64_t)df->_maximalSize;
    ++info->_numberDatafiles;
  }

  for (size_t i = 0; i < base->_journals._length; ++i) {
    auto df = static_cast<TRI_datafile_t const*>(base->_journals._buffer[i]);

    info->_journalfileSize += (int64_t)df->_maximalSize;
    ++info->_numberJournalfiles;
  }

  for (size_t i = 0; i < base->_compactors._length; ++i) {
    auto df = static_cast<TRI_datafile_t const*>(base->_compactors._buffer[i]);

    info->_compactorfileSize += (int64_t)df->_maximalSize;
    ++info->_numberCompactorfiles;
  }

  // add index information
  info->_numberIndexes = 0;
  info->_sizeIndexes = 0;

  if (_headersPtr != nullptr) {
    info->_sizeIndexes += static_cast<int64_t>(_headersPtr->memory());
  }

  for (auto& idx : allIndexes()) {
    info->_sizeIndexes += idx->memory();
    info->_numberIndexes++;
  }

  // get information about shape files (DEPRECATED, thus hard-coded to 0)
  info->_shapefileSize = 0;
  info->_numberShapefiles = 0;

  info->_uncollectedLogfileEntries = _uncollectedLogfileEntries;
  info->_tickMax = _tickMax;

  info->_numberDocumentDitches = _ditches.numDocumentDitches();
  info->_waitingForDitch = _ditches.head();

  // fills in compaction status
  getCompactionStatus(info->_lastCompactionStatus,
                      &info->_lastCompactionStamp[0],
                      sizeof(info->_lastCompactionStamp));

  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an index to the collection
/// note: this may throw. it's the caller's responsibility to catch and clean up
////////////////////////////////////////////////////////////////////////////////

void TRI_document_collection_t::addIndex(arangodb::Index* idx) {
  _indexes.emplace_back(idx);

  if (idx->type() == arangodb::Index::TRI_IDX_TYPE_CAP_CONSTRAINT) {
    // register cap constraint
    _capConstraint = static_cast<arangodb::CapConstraint*>(idx);
  } else if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
    ++_cleanupIndexes;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an index by id
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_document_collection_t::removeIndex(TRI_idx_iid_t iid) {
  size_t const n = _indexes.size();

  for (size_t i = 0; i < n; ++i) {
    auto idx = _indexes[i];

    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
        idx->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      continue;
    }

    if (idx->id() == iid) {
      // found!
      _indexes.erase(_indexes.begin() + i);

      if (idx->type() == arangodb::Index::TRI_IDX_TYPE_CAP_CONSTRAINT) {
        // unregister cap constraint
        _capConstraint = nullptr;
      } else if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
        --_cleanupIndexes;
      }

      return idx;
    }
  }

  // not found
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all indexes of the collection
////////////////////////////////////////////////////////////////////////////////

std::vector<arangodb::Index*> TRI_document_collection_t::allIndexes() const {
  return _indexes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the primary index
////////////////////////////////////////////////////////////////////////////////

arangodb::PrimaryIndex* TRI_document_collection_t::primaryIndex() {
  TRI_ASSERT(!_indexes.empty());
  // the primary index must be the index at position #0
  return static_cast<arangodb::PrimaryIndex*>(_indexes[0]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection's edge index, if it exists
////////////////////////////////////////////////////////////////////////////////

arangodb::EdgeIndex* TRI_document_collection_t::edgeIndex() {
  if (_indexes.size() >= 2 &&
      _indexes[1]->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    // edge index must be the index at position #1
    return static_cast<arangodb::EdgeIndex*>(_indexes[1]);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cap constraint index, if it exists
////////////////////////////////////////////////////////////////////////////////

arangodb::CapConstraint* TRI_document_collection_t::capConstraint() {
  for (auto const& idx : _indexes) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_CAP_CONSTRAINT) {
      return static_cast<arangodb::CapConstraint*>(idx);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an index by id
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_document_collection_t::lookupIndex(
    TRI_idx_iid_t iid) const {
  for (auto const& it : _indexes) {
    if (it->id() == iid) {
      return it;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the shaper
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
VocShaper* TRI_document_collection_t::getShaper() const {
  if (!_ditches.contains(arangodb::Ditch::TRI_DITCH_DOCUMENT)) {
  }
  return _shaper;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief add a WAL operation for a transaction collection
////////////////////////////////////////////////////////////////////////////////

int TRI_AddOperationTransaction(TRI_transaction_t*,
                                arangodb::wal::DocumentOperation&, bool&);

static int FillIndex(arangodb::Transaction*, TRI_document_collection_t*,
                     arangodb::Index*);

static int CapConstraintFromVelocyPack(arangodb::Transaction*,
                                       TRI_document_collection_t*,
                                       VPackSlice const&, TRI_idx_iid_t,
                                       arangodb::Index**);

static int GeoIndexFromVelocyPack(arangodb::Transaction*,
                                  TRI_document_collection_t*, VPackSlice const&,
                                  TRI_idx_iid_t, arangodb::Index**);

static int HashIndexFromVelocyPack(arangodb::Transaction*,
                                   TRI_document_collection_t*,
                                   VPackSlice const&, TRI_idx_iid_t,
                                   arangodb::Index**);

static int SkiplistIndexFromVelocyPack(arangodb::Transaction*,
                                       TRI_document_collection_t*,
                                       VPackSlice const&, TRI_idx_iid_t,
                                       arangodb::Index**);

static int FulltextIndexFromVelocyPack(arangodb::Transaction*,
                                       TRI_document_collection_t*,
                                       VPackSlice const&, TRI_idx_iid_t,
                                       arangodb::Index**);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the collection tick with the marker's tick value
////////////////////////////////////////////////////////////////////////////////

static inline void SetRevision(TRI_document_collection_t* document,
                               TRI_voc_rid_t rid, bool force) {
  document->_info.setRevision(rid, force);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that an error code is set in all required places
////////////////////////////////////////////////////////////////////////////////

static void EnsureErrorCode(int code) {
  if (code == TRI_ERROR_NO_ERROR) {
    // must have an error code
    code = TRI_ERROR_INTERNAL;
  }

  TRI_set_errno(code);
  errno = code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the primary index
////////////////////////////////////////////////////////////////////////////////

static int InsertPrimaryIndex(arangodb::Transaction* trx,
                              TRI_document_collection_t* document,
                              TRI_doc_mptr_t* header, bool isRollback) {
  TRI_IF_FAILURE("InsertPrimaryIndex") { return TRI_ERROR_DEBUG; }

  TRI_doc_mptr_t* found;

  TRI_ASSERT(document != nullptr);
  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(header->getDataPtr() !=
             nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME

  // insert into primary index
  auto primaryIndex = document->primaryIndex();
  int res = primaryIndex->insertKey(trx, header, (void const**)&found);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (found == nullptr) {
    // success
    return TRI_ERROR_NO_ERROR;
  }

  // we found a previous revision in the index
  // the found revision is still alive
  LOG(TRACE) << "document '" << TRI_EXTRACT_MARKER_KEY(header) << "' already existed with revision " << // ONLY IN INDEX << " while creating revision " << PROTECTED by RUNTIME
      (unsigned long long)found->_rid;

  return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int InsertSecondaryIndexes(arangodb::Transaction* trx,
                                  TRI_document_collection_t* document,
                                  TRI_doc_mptr_t const* header,
                                  bool isRollback) {
  TRI_IF_FAILURE("InsertSecondaryIndexes") { return TRI_ERROR_DEBUG; }

  if (!document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  int result = TRI_ERROR_NO_ERROR;

  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    int res = idx->insert(trx, header, isRollback);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    } else if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
          result == TRI_ERROR_NO_ERROR) {
        // "prefer" unique constraint violated
        result = res;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the primary index
////////////////////////////////////////////////////////////////////////////////

static int DeletePrimaryIndex(arangodb::Transaction* trx,
                              TRI_document_collection_t* document,
                              TRI_doc_mptr_t const* header, bool isRollback) {
  TRI_IF_FAILURE("DeletePrimaryIndex") { return TRI_ERROR_DEBUG; }

  auto primaryIndex = document->primaryIndex();
  auto found = primaryIndex->removeKey(
      trx,
      TRI_EXTRACT_MARKER_KEY(header));  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (found == nullptr) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int DeleteSecondaryIndexes(arangodb::Transaction* trx,
                                  TRI_document_collection_t* document,
                                  TRI_doc_mptr_t const* header,
                                  bool isRollback) {
  if (!document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("DeleteSecondaryIndexes") { return TRI_ERROR_DEBUG; }

  int result = TRI_ERROR_NO_ERROR;

  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    int res = idx->remove(trx, header, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates and initially populates a document master pointer
////////////////////////////////////////////////////////////////////////////////

static int CreateHeader(TRI_document_collection_t* document,
                        TRI_doc_document_key_marker_t const* marker,
                        TRI_voc_fid_t fid, TRI_voc_key_t key, uint64_t hash,
                        TRI_doc_mptr_t** result) {
  size_t markerSize = (size_t)marker->base._size;
  TRI_ASSERT(markerSize > 0);

  // get a new header pointer
  TRI_doc_mptr_t* header =
      document->_headersPtr->request(markerSize);  // ONLY IN OPENITERATOR

  if (header == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  header->_rid = marker->_rid;
  header->_fid = fid;
  header->setDataPtr(marker);  // ONLY IN OPENITERATOR
  header->_hash = hash;
  *result = header;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file
////////////////////////////////////////////////////////////////////////////////

static bool RemoveIndexFile(TRI_document_collection_t* collection,
                            TRI_idx_iid_t id) {
  // construct filename
  char* number = TRI_StringUInt64(id);

  if (number == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG(ERR) << "out of memory when creating index number";
    return false;
  }

  char* name = TRI_Concatenate3String("index-", number, ".json");

  if (name == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    LOG(ERR) << "out of memory when creating index name";
    return false;
  }

  char* filename = TRI_Concatenate2File(collection->_directory, name);

  if (filename == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, name);
    LOG(ERR) << "out of memory when creating index filename";
    return false;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, name);
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  int res = TRI_UnlinkFile(filename);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot remove index definition: " << TRI_last_error();
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing header
////////////////////////////////////////////////////////////////////////////////

static void UpdateHeader(TRI_voc_fid_t fid, TRI_df_marker_t const* m,
                         TRI_doc_mptr_t* newHeader,
                         TRI_doc_mptr_t const* oldHeader) {
  TRI_doc_document_key_marker_t const* marker;

  marker = (TRI_doc_document_key_marker_t const*)m;

  TRI_ASSERT(marker != nullptr);
  TRI_ASSERT(m->_size > 0);

  newHeader->_rid = marker->_rid;
  newHeader->_fid = fid;
  newHeader->setDataPtr(marker);  // ONLY IN OPENITERATOR
}

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage-collect a collection's indexes
////////////////////////////////////////////////////////////////////////////////

static int CleanupIndexes(TRI_document_collection_t* document) {
  int res = TRI_ERROR_NO_ERROR;

  // cleaning indexes is expensive, so only do it if the flag is set for the
  // collection
  if (document->_cleanupIndexes > 0) {
    WRITE_LOCKER(writeLocker, document->_lock);

    for (auto& idx : document->allIndexes()) {
      if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
        res = idx->cleanup();

        if (res != TRI_ERROR_NO_ERROR) {
          break;
        }
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief post-insert operation
////////////////////////////////////////////////////////////////////////////////

static int PostInsertIndexes(arangodb::Transaction* trx,
                             TRI_transaction_collection_t* trxCollection,
                             TRI_doc_mptr_t* header) {
  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  if (!document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    idx->postInsert(trx, trxCollection, header);
  }

  // post-insert will never return an error
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a new revision id if not yet set
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_rid_t GetRevisionId(TRI_voc_rid_t previous) {
  if (previous != 0) {
    return previous;
  }

  // generate new revision id
  return static_cast<TRI_voc_rid_t>(TRI_NewTickServer());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a document
////////////////////////////////////////////////////////////////////////////////

static int InsertDocument(arangodb::Transaction* trx,
                          TRI_transaction_collection_t* trxCollection,
                          TRI_doc_mptr_t* header,
                          arangodb::wal::DocumentOperation& operation,
                          TRI_doc_mptr_copy_t* mptr, bool& waitForSync) {
  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(mptr != nullptr);
  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  // .............................................................................
  // insert into indexes
  // .............................................................................

  // insert into primary index first
  int res = InsertPrimaryIndex(trx, document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // insert has failed
    return res;
  }

  // insert into secondary indexes
  res = InsertSecondaryIndexes(trx, document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    DeleteSecondaryIndexes(trx, document, header, true);
    DeletePrimaryIndex(trx, document, header, true);
    return res;
  }

  document->_numberDocuments++;

  operation.indexed();

  TRI_IF_FAILURE("InsertDocumentNoOperation") { return TRI_ERROR_DEBUG; }

  TRI_IF_FAILURE("InsertDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  res = TRI_AddOperationTransaction(trxCollection->_transaction, operation,
                                    waitForSync);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  *mptr = *header;

  res = PostInsertIndexes(trx, trxCollection, header);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document by key
/// the caller must make sure the read lock on the collection is held
////////////////////////////////////////////////////////////////////////////////

static int LookupDocument(arangodb::Transaction* trx,
                          TRI_document_collection_t* document,
                          TRI_voc_key_t key,
                          TRI_doc_update_policy_t const* policy,
                          TRI_doc_mptr_t*& header) {
  auto primaryIndex = document->primaryIndex();
  header = primaryIndex->lookupKey(trx, key);

  if (header == nullptr) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  if (policy != nullptr) {
    return policy->check(header->_rid);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing document
////////////////////////////////////////////////////////////////////////////////

static int UpdateDocument(arangodb::Transaction* trx,
                          TRI_transaction_collection_t* trxCollection,
                          TRI_doc_mptr_t* oldHeader,
                          arangodb::wal::DocumentOperation& operation,
                          TRI_doc_mptr_copy_t* mptr, bool syncRequested) {
  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  // save the old data, remember
  TRI_doc_mptr_copy_t oldData = *oldHeader;

  // .............................................................................
  // update indexes
  // .............................................................................

  // remove old document from secondary indexes
  // (it will stay in the primary index as the key won't change)

  int res = DeleteSecondaryIndexes(trx, document, oldHeader, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // re-enter the document in case of failure, ignore errors during rollback
    InsertSecondaryIndexes(trx, document, oldHeader, true);

    return res;
  }

  // .............................................................................
  // update header
  // .............................................................................

  TRI_doc_mptr_t* newHeader = oldHeader;

  // update the header. this will modify oldHeader, too !!!
  newHeader->_rid = operation.rid;
  newHeader->setDataPtr(
      operation.marker->mem());  // PROTECTED by trx in trxCollection

  // insert new document into secondary indexes
  res = InsertSecondaryIndexes(trx, document, newHeader, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // rollback
    DeleteSecondaryIndexes(trx, document, newHeader, true);

    // copy back old header data
    oldHeader->copy(oldData);

    InsertSecondaryIndexes(trx, document, oldHeader, true);

    return res;
  }

  operation.indexed();

  TRI_IF_FAILURE("UpdateDocumentNoOperation") { return TRI_ERROR_DEBUG; }

  TRI_IF_FAILURE("UpdateDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  res = TRI_AddOperationTransaction(trxCollection->_transaction, operation,
                                    syncRequested);

  if (res == TRI_ERROR_NO_ERROR) {
    // write new header into result
    *mptr = *((TRI_doc_mptr_t*)newHeader);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document or edge marker, without using a legend
////////////////////////////////////////////////////////////////////////////////

static int CreateMarkerNoLegend(arangodb::wal::Marker*& marker,
                                TRI_document_collection_t* document,
                                TRI_voc_rid_t rid,
                                TRI_transaction_collection_t* trxCollection,
                                std::string const& keyString,
                                TRI_shaped_json_t const* shaped,
                                TRI_document_edge_t const* edge) {
  TRI_ASSERT(marker == nullptr);

  TRI_IF_FAILURE("InsertDocumentNoLegend") {
    // test what happens when no legend can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("InsertDocumentNoLegendExcept") {
    // test what happens if no legend can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("InsertDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("InsertDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (edge == nullptr) {
    // document
    marker = new arangodb::wal::DocumentMarker(
        document->_vocbase->_id, document->_info.id(), rid,
        TRI_MarkerIdTransaction(trxCollection->_transaction), keyString, 8,
        shaped);
  } else {
    // edge
    marker = new arangodb::wal::EdgeMarker(
        document->_vocbase->_id, document->_info.id(), rid,
        TRI_MarkerIdTransaction(trxCollection->_transaction), keyString, edge,
        8, shaped);
  }

  TRI_ASSERT(marker != nullptr);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a document or edge marker, without using a legend
////////////////////////////////////////////////////////////////////////////////

static int CloneMarkerNoLegend(arangodb::wal::Marker*& marker,
                               TRI_df_marker_t const* original,
                               TRI_document_collection_t* document,
                               TRI_voc_rid_t rid,
                               TRI_transaction_collection_t* trxCollection,
                               TRI_shaped_json_t const* shaped) {
  TRI_ASSERT(marker == nullptr);

  TRI_IF_FAILURE("UpdateDocumentNoLegend") {
    // test what happens when no legend can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("UpdateDocumentNoLegendExcept") {
    // test what happens when no legend can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (original->_type == TRI_WAL_MARKER_DOCUMENT ||
      original->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    marker = arangodb::wal::DocumentMarker::clone(
        original, document->_vocbase->_id, document->_info.id(), rid,
        TRI_MarkerIdTransaction(trxCollection->_transaction), 8, shaped);

    return TRI_ERROR_NO_ERROR;
  } else if (original->_type == TRI_WAL_MARKER_EDGE ||
             original->_type == TRI_DOC_MARKER_KEY_EDGE) {
    marker = arangodb::wal::EdgeMarker::clone(
        original, document->_vocbase->_id, document->_info.id(), rid,
        TRI_MarkerIdTransaction(trxCollection->_transaction), 8, shaped);
    return TRI_ERROR_NO_ERROR;
  }

  // invalid marker type
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief size of operations buffer for the open iterator
////////////////////////////////////////////////////////////////////////////////

static size_t OpenIteratorBufferSize = 128;

////////////////////////////////////////////////////////////////////////////////
/// @brief state during opening of a collection
////////////////////////////////////////////////////////////////////////////////

struct open_iterator_state_t {
  TRI_document_collection_t* _document;
  TRI_voc_tid_t _tid;
  TRI_voc_fid_t _fid;
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer*> _stats;
  DatafileStatisticsContainer* _dfi;
  TRI_vector_t _operations;
  TRI_vocbase_t* _vocbase;
  arangodb::Transaction* _trx;
  uint64_t _deletions;
  uint64_t _documents;
  int64_t _initialCount;
  uint32_t _trxCollections;
  bool _trxPrepared;

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
        _initialCount(-1),
        _trxCollections(0),
        _trxPrepared(false) {}

  ~open_iterator_state_t() {
    for (auto& it : _stats) {
      delete it.second;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief container for a single collection operation (used during opening)
////////////////////////////////////////////////////////////////////////////////

struct open_iterator_operation_t {
  TRI_voc_document_operation_e _type;
  TRI_df_marker_t const* _marker;
  TRI_voc_fid_t _fid;
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
/// @brief mark a transaction as failed during opening of a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorNoteFailedTransaction(
    open_iterator_state_t const* state) {
  TRI_ASSERT(state->_tid > 0);

  if (state->_document->_failedTransactions == nullptr) {
    state->_document->_failedTransactions = new std::set<TRI_voc_tid_t>;
  }

  state->_document->_failedTransactions->insert(state->_tid);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an insert/update operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyInsert(open_iterator_state_t* state,
                                   open_iterator_operation_t const* operation) {
  TRI_document_collection_t* document = state->_document;
  arangodb::Transaction* trx = state->_trx;

  TRI_df_marker_t const* marker = operation->_marker;
  TRI_doc_document_key_marker_t const* d =
      reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);

  if (state->_fid != operation->_fid) {
    // update the state
    state->_fid = operation->_fid;
    state->_dfi = FindDatafileStats(state, operation->_fid);
  }

  SetRevision(document, d->_rid, false);

#ifdef TRI_ENABLE_MAINTAINER_MODE

#if 0
  // currently disabled because it is too chatty in trace mode
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    LOG(TRACE) << "document: fid " << operation->_fid << ", key " << ((char*) d + d->_offsetKey) << ", rid " << d->_rid << ", _offsetJson " << d->_offsetJson << ", _offsetKey " << d->_offsetKey;
  }
  else {
    TRI_doc_edge_key_marker_t const* e = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);
    LOG(TRACE) << "edge: fid " << operation->_fid << ", key " << ((char*) d + d->_offsetKey) << ", fromKey " << ((char*) e + e->_offsetFromKey) << ", toKey " << ((char*) e + e->_offsetToKey) << ", rid " << d->_rid << ", _offsetJson " << d->_offsetJson << ", _offsetKey " << d->_offsetKey;

  }
#endif

#endif

  TRI_voc_key_t key = ((char*)d) + d->_offsetKey;
  document->_keyGenerator->track(key);

  ++state->_documents;

  auto primaryIndex = document->primaryIndex();

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  arangodb::basics::BucketPosition slot;
  uint64_t hash;
  auto found = static_cast<TRI_doc_mptr_t const*>(
      primaryIndex->lookupKey(trx, key, slot, hash));

  // it is a new entry
  if (found == nullptr) {
    TRI_doc_mptr_t* header;

    // get a header
    int res = CreateHeader(document, (TRI_doc_document_key_marker_t*)marker,
                           operation->_fid, key, hash, &header);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "out of memory";

      return TRI_set_errno(res);
    }

    TRI_ASSERT(header != nullptr);
    // insert into primary index
    if (state->_initialCount != -1) {
      // we can now use an optimized insert method
      res = primaryIndex->insertKey(trx, header, slot);

      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
        document->_headersPtr->release(header, true);  // ONLY IN OPENITERATOR
      }
    } else {
      // use regular insert method
      res = InsertPrimaryIndex(trx, document, header, false);

      if (res != TRI_ERROR_NO_ERROR) {
        // insertion failed
        document->_headersPtr->release(header, true);  // ONLY IN OPENITERATOR
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "inserting document into indexes failed with error: " << TRI_errno_string(res);

      return res;
    }

    ++document->_numberDocuments;

    // update the datafile info
    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += (int64_t)TRI_DF_ALIGN_BLOCK(marker->_size);
  }

  // it is an update, but only if found has a smaller revision identifier
  else if (found->_rid < d->_rid ||
           (found->_rid == d->_rid && found->_fid <= operation->_fid)) {
    // save the old data
    TRI_doc_mptr_copy_t oldData = *found;

    TRI_doc_mptr_t* newHeader = const_cast<TRI_doc_mptr_t*>(found);

    // update the header info
    UpdateHeader(operation->_fid, marker, newHeader, found);
    document->_headersPtr->moveBack(newHeader,
                                    &oldData);  // ONLY IN OPENITERATOR

    // update the datafile info
    DatafileStatisticsContainer* dfi;
    if (oldData._fid == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, oldData._fid);
    }

    if (oldData.getDataPtr() !=
        nullptr) {  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
      TRI_ASSERT(oldData.getDataPtr() !=
                 nullptr);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
      int64_t size = (int64_t)((TRI_df_marker_t const*)oldData.getDataPtr())
                         ->_size;  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

      dfi->numberAlive--;
      dfi->sizeAlive -= TRI_DF_ALIGN_BLOCK(size);
      dfi->numberDead++;
      dfi->sizeDead += TRI_DF_ALIGN_BLOCK(size);
    }

    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += (int64_t)TRI_DF_ALIGN_BLOCK(marker->_size);
  }

  // it is a stale update
  else {
    TRI_ASSERT(found->getDataPtr() !=
               nullptr);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

    state->_dfi->numberDead++;
    state->_dfi->sizeDead += (int64_t)TRI_DF_ALIGN_BLOCK(
        ((TRI_df_marker_t*)found->getDataPtr())
            ->_size);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a delete operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyRemove(open_iterator_state_t* state,
                                   open_iterator_operation_t const* operation) {
  TRI_doc_mptr_t* found;

  TRI_document_collection_t* document = state->_document;
  arangodb::Transaction* trx = state->_trx;

  TRI_df_marker_t const* marker = operation->_marker;
  TRI_doc_deletion_key_marker_t const* d =
      (TRI_doc_deletion_key_marker_t const*)marker;

  SetRevision(document, d->_rid, false);

  ++state->_deletions;

  if (state->_fid != operation->_fid) {
    // update the state
    state->_fid = operation->_fid;
    state->_dfi = FindDatafileStats(state, operation->_fid);
  }

  TRI_voc_key_t key = ((char*)d) + d->_offsetKey;

#ifdef TRI_ENABLE_MAINTAINER_MODE
  LOG(TRACE) << "deletion: fid " << operation->_fid << ", key " << (char*)key << ", rid " << d->_rid << ", deletion " << marker->_tick;
#endif

  document->_keyGenerator->track(key);

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto primaryIndex = document->primaryIndex();
  found = static_cast<TRI_doc_mptr_t*>(primaryIndex->lookupKey(trx, key));

  // it is a new entry, so we missed the create
  if (found == nullptr) {
    // update the datafile info
    state->_dfi->numberDeletions++;
  }

  // it is a real delete
  else {
    // update the datafile info
    DatafileStatisticsContainer* dfi;

    if (found->_fid == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, found->_fid);
    }

    TRI_ASSERT(found->getDataPtr() !=
               nullptr);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

    int64_t size = (int64_t)((TRI_df_marker_t*)found->getDataPtr())
                       ->_size;  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

    dfi->numberAlive--;
    dfi->sizeAlive -= TRI_DF_ALIGN_BLOCK(size);
    dfi->numberDead++;
    dfi->sizeDead += TRI_DF_ALIGN_BLOCK(size);
    state->_dfi->numberDeletions++;

    DeletePrimaryIndex(trx, document, found, false);
    --document->_numberDocuments;

    // free the header
    document->_headersPtr->release(found, true);  // ONLY IN OPENITERATOR
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyOperation(
    open_iterator_state_t* state, open_iterator_operation_t const* operation) {
  if (operation->_type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    return OpenIteratorApplyRemove(state, operation);
  } else if (operation->_type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    return OpenIteratorApplyInsert(state, operation);
  }

  LOG(ERR) << "logic error in " << __FUNCTION__;
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an operation to the list of operations when opening a collection
/// if the operation does not belong to a designated transaction, it is
/// executed directly
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorAddOperation(open_iterator_state_t* state,
                                    TRI_voc_document_operation_e type,
                                    TRI_df_marker_t const* marker,
                                    TRI_voc_fid_t fid) {
  open_iterator_operation_t operation;
  operation._type = type;
  operation._marker = marker;
  operation._fid = fid;

  if (state->_tid == 0) {
    return OpenIteratorApplyOperation(state, &operation);
  }

  return TRI_PushBackVector(&state->_operations, &operation);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the list of operations during opening
////////////////////////////////////////////////////////////////////////////////

static void OpenIteratorResetOperations(open_iterator_state_t* state) {
  size_t n = TRI_LengthVector(&state->_operations);

  if (n > OpenIteratorBufferSize * 2) {
    // free some memory
    TRI_DestroyVector(&state->_operations);
    TRI_InitVector2(&state->_operations, TRI_UNKNOWN_MEM_ZONE,
                    sizeof(open_iterator_operation_t), OpenIteratorBufferSize);
  } else {
    TRI_ClearVector(&state->_operations);
  }

  state->_tid = 0;
  state->_trxPrepared = false;
  state->_trxCollections = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorStartTransaction(open_iterator_state_t* state,
                                        TRI_voc_tid_t tid,
                                        uint32_t numCollections) {
  state->_tid = tid;
  state->_trxCollections = numCollections;

  TRI_ASSERT(TRI_LengthVector(&state->_operations) == 0);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare an ongoing transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorPrepareTransaction(open_iterator_state_t* state) {
  if (state->_tid != 0) {
    state->_trxPrepared = true;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort an ongoing transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorAbortTransaction(open_iterator_state_t* state) {
  if (state->_tid != 0) {
    if (state->_trxCollections > 1 && state->_trxPrepared) {
      // multi-collection transaction...
      // check if we have a coordinator entry in _trx
      // if yes, then we'll recover the transaction, otherwise we'll abort it

      if (state->_vocbase->_oldTransactions != nullptr &&
          state->_vocbase->_oldTransactions->find(state->_tid) !=
              state->_vocbase->_oldTransactions->end()) {
        // we have found a coordinator entry
        // otherwise we would have got TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND etc.
        int res = TRI_ERROR_NO_ERROR;

        LOG(INFO) << "recovering transaction " << state->_tid;
        size_t const n = TRI_LengthVector(&state->_operations);

        for (size_t i = 0; i < n; ++i) {
          open_iterator_operation_t* operation =
              static_cast<open_iterator_operation_t*>(
                  TRI_AtVector(&state->_operations, i));

          int r = OpenIteratorApplyOperation(state, operation);

          if (r != TRI_ERROR_NO_ERROR) {
            res = r;
          }
        }

        OpenIteratorResetOperations(state);
        return res;
      }

      // fall-through
    }

    OpenIteratorNoteFailedTransaction(state);

    LOG(INFO) << "rolling back uncommitted transaction " << state->_tid;
    OpenIteratorResetOperations(state);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorCommitTransaction(open_iterator_state_t* state) {
  int res = TRI_ERROR_NO_ERROR;

  if (state->_trxCollections <= 1 || state->_trxPrepared) {
    size_t const n = TRI_LengthVector(&state->_operations);

    for (size_t i = 0; i < n; ++i) {
      open_iterator_operation_t* operation =
          static_cast<open_iterator_operation_t*>(
              TRI_AtVector(&state->_operations, i));

      int r = OpenIteratorApplyOperation(state, operation);
      if (r != TRI_ERROR_NO_ERROR) {
        res = r;
      }
    }
  } else if (state->_trxCollections > 1 && !state->_trxPrepared) {
    OpenIteratorAbortTransaction(state);
  }

  // clean up
  OpenIteratorResetOperations(state);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a document (or edge) marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDocumentMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            open_iterator_state_t* state) {
  TRI_doc_document_key_marker_t const* d =
      (TRI_doc_document_key_marker_t const*)marker;

  if (d->_tid > 0) {
    // marker has a transaction id
    if (d->_tid != state->_tid) {
      // we have a different transaction ongoing
      LOG(WARN) << "logic error in " << __FUNCTION__ << ", fid " << datafile->_fid << ". found tid: " << d->_tid << ", expected tid: " << state->_tid << ". this may also be the result of an aborted transaction";

      OpenIteratorAbortTransaction(state);

      return TRI_ERROR_INTERNAL;
    }
  }

  return OpenIteratorAddOperation(state, TRI_VOC_DOCUMENT_OPERATION_INSERT,
                                  marker, datafile->_fid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a deletion marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDeletionMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            open_iterator_state_t* state) {
  TRI_doc_deletion_key_marker_t const* d =
      (TRI_doc_deletion_key_marker_t const*)marker;

  if (d->_tid > 0) {
    // marker has a transaction id
    if (d->_tid != state->_tid) {
      // we have a different transaction ongoing
      LOG(WARN) << "logic error in " << __FUNCTION__ << ", fid " << datafile->_fid << ". found tid: " << d->_tid << ", expected tid: " << state->_tid << ". this may also be the result of an aborted transaction";

      OpenIteratorAbortTransaction(state);

      return TRI_ERROR_INTERNAL;
    }
  }

  OpenIteratorAddOperation(state, TRI_VOC_DOCUMENT_OPERATION_REMOVE, marker,
                           datafile->_fid);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a shape marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleShapeMarker(TRI_df_marker_t const* marker,
                                         TRI_datafile_t* datafile,
                                         open_iterator_state_t* state) {
  TRI_document_collection_t* document = state->_document;

  int res = document->getShaper()->insertShape(
      marker, true);  // ONLY IN OPENITERATOR, PROTECTED by fake trx from above

  if (res == TRI_ERROR_NO_ERROR) {
    if (state->_fid != datafile->_fid) {
      state->_fid = datafile->_fid;
      state->_dfi = FindDatafileStats(state, state->_fid);
    }

    state->_dfi->numberShapes++;
    state->_dfi->sizeShapes += (int64_t)TRI_DF_ALIGN_BLOCK(marker->_size);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process an attribute marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleAttributeMarker(TRI_df_marker_t const* marker,
                                             TRI_datafile_t* datafile,
                                             open_iterator_state_t* state) {
  TRI_document_collection_t* document = state->_document;

  int res = document->getShaper()->insertAttribute(
      marker, true);  // ONLY IN OPENITERATOR, PROTECTED by fake trx from above

  if (res == TRI_ERROR_NO_ERROR) {
    if (state->_fid != datafile->_fid) {
      state->_fid = datafile->_fid;
      state->_dfi = FindDatafileStats(state, state->_fid);
    }

    state->_dfi->numberAttributes++;
    state->_dfi->sizeAttributes += (int64_t)TRI_DF_ALIGN_BLOCK(marker->_size);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "begin transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleBeginMarker(TRI_df_marker_t const* marker,
                                         TRI_datafile_t* datafile,
                                         open_iterator_state_t* state) {
  TRI_doc_begin_transaction_marker_t const* m =
      (TRI_doc_begin_transaction_marker_t const*)marker;

  if (m->_tid != state->_tid && state->_tid != 0) {
    // some incomplete transaction was going on before us...
    LOG(WARN) << "logic error in " << __FUNCTION__ << ", fid " << datafile->_fid << ". found tid: " << m->_tid << ", expected tid: " << state->_tid << ". this may also be the result of an aborted transaction";

    OpenIteratorAbortTransaction(state);
  }

  OpenIteratorStartTransaction(state, m->_tid, (uint32_t)m->_numCollections);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "commit transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleCommitMarker(TRI_df_marker_t const* marker,
                                          TRI_datafile_t* datafile,
                                          open_iterator_state_t* state) {
  TRI_doc_commit_transaction_marker_t const* m =
      (TRI_doc_commit_transaction_marker_t const*)marker;

  if (m->_tid != state->_tid) {
    // we found a commit marker, but we did not find any begin marker
    // beforehand. strange
    LOG(WARN) << "logic error in " << __FUNCTION__ << ", fid " << datafile->_fid << ". found tid: " << m->_tid << ", expected tid: " << state->_tid;

    OpenIteratorAbortTransaction(state);
  } else {
    OpenIteratorCommitTransaction(state);
  }

  // reset transaction id
  state->_tid = 0;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "prepare transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandlePrepareMarker(TRI_df_marker_t const* marker,
                                           TRI_datafile_t* datafile,
                                           open_iterator_state_t* state) {
  TRI_doc_prepare_transaction_marker_t const* m =
      (TRI_doc_prepare_transaction_marker_t const*)marker;

  if (m->_tid != state->_tid) {
    // we found a commit marker, but we did not find any begin marker
    // beforehand. strange
    LOG(WARN) << "logic error in " << __FUNCTION__ << ", fid " << datafile->_fid << ". found tid: " << m->_tid << ", expected tid: " << state->_tid;

    OpenIteratorAbortTransaction(state);
  } else {
    OpenIteratorPrepareTransaction(state);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process an "abort transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleAbortMarker(TRI_df_marker_t const* marker,
                                         TRI_datafile_t* datafile,
                                         open_iterator_state_t* state) {
  TRI_doc_abort_transaction_marker_t const* m =
      (TRI_doc_abort_transaction_marker_t const*)marker;

  if (m->_tid != state->_tid) {
    // we found an abort marker, but we did not find any begin marker
    // beforehand. strange
    LOG(WARN) << "logic error in " << __FUNCTION__ << ", fid " << datafile->_fid << ". found tid: " << m->_tid << ", expected tid: " << state->_tid;
  }

  OpenIteratorAbortTransaction(state);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIterator(TRI_df_marker_t const* marker, void* data,
                         TRI_datafile_t* datafile) {
  TRI_document_collection_t* document =
      static_cast<open_iterator_state_t*>(data)->_document;
  TRI_voc_tick_t tick = marker->_tick;

  int res;

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
      marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile,
                                           (open_iterator_state_t*)data);

    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (tick > datafile->_dataMax) {
      datafile->_dataMax = tick;
    }
  } else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile,
                                           (open_iterator_state_t*)data);
  } else if (marker->_type == TRI_DF_MARKER_SHAPE) {
    res = OpenIteratorHandleShapeMarker(marker, datafile,
                                        (open_iterator_state_t*)data);
  } else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    res = OpenIteratorHandleAttributeMarker(marker, datafile,
                                            (open_iterator_state_t*)data);
  } else if (marker->_type == TRI_DOC_MARKER_BEGIN_TRANSACTION) {
    res = OpenIteratorHandleBeginMarker(marker, datafile,
                                        (open_iterator_state_t*)data);
  } else if (marker->_type == TRI_DOC_MARKER_COMMIT_TRANSACTION) {
    res = OpenIteratorHandleCommitMarker(marker, datafile,
                                         (open_iterator_state_t*)data);
  } else if (marker->_type == TRI_DOC_MARKER_PREPARE_TRANSACTION) {
    res = OpenIteratorHandlePrepareMarker(marker, datafile,
                                          (open_iterator_state_t*)data);
  } else if (marker->_type == TRI_DOC_MARKER_ABORT_TRANSACTION) {
    res = OpenIteratorHandleAbortMarker(marker, datafile,
                                        (open_iterator_state_t*)data);
  } else {
    if (marker->_type == TRI_DF_MARKER_HEADER) {
      // ensure there is a datafile info entry for each datafile of the
      // collection
      FindDatafileStats((open_iterator_state_t*)data, datafile->_fid);
    }

    LOG(TRACE) << "skipping marker type " << marker->_type;
    res = TRI_ERROR_NO_ERROR;
  }

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }

  if (tick > datafile->_tickMax) {
    datafile->_tickMax = tick;
  }

  if (tick > document->_tickMax) {
    if (marker->_type != TRI_DF_MARKER_HEADER &&
        marker->_type != TRI_DF_MARKER_FOOTER &&
        marker->_type != TRI_COL_MARKER_HEADER) {
      document->_tickMax = tick;
    }
  }

  return (res == TRI_ERROR_NO_ERROR);
}

struct OpenIndexIteratorContext {
  arangodb::Transaction* trx;
  TRI_document_collection_t* collection;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for index open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIndexIterator(char const* filename, void* data) {
  // load VelocyPack description of the index
  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
  } catch (...) {
    // Failed to parse file
    LOG(ERR) << "failed to parse index definition from '" << filename << "'";
    return false;
  }

  VPackSlice description = builder->slice();
  // VelocyPack must be a index description
  if (!description.isObject()) {
    LOG(ERR) << "cannot read index definition from '" << filename << "'";
    return false;
  }

  auto ctx = static_cast<OpenIndexIteratorContext*>(data);
  arangodb::Transaction* trx = ctx->trx;
  TRI_document_collection_t* collection = ctx->collection;

  int res = TRI_FromVelocyPackIndexDocumentCollection(trx, collection,
                                                      description, nullptr);

  if (res != TRI_ERROR_NO_ERROR) {
    // error was already printed if we get here
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a document collection
////////////////////////////////////////////////////////////////////////////////

static int InitBaseDocumentCollection(TRI_document_collection_t* document,
                                      VocShaper* shaper) {
  TRI_ASSERT(document != nullptr);

  document->setShaper(shaper);
  document->_numberDocuments = 0;
  document->_lastCompaction = 0.0;

  TRI_InitReadWriteLock(&document->_compactionLock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary collection
////////////////////////////////////////////////////////////////////////////////

static void DestroyBaseDocumentCollection(TRI_document_collection_t* document) {
  if (document->_keyGenerator != nullptr) {
    delete document->_keyGenerator;
    document->_keyGenerator = nullptr;
  }

  TRI_DestroyReadWriteLock(&document->_compactionLock);

  if (document->getShaper() != nullptr) {  // PROTECTED by trx here
    delete document->getShaper();          // PROTECTED by trx here
  }

  if (document->_headersPtr != nullptr) {
    delete document->_headersPtr;
    document->_headersPtr = nullptr;
  }

  document->ditches()->destroy();
  TRI_DestroyCollection(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a document collection
////////////////////////////////////////////////////////////////////////////////

static bool InitDocumentCollection(TRI_document_collection_t* document,
                                   VocShaper* shaper) {
  // TODO: Here are leaks, in particular with _headersPtr etc., need sane
  // convention of who frees what when. Do this in the context of the
  // TRI_document_collection_t cleanup.
  TRI_ASSERT(document != nullptr);

  document->_cleanupIndexes = false;
  document->_failedTransactions = nullptr;

  document->_uncollectedLogfileEntries.store(0);

  int res = InitBaseDocumentCollection(document, shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyCollection(document);
    TRI_set_errno(res);

    return false;
  }

  document->_headersPtr = new TRI_headers_t;  // ONLY IN CREATE COLLECTION

  if (document->_headersPtr == nullptr) {  // ONLY IN CREATE COLLECTION
    DestroyBaseDocumentCollection(document);

    return false;
  }

  // create primary index
  std::unique_ptr<arangodb::Index> primaryIndex(
      new arangodb::PrimaryIndex(document));

  try {
    document->addIndex(primaryIndex.get());
    primaryIndex.release();
  } catch (...) {
    DestroyBaseDocumentCollection(document);
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
      DestroyBaseDocumentCollection(document);
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return false;
    }
  }

  TRI_InitCondition(&document->_journalsCondition);

  // crud methods
  document->cleanupIndexes = CleanupIndexes;

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

  int res = TRI_InitVector2(&openState._operations, TRI_UNKNOWN_MEM_ZONE,
                            sizeof(open_iterator_operation_t),
                            OpenIteratorBufferSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // read all documents and fill primary index
  TRI_IterateCollection(collection, OpenIterator, &openState);

  LOG(TRACE) << "found " << openState._documents << " document markers, " << openState._deletions << " deletion markers for collection '" << collection->_info.namec_str() << "'";

  // abort any transaction that's unfinished after iterating over all markers
  OpenIteratorAbortTransaction(&openState);

  TRI_DestroyVector(&openState._operations);

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
    TRI_vocbase_t* vocbase, char const* path, VocbaseCollectionInfo& parameters,
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
  KeyGenerator* keyGenerator = KeyGenerator::factory(slice);

  if (keyGenerator == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);
    return nullptr;
  }

  // first create the document collection
  TRI_document_collection_t* document;
  try {
    document = new TRI_document_collection_t();
  } catch (std::exception&) {
    document = nullptr;
  }

  if (document == nullptr) {
    delete keyGenerator;
    LOG(WARN) << "cannot create document collection";
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  TRI_ASSERT(document != nullptr);

  document->_keyGenerator = keyGenerator;

  TRI_collection_t* collection =
      TRI_CreateCollection(vocbase, document, path, parameters);

  if (collection == nullptr) {
    delete document;
    LOG(ERR) << "cannot create document collection";

    return nullptr;
  }

  auto shaper = new VocShaper(TRI_UNKNOWN_MEM_ZONE, document);

  // create document collection and shaper
  if (false == InitDocumentCollection(document, shaper)) {
    LOG(ERR) << "cannot initialize document collection";

    // TODO: shouldn't we free document->_headersPtr etc.?
    // Yes, do this in the context of the TRI_document_collection_t cleanup.
    TRI_CloseCollection(collection);
    TRI_DestroyCollection(collection);
    delete document;
    return nullptr;
  }

  document->_keyGenerator = keyGenerator;

  // save the parameters block (within create, no need to lock)
  bool doSync = vocbase->_settings.forceSyncProperties;
  int res = parameters.saveToFile(collection->_directory, doSync);

  if (res != TRI_ERROR_NO_ERROR) {
    // TODO: shouldn't we free document->_headersPtr etc.?
    // Yes, do this in the context of the TRI_document_collection_t cleanup.
    LOG(ERR) << "cannot save collection parameters in directory '" << collection->_directory << "': '" << TRI_last_error() << "'";

    TRI_CloseCollection(collection);
    TRI_DestroyCollection(collection);
    delete document;
    return nullptr;
  }

  // remove the temporary file
  char* tmpfile = TRI_Concatenate2File(collection->_directory, ".tmp");
  TRI_UnlinkFile(tmpfile);
  TRI_Free(TRI_CORE_MEM_ZONE, tmpfile);

  TRI_ASSERT(document->getShaper() !=
             nullptr);  // ONLY IN COLLECTION CREATION, PROTECTED by trx here

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocumentCollection(TRI_document_collection_t* document) {
  TRI_DestroyCondition(&document->_journalsCondition);

  // free memory allocated for indexes
  for (auto& idx : document->allIndexes()) {
    delete idx;
  }

  if (document->_failedTransactions != nullptr) {
    delete document->_failedTransactions;
  }

  DestroyBaseDocumentCollection(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeDocumentCollection(TRI_document_collection_t* document) {
  TRI_DestroyDocumentCollection(document);
  delete document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateDatafileDocumentCollection(
    TRI_document_collection_t* document, TRI_voc_fid_t fid,
    TRI_voc_size_t journalSize, bool isCompactor) {
  TRI_ASSERT(fid > 0);

  // create a datafile entry for the new journal
  try {
    document->_datafileStatistics.create(fid);
  } catch (...) {
    EnsureErrorCode(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  TRI_datafile_t* journal;

  if (document->_info.isVolatile()) {
    // in-memory collection
    journal = TRI_CreateDatafile(nullptr, fid, journalSize, true);
  } else {
    // construct a suitable filename (which may be temporary at the beginning)
    char* number = TRI_StringUInt64(fid);
    char* jname;
    if (isCompactor) {
      jname = TRI_Concatenate3String("compaction-", number, ".db");
    } else {
      jname = TRI_Concatenate3String("temp-", number, ".db");
    }

    char* filename = TRI_Concatenate2File(document->_directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    TRI_IF_FAILURE("CreateJournalDocumentCollection") {
      // simulate disk full
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
      document->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);

      EnsureErrorCode(TRI_ERROR_ARANGO_FILESYSTEM_FULL);

      return nullptr;
    }

    // remove an existing temporary file first
    if (TRI_ExistsFile(filename)) {
      // remove an existing file first
      TRI_UnlinkFile(filename);
    }

    journal = TRI_CreateDatafile(filename, fid, journalSize, true);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }

  if (journal == nullptr) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      document->_lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);
    } else {
      document->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    }

    EnsureErrorCode(document->_lastError);

    return nullptr;
  }

  // journal is there now
  TRI_ASSERT(journal != nullptr);

  if (isCompactor) {
    LOG(TRACE) << "created new compactor '" << journal->getName(journal) << "'";
  } else {
    LOG(TRACE) << "created new journal '" << journal->getName(journal) << "'";
  }

  // create a collection header, still in the temporary file
  TRI_df_marker_t* position;
  int res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t),
                                       &position, journalSize);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve1") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    document->_lastError = journal->_lastError;
    LOG(ERR) << "cannot create collection header in file '" << journal->getName(journal) << "': " << TRI_errno_string(res);

    // close the journal and remove it
    TRI_CloseDatafile(journal);
    TRI_UnlinkFile(journal->getName(journal));
    TRI_FreeDatafile(journal);

    EnsureErrorCode(res);

    return nullptr;
  }

  TRI_col_header_marker_t cm;
  TRI_InitMarkerDatafile((char*)&cm, TRI_COL_MARKER_HEADER,
                         sizeof(TRI_col_header_marker_t));
  cm.base._tick = static_cast<TRI_voc_tick_t>(fid);
  cm._type = (TRI_col_type_t)document->_info.type();
  cm._cid = document->_info.id();

  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, false);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    document->_lastError = journal->_lastError;
    LOG(ERR) << "cannot create collection header in file '" << journal->getName(journal) << "': " << TRI_last_error();

    // close the journal and remove it
    TRI_CloseDatafile(journal);
    TRI_UnlinkFile(journal->getName(journal));
    TRI_FreeDatafile(journal);

    EnsureErrorCode(document->_lastError);

    return nullptr;
  }

  TRI_ASSERT(fid == journal->_fid);

  // if a physical file, we can rename it from the temporary name to the correct
  // name
  if (!isCompactor) {
    if (journal->isPhysical(journal)) {
      // and use the correct name
      char* number = TRI_StringUInt64(journal->_fid);
      char* jname = TRI_Concatenate3String("journal-", number, ".db");
      char* filename = TRI_Concatenate2File(document->_directory, jname);

      TRI_FreeString(TRI_CORE_MEM_ZONE, number);
      TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

      bool ok = TRI_RenameDatafile(journal, filename);

      if (!ok) {
        LOG(ERR) << "failed to rename journal '" << journal->getName(journal) << "' to '" << filename << "': " << TRI_last_error();

        TRI_CloseDatafile(journal);
        TRI_UnlinkFile(journal->getName(journal));
        TRI_FreeDatafile(journal);
        TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

        EnsureErrorCode(document->_lastError);

        return nullptr;
      } else {
        LOG(TRACE) << "renamed journal from '" << journal->getName(journal) << "' to '" << filename << "'";
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    }

    TRI_PushBackVectorPointer(&document->_journals, journal);
  }

  return journal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all documents in the collection, using a user-defined
/// callback function. Returns the total number of documents in the collection
///
/// The user can abort the iteration by return "false" from the callback
/// function.
///
/// Note: the function will not acquire any locks. It is the task of the caller
/// to ensure the collection is properly locked
////////////////////////////////////////////////////////////////////////////////

size_t TRI_DocumentIteratorDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document, void* data,
    bool (*callback)(TRI_doc_mptr_t const*, TRI_document_collection_t*,
                     void*)) {
  // The first argument is only used to make the compiler prove that a
  // transaction is ongoing. We need this to prove that accesses to
  // master pointers and their data pointers in the callback are
  // protected.

  auto idx = document->primaryIndex();
  size_t const nrUsed = idx->size();

  if (nrUsed > 0) {
    arangodb::basics::BucketPosition position;
    uint64_t total = 0;

    while (true) {
      TRI_doc_mptr_t const* mptr = idx->lookupSequential(trx, position, total);

      if (mptr == nullptr || !callback(mptr, document, data)) {
        break;
      }
    }
  }

  return nrUsed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index, based on a VelocyPack description
////////////////////////////////////////////////////////////////////////////////

int TRI_FromVelocyPackIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    VPackSlice const& slice, arangodb::Index** idx) {
  TRI_ASSERT(slice.isObject());

  if (idx != nullptr) {
    *idx = nullptr;
  }

  // extract the type
  VPackSlice type = slice.get("type");

  if (!type.isString()) {
    return TRI_ERROR_INTERNAL;
  }
  std::string typeStr = type.copyString();

  // extract the index identifier
  VPackSlice iis = slice.get("id");

  TRI_idx_iid_t iid;
  if (iis.isNumber()) {
    iid = iis.getNumericValue<TRI_idx_iid_t>();
  } else if (iis.isString()) {
    std::string tmp = iis.copyString();
    iid = static_cast<TRI_idx_iid_t>(TRI_UInt64String(tmp.c_str()));
  } else {
    LOG(ERR) << "ignoring index, index identifier could not be located";

    return TRI_ERROR_INTERNAL;
  }

  TRI_UpdateTickServer(iid);

  // ...........................................................................
  // CAP CONSTRAINT
  // ...........................................................................
  if (typeStr == "cap") {
    return CapConstraintFromVelocyPack(trx, document, slice, iid, idx);
  }

  // ...........................................................................
  // GEO INDEX (list or attribute)
  // ...........................................................................
  if (typeStr == "geo1" || typeStr == "geo2") {
    return GeoIndexFromVelocyPack(trx, document, slice, iid, idx);
  }

  // ...........................................................................
  // HASH INDEX
  // ...........................................................................
  if (typeStr == "hash") {
    return HashIndexFromVelocyPack(trx, document, slice, iid, idx);
  }

  // ...........................................................................
  // SKIPLIST INDEX
  // ...........................................................................
  if (typeStr == "skiplist") {
    return SkiplistIndexFromVelocyPack(trx, document, slice, iid, idx);
  }

  // ...........................................................................
  // FULLTEXT INDEX
  // ...........................................................................
  if (typeStr == "fulltext") {
    return FulltextIndexFromVelocyPack(trx, document, slice, iid, idx);
  }

  // ...........................................................................
  // EDGES INDEX
  // ...........................................................................
  if (typeStr == "edge") {
    // we should never get here, as users cannot create their own edge indexes
    LOG(ERR) << "logic error. there should never be a JSON file describing an edges index";
    return TRI_ERROR_INTERNAL;
  }

  // default:
  LOG(WARN) << "index type '" << typeStr.c_str() << "' is not supported in this version of ArangoDB and is ignored";

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RollbackOperationDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    TRI_voc_document_operation_e type, TRI_doc_mptr_t* header,
    TRI_doc_mptr_copy_t const* oldData) {
  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    // ignore any errors we're getting from this
    DeletePrimaryIndex(trx, document, header, true);
    DeleteSecondaryIndexes(trx, document, header, true);

    TRI_ASSERT(document->_numberDocuments > 0);
    document->_numberDocuments--;

    return TRI_ERROR_NO_ERROR;
  } else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    // copy the existing header's state
    TRI_doc_mptr_copy_t copy = *header;

    // remove the current values from the indexes
    DeleteSecondaryIndexes(trx, document, header, true);
    // revert to the old state
    header->copy(*oldData);
    // re-insert old state
    int res = InsertSecondaryIndexes(trx, document, header, true);
    // revert again to the new state, because other parts of the new state
    // will be reverted at some other place
    header->copy(copy);

    return res;
  } else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    int res = InsertPrimaryIndex(trx, document, header, true);

    if (res == TRI_ERROR_NO_ERROR) {
      res = InsertSecondaryIndexes(trx, document, header, true);
      document->_numberDocuments++;
    } else {
      LOG(ERR) << "error rolling back remove operation";
    }
    return res;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing datafile
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseDatafileDocumentCollection(TRI_document_collection_t* document,
                                         size_t position, bool isCompactor) {
  TRI_vector_pointer_t* vector;

  // either use a journal or a compactor
  if (isCompactor) {
    vector = &document->_compactors;
  } else {
    vector = &document->_journals;
  }

  // no journal at this position
  if (vector->_length <= position) {
    TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    return false;
  }

  // seal and rename datafile
  TRI_datafile_t* journal =
      static_cast<TRI_datafile_t*>(vector->_buffer[position]);
  int res = TRI_SealDatafile(journal);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to seal datafile '" << journal->getName(journal) << "': " << TRI_last_error();

    if (!isCompactor) {
      TRI_RemoveVectorPointer(vector, position);
      TRI_PushBackVectorPointer(&document->_datafiles, journal);
    }

    return false;
  }

  if (!isCompactor && journal->isPhysical(journal)) {
    // rename the file
    char* number = TRI_StringUInt64(journal->_fid);
    char* dname = TRI_Concatenate3String("datafile-", number, ".db");
    char* filename = TRI_Concatenate2File(document->_directory, dname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, number);

    bool ok = TRI_RenameDatafile(journal, filename);

    if (!ok) {
      LOG(ERR) << "failed to rename datafile '" << journal->getName(journal) << "' to '" << filename << "': " << TRI_last_error();

      TRI_RemoveVectorPointer(vector, position);
      TRI_PushBackVectorPointer(&document->_datafiles, journal);
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      return false;
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    LOG(TRACE) << "closed file '" << journal->getName(journal) << "'";
  }

  if (!isCompactor) {
    TRI_RemoveVectorPointer(vector, position);
    TRI_PushBackVectorPointer(&document->_datafiles, journal);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper struct for filling indexes
////////////////////////////////////////////////////////////////////////////////

class IndexFiller {
 public:
  IndexFiller(arangodb::Transaction* trx, TRI_document_collection_t* document,
              arangodb::Index* idx, std::function<void(int)> callback)
      : _trx(trx), _document(document), _idx(idx), _callback(callback) {}

  void operator()() {
    int res = TRI_ERROR_INTERNAL;

    try {
      res = FillIndex(_trx, _document, _idx);
    } catch (...) {
    }

    _callback(res);
  }

 private:
  arangodb::Transaction* _trx;
  TRI_document_collection_t* _document;
  arangodb::Index* _idx;
  std::function<void(int)> _callback;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief fill the additional (non-primary) indexes
////////////////////////////////////////////////////////////////////////////////

int TRI_FillIndexesDocumentCollection(arangodb::Transaction* trx,
                                      TRI_vocbase_col_t* collection,
                                      TRI_document_collection_t* document) {
  auto old = document->useSecondaryIndexes();

  // turn filling of secondary indexes off. we're now only interested in getting
  // the indexes' definition. we'll fill them below ourselves.
  document->useSecondaryIndexes(false);

  try {
    OpenIndexIteratorContext ctx;
    ctx.trx = trx;
    ctx.collection = document;

    TRI_IterateIndexCollection(reinterpret_cast<TRI_collection_t*>(document),
                               OpenIndexIterator, static_cast<void*>(&ctx));
    document->useSecondaryIndexes(old);
  } catch (...) {
    document->useSecondaryIndexes(old);
    return TRI_ERROR_INTERNAL;
  }

  // distribute the work to index threads plus this thread
  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  if (n == 1) {
    return TRI_ERROR_NO_ERROR;
  }

  double start = TRI_microtime();

  // only log performance infos for indexes with more than this number of
  // entries
  static size_t const NotificationSizeThreshold = 131072;
  auto primaryIndex = document->primaryIndex();

  if (primaryIndex->size() > NotificationSizeThreshold) {
    LOG_TOPIC(TRACE, Logger::PERFORMANCE) <<
        "fill-indexes-document-collection { collection: " << document->_vocbase->_name << "/" << document->_info.name() << " }, indexes: " << (n - 1);
  }

  TRI_ASSERT(n > 1);

  std::atomic<int> result(TRI_ERROR_NO_ERROR);

  {
    arangodb::basics::Barrier barrier(n - 1);

    auto indexPool = document->_vocbase->_server->_indexPool;

    auto callback = [&barrier, &result](int res) -> void {
      // update the error code
      if (res != TRI_ERROR_NO_ERROR) {
        int expected = TRI_ERROR_NO_ERROR;
        result.compare_exchange_strong(expected, res,
                                       std::memory_order_acquire);
      }

      barrier.join();
    };

    // now actually fill the secondary indexes
    for (size_t i = 1; i < n; ++i) {
      auto idx = indexes[i];

      // index threads must come first, otherwise this thread will block the
      // loop and
      // prevent distribution to threads
      if (indexPool != nullptr && i != (n - 1)) {
        try {
          // move task into thread pool
          IndexFiller indexTask(trx, document, idx, callback);

          static_cast<arangodb::basics::ThreadPool*>(indexPool)
              ->enqueue(indexTask);
        } catch (...) {
          // set error code
          int expected = TRI_ERROR_NO_ERROR;
          result.compare_exchange_strong(expected, TRI_ERROR_INTERNAL,
                                         std::memory_order_acquire);

          barrier.join();
        }
      } else {
        // fill index in this thread
        int res;

        try {
          res = FillIndex(trx, document, idx);
        } catch (...) {
          res = TRI_ERROR_INTERNAL;
        }

        if (res != TRI_ERROR_NO_ERROR) {
          int expected = TRI_ERROR_NO_ERROR;
          result.compare_exchange_strong(expected, res,
                                         std::memory_order_acquire);
        }

        barrier.join();
      }
    }

    // barrier waits here until all threads have joined
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::DURATION(TRI_microtime() - start) << " s, fill-indexes-document-collection { collection: " << document->_vocbase->_name << "/" << document->_info.name() << " }, indexes: " << (n - 1);

  return result.load();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection(TRI_vocbase_t* vocbase,
                                                      TRI_vocbase_col_t* col,
                                                      bool ignoreErrors) {
  char const* path = col->pathc_str();

  // first open the document collection
  TRI_document_collection_t* document = nullptr;
  try {
    document = new TRI_document_collection_t();
  } catch (std::exception&) {
  }

  if (document == nullptr) {
    return nullptr;
  }

  TRI_ASSERT(document != nullptr);

  double start = TRI_microtime();
  LOG_TOPIC(TRACE, Logger::PERFORMANCE) <<
      "open-document-collection { collection: " << vocbase->_name << "/" << col->name() << " }";

  TRI_collection_t* collection =
      TRI_OpenCollection(vocbase, document, path, ignoreErrors);

  if (collection == nullptr) {
    delete document;
    LOG(ERR) << "cannot open document collection from path '" << path << "'";

    return nullptr;
  }

  auto shaper = new VocShaper(TRI_UNKNOWN_MEM_ZONE, document);

  // create document collection and shaper
  if (false == InitDocumentCollection(document, shaper)) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    LOG(ERR) << "cannot initialize document collection";

    return nullptr;
  }

  // check if we can generate the key generator
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> buffer =
      collection->_info.keyOptions();

  VPackSlice slice;
  if (buffer.get() != nullptr) {
    slice = VPackSlice(buffer->data());
  }

  KeyGenerator* keyGenerator = KeyGenerator::factory(slice);

  if (keyGenerator == nullptr) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    TRI_set_errno(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);

    return nullptr;
  }

  document->_keyGenerator = keyGenerator;

  arangodb::SingleCollectionWriteTransaction<UINT64_MAX> trx(
      new arangodb::StandaloneTransactionContext(), vocbase,
      document->_info.id());

  // build the primary index
  {
    double start = TRI_microtime();

    LOG_TOPIC(TRACE, Logger::PERFORMANCE) <<
        "iterate-markers { collection: " << vocbase->_name << "/" << document->_info.name() << " }";

    // iterate over all markers of the collection
    int res = IterateMarkersCollection(&trx, collection);

    LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::DURATION(TRI_microtime() - start) << " s, iterate-markers { collection: " << vocbase->_name << "/" << document->_info.name() << " }";

    if (res != TRI_ERROR_NO_ERROR) {
      if (document->_failedTransactions != nullptr) {
        delete document->_failedTransactions;
      }
      TRI_CloseCollection(collection);
      TRI_FreeCollection(collection);

      LOG(ERR) << "cannot iterate data of document collection";
      TRI_set_errno(res);

      return nullptr;
    }
  }

  TRI_ASSERT(document->getShaper() !=
             nullptr);  // ONLY in OPENCOLLECTION, PROTECTED by fake trx here

  if (!arangodb::wal::LogfileManager::instance()->isInRecovery()) {
    TRI_FillIndexesDocumentCollection(&trx, col, document);
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::DURATION(TRI_microtime() - start) << " s, open-document-collection { collection: " << vocbase->_name << "/" << document->_info.name() << " }";

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection(TRI_document_collection_t* document,
                                bool updateStats) {
  auto primaryIndex = document->primaryIndex();
  auto idxSize = primaryIndex->size();

  if (!document->_info.deleted() &&
      document->_info.initialCount() != static_cast<int64_t>(idxSize)) {
    document->_info.updateCount(idxSize);

    bool doSync = document->_vocbase->_settings.forceSyncProperties;
    // Ignore the error?
    document->_info.saveToFile(document->_directory, doSync);
  }

  // closes all open compactors, journals, datafiles
  int res = TRI_CloseCollection(document);

  delete document
      ->getShaper();  // ONLY IN CLOSECOLLECTION, PROTECTED by fake trx here
  document->setShaper(nullptr);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pid name structure
////////////////////////////////////////////////////////////////////////////////

typedef struct pid_name_s {
  TRI_shape_pid_t _pid;
  char* _name;
} pid_name_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief converts extracts a field list from a VelocyPack object
///        Does not copy any data, caller has to make sure that data
///        in slice stays valid until this return value is destroyed.
////////////////////////////////////////////////////////////////////////////////

static VPackSlice ExtractFields(VPackSlice const& slice, TRI_idx_iid_t iid) {
  VPackSlice fld = slice.get("fields");
  if (!fld.isArray()) {
    LOG(ERR) << "ignoring index " << iid << ", 'fields' must be an array";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  for (auto const& sub : VPackArrayIterator(fld)) {
    if (!sub.isString()) {
      LOG(ERR) << "ignoring index " << iid << ", 'fields' must be an array of attribute paths";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
  }
  return fld;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fill an index in batches
////////////////////////////////////////////////////////////////////////////////

static int FillIndexBatch(arangodb::Transaction* trx,
                          TRI_document_collection_t* document,
                          arangodb::Index* idx) {
  auto indexPool = document->_vocbase->_server->_indexPool;
  TRI_ASSERT(indexPool != nullptr);

  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE) <<
      "fill-index-batch { collection: " << document->_vocbase->_name << "/" << document->_info.name() << " }, " << 
      idx->context() << ", threads: " << indexPool->numThreads() << ", buckets: " << document->_info.indexBuckets();

  // give the index a size hint
  auto primaryIndex = document->primaryIndex();

  auto nrUsed = primaryIndex->size();

  idx->sizeHint(trx, nrUsed);

  // process documents a million at a time
  size_t blockSize = 1024 * 1024;

  if (nrUsed < blockSize) {
    blockSize = nrUsed;
  }
  if (blockSize == 0) {
    blockSize = 1;
  }

  int res = TRI_ERROR_NO_ERROR;

  std::vector<TRI_doc_mptr_t const*> documents;
  documents.reserve(blockSize);

  if (nrUsed > 0) {
    arangodb::basics::BucketPosition position;
    uint64_t total = 0;
    while (true) {
      TRI_doc_mptr_t const* mptr =
          primaryIndex->lookupSequential(trx, position, total);

      if (mptr == nullptr) {
        break;
      }

      documents.emplace_back(mptr);

      if (documents.size() == blockSize) {
        res = idx->batchInsert(trx, &documents, indexPool->numThreads());
        documents.clear();

        // some error occurred
        if (res != TRI_ERROR_NO_ERROR) {
          break;
        }
      }
    }
  }

  // process the remainder of the documents
  if (res == TRI_ERROR_NO_ERROR && !documents.empty()) {
    res = idx->batchInsert(trx, &documents, indexPool->numThreads());
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::DURATION(TRI_microtime() - start) << " s, fill-index-batch { collection: " << document->_vocbase->_name << "/" << document->_info.name() << " }, " << idx->context() << ", threads: " << indexPool->numThreads() << ", buckets: " << document->_info.indexBuckets();

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fill an index sequentially
////////////////////////////////////////////////////////////////////////////////

static int FillIndexSequential(arangodb::Transaction* trx,
                               TRI_document_collection_t* document,
                               arangodb::Index* idx) {
  double start = TRI_microtime();

  LOG_TOPIC(TRACE, Logger::PERFORMANCE) << 
      "fill-index-sequential { collection: " << document->_vocbase->_name << "/" << document->_info.name() << " }, " <<
      idx->context() << ", buckets: " << document->_info.indexBuckets();

  // give the index a size hint
  auto primaryIndex = document->primaryIndex();
  size_t nrUsed = primaryIndex->size();

  idx->sizeHint(trx, nrUsed);

  if (nrUsed > 0) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    static int const LoopSize = 10000;
    int counter = 0;
    int loops = 0;
#endif

    arangodb::basics::BucketPosition position;
    uint64_t total = 0;

    while (true) {
      TRI_doc_mptr_t const* mptr =
          primaryIndex->lookupSequential(trx, position, total);

      if (mptr == nullptr) {
        break;
      }

      int res = idx->insert(trx, mptr, false);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
#ifdef TRI_ENABLE_MAINTAINER_MODE
      if (++counter == LoopSize) {
        counter = 0;
        ++loops;
        LOG(TRACE) << "indexed " << (LoopSize * loops) << " documents of collection " << document->_info.id();
      }
#endif
    }
  }

  LOG_TOPIC(TRACE, Logger::PERFORMANCE) << "[timer] " << Logger::DURATION(TRI_microtime() - start) << " s, fill-index-sequential { collection: " << document->_vocbase->_name << "/" << document->_info.name() << " }, " << idx->context() << ", buckets: " << document->_info.indexBuckets();

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes an index with all existing documents
////////////////////////////////////////////////////////////////////////////////

static int FillIndex(arangodb::Transaction* trx,
                     TRI_document_collection_t* document,
                     arangodb::Index* idx) {
  if (!document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  try {
    size_t nrUsed = document->primaryIndex()->size();
    auto indexPool = document->_vocbase->_server->_indexPool;

    int res;

    if (indexPool != nullptr && idx->hasBatchInsert() && nrUsed > 256 * 1024 &&
        document->_info.indexBuckets() > 1) {
      // use batch insert if there is an index pool,
      // the collection has more than one index bucket
      // and it contains a significant amount of documents
      res = FillIndexBatch(trx, document, idx);
    } else {
      res = FillIndexSequential(trx, document, idx);
    }

    return res;
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (std::bad_alloc&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a path based, unique or non-unique index
////////////////////////////////////////////////////////////////////////////////

static arangodb::Index* LookupPathIndexDocumentCollection(
    TRI_document_collection_t* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& paths,
    arangodb::Index::IndexType type, int sparsity, bool unique,
    bool allowAnyAttributeOrder) {
  for (auto const& idx : collection->allIndexes()) {
    if (idx->type() != type) {
      continue;
    }

    // .........................................................................
    // Now perform checks which are specific to the type of index
    // .........................................................................

    switch (idx->type()) {
      case arangodb::Index::TRI_IDX_TYPE_HASH_INDEX: {
        auto hashIndex = static_cast<arangodb::HashIndex*>(idx);

        if (unique != hashIndex->unique() ||
            (sparsity != -1 && sparsity != (hashIndex->sparse() ? 1 : 0))) {
          continue;
        }
        break;
      }

      case arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX: {
        auto skiplistIndex = static_cast<arangodb::SkiplistIndex*>(idx);

        if (unique != skiplistIndex->unique() ||
            (sparsity != -1 && sparsity != (skiplistIndex->sparse() ? 1 : 0))) {
          continue;
        }
        break;
      }

      default: { continue; }
    }

    // .........................................................................
    // check that the number of paths (fields) in the index matches that
    // of the number of attributes
    // .........................................................................

    auto const& idxFields = idx->fields();
    size_t const n = idxFields.size();

    if (n != paths.size()) {
      continue;
    }

    // .........................................................................
    // go through all the attributes and see if they match
    // .........................................................................

    bool found = true;

    if (allowAnyAttributeOrder) {
      // any permutation of attributes is allowed
      for (size_t i = 0; i < n; ++i) {
        found = false;
        size_t fieldSize = idxFields[i].size();

        for (size_t j = 0; j < n; ++j) {
          if (fieldSize == paths[j].size()) {
            bool allEqual = true;
            for (size_t k = 0; k < fieldSize; ++k) {
              if (idxFields[j][k] != paths[j][k]) {
                allEqual = false;
                break;
              }
            }
            if (allEqual) {
              found = true;
              break;
            }
          }
        }

        if (!found) {
          break;
        }
      }
    } else {
      // attributes need to be present in a given order
      for (size_t i = 0; i < n; ++i) {
        size_t fieldSize = idxFields[i].size();
        if (fieldSize == paths[i].size()) {
          for (size_t k = 0; k < fieldSize; ++k) {
            if (idxFields[i][k] != paths[i][k]) {
              found = false;
              break;
            }
          }
          if (!found) {
            break;
          }
        } else {
          found = false;
          break;
        }
      }
    }

    // stop if we found a match
    if (found) {
      return idx;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores a path based index (template)
////////////////////////////////////////////////////////////////////////////////

static int PathBasedIndexFromVelocyPack(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    VPackSlice const& definition, TRI_idx_iid_t iid,
    arangodb::Index* (*creator)(arangodb::Transaction*,
                                TRI_document_collection_t*,
                                std::vector<std::string> const&, TRI_idx_iid_t,
                                bool, bool, bool&),
    arangodb::Index** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  // extract fields
  VPackSlice fld;
  try {
    fld = ExtractFields(definition, iid);
  } catch (arangodb::basics::Exception const& e) {
    return TRI_set_errno(e.code());
  }
  VPackValueLength fieldCount = fld.length();

  // extract the list of fields
  if (fieldCount < 1) {
    LOG(ERR) << "ignoring index " << iid << ", need at least one attribute path";

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  // determine if the index is unique or non-unique
  VPackSlice bv = definition.get("unique");

  if (!bv.isBoolean()) {
    LOG(ERR) << "ignoring index " << iid << ", could not determine if unique or non-unique";
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  bool unique = bv.getBoolean();

  // determine sparsity
  bool sparse = false;

  bv = definition.get("sparse");

  if (bv.isBoolean()) {
    sparse = bv.getBoolean();
  } else {
    // no sparsity information given for index
    // now use pre-2.5 defaults: unique hash indexes were sparse, all other
    // indexes were non-sparse
    bool isHashIndex = false;
    VPackSlice typeSlice = definition.get("type");
    if (typeSlice.isString()) {
      isHashIndex = typeSlice.copyString() == "hash";
    }

    if (isHashIndex && unique) {
      sparse = true;
    }
  }

  // Initialize the vector in which we store the fields on which the hashing
  // will be based.
  std::vector<std::string> attributes;
  attributes.reserve(fieldCount);

  // find fields
  for (auto const& fieldStr : VPackArrayIterator(fld)) {
    attributes.emplace_back(fieldStr.copyString());
  }

  // create the index
  bool created;
  auto idx = creator(trx, document, attributes, iid, sparse, unique, created);

  if (dst != nullptr) {
    *dst = idx;
  }

  if (idx == nullptr) {
    LOG(ERR) << "cannot create index " << iid << " in collection '" << document->_info.namec_str() << "'";
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update statistics for a collection
/// note: the write-lock for the collection must be held to call this
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateRevisionDocumentCollection(TRI_document_collection_t* document,
                                          TRI_voc_rid_t rid, bool force) {
  if (rid > 0) {
    SetRevision(document, rid, force);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection is fully collected
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFullyCollectedDocumentCollection(
    TRI_document_collection_t* document) {
  READ_LOCKER(readLocker, document->_lock);

  int64_t uncollected = document->_uncollectedLogfileEntries.load();

  return (uncollected == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveIndex(TRI_document_collection_t* document, arangodb::Index* idx,
                  bool writeMarker) {
  // convert into JSON
  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = idx->toVelocyPack(false);
  } catch (...) {
    LOG(ERR) << "cannot save index definition.";
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  if (builder == nullptr) {
    LOG(ERR) << "cannot save index definition.";
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  // construct filename
  char* number = TRI_StringUInt64(idx->id());
  char* name = TRI_Concatenate3String("index-", number, ".json");
  char* filename = TRI_Concatenate2File(document->_directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, name);
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  TRI_vocbase_t* vocbase = document->_vocbase;

  VPackSlice const idxSlice = builder->slice();
  // and save
  bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
      filename, idxSlice, document->_vocbase->_settings.forceSyncProperties);

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (!ok) {
    LOG(ERR) << "cannot save index definition: " << TRI_last_error();

    return TRI_errno();
  }

  if (!writeMarker) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::CreateIndexMarker marker(vocbase->_id, document->_info.id(),
                                            idx->id(), idxSlice.toJson());
    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                    false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // TODO: what to do here?
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of all indexes
///
/// the caller must have read-locked the underlying collection!
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<VPackBuilder>> TRI_IndexesDocumentCollection(
    TRI_document_collection_t* document, bool withFigures) {
  auto const& indexes = document->allIndexes();

  std::vector<std::shared_ptr<VPackBuilder>> result;
  result.reserve(indexes.size());

  for (auto const& idx : indexes) {
    auto builder = idx->toVelocyPack(withFigures);

    // shouldn't fail because of reserve
    result.emplace_back(builder);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, including index file removal and replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexDocumentCollection(TRI_document_collection_t* document,
                                     TRI_idx_iid_t iid, bool writeMarker) {
  if (iid == 0) {
    // invalid index id or primary index
    return true;
  }

  TRI_vocbase_t* vocbase = document->_vocbase;
  arangodb::Index* found = nullptr;
  {
    READ_LOCKER(readLocker, document->_vocbase->_inventoryLock);

    WRITE_LOCKER(writeLocker, document->_lock);

    arangodb::aql::QueryCache::instance()->invalidate(
        vocbase, document->_info.namec_str());
    found = document->removeIndex(iid);
  }

  if (found != nullptr) {
    bool result = RemoveIndexFile(document, found->id());

    delete found;
    found = nullptr;

    if (writeMarker) {
      int res = TRI_ERROR_NO_ERROR;

      try {
        arangodb::wal::DropIndexMarker marker(vocbase->_id,
                                              document->_info.id(), iid);
        arangodb::wal::SlotInfoCopy slotInfo =
            arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                        false);

        if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
        }

        return true;
      } catch (arangodb::basics::Exception const& ex) {
        res = ex.code();
      } catch (...) {
        res = TRI_ERROR_INTERNAL;
      }

      LOG(WARN) << "could not save index drop marker in log: " << TRI_errno_string(res);
    }

    // TODO: what to do here?
    return result;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts attribute names to lists of pids and names
///
/// In case of an error, all allocated memory in pids and names will be
/// freed.
////////////////////////////////////////////////////////////////////////////////

static int PidNamesByAttributeNames(
    std::vector<std::string> const& attributes, VocShaper* shaper,
    std::vector<TRI_shape_pid_t>& pids,
    std::vector<std::vector<arangodb::basics::AttributeName>>& names,
    bool sorted, bool create) {
  pids.reserve(attributes.size());
  names.reserve(attributes.size());

  // .............................................................................
  // sorted case (hash index)
  // .............................................................................

  if (sorted) {
    // combine name and pid
    typedef std::pair<std::vector<arangodb::basics::AttributeName>,
                      TRI_shape_pid_t> PidNameType;
    std::vector<PidNameType> pidNames;
    pidNames.reserve(attributes.size());

    for (auto const& name : attributes) {
      std::vector<arangodb::basics::AttributeName> attrNameList;
      TRI_ParseAttributeString(name, attrNameList);
      TRI_ASSERT(!attrNameList.empty());
      std::vector<std::string> joinedNames;
      TRI_AttributeNamesJoinNested(attrNameList, joinedNames, true);
      // We only need the first pid here
      std::string pidPath = joinedNames[0];

      TRI_shape_pid_t pid;

      if (create) {
        pid = shaper->findOrCreateAttributePathByName(pidPath.c_str());
      } else {
        pid = shaper->lookupAttributePathByName(pidPath.c_str());
      }

      if (pid == 0) {
        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }

      pidNames.emplace_back(std::make_pair(attrNameList, pid));
    }

    // sort according to pid
    std::sort(pidNames.begin(), pidNames.end(),
              [](PidNameType const& l, PidNameType const& r)
                  -> bool { return l.second < r.second; });

    for (auto const& it : pidNames) {
      pids.emplace_back(it.second);
      names.emplace_back(it.first);
    }
  }

  // .............................................................................
  // unsorted case (skiplist index)
  // .............................................................................

  else {
    for (auto const& name : attributes) {
      std::vector<arangodb::basics::AttributeName> attrNameList;
      TRI_ParseAttributeString(name, attrNameList);
      TRI_ASSERT(!attrNameList.empty());
      std::vector<std::string> joinedNames;
      TRI_AttributeNamesJoinNested(attrNameList, joinedNames, true);
      // We only need the first pid here
      std::string pidPath = joinedNames[0];

      TRI_shape_pid_t pid;

      if (create) {
        pid = shaper->findOrCreateAttributePathByName(pidPath.c_str());
      } else {
        pid = shaper->lookupAttributePathByName(pidPath.c_str());
      }

      if (pid == 0) {
        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }

      pids.emplace_back(pid);
      names.emplace_back(attrNameList);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a cap constraint to a collection
////////////////////////////////////////////////////////////////////////////////

static arangodb::Index* CreateCapConstraintDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    size_t count, int64_t size, TRI_idx_iid_t iid, bool& created) {
  created = false;

  // check if we already know a cap constraint
  auto existing = document->capConstraint();

  if (existing != nullptr) {
    if (static_cast<size_t>(existing->count()) == count &&
        existing->size() == size) {
      return static_cast<arangodb::Index*>(existing);
    }

    TRI_set_errno(TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED);
    return nullptr;
  }

  if (iid == 0) {
    iid = arangodb::Index::generateId();
  }

  // create a new index
  auto cc = new arangodb::CapConstraint(iid, document, count, size);
  std::unique_ptr<arangodb::Index> capConstraint(cc);

  cc->initialize(trx);
  arangodb::Index* idx = static_cast<arangodb::Index*>(capConstraint.get());

  // initializes the index with all existing documents
  int res = FillIndex(trx, document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // and store index
  try {
    document->addIndex(idx);
    capConstraint.release();
  } catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  created = true;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int CapConstraintFromVelocyPack(arangodb::Transaction* trx,
                                       TRI_document_collection_t* document,
                                       VPackSlice const& definition,
                                       TRI_idx_iid_t iid,
                                       arangodb::Index** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  VPackSlice val1 = definition.get("size");
  VPackSlice val2 = definition.get("byteSize");

  if (!val1.isNumber() && !val2.isNumber()) {
    LOG(ERR) << "ignoring cap constraint " << iid << ", 'size' and 'byteSize' missing";

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  size_t count = 0;
  if (val1.isNumber()) {
    if (val1.isDouble()) {
      double tmp = val1.getDouble();
      if (tmp > 0.0) {
        count = static_cast<size_t>(tmp);
      }
    } else {
      count = val1.getNumericValue<size_t>();
    }
  }

  int64_t size = 0;
  if (val2.isNumber()) {
    if (val2.isDouble()) {
      double tmp = val2.getDouble();
      if (tmp > arangodb::CapConstraint::MinSize) {
        size = static_cast<int64_t>(tmp);
      }
    } else {
      int64_t tmp = val2.getNumericValue<int64_t>();
      if (tmp > arangodb::CapConstraint::MinSize) {
        size = static_cast<int64_t>(tmp);
      }
    }
  }

  if (count == 0 && size == 0) {
    LOG(ERR) << "ignoring cap constraint " << iid << ", 'size' must be at least 1, or 'byteSize' must be at least " << arangodb::CapConstraint::MinSize;

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  bool created;
  auto idx = CreateCapConstraintDocumentCollection(trx, document, count, size,
                                                   iid, created);

  if (dst != nullptr) {
    *dst = idx;
  }

  return idx == nullptr ? TRI_errno() : TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a cap constraint
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupCapConstraintDocumentCollection(
    TRI_document_collection_t* document) {
  return static_cast<arangodb::Index*>(document->capConstraint());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureCapConstraintDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    TRI_idx_iid_t iid, size_t count, int64_t size, bool& created) {
  READ_LOCKER(readLocker, document->_vocbase->_inventoryLock);

  WRITE_LOCKER(writeLocker, document->_lock);

  auto idx = CreateCapConstraintDocumentCollection(trx, document, count, size,
                                                   iid, created);

  if (idx != nullptr) {
    if (created) {
      arangodb::aql::QueryCache::instance()->invalidate(
          document->_vocbase, document->_info.namec_str());
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        delete idx;
        idx = nullptr;
      }
    }
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a geo index to a collection
////////////////////////////////////////////////////////////////////////////////

static arangodb::Index* CreateGeoIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    std::string const& location, std::string const& latitude,
    std::string const& longitude, bool geoJson, TRI_idx_iid_t iid,
    bool& created) {
  TRI_shape_pid_t lat = 0;
  TRI_shape_pid_t lon = 0;
  TRI_shape_pid_t loc = 0;

  created = false;
  auto shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (!location.empty()) {
    loc = shaper->findOrCreateAttributePathByName(location.c_str());

    if (loc == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  if (!latitude.empty()) {
    lat = shaper->findOrCreateAttributePathByName(latitude.c_str());

    if (lat == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  if (!longitude.empty()) {
    lon = shaper->findOrCreateAttributePathByName(longitude.c_str());

    if (lon == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  // check, if we know the index
  arangodb::Index* idx = nullptr;

  if (!location.empty()) {
    idx = TRI_LookupGeoIndex1DocumentCollection(document, location, geoJson);
  } else if (!longitude.empty() && !latitude.empty()) {
    idx = TRI_LookupGeoIndex2DocumentCollection(document, latitude, longitude);
  } else {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG(TRACE) << "expecting either 'location' or 'latitude' and 'longitude'";
    return nullptr;
  }

  if (idx != nullptr) {
    LOG(TRACE) << "geo-index already created for location '" << location.c_str() << "'";

    created = false;

    return idx;
  }

  if (iid == 0) {
    iid = arangodb::Index::generateId();
  }

  std::unique_ptr<arangodb::GeoIndex2> geoIndex;

  // create a new index
  if (!location.empty()) {
    geoIndex.reset(new arangodb::GeoIndex2(
        iid, document,
        std::vector<std::vector<arangodb::basics::AttributeName>>{
            {{location, false}}},
        std::vector<TRI_shape_pid_t>{loc}, geoJson));

    LOG(TRACE) << "created geo-index for location '" << location.c_str() << "': " << loc;
  } else if (!longitude.empty() && !latitude.empty()) {
    geoIndex.reset(new arangodb::GeoIndex2(
        iid, document,
        std::vector<std::vector<arangodb::basics::AttributeName>>{
            {{latitude, false}}, {{longitude, false}}},
        std::vector<TRI_shape_pid_t>{lat, lon}));

    LOG(TRACE) << "created geo-index for location '" << location.c_str() << "': " << lat << ", " << lon;
  }

  idx = static_cast<arangodb::GeoIndex2*>(geoIndex.get());

  if (idx == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // initializes the index with all existing documents
  int res = FillIndex(trx, document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // and store index
  try {
    document->addIndex(idx);
    geoIndex.release();
  } catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  created = true;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int GeoIndexFromVelocyPack(arangodb::Transaction* trx,
                                  TRI_document_collection_t* document,
                                  VPackSlice const& definition,
                                  TRI_idx_iid_t iid, arangodb::Index** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  VPackSlice const type = definition.get("type");

  if (!type.isString()) {
    return TRI_ERROR_INTERNAL;
  }

  std::string typeStr = type.copyString();

  // extract fields
  VPackSlice fld;
  try {
    fld = ExtractFields(definition, iid);
  } catch (arangodb::basics::Exception const& e) {
    return TRI_set_errno(e.code());
  }
  VPackValueLength fieldCount = fld.length();

  arangodb::Index* idx = nullptr;

  // list style
  if (typeStr == "geo1") {
    // extract geo json
    bool geoJson = arangodb::basics::VelocyPackHelper::getBooleanValue(
        definition, "geoJson", false);

    // need just one field
    if (fieldCount == 1) {
      VPackSlice loc = fld.at(0);
      bool created;

      idx = CreateGeoIndexDocumentCollection(trx, document, loc.copyString(),
                                             std::string(), std::string(),
                                             geoJson, iid, created);

      if (dst != nullptr) {
        *dst = idx;
      }

      return idx == nullptr ? TRI_errno() : TRI_ERROR_NO_ERROR;
    } else {
      LOG(ERR) << "ignoring " << typeStr.c_str() << "-index " << iid << ", 'fields' must be a list with 1 entries";

      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }
  }

  // attribute style
  else if (typeStr == "geo2") {
    if (fieldCount == 2) {
      VPackSlice lat = fld.at(0);
      VPackSlice lon = fld.at(1);

      bool created;

      idx = CreateGeoIndexDocumentCollection(trx, document, std::string(),
                                             lat.copyString(), lon.copyString(),
                                             false, iid, created);

      if (dst != nullptr) {
        *dst = idx;
      }

      return idx == nullptr ? TRI_errno() : TRI_ERROR_NO_ERROR;
    } else {
      LOG(ERR) << "ignoring " << typeStr.c_str() << "-index " << iid << ", 'fields' must be a list with 2 entries";

      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }
  } else {
    TRI_ASSERT(false);
  }

  return TRI_ERROR_NO_ERROR;  // shut the vc++ up
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, list style
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupGeoIndex1DocumentCollection(
    TRI_document_collection_t* document, std::string const& location,
    bool geoJson) {
  auto shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  TRI_shape_pid_t loc = shaper->lookupAttributePathByName(location.c_str());

  if (loc == 0) {
    return nullptr;
  }

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX) {
      auto geoIndex = static_cast<arangodb::GeoIndex2*>(idx);

      if (geoIndex->isSame(loc, geoJson)) {
        return idx;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupGeoIndex2DocumentCollection(
    TRI_document_collection_t* document, std::string const& latitude,
    std::string const& longitude) {
  auto shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  TRI_shape_pid_t lat = shaper->lookupAttributePathByName(latitude.c_str());
  TRI_shape_pid_t lon = shaper->lookupAttributePathByName(longitude.c_str());

  if (lat == 0 || lon == 0) {
    return nullptr;
  }

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      auto geoIndex = static_cast<arangodb::GeoIndex2*>(idx);

      if (geoIndex->isSame(lat, lon)) {
        return idx;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureGeoIndex1DocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    TRI_idx_iid_t iid, std::string const& location, bool geoJson,
    bool& created) {
  READ_LOCKER(readLocker, document->_vocbase->_inventoryLock);

  WRITE_LOCKER(writeLocker, document->_lock);

  auto idx =
      CreateGeoIndexDocumentCollection(trx, document, location, std::string(),
                                       std::string(), geoJson, iid, created);

  if (idx != nullptr) {
    if (created) {
      arangodb::aql::QueryCache::instance()->invalidate(
          document->_vocbase, document->_info.namec_str());
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureGeoIndex2DocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    TRI_idx_iid_t iid, std::string const& latitude,
    std::string const& longitude, bool& created) {
  READ_LOCKER(readLocker, document->_vocbase->_inventoryLock);

  WRITE_LOCKER(writeLocker, document->_lock);

  auto idx = CreateGeoIndexDocumentCollection(
      trx, document, std::string(), latitude, longitude, false, iid, created);

  if (idx != nullptr) {
    if (created) {
      arangodb::aql::QueryCache::instance()->invalidate(
          document->_vocbase, document->_info.namec_str());
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a hash index to the collection
////////////////////////////////////////////////////////////////////////////////

static arangodb::Index* CreateHashIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    std::vector<std::string> const& attributes, TRI_idx_iid_t iid, bool sparse,
    bool unique, bool& created) {
  created = false;
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::vector<arangodb::basics::AttributeName>> fields;

  // determine the sorted shape ids for the attributes
  int res = PidNamesByAttributeNames(
      attributes,
      document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
      paths, fields, true, true);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  int sparsity = sparse ? 1 : 0;
  auto idx = LookupPathIndexDocumentCollection(
      document, fields, arangodb::Index::TRI_IDX_TYPE_HASH_INDEX, sparsity,
      unique, false);

  if (idx != nullptr) {
    LOG(TRACE) << "hash-index already created";

    return idx;
  }

  if (iid == 0) {
    iid = arangodb::Index::generateId();
  }

  // create the hash index. we'll provide it with the current number of
  // documents
  // in the collection so the index can do a sensible memory preallocation
  auto hashIndex = std::make_unique<arangodb::HashIndex>(iid, document, fields,
                                                         unique, sparse);
  idx = static_cast<arangodb::Index*>(hashIndex.get());

  // initializes the index with all existing documents
  res = FillIndex(trx, document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // store index and return
  try {
    document->addIndex(idx);
    hashIndex.release();
  } catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  created = true;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int HashIndexFromVelocyPack(arangodb::Transaction* trx,
                                   TRI_document_collection_t* document,
                                   VPackSlice const& definition,
                                   TRI_idx_iid_t iid, arangodb::Index** dst) {
  return PathBasedIndexFromVelocyPack(trx, document, definition, iid,
                                      CreateHashIndexDocumentCollection, dst);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupHashIndexDocumentCollection(
    TRI_document_collection_t* document,
    std::vector<std::string> const& attributes, int sparsity, bool unique) {
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::vector<arangodb::basics::AttributeName>> fields;

  // determine the sorted shape ids for the attributes
  int res = PidNamesByAttributeNames(
      attributes,
      document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
      paths, fields, true, false);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  return LookupPathIndexDocumentCollection(
      document, fields, arangodb::Index::TRI_IDX_TYPE_HASH_INDEX, sparsity,
      unique, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureHashIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    TRI_idx_iid_t iid, std::vector<std::string> const& attributes, bool sparse,
    bool unique, bool& created) {
  READ_LOCKER(readLocker, document->_vocbase->_inventoryLock);

  WRITE_LOCKER(writeLocker, document->_lock);

  auto idx = CreateHashIndexDocumentCollection(trx, document, attributes, iid,
                                               sparse, unique, created);

  if (idx != nullptr) {
    if (created) {
      arangodb::aql::QueryCache::instance()->invalidate(
          document->_vocbase, document->_info.namec_str());
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a skiplist index to the collection
////////////////////////////////////////////////////////////////////////////////

static arangodb::Index* CreateSkiplistIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    std::vector<std::string> const& attributes, TRI_idx_iid_t iid, bool sparse,
    bool unique, bool& created) {
  created = false;
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::vector<arangodb::basics::AttributeName>> fields;

  int res = PidNamesByAttributeNames(
      attributes,
      document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
      paths, fields, false, true);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  int sparsity = sparse ? 1 : 0;
  auto idx = LookupPathIndexDocumentCollection(
      document, fields, arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, sparsity,
      unique, false);

  if (idx != nullptr) {
    LOG(TRACE) << "skiplist-index already created";

    return idx;
  }

  if (iid == 0) {
    iid = arangodb::Index::generateId();
  }

  // Create the skiplist index
  auto skiplistIndex = std::make_unique<arangodb::SkiplistIndex>(
      iid, document, fields, unique, sparse);
  idx = static_cast<arangodb::Index*>(skiplistIndex.get());

  // initializes the index with all existing documents
  res = FillIndex(trx, document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // store index and return
  try {
    document->addIndex(idx);
    skiplistIndex.release();
  } catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  created = true;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int SkiplistIndexFromVelocyPack(arangodb::Transaction* trx,
                                       TRI_document_collection_t* document,
                                       VPackSlice const& definition,
                                       TRI_idx_iid_t iid,
                                       arangodb::Index** dst) {
  return PathBasedIndexFromVelocyPack(trx, document, definition, iid,
                                      CreateSkiplistIndexDocumentCollection,
                                      dst);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a skiplist index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupSkiplistIndexDocumentCollection(
    TRI_document_collection_t* document,
    std::vector<std::string> const& attributes, int sparsity, bool unique) {
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::vector<arangodb::basics::AttributeName>> fields;

  // determine the unsorted shape ids for the attributes
  int res = PidNamesByAttributeNames(
      attributes,
      document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
      paths, fields, false, false);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  return LookupPathIndexDocumentCollection(
      document, fields, arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, sparsity,
      unique, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureSkiplistIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    TRI_idx_iid_t iid, std::vector<std::string> const& attributes, bool sparse,
    bool unique, bool& created) {
  READ_LOCKER(readLocker, document->_vocbase->_inventoryLock);

  WRITE_LOCKER(writeLocker, document->_lock);

  auto idx = CreateSkiplistIndexDocumentCollection(
      trx, document, attributes, iid, sparse, unique, created);

  if (idx != nullptr) {
    if (created) {
      arangodb::aql::QueryCache::instance()->invalidate(
          document->_vocbase, document->_info.namec_str());
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  return idx;
}

static arangodb::Index* LookupFulltextIndexDocumentCollection(
    TRI_document_collection_t* document, std::string const& attribute,
    int minWordLength) {
  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      auto fulltextIndex = static_cast<arangodb::FulltextIndex*>(idx);

      if (fulltextIndex->isSame(attribute, minWordLength)) {
        return idx;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a fulltext index to the collection
////////////////////////////////////////////////////////////////////////////////

static arangodb::Index* CreateFulltextIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    std::string const& attribute, int minWordLength, TRI_idx_iid_t iid,
    bool& created) {
  created = false;

  // ...........................................................................
  // Attempt to find an existing index with the same attribute
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  auto idx =
      LookupFulltextIndexDocumentCollection(document, attribute, minWordLength);

  if (idx != nullptr) {
    LOG(TRACE) << "fulltext-index already created";

    return idx;
  }

  if (iid == 0) {
    iid = arangodb::Index::generateId();
  }

  // Create the fulltext index
  auto fulltextIndex = std::make_unique<arangodb::FulltextIndex>(
      iid, document, attribute, minWordLength);
  idx = static_cast<arangodb::Index*>(fulltextIndex.get());

  // initializes the index with all existing documents
  int res = FillIndex(trx, document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // store index and return
  try {
    document->addIndex(idx);
    fulltextIndex.release();
  } catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  created = true;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int FulltextIndexFromVelocyPack(arangodb::Transaction* trx,
                                       TRI_document_collection_t* document,
                                       VPackSlice const& definition,
                                       TRI_idx_iid_t iid,
                                       arangodb::Index** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  // extract fields
  VPackSlice fld;
  try {
    fld = ExtractFields(definition, iid);
  } catch (arangodb::basics::Exception const& e) {
    return TRI_set_errno(e.code());
  }
  VPackValueLength fieldCount = fld.length();

  // extract the list of fields
  if (fieldCount != 1) {
    LOG(ERR) << "ignoring index " << iid << ", has an invalid number of attributes";

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  VPackSlice value = fld.at(0);

  if (!value.isString()) {
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  std::string const attribute = value.copyString();

  // 2013-01-17: deactivated substring indexing
  // indexSubstrings = TRI_LookupObjectJson(definition, "indexSubstrings");

  int minWordLengthValue =
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          definition, "minLength", TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT);

  // create the index
  auto idx = LookupFulltextIndexDocumentCollection(document, attribute,
                                                   minWordLengthValue);

  if (idx == nullptr) {
    bool created;
    idx = CreateFulltextIndexDocumentCollection(
        trx, document, attribute, minWordLengthValue, iid, created);
  }

  if (dst != nullptr) {
    *dst = idx;
  }

  if (idx == nullptr) {
    LOG(ERR) << "cannot create fulltext index " << iid;
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a fulltext index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupFulltextIndexDocumentCollection(
    TRI_document_collection_t* document, std::string const& attribute,
    int minWordLength) {
  return LookupFulltextIndexDocumentCollection(document, attribute,
                                               minWordLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureFulltextIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    TRI_idx_iid_t iid, std::string const& attribute, int minWordLength,
    bool& created) {
  READ_LOCKER(readLocker, document->_vocbase->_inventoryLock);

  WRITE_LOCKER(writeLocker, document->_lock);

  auto idx = CreateFulltextIndexDocumentCollection(trx, document, attribute,
                                                   minWordLength, iid, created);

  if (idx != nullptr) {
    if (created) {
      arangodb::aql::QueryCache::instance()->invalidate(
          document->_vocbase, document->_info.namec_str());
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_copy_t> TRI_SelectByExample(
    TRI_transaction_collection_t* trxCollection,
    ExampleMatcher const& matcher) {
  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  // use filtered to hold copies of the master pointer
  std::vector<TRI_doc_mptr_copy_t> filtered;

  auto work = [&](TRI_doc_mptr_t const* ptr) -> void {
    if (matcher.matches(0, ptr)) {
      filtered.emplace_back(*ptr);
    }
  };
  document->primaryIndex()->invokeOnAllElements(work);
  return filtered;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document given by a master pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteDocumentDocumentCollection(
    arangodb::Transaction* trx, TRI_transaction_collection_t* trxCollection,
    TRI_doc_update_policy_t const* policy, TRI_doc_mptr_t* doc) {
  return TRI_RemoveShapedJsonDocumentCollection(
      trx, trxCollection, (const TRI_voc_key_t)TRI_EXTRACT_MARKER_KEY(doc), 0,
      nullptr, policy, false,
      false);  // PROTECTED by trx in trxCollection
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection(TRI_document_collection_t* document) {
  int res = TRI_ERROR_ARANGO_NO_JOURNAL;

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  if (document->_state == TRI_COL_STATE_WRITE) {
    size_t const n = document->_journals._length;

    if (n > 0) {
      TRI_ASSERT(document->_journals._buffer[0] != nullptr);
      TRI_CloseDatafileDocumentCollection(document, 0, false);

      res = TRI_ERROR_NO_ERROR;
    }
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

int TRI_ReadShapedJsonDocumentCollection(
    arangodb::Transaction* trx, TRI_transaction_collection_t* trxCollection,
    const TRI_voc_key_t key, TRI_doc_mptr_copy_t* mptr, bool lock) {
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  {
    TRI_IF_FAILURE("ReadDocumentNoLock") {
      // test what happens if no lock can be acquired
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("ReadDocumentNoLockExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_document_collection_t* document =
        trxCollection->_collection->_collection;
    arangodb::CollectionReadLocker collectionLocker(document, lock);

    TRI_doc_mptr_t* header;
    int res = LookupDocument(trx, document, key, nullptr, header);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    // we found a document, now copy it over
    *mptr = *header;
  }

  TRI_ASSERT(mptr->getDataPtr() !=
             nullptr);  // PROTECTED by trx in trxCollection
  TRI_ASSERT(mptr->_rid > 0);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a shaped-json document (or edge)
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveShapedJsonDocumentCollection(
    arangodb::Transaction* trx, TRI_transaction_collection_t* trxCollection,
    TRI_voc_key_t key, TRI_voc_rid_t rid, arangodb::wal::Marker* marker,
    TRI_doc_update_policy_t const* policy, bool lock, bool forceSync) {
  bool const freeMarker = (marker == nullptr);
  rid = GetRevisionId(rid);

  TRI_ASSERT(key != nullptr);

  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  TRI_IF_FAILURE("RemoveDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("RemoveDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (marker == nullptr) {
    marker = new arangodb::wal::RemoveMarker(
        document->_vocbase->_id, document->_info.id(), rid,
        TRI_MarkerIdTransaction(trxCollection->_transaction), std::string(key));
  }

  TRI_ASSERT(marker != nullptr);

  TRI_doc_mptr_t* header;
  int res;
  TRI_voc_tick_t markerTick = 0;
  {
    TRI_IF_FAILURE("RemoveDocumentNoLock") {
      // test what happens if no lock can be acquired
      if (freeMarker) {
        delete marker;
      }
      return TRI_ERROR_DEBUG;
    }

    arangodb::CollectionWriteLocker collectionLocker(document, lock);

    arangodb::wal::DocumentOperation operation(
        trx, marker, freeMarker, document, TRI_VOC_DOCUMENT_OPERATION_REMOVE,
        rid);

    res = LookupDocument(trx, document, key, policy, header);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    // we found a document to remove
    TRI_ASSERT(header != nullptr);
    operation.header = header;
    operation.init();

    // delete from indexes
    res = DeleteSecondaryIndexes(trx, document, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      InsertSecondaryIndexes(trx, document, header, true);
      return res;
    }

    res = DeletePrimaryIndex(trx, document, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      InsertSecondaryIndexes(trx, document, header, true);
      return res;
    }

    operation.indexed();

    document->_headersPtr->unlink(header);  // PROTECTED by trx in trxCollection
    document->_numberDocuments--;

    TRI_IF_FAILURE("RemoveDocumentNoOperation") { return TRI_ERROR_DEBUG; }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res = TRI_AddOperationTransaction(trxCollection->_transaction, operation,
                                      forceSync);

    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    } else if (forceSync) {
      markerTick = operation.tick;
    }
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shaped-json document (or edge)
/// note: key might be NULL. in this case, a key is auto-generated
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapedJsonDocumentCollection(
    arangodb::Transaction* trx, TRI_transaction_collection_t* trxCollection,
    const TRI_voc_key_t key, TRI_voc_rid_t rid, arangodb::wal::Marker* marker,
    TRI_doc_mptr_copy_t* mptr, TRI_shaped_json_t const* shaped,
    TRI_document_edge_t const* edge, bool lock, bool forceSync,
    bool isRestore) {
  bool const freeMarker = (marker == nullptr);

  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  rid = GetRevisionId(rid);
  TRI_voc_tick_t tick = static_cast<TRI_voc_tick_t>(rid);

  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  // TRI_ASSERT(lock ||
  // TRI_IsLockedCollectionTransaction(trxCollection, TRI_TRANSACTION_WRITE,
  // 0));

  std::string keyString;

  if (key == nullptr) {
    // no key specified, now generate a new one
    keyString.assign(document->_keyGenerator->generate(tick));

    if (keyString.empty()) {
      return TRI_ERROR_ARANGO_OUT_OF_KEYS;
    }
  } else {
    // key was specified, now validate it
    int res = document->_keyGenerator->validate(key, isRestore);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    keyString = key;
  }

  uint64_t const hash = document->primaryIndex()->calculateHash(
      trx, keyString.c_str(), keyString.size());

  int res = TRI_ERROR_NO_ERROR;

  if (marker == nullptr) {
    res = CreateMarkerNoLegend(marker, document, rid, trxCollection, keyString,
                               shaped, edge);

    if (res != TRI_ERROR_NO_ERROR) {
      if (marker != nullptr) {
        // avoid memleak
        delete marker;
      }

      return res;
    }
  }

  TRI_ASSERT(marker != nullptr);

  TRI_voc_tick_t markerTick = 0;
  // now insert into indexes
  {
    TRI_IF_FAILURE("InsertDocumentNoLock") {
      // test what happens if no lock can be acquired

      if (freeMarker) {
        delete marker;
      }

      return TRI_ERROR_DEBUG;
    }

    arangodb::CollectionWriteLocker collectionLocker(document, lock);

    arangodb::wal::DocumentOperation operation(
        trx, marker, freeMarker, document, TRI_VOC_DOCUMENT_OPERATION_INSERT,
        rid);

    TRI_IF_FAILURE("InsertDocumentNoHeader") {
      // test what happens if no header can be acquired
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("InsertDocumentNoHeaderExcept") {
      // test what happens if no header can be acquired
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // create a new header
    TRI_doc_mptr_t* header = operation.header = document->_headersPtr->request(
        marker->size());  // PROTECTED by trx in trxCollection

    if (header == nullptr) {
      // out of memory. no harm done here. just return the error
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // update the header we got
    void* mem = operation.marker->mem();
    header->_rid = rid;
    header->setDataPtr(mem);  // PROTECTED by trx in trxCollection
    header->_hash = hash;

    // insert into indexes
    res =
        InsertDocument(trx, trxCollection, header, operation, mptr, forceSync);

    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    } else {
      TRI_ASSERT(mptr->getDataPtr() !=
                 nullptr);  // PROTECTED by trx in trxCollection

      if (forceSync) {
        markerTick = operation.tick;
      }
    }
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateShapedJsonDocumentCollection(
    arangodb::Transaction* trx, TRI_transaction_collection_t* trxCollection,
    TRI_voc_key_t key, TRI_voc_rid_t rid, arangodb::wal::Marker* marker,
    TRI_doc_mptr_copy_t* mptr, TRI_shaped_json_t const* shaped,
    TRI_doc_update_policy_t const* policy, bool lock, bool forceSync) {
  bool const freeMarker = (marker == nullptr);

  rid = GetRevisionId(rid);

  TRI_ASSERT(key != nullptr);

  // initialize the result
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  // TRI_ASSERT(lock ||
  // TRI_IsLockedCollectionTransaction(trxCollection, TRI_TRANSACTION_WRITE,
  // 0));

  int res = TRI_ERROR_NO_ERROR;
  TRI_voc_tick_t markerTick = 0;
  {
    TRI_IF_FAILURE("UpdateDocumentNoLock") { return TRI_ERROR_DEBUG; }

    arangodb::CollectionWriteLocker collectionLocker(document, lock);

    // get the header pointer of the previous revision
    TRI_doc_mptr_t* oldHeader;
    res = LookupDocument(trx, document, key, policy, oldHeader);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_IF_FAILURE("UpdateDocumentNoMarker") {
      // test what happens when no marker can be created
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("UpdateDocumentNoMarkerExcept") {
      // test what happens when no marker can be created
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (marker == nullptr) {
      TRI_IF_FAILURE("UpdateDocumentNoLegend") {
        // test what happens when no legend can be created
        return TRI_ERROR_DEBUG;
      }

      TRI_IF_FAILURE("UpdateDocumentNoLegendExcept") {
        // test what happens when no legend can be created
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      TRI_df_marker_t const* original = static_cast<TRI_df_marker_t const*>(
          oldHeader->getDataPtr());  // PROTECTED by trx in trxCollection

      res = CloneMarkerNoLegend(marker, original, document, rid, trxCollection,
                                shaped);

      if (res != TRI_ERROR_NO_ERROR) {
        if (marker != nullptr) {
          // avoid memleak
          delete marker;
        }
        return res;
      }
    }

    TRI_ASSERT(marker != nullptr);

    arangodb::wal::DocumentOperation operation(
        trx, marker, freeMarker, document, TRI_VOC_DOCUMENT_OPERATION_UPDATE,
        rid);
    operation.header = oldHeader;
    operation.init();

    res = UpdateDocument(trx, trxCollection, oldHeader, operation, mptr,
                         forceSync);

    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    } else if (forceSync) {
      markerTick = operation.tick;
    }
  }

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT(mptr->getDataPtr() !=
               nullptr);  // PROTECTED by trx in trxCollection
    TRI_ASSERT(mptr->_rid > 0);
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document or edge into the collection
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::insert(Transaction* trx, VPackSlice const* slice,
                                      TRI_doc_mptr_copy_t* mptr, bool lock,
                                      bool waitForSync) {
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);

  VPackSlice const key(slice->get(TRI_VOC_ATTRIBUTE_KEY));
  std::string const keyString(key.copyString());  // TODO: use slice.hash()
  uint64_t const hash = primaryIndex()->calculateHash(
      trx, keyString.c_str(), keyString.size());  // TODO: remove here

  std::unique_ptr<arangodb::wal::Marker> marker(
      createVPackInsertMarker(trx, slice));

  TRI_voc_tick_t markerTick = 0;
  int res;
  // now insert into indexes
  {
    TRI_IF_FAILURE("InsertDocumentNoLock") {
      // test what happens if no lock can be acquired
      return TRI_ERROR_DEBUG;
    }

    arangodb::CollectionWriteLocker collectionLocker(this, lock);

    arangodb::wal::DocumentOperation operation(
        trx, marker.get(), false, this, TRI_VOC_DOCUMENT_OPERATION_INSERT,
        0 /*rid*/);  // TODO: fix rid

    TRI_IF_FAILURE("InsertDocumentNoHeader") {
      // test what happens if no header can be acquired
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("InsertDocumentNoHeaderExcept") {
      // test what happens if no header can be acquired
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // create a new header
    TRI_doc_mptr_t* header = operation.header = _headersPtr->request(
        marker->size());  // PROTECTED by trx in trxCollection

    if (header == nullptr) {
      // out of memory. no harm done here. just return the error
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // update the header we got
    void* mem = operation.marker->mem();
    header->_rid = 0;         // TODO: fix revision id
    header->setDataPtr(mem);  // PROTECTED by trx in trxCollection
    header->_hash = hash;

    // insert into indexes
    res = insertDocument(trx, header, operation, mptr, waitForSync);

    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    } else {
      TRI_ASSERT(mptr->getDataPtr() !=
                 nullptr);  // PROTECTED by trx in trxCollection

      if (waitForSync) {
        markerTick = operation.tick;
      }
    }
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document or edge
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::remove(arangodb::Transaction* trx,
                                      VPackSlice const* slice,
                                      TRI_doc_update_policy_t const* policy,
                                      bool lock, bool waitForSync) {
  TRI_IF_FAILURE("RemoveDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("RemoveDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  std::unique_ptr<arangodb::wal::Marker> marker(
      createVPackRemoveMarker(trx, slice));

  TRI_doc_mptr_t* header;
  int res;
  TRI_voc_tick_t markerTick = 0;
  {
    TRI_IF_FAILURE("RemoveDocumentNoLock") {
      // test what happens if no lock can be acquired
      return TRI_ERROR_DEBUG;
    }

    arangodb::CollectionWriteLocker collectionLocker(this, lock);

    arangodb::wal::DocumentOperation operation(
        trx, marker.get(), false, this, TRI_VOC_DOCUMENT_OPERATION_REMOVE,
        0 /*rid*/);  // TODO: fix rid

    res = lookupDocument(trx, slice, policy, header);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    // we found a document to remove
    TRI_ASSERT(header != nullptr);
    operation.header = header;
    operation.init();

    // delete from indexes
    res = deleteSecondaryIndexes(trx, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      insertSecondaryIndexes(trx, header, true);
      return res;
    }

    res = deletePrimaryIndex(trx, header);

    if (res != TRI_ERROR_NO_ERROR) {
      insertSecondaryIndexes(trx, header, true);
      return res;
    }

    operation.indexed();

    _headersPtr->unlink(header);
    _numberDocuments--;

    TRI_IF_FAILURE("RemoveDocumentNoOperation") { return TRI_ERROR_DEBUG; }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res = TRI_AddOperationTransaction(trx->getInternals(), operation,
                                      waitForSync);

    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    } else if (waitForSync) {
      markerTick = operation.tick;
    }
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a vpack-based insert marker for documents / edges
////////////////////////////////////////////////////////////////////////////////

arangodb::wal::Marker* TRI_document_collection_t::createVPackInsertMarker(
    Transaction* trx, VPackSlice const* slice) {
  auto marker = new arangodb::wal::VPackDocumentMarker(
      _vocbase->_id, _info.id(), trx->getInternals()->_id, slice);
  return marker;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a vpack-based remove marker for documents / edges
////////////////////////////////////////////////////////////////////////////////

arangodb::wal::Marker* TRI_document_collection_t::createVPackRemoveMarker(
    Transaction* trx, VPackSlice const* slice) {
  auto marker = new arangodb::wal::VPackRemoveMarker(
      _vocbase->_id, _info.id(), trx->getInternals()->_id, slice);
  return marker;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document by key
/// the caller must make sure the read lock on the collection is held
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::lookupDocument(
    arangodb::Transaction* trx, VPackSlice const* slice,
    TRI_doc_update_policy_t const* policy, TRI_doc_mptr_t*& header) {
  VPackSlice key = slice->get(TRI_VOC_ATTRIBUTE_KEY);

  if (!key.isString()) {
    return TRI_ERROR_INTERNAL;
  }

  // we need to have a null-terminated string for lookup, so copy the
  // key from the slice to local memory
  char buffer[TRI_VOC_KEY_MAX_LENGTH + 1];
  uint64_t length;
  char const* p = key.getString(length);
  memcpy(&buffer[0], p, length);
  buffer[length] = '\0';

  header = primaryIndex()->lookupKey(trx, &buffer[0]);

  if (header == nullptr) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  if (policy != nullptr) {
    return policy->check(header->_rid);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a document, low level worker
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::insertDocument(
    arangodb::Transaction* trx, TRI_doc_mptr_t* header,
    arangodb::wal::DocumentOperation& operation, TRI_doc_mptr_copy_t* mptr,
    bool& waitForSync) {
  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(mptr != nullptr);

  // .............................................................................
  // insert into indexes
  // .............................................................................

  // insert into primary index first
  int res = insertPrimaryIndex(trx, header);

  if (res != TRI_ERROR_NO_ERROR) {
    // insert has failed
    return res;
  }

  // insert into secondary indexes
  res = insertSecondaryIndexes(trx, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    deleteSecondaryIndexes(trx, header, true);
    deletePrimaryIndex(trx, header);
    return res;
  }

  _numberDocuments++;

  operation.indexed();

  TRI_IF_FAILURE("InsertDocumentNoOperation") { return TRI_ERROR_DEBUG; }

  TRI_IF_FAILURE("InsertDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  res =
      TRI_AddOperationTransaction(trx->getInternals(), operation, waitForSync);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  *mptr = *header;

  res = postInsertIndexes(trx, header);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the primary index
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::insertPrimaryIndex(arangodb::Transaction* trx,
                                                  TRI_doc_mptr_t* header) {
  TRI_IF_FAILURE("InsertPrimaryIndex") { return TRI_ERROR_DEBUG; }

  TRI_doc_mptr_t* found;

  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(header->getDataPtr() !=
             nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME

  // insert into primary index
  int res = primaryIndex()->insertKey(trx, header, (void const**)&found);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (found == nullptr) {
    // success
    return TRI_ERROR_NO_ERROR;
  }

  // we found a previous revision in the index
  // the found revision is still alive
  LOG(TRACE) << "document '" << TRI_EXTRACT_MARKER_KEY(header) << "' already existed with revision " << // ONLY IN INDEX << " while creating revision " << PROTECTED by RUNTIME
      (unsigned long long)found->_rid;

  return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the secondary indexes
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::insertSecondaryIndexes(
    arangodb::Transaction* trx, TRI_doc_mptr_t const* header, bool isRollback) {
  TRI_IF_FAILURE("InsertSecondaryIndexes") { return TRI_ERROR_DEBUG; }

  if (!useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  int result = TRI_ERROR_NO_ERROR;

  auto const& indexes = allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    int res = idx->insert(trx, header, isRollback);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    } else if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
          result == TRI_ERROR_NO_ERROR) {
        // "prefer" unique constraint violated
        result = res;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the primary index
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::deletePrimaryIndex(
    arangodb::Transaction* trx, TRI_doc_mptr_t const* header) {
  TRI_IF_FAILURE("DeletePrimaryIndex") { return TRI_ERROR_DEBUG; }

  auto found = primaryIndex()->removeKey(
      trx,
      TRI_EXTRACT_MARKER_KEY(header));  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (found == nullptr) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the secondary indexes
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::deleteSecondaryIndexes(
    arangodb::Transaction* trx, TRI_doc_mptr_t const* header, bool isRollback) {
  if (!useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("DeleteSecondaryIndexes") { return TRI_ERROR_DEBUG; }

  int result = TRI_ERROR_NO_ERROR;

  auto const& indexes = allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    int res = idx->remove(trx, header, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief post-insert operation
////////////////////////////////////////////////////////////////////////////////

int TRI_document_collection_t::postInsertIndexes(arangodb::Transaction* trx,
                                                 TRI_doc_mptr_t* header) {
  if (!useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  auto const& indexes = allIndexes();
  size_t const n = indexes.size();
  // TODO: remove usage of TRI_transaction_collection_t here
  TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(
      trx->getInternals(), _info.id(), TRI_TRANSACTION_WRITE);

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    idx->postInsert(trx, trxCollection, header);
  }

  // post-insert will never return an error
  return TRI_ERROR_NO_ERROR;
}
