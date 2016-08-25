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
#include "Indexes/GeoIndex2.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
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

/// @brief extracts a field list from a VelocyPack object
///        Does not copy any data, caller has to make sure that data
///        in slice stays valid until this return value is destroyed.
static VPackSlice ExtractFields(VPackSlice const& slice, TRI_idx_iid_t iid) {
  VPackSlice fld = slice.get("fields");
  if (!fld.isArray()) {
    LOG(ERR) << "ignoring index " << iid << ", 'fields' must be an array";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  for (auto const& sub : VPackArrayIterator(fld)) {
    if (!sub.isString()) {
      LOG(ERR) << "ignoring index " << iid
               << ", 'fields' must be an array of attribute paths";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
  }
  return fld;
}

/// @brief ensures that an error code is set in all required places
static void EnsureErrorCode(int code) {
  if (code == TRI_ERROR_NO_ERROR) {
    // must have an error code
    code = TRI_ERROR_INTERNAL;
  }

  TRI_set_errno(code);
  errno = code;
}

TRI_collection_t::TRI_collection_t(TRI_vocbase_t* vocbase, 
                                   arangodb::VocbaseCollectionInfo const& parameters)
      : _vocbase(vocbase), 
        _tickMax(0),
        _info(parameters), 
        _uncollectedLogfileEntries(0),
        _numberDocuments(0),
        _ditches(this),
        _nextCompactionStartIndex(0),
        _lastCompactionStatus(nullptr),
        _lastCompaction(0.0) {

  // check if we can generate the key generator
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> buffer =
      parameters.keyOptions();

  VPackSlice slice;
  if (buffer != nullptr) {
    slice = VPackSlice(buffer->data());
  }
  
  std::unique_ptr<KeyGenerator> keyGenerator(KeyGenerator::factory(slice));

  if (keyGenerator == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);
  }

  _keyGenerator.reset(keyGenerator.release());

  setCompactionStatus("compaction not yet started");
  if (ServerState::instance()->isDBServer()) {
    _followers.reset(new FollowerInfo(this));
  }
}
  
TRI_collection_t::~TRI_collection_t() {
  try {
    this->close();
  } catch (...) {
    // ignore any errors here
  }

  _ditches.destroy();

  _info.clearKeyOptions();
  
  for (auto& it : _datafiles) {
    TRI_FreeDatafile(it);
  }
  for (auto& it : _journals) {
    TRI_FreeDatafile(it);
  }
  for (auto& it : _compactors) {
    TRI_FreeDatafile(it);
  }
}

/// @brief update statistics for a collection
/// note: the write-lock for the collection must be held to call this
void TRI_collection_t::setLastRevision(TRI_voc_rid_t rid, bool force) {
  if (rid > 0) {
    _info.setRevision(rid, force);
  }
}

/// @brief whether or not a collection is fully collected
bool TRI_collection_t::isFullyCollected() {
  READ_LOCKER(readLocker, _lock);

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

/// @brief read locks a collection
int TRI_collection_t::beginRead() {
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
  READ_LOCKER(locker, _lock);

  try {
    _vocbase->_deadlockDetector.addReader(this, false);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  locker.steal();

  return TRI_ERROR_NO_ERROR;
}

/// @brief read unlocks a collection
int TRI_collection_t::endRead() {
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
  } catch (...) {
  }

  // LOCKING-DEBUG
  // std::cout << "EndRead: " << document->_info._name << std::endl;
  _lock.unlockRead();

  return TRI_ERROR_NO_ERROR;
}

/// @brief write locks a collection
int TRI_collection_t::beginWrite() {
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
  WRITE_LOCKER(locker, _lock);

  // register writer
  try {
    _vocbase->_deadlockDetector.addWriter(this, false);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  locker.steal();

  return TRI_ERROR_NO_ERROR;
}

/// @brief write unlocks a collection
int TRI_collection_t::endWrite() {
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
  } catch (...) {
    // must go on here to unlock the lock
  }

  // LOCKING-DEBUG
  // std::cout << "EndWrite: " << document->_info._name << std::endl;
  _lock.unlockWrite();

  return TRI_ERROR_NO_ERROR;
}

/// @brief read locks a collection, with a timeout (in µseconds)
int TRI_collection_t::beginReadTimed(uint64_t timeout,
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

  while (true) {
    TRY_READ_LOCKER(locker, _lock);

    if (locker.isLocked()) {
      // when we are here, we've got the read lock
      _vocbase->_deadlockDetector.addReader(this, wasBlocked);
      
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }

    try {
      if (!wasBlocked) {
        // insert reader
        wasBlocked = true;
        if (_vocbase->_deadlockDetector.setReaderBlocked(this) ==
            TRI_ERROR_DEADLOCK) {
          // deadlock
          LOG(TRACE) << "deadlock detected while trying to acquire read-lock on collection '" << _info.name() << "'";
          return TRI_ERROR_DEADLOCK;
        }
        LOG(TRACE) << "waiting for read-lock on collection '" << _info.name() << "'";
      } else if (++iterations >= 5) {
        // periodically check for deadlocks
        TRI_ASSERT(wasBlocked);
        iterations = 0;
        if (_vocbase->_deadlockDetector.detectDeadlock(this, false) ==
            TRI_ERROR_DEADLOCK) {
          // deadlock
          _vocbase->_deadlockDetector.unsetReaderBlocked(this);
          LOG(TRACE) << "deadlock detected while trying to acquire read-lock on collection '" << _info.name() << "'";
          return TRI_ERROR_DEADLOCK;
        }
      }
    } catch (...) {
      // clean up!
      if (wasBlocked) {
        _vocbase->_deadlockDetector.unsetReaderBlocked(this);
      }
      // always exit
      return TRI_ERROR_OUT_OF_MEMORY;
    }

#ifdef _WIN32
    usleep((unsigned long)sleepPeriod);
#else
    usleep((useconds_t)sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      _vocbase->_deadlockDetector.unsetReaderBlocked(this);
      LOG(TRACE) << "timed out waiting for read-lock on collection '" << _info.name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }
}

/// @brief write locks a collection, with a timeout
int TRI_collection_t::beginWriteTimed(uint64_t timeout,
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

  while (true) {
    TRY_WRITE_LOCKER(locker, _lock);

    if (locker.isLocked()) {
      // register writer
      _vocbase->_deadlockDetector.addWriter(this, wasBlocked);
      // keep lock and exit loop
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }

    try {
      if (!wasBlocked) {
        // insert writer
        wasBlocked = true;
        if (_vocbase->_deadlockDetector.setWriterBlocked(this) ==
            TRI_ERROR_DEADLOCK) {
          // deadlock
          LOG(TRACE) << "deadlock detected while trying to acquire write-lock on collection '" << _info.name() << "'";
          return TRI_ERROR_DEADLOCK;
        }
        LOG(TRACE) << "waiting for write-lock on collection '" << _info.name() << "'";
      } else if (++iterations >= 5) {
        // periodically check for deadlocks
        TRI_ASSERT(wasBlocked);
        iterations = 0;
        if (_vocbase->_deadlockDetector.detectDeadlock(this, true) ==
            TRI_ERROR_DEADLOCK) {
          // deadlock
          _vocbase->_deadlockDetector.unsetWriterBlocked(this);
          LOG(TRACE) << "deadlock detected while trying to acquire write-lock on collection '" << _info.name() << "'";
          return TRI_ERROR_DEADLOCK;
        }
      }
    } catch (...) {
      // clean up!
      if (wasBlocked) {
        _vocbase->_deadlockDetector.unsetWriterBlocked(this);
      }
      // always exit
      return TRI_ERROR_OUT_OF_MEMORY;
    }

#ifdef _WIN32
    usleep((unsigned long)sleepPeriod);
#else
    usleep((useconds_t)sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      _vocbase->_deadlockDetector.unsetWriterBlocked(this);
      LOG(TRACE) << "timed out waiting for write-lock on collection '" << _info.name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }
}

/// @brief returns information about the collection
/// note: the collection lock must be held when calling this function
TRI_doc_collection_info_t* TRI_collection_t::figures() {
  // prefill with 0's to init counters
  auto info = static_cast<TRI_doc_collection_info_t*>(TRI_Allocate(
          TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_collection_info_t), true));

  if (info == nullptr) {
    return nullptr;
  }

  DatafileStatisticsContainer dfi = _datafileStatistics.all();
  info->_numberAlive += static_cast<TRI_voc_ssize_t>(dfi.numberAlive);
  info->_numberDead += static_cast<TRI_voc_ssize_t>(dfi.numberDead);
  info->_numberDeletions += static_cast<TRI_voc_ssize_t>(dfi.numberDeletions);

  info->_sizeAlive += dfi.sizeAlive;
  info->_sizeDead += dfi.sizeDead;

  // add the file sizes for datafiles and journals
  TRI_collection_t* base = this;

  for (auto& df : base->_datafiles) {
    info->_datafileSize += (int64_t)df->_initSize;
    ++info->_numberDatafiles;
  }

  for (auto& df : base->_journals) {
    info->_journalfileSize += (int64_t)df->_initSize;
    ++info->_numberJournalfiles;
  }

  for (auto& df : base->_compactors) {
    info->_compactorfileSize += (int64_t)df->_initSize;
    ++info->_numberCompactorfiles;
  }

  // add index information
  info->_numberIndexes = 0;
  info->_sizeIndexes = 0;

  info->_sizeIndexes += static_cast<int64_t>(_masterPointers.memory());

#warning Needs merge with Jan
  /*
  for (auto const& idx : allIndexes()) {
    info->_sizeIndexes += idx->memory();
    info->_numberIndexes++;
  }
  */

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

std::string TRI_collection_t::label() const {
  return _vocbase->name() + " / " + _info.name();
}
  
/// @brief seal a datafile
int TRI_collection_t::sealDatafile(TRI_datafile_t* datafile, bool isCompactor) {
  int res = TRI_SealDatafile(datafile);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to seal journal '" << datafile->getName(datafile)
             << "': " << TRI_errno_string(res);
    return res;
  }

  if (!isCompactor && datafile->isPhysical(datafile)) {
    // rename the file
    std::string dname("datafile-" + std::to_string(datafile->_fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(path(), dname);

    res = TRI_RenameDatafile(datafile, filename.c_str());

    if (res == TRI_ERROR_NO_ERROR) {
      LOG(TRACE) << "closed file '" << datafile->getName(datafile) << "'";
    } else {
      LOG(ERR) << "failed to rename datafile '" << datafile->getName(datafile)
               << "' to '" << filename << "': " << TRI_errno_string(res);
    }
  }

  return res;
}

/// @brief rotate the active journal - will do nothing if there is no journal
int TRI_collection_t::rotateActiveJournal() {
  WRITE_LOCKER(writeLocker, _filesLock);

  // note: only journals need to be handled here as the journal is the
  // only place that's ever written to. if a journal is full, it will have been
  // sealed and synced already
  if (_journals.empty()) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  TRI_datafile_t* datafile = _journals[0];
  TRI_ASSERT(datafile != nullptr);

  // make sure we have enough room in the target vector before we go on
  _datafiles.reserve(_datafiles.size() + 1);

  int res = sealDatafile(datafile, false);
 
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
   
  // shouldn't throw as we reserved enough space before
  _datafiles.emplace_back(datafile);


  TRI_ASSERT(!_journals.empty());
  _journals.erase(_journals.begin());
  TRI_ASSERT(_journals.empty());

  return res;
}

/// @brief sync the active journal - will do nothing if there is no journal
/// or if the journal is volatile
int TRI_collection_t::syncActiveJournal() {
  WRITE_LOCKER(writeLocker, _filesLock);

  // note: only journals need to be handled here as the journal is the
  // only place that's ever written to. if a journal is full, it will have been
  // sealed and synced already
  if (_journals.empty()) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  TRI_datafile_t* datafile = _journals[0];
  TRI_ASSERT(datafile != nullptr);

  int res = TRI_ERROR_NO_ERROR;

  // we only need to care about physical datafiles
  // anonymous regions do not need to be synced
  if (datafile->isPhysical(datafile)) {
    char const* synced = datafile->_synced;
    char* written = datafile->_written;

    if (synced < written) {
      bool ok = datafile->sync(datafile, synced, written);

      if (ok) {
        LOG_TOPIC(TRACE, Logger::COLLECTOR) << "msync succeeded "
                                            << (void*)synced << ", size "
                                            << (written - synced);
        datafile->_synced = written;
      } else {
        res = TRI_errno();
        if (res == TRI_ERROR_NO_ERROR) {
          // oops, error code got lost
          res = TRI_ERROR_INTERNAL;
        }

        LOG_TOPIC(ERR, Logger::COLLECTOR)
            << "msync failed with: " << TRI_last_error();
        datafile->_state = TRI_DF_STATE_WRITE_ERROR;
      }
    }
  }

  return res;
}

/// @brief reserve space in the current journal. if no create exists or the
/// current journal cannot provide enough space, close the old journal and
/// create a new one
int TRI_collection_t::reserveJournalSpace(TRI_voc_tick_t tick,
                                          TRI_voc_size_t size,
                                          char*& resultPosition,
                                          TRI_datafile_t*& resultDatafile) {
  // reset results
  resultPosition = nullptr;
  resultDatafile = nullptr;

  WRITE_LOCKER(writeLocker, _filesLock);

  // start with configured journal size
  TRI_voc_size_t targetSize = _info.maximalSize();

  // make sure that the document fits
  while (targetSize - 256 < size) {
    targetSize *= 2;
  }

  while (true) {
    TRI_datafile_t* datafile = nullptr;

    if (_journals.empty()) {
      // create enough room in the journals vector
      _journals.reserve(_journals.size() + 1);

      datafile = createDatafile(tick, targetSize, false);

      if (datafile == nullptr) {
        int res = TRI_errno();
        // could not create a datafile, this is a serious error

        if (res == TRI_ERROR_NO_ERROR) {
          // oops, error code got lost
          res = TRI_ERROR_INTERNAL;
        }

        return res;
      }

      // shouldn't throw as we reserved enough space before
      _journals.emplace_back(datafile);
    } else {
      // select datafile
      datafile = _journals[0];
    }

    TRI_ASSERT(datafile != nullptr);

    // try to reserve space in the datafile

    TRI_df_marker_t* position = nullptr;
    int res = TRI_ReserveElementDatafile(datafile, size, &position, targetSize);

    // found a datafile with enough space left
    if (res == TRI_ERROR_NO_ERROR) {
      datafile->_written = ((char*)position) + size;
      // set result
      resultPosition = reinterpret_cast<char*>(position);
      resultDatafile = datafile;
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_ARANGO_DATAFILE_FULL) {
      // some other error
      LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: '"
                                        << TRI_last_error() << "'";
      return res;
    }

    // TRI_ERROR_ARANGO_DATAFILE_FULL...
    // journal is full, close it and sync
    LOG_TOPIC(DEBUG, Logger::COLLECTOR) << "closing full journal '"
                                        << datafile->getName(datafile) << "'";

    // make sure we have enough room in the target vector before we go on
    _datafiles.reserve(_datafiles.size() + 1);

    res = sealDatafile(datafile, false);

    // move journal into datafiles vector, regardless of whether an error
    // occurred
    TRI_ASSERT(!_journals.empty());
    _journals.erase(_journals.begin());
    TRI_ASSERT(_journals.empty());
    // this shouldn't fail, as we have reserved space before already
    _datafiles.emplace_back(datafile);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred, we must stop here
      return res;
    }
  }  // otherwise, next iteration!

  return TRI_ERROR_ARANGO_NO_JOURNAL;
}

/// @brief create compactor file
TRI_datafile_t* TRI_collection_t::createCompactor(TRI_voc_fid_t fid,
                                                  TRI_voc_size_t maximalSize) {
  try {
    WRITE_LOCKER(writeLocker, _filesLock);

    TRI_ASSERT(_compactors.empty());
    // reserve enough space for the later addition
    _compactors.reserve(_compactors.size() + 1);

    TRI_datafile_t* compactor =
        createDatafile(fid, static_cast<TRI_voc_size_t>(maximalSize), true);

    if (compactor != nullptr) {
      // should not throw, as we've reserved enough space before
      _compactors.emplace_back(compactor);
    }
    return compactor;
  } catch (...) {
    return nullptr;
  }
}

/// @brief close an existing compactor
int TRI_collection_t::closeCompactor(TRI_datafile_t* datafile) {
  WRITE_LOCKER(writeLocker, _filesLock);

  if (_compactors.size() != 1) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  TRI_datafile_t* compactor = _compactors[0];

  if (datafile != compactor) {
    // wrong compactor file specified... should not happen
    return TRI_ERROR_INTERNAL;
  }

  return sealDatafile(datafile, true);
}

/// @brief replace a datafile with a compactor
int TRI_collection_t::replaceDatafileWithCompactor(TRI_datafile_t* datafile,
                                                   TRI_datafile_t* compactor) {
  TRI_ASSERT(datafile != nullptr);
  TRI_ASSERT(compactor != nullptr);

  WRITE_LOCKER(writeLocker, _filesLock);

  TRI_ASSERT(!_compactors.empty());

  for (size_t i = 0; i < _datafiles.size(); ++i) {
    if (_datafiles[i]->_fid == datafile->_fid) {
      // found!
      // now put the compactor in place of the datafile
      _datafiles[i] = compactor;

      // remove the compactor file from the list of compactors
      TRI_ASSERT(_compactors[0] != nullptr);
      TRI_ASSERT(_compactors[0]->_fid == compactor->_fid);

      _compactors.erase(_compactors.begin());
      TRI_ASSERT(_compactors.empty());

      return TRI_ERROR_NO_ERROR;
    }
  }

  return TRI_ERROR_INTERNAL;
}

/// @brief creates a datafile
TRI_datafile_t* TRI_collection_t::createDatafile(TRI_voc_fid_t fid,
                                                 TRI_voc_size_t journalSize,
                                                 bool isCompactor) {
  TRI_ASSERT(fid > 0);
  TRI_collection_t* document =
      static_cast<TRI_collection_t*>(this);
  TRI_ASSERT(document != nullptr);

  // create an entry for the new datafile
  try {
    document->_datafileStatistics.create(fid);
  } catch (...) {
    EnsureErrorCode(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  TRI_datafile_t* datafile;

  if (document->_info.isVolatile()) {
    // in-memory collection
    datafile = TRI_CreateDatafile(nullptr, fid, journalSize, true);
  } else {
    // construct a suitable filename (which may be temporary at the beginning)
    std::string jname;
    if (isCompactor) {
      jname = "compaction-";
    } else {
      jname = "temp-";
    }

    jname.append(std::to_string(fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(path(), jname);

    TRI_IF_FAILURE("CreateJournalDocumentCollection") {
      // simulate disk full
      EnsureErrorCode(TRI_ERROR_ARANGO_FILESYSTEM_FULL);

      return nullptr;
    }

    // remove an existing temporary file first
    if (TRI_ExistsFile(filename.c_str())) {
      // remove an existing file first
      TRI_UnlinkFile(filename.c_str());
    }

    datafile = TRI_CreateDatafile(filename.c_str(), fid, journalSize, true);
  }

  if (datafile == nullptr) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      EnsureErrorCode(TRI_ERROR_OUT_OF_MEMORY_MMAP);
    } else {
      EnsureErrorCode(TRI_ERROR_ARANGO_NO_JOURNAL);
    }

    return nullptr;
  }

  // datafile is there now
  TRI_ASSERT(datafile != nullptr);

  if (isCompactor) {
    LOG(TRACE) << "created new compactor '" << datafile->getName(datafile)
               << "'";
  } else {
    LOG(TRACE) << "created new journal '" << datafile->getName(datafile) << "'";
  }

  // create a collection header, still in the temporary file
  TRI_df_marker_t* position;
  int res = TRI_ReserveElementDatafile(
      datafile, sizeof(TRI_col_header_marker_t), &position, journalSize);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve1") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot create collection header in file '"
             << datafile->getName(datafile) << "': " << TRI_errno_string(res);

    // close the journal and remove it
    TRI_CloseDatafile(datafile);
    TRI_UnlinkFile(datafile->getName(datafile));
    TRI_FreeDatafile(datafile);

    EnsureErrorCode(res);

    return nullptr;
  }

  TRI_col_header_marker_t cm;
  DatafileHelper::InitMarker(
      reinterpret_cast<TRI_df_marker_t*>(&cm), TRI_DF_MARKER_COL_HEADER,
      sizeof(TRI_col_header_marker_t), static_cast<TRI_voc_tick_t>(fid));
  cm._cid = document->_info.id();

  res = TRI_WriteCrcElementDatafile(datafile, position, &cm.base, false);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    int res = datafile->_lastError;
    LOG(ERR) << "cannot create collection header in file '"
             << datafile->getName(datafile) << "': " << TRI_last_error();

    // close the datafile and remove it
    TRI_CloseDatafile(datafile);
    TRI_UnlinkFile(datafile->getName(datafile));
    TRI_FreeDatafile(datafile);

    EnsureErrorCode(res);

    return nullptr;
  }

  TRI_ASSERT(fid == datafile->_fid);

  // if a physical file, we can rename it from the temporary name to the correct
  // name
  if (!isCompactor && datafile->isPhysical(datafile)) {
    // and use the correct name
    std::string jname("journal-" + std::to_string(datafile->_fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(path(), jname);

    int res = TRI_RenameDatafile(datafile, filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "failed to rename journal '" << datafile->getName(datafile)
               << "' to '" << filename << "': " << TRI_errno_string(res);

      TRI_CloseDatafile(datafile);
      TRI_UnlinkFile(datafile->getName(datafile));
      TRI_FreeDatafile(datafile);

      EnsureErrorCode(res);

      return nullptr;
    } 
      
    LOG(TRACE) << "renamed journal from '" << datafile->getName(datafile)
               << "' to '" << filename << "'";
  }

  return datafile;
}

/// @brief remove a compactor file from the list of compactors
bool TRI_collection_t::removeCompactor(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _filesLock);

  for (auto it = _compactors.begin(); it != _compactors.end(); ++it) {
    if ((*it) == df) {
      // and finally remove the file from the _compactors vector
      _compactors.erase(it);
      return true;
    }
  }

  // not found
  return false;
}

/// @brief remove a datafile from the list of datafiles
bool TRI_collection_t::removeDatafile(TRI_datafile_t* df) {
  WRITE_LOCKER(writeLocker, _filesLock);

  for (auto it = _datafiles.begin(); it != _datafiles.end(); ++it) {
    if ((*it) == df) {
      // and finally remove the file from the _compactors vector
      _datafiles.erase(it);
      return true;
    }
  }

  // not found
  return false;
}

/// @brief iterate over all datafiles in a vector
bool TRI_collection_t::iterateDatafilesVector(std::vector<TRI_datafile_t*> const& files,
                                              std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb) {
  for (auto const& datafile : files) {
    if (!TRI_IterateDatafile(datafile, cb)) {
      return false;
    }

    if (datafile->isPhysical(datafile) && datafile->_isSealed) {
      TRI_MMFileAdvise(datafile->_data, datafile->_maximalSize,
                       TRI_MADVISE_RANDOM);
    }
  }

  return true;
}

/// @brief closes the datafiles passed in the vector
bool TRI_collection_t::closeDataFiles(std::vector<TRI_datafile_t*> const& files) {
  bool result = true;

  for (auto const& datafile : files) {
    TRI_ASSERT(datafile != nullptr);
    if (datafile->_state == TRI_DF_STATE_CLOSED) {
      continue;
    }

    int res = TRI_CloseDatafile(datafile);

    if (res != TRI_ERROR_NO_ERROR) {
      result = false;
    }
  }

  return result;
}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             std::string const& name,
                                             TRI_col_type_e type,
                                             TRI_voc_size_t maximalSize,
                                             VPackSlice const& keyOptions)
    : _type(type),
      _revision(0),
      _cid(0),
      _planId(0),
      _maximalSize(32 * 1024 * 1024), // just to have a default
      _initialCount(-1),
      _indexBuckets(DatabaseFeature::DefaultIndexBuckets),
      _keyOptions(nullptr),
      _isSystem(false),
      _deleted(false),
      _doCompact(true),
      _isVolatile(false),
      _waitForSync(false) {

  auto database = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
  _maximalSize = database->maximalJournalSize();
  _waitForSync = database->waitForSync();

  size_t pageSize = PageSizeFeature::getPageSize();
  _maximalSize =
      static_cast<TRI_voc_size_t>((maximalSize / pageSize) * pageSize);
  if (_maximalSize == 0 && maximalSize != 0) {
    _maximalSize = static_cast<TRI_voc_size_t>(pageSize);
  }
  memset(_name, 0, sizeof(_name));
  TRI_CopyString(_name, name.c_str(), sizeof(_name) - 1);

  if (!keyOptions.isNone()) {
    VPackBuilder builder;
    builder.add(keyOptions);
    _keyOptions = builder.steal();
  }
}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             std::string const& name,
                                             VPackSlice const& options, 
                                             bool forceIsSystem)
    : VocbaseCollectionInfo(vocbase, name, TRI_COL_TYPE_DOCUMENT, options,
                            forceIsSystem) {}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             std::string const& name,
                                             TRI_col_type_e type,
                                             VPackSlice const& options,
                                             bool forceIsSystem)
    : _type(type),
      _revision(0),
      _cid(0),
      _planId(0),
      _maximalSize(32 * 1024 * 1024), // just to have a default
      _initialCount(-1),
      _indexBuckets(DatabaseFeature::DefaultIndexBuckets),
      _keyOptions(nullptr),
      _isSystem(false),
      _deleted(false),
      _doCompact(true),
      _isVolatile(false),
      _waitForSync(false) {
  
  auto database = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
  _maximalSize = database->maximalJournalSize();
  _waitForSync = database->waitForSync();

  memset(_name, 0, sizeof(_name));

  TRI_CopyString(_name, name.c_str(), sizeof(_name) - 1);

  if (options.isObject()) {
    TRI_voc_size_t maximalSize;
    if (options.hasKey("journalSize")) {
      maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              options, "journalSize", _maximalSize);
    } else {
      maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              options, "maximalSize", _maximalSize);
    }

    size_t pageSize = PageSizeFeature::getPageSize();
    _maximalSize =
        static_cast<TRI_voc_size_t>((maximalSize / pageSize) * pageSize);
    if (_maximalSize == 0 && maximalSize != 0) {
      _maximalSize = static_cast<TRI_voc_size_t>(pageSize);
    }

    if (options.hasKey("count")) {
      _initialCount =
          arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
              options, "count", -1);
    }

    _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "doCompact", true);
    _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "waitForSync", _waitForSync);
    _isVolatile = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "isVolatile", false);
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            options, "indexBuckets", DatabaseFeature::DefaultIndexBuckets);
    _type = static_cast<TRI_col_type_e>(
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
            options, "type", _type));

    std::string cname =
        arangodb::basics::VelocyPackHelper::getStringValue(options, "name", "");
    if (!cname.empty()) {
      TRI_CopyString(_name, cname.c_str(), sizeof(_name) - 1);
    }

    std::string cidString =
        arangodb::basics::VelocyPackHelper::getStringValue(options, "cid", "");
    if (!cidString.empty()) {
      // note: this may throw
      _cid = std::stoull(cidString);
    }

    if (options.hasKey("isSystem")) {
      VPackSlice isSystemSlice = options.get("isSystem");
      if (isSystemSlice.isBoolean()) {
        _isSystem = isSystemSlice.getBoolean();
      }
    } else {
      _isSystem = false;
    }

    if (options.hasKey("journalSize")) {
      VPackSlice maxSizeSlice = options.get("journalSize");
      TRI_voc_size_t maximalSize =
          maxSizeSlice.getNumericValue<TRI_voc_size_t>();
      if (maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "journalSize is too small");
      }
    }

    VPackSlice const planIdSlice = options.get("planId");
    TRI_voc_cid_t planId = 0;
    if (planIdSlice.isNumber()) {
      planId = planIdSlice.getNumericValue<TRI_voc_cid_t>();
    } else if (planIdSlice.isString()) {
      std::string tmp = planIdSlice.copyString();
      planId = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
    }

    if (planId > 0) {
      _planId = planId;
    }

    VPackSlice const cidSlice = options.get("id");
    if (cidSlice.isNumber()) {
      _cid = cidSlice.getNumericValue<TRI_voc_cid_t>();
    } else if (cidSlice.isString()) {
      std::string tmp = cidSlice.copyString();
      _cid = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
    }

    if (options.hasKey("keyOptions")) {
      VPackSlice const slice = options.get("keyOptions");
      VPackBuilder builder;
      builder.add(slice);
      // Copy the ownership of the options over
      _keyOptions = builder.steal();
    }

    if (options.hasKey("deleted")) {
      VPackSlice const slice = options.get("deleted");
      if (slice.isBoolean()) {
        _deleted = slice.getBoolean();
      }
    }
  }

#ifndef TRI_HAVE_ANONYMOUS_MMAP
  if (_isVolatile) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections are not supported on this platform");
  }
#endif

  if (_isVolatile && _waitForSync) {
    // the combination of waitForSync and isVolatile makes no sense
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections do not support the waitForSync option");
  }

  if (_indexBuckets < 1 || _indexBuckets > 1024) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "indexBuckets must be a two-power between 1 and 1024");
  }

  if (!TRI_collection_t::IsAllowedName(_isSystem || forceIsSystem, _name)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // fix _isSystem value if mis-specified by user
  _isSystem = (*_name == '_');
}

// collection type
TRI_col_type_e VocbaseCollectionInfo::type() const { return _type; }

// local collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::id() const { return _cid; }

// cluster-wide collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::planId() const { return _planId; }

// last revision id written
TRI_voc_rid_t VocbaseCollectionInfo::revision() const { return _revision; }

// maximal size of memory mapped file
TRI_voc_size_t VocbaseCollectionInfo::maximalSize() const {
  return _maximalSize;
}

// initial count, used when loading a collection
int64_t VocbaseCollectionInfo::initialCount() const { return _initialCount; }

// number of buckets used in hash tables for indexes
uint32_t VocbaseCollectionInfo::indexBuckets() const { return _indexBuckets; }

// name of the collection
std::string VocbaseCollectionInfo::name() const { return std::string(_name); }

// options for key creation
std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
VocbaseCollectionInfo::keyOptions() const {
  return _keyOptions;
}

// If true, collection has been deleted
bool VocbaseCollectionInfo::deleted() const { return _deleted; }

// If true, collection will be compacted
bool VocbaseCollectionInfo::doCompact() const { return _doCompact; }

// If true, collection is a system collection
bool VocbaseCollectionInfo::isSystem() const { return _isSystem; }

// If true, collection is memory-only
bool VocbaseCollectionInfo::isVolatile() const { return _isVolatile; }

// If true waits for mysnc
bool VocbaseCollectionInfo::waitForSync() const { return _waitForSync; }

void VocbaseCollectionInfo::rename(std::string const& name) {
  TRI_CopyString(_name, name.c_str(), sizeof(_name) - 1);
}

void VocbaseCollectionInfo::setRevision(TRI_voc_rid_t rid, bool force) {
  if (force || rid > _revision) {
    _revision = rid;
  }
}

void VocbaseCollectionInfo::setCollectionId(TRI_voc_cid_t cid) { _cid = cid; }

void VocbaseCollectionInfo::updateCount(size_t size) { _initialCount = size; }

void VocbaseCollectionInfo::setPlanId(TRI_voc_cid_t planId) {
  _planId = planId;
}

void VocbaseCollectionInfo::setDeleted(bool deleted) { _deleted = deleted; }

void VocbaseCollectionInfo::clearKeyOptions() { _keyOptions.reset(); }

void VocbaseCollectionInfo::update(VPackSlice const& slice, bool preferDefaults,
                                   TRI_vocbase_t const* vocbase) {
  // the following collection properties are intentionally not updated as
  // updating
  // them would be very complicated:
  // - _cid
  // - _name
  // - _type
  // - _isSystem
  // - _isVolatile
  // ... probably a few others missing here ...

  if (preferDefaults) {
    if (vocbase != nullptr) {
      auto database = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");

      _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "doCompact", true);
      _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync", database->waitForSync());
      if (slice.hasKey("journalSize")) {
        _maximalSize = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            slice, "journalSize", database->maximalJournalSize());
      } else {
        _maximalSize = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            slice, "maximalSize", database->maximalJournalSize());
      }
    } else {
      _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "doCompact", true);
      _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync", false);
      if (slice.hasKey("journalSize")) {
        _maximalSize =
            arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
                slice, "journalSize", TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE);
      } else {
        _maximalSize =
            arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
                slice, "maximalSize", TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE);
      }
    }
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            slice, "indexBuckets", DatabaseFeature::DefaultIndexBuckets);
  } else {
    _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "doCompact", _doCompact);
    _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "waitForSync", _waitForSync);
    if (slice.hasKey("journalSize")) {
      _maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              slice, "journalSize", _maximalSize);
    } else {
      _maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              slice, "maximalSize", _maximalSize);
    }
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            slice, "indexBuckets", _indexBuckets);

    _initialCount =
        arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
            slice, "count", _initialCount);
  }
}

void VocbaseCollectionInfo::update(VocbaseCollectionInfo const& other) {
  _type = other.type();
  _cid = other.id();
  _planId = other.planId();
  _revision = other.revision();
  _maximalSize = other.maximalSize();
  _initialCount = other.initialCount();
  _indexBuckets = other.indexBuckets();

  TRI_CopyString(_name, other.name().c_str(), sizeof(_name) - 1);

  _keyOptions = other.keyOptions();

  _deleted = other.deleted();
  _doCompact = other.doCompact();
  _isSystem = other.isSystem();
  _isVolatile = other.isVolatile();
  _waitForSync = other.waitForSync();
}

std::shared_ptr<VPackBuilder> VocbaseCollectionInfo::toVelocyPack() const {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  toVelocyPack(*builder);
  builder->close();
  return builder;
}

void VocbaseCollectionInfo::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());

  std::string cidString = std::to_string(id());
  std::string planIdString = std::to_string(planId());

  builder.add("version", VPackValue(version()));
  builder.add("type", VPackValue(type()));
  builder.add("cid", VPackValue(cidString));

  if (planId() > 0) {
    builder.add("planId", VPackValue(planIdString));
  }

  if (initialCount() >= 0) {
    builder.add("count", VPackValue(initialCount()));
  }
  builder.add("indexBuckets", VPackValue(indexBuckets()));
  builder.add("deleted", VPackValue(deleted()));
  builder.add("doCompact", VPackValue(doCompact()));
  builder.add("maximalSize", VPackValue(maximalSize()));
  builder.add("name", VPackValue(name()));
  builder.add("isVolatile", VPackValue(isVolatile()));
  builder.add("waitForSync", VPackValue(waitForSync()));
  builder.add("isSystem", VPackValue(isSystem()));

  auto opts = keyOptions();
  if (opts.get() != nullptr) {
    VPackSlice const slice(opts->data());
    builder.add("keyOptions", slice);
  }
}

/// @brief renames a collection
int TRI_collection_t::rename(std::string const& name) {
  // Save name for rollback
  std::string const oldName = _info.name();
  _info.rename(name);
  
  int res = TRI_ERROR_NO_ERROR;
  try {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    engine->renameCollection(_vocbase, _info.id(), name);
  } catch (basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // Rollback
    _info.rename(oldName);
  }

  return res;
}

/// @brief iterates over a collection
bool TRI_collection_t::iterateDatafiles(std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb) {
  if (!iterateDatafilesVector(_datafiles, cb) ||
      !iterateDatafilesVector(_compactors, cb) ||
      !iterateDatafilesVector(_journals, cb)) {
    return false;
  }
  return true;
}

/// @brief closes an open collection
int TRI_collection_t::close() {
  // close compactor files
  closeDataFiles(_compactors);

  // close journal files
  closeDataFiles(_journals);

  // close datafiles
  closeDataFiles(_datafiles);

  return TRI_ERROR_NO_ERROR;
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

  OpenIteratorState(LogicalCollection* collection,
                    TRI_vocbase_t* vocbase)
      : _collection(collection),
        _document(collection->collection()),
        _tid(0),
        _fid(0),
        _stats(),
        _dfi(nullptr),
        _vocbase(vocbase),
        _trx(nullptr),
        _deletions(0),
        _documents(0),
        _initialCount(-1) {}

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
  auto const fid = datafile->_fid;
  LogicalCollection* collection = state->_collection;
  TRI_collection_t* document = state->_document;
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

/// @brief process a deletion marker when opening a collection
static int OpenIteratorHandleDeletionMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            OpenIteratorState* state) {
  LogicalCollection* collection = state->_collection;
  TRI_collection_t* document = state->_document;
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
      FindDatafileStats(data, datafile->_fid);
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
                                    LogicalCollection* collection) {

  // FIXME All still reference collection()->_info.
  // initialize state for iteration
  OpenIteratorState openState(collection, collection->vocbase());

  if (collection->collection()->_info.initialCount() != -1) {
    auto primaryIndex = collection->primaryIndex();

    int res = primaryIndex->resize(
        trx, static_cast<size_t>(collection->collection()->_info.initialCount() * 1.1));

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    openState._initialCount = collection->collection()->_info.initialCount();
  }

  // read all documents and fill primary index
  auto cb = [&openState](TRI_df_marker_t const* marker, TRI_datafile_t* datafile) -> bool {
    return OpenIterator(marker, &openState, datafile); 
  };

  collection->collection()->iterateDatafiles(cb);

  LOG(TRACE) << "found " << openState._documents << " document markers, " << openState._deletions << " deletion markers for collection '" << collection->name() << "'";
  
  // update the real statistics for the collection
  try {
    for (auto& it : openState._stats) {
      collection->collection()->_datafileStatistics.create(it.first, *(it.second));
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
    TRI_vocbase_t* vocbase, VocbaseCollectionInfo& parameters,
    TRI_voc_cid_t cid) {
  if (cid > 0) {
    TRI_UpdateTickServer(cid);
  } else {
    cid = TRI_NewTickServer();
  }

  parameters.setCollectionId(cid);
      
  auto collection = std::make_unique<TRI_collection_t>(vocbase, parameters); 

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::string const path = engine->createCollection(vocbase, cid, parameters);
  collection->setPath(path);
  
  return collection.release();
}

