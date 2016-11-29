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

#include "LogfileManager.h"

#include "ApplicationFeatures/PageSizeFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Wal/AllocatorThread.h"
#include "Wal/CollectorThread.h"
#include "Wal/RecoverState.h"
#include "Wal/RemoverThread.h"
#include "Wal/Slots.h"
#include "Wal/SynchronizerThread.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::wal;

// the logfile manager singleton
LogfileManager* LogfileManager::Instance = nullptr;

// minimum value for --wal.throttle-when-pending
static inline uint64_t MinThrottleWhenPending() { return 1024 * 1024; }

// minimum value for --wal.sync-interval
static inline uint64_t MinSyncInterval() { return 5; }

// minimum value for --wal.logfile-size
static inline uint32_t MinFileSize() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // this allows testing with smaller logfile-sizes
  return 1 * 1024 * 1024;
#else
  return 8 * 1024 * 1024;
#endif
}

// get the maximum size of a logfile entry
static inline uint32_t MaxEntrySize() {
  return 2 << 30;  // 2 GB
}

// minimum number of slots
static inline uint32_t MinSlots() { return 1024 * 8; }

// maximum number of slots
static inline uint32_t MaxSlots() { return 1024 * 1024 * 16; }

// create the logfile manager
LogfileManager::LogfileManager(ApplicationServer* server)
    : ApplicationFeature(server, "LogfileManager"),
      _recoverState(nullptr),
      _allowWrites(false),  // start in read-only mode
      _hasFoundLastTick(false),
      _inRecovery(true),
      _logfilesLock(),
      _logfiles(),
      _slots(nullptr),
      _synchronizerThread(nullptr),
      _allocatorThread(nullptr),
      _collectorThread(nullptr),
      _removerThread(nullptr),
      _lastOpenedId(0),
      _lastCollectedId(0),
      _lastSealedId(0),
      _shutdownFileLock(),
      _droppedCollections(),
      _droppedDatabases(),
      _idLock(),
      _writeThrottled(false),
      _shutdown(0) {
  LOG(TRACE) << "creating WAL logfile manager";
  TRI_ASSERT(!_allowWrites);

  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");
  startsAfter("EngineSelector");
  startsAfter("RevisionCache");

  for (auto const& it : EngineSelectorFeature::availableEngines()) {
    startsAfter(it.second);
  }
}

// destroy the logfile manager
LogfileManager::~LogfileManager() {
  LOG(TRACE) << "shutting down WAL logfile manager";

  for (auto& it : _barriers) {
    delete it.second;
  }

  _barriers.clear();

  delete _recoverState;
  delete _slots;

  for (auto& it : _logfiles) {
    if (it.second != nullptr) {
      delete it.second;
    }
  }
  
  Instance = nullptr;
}

void LogfileManager::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection(
      Section("wal", "Configure the WAL", "wal", false, false));

  options->addHiddenOption(
      "--wal.allow-oversize-entries",
      "allow entries that are bigger than '--wal.logfile-size'",
      new BooleanParameter(&_allowOversizeEntries));
  
  options->addHiddenOption(
      "--wal.use-mlock",
      "mlock WAL logfiles in memory (may require elevated privileges or limits)",
      new BooleanParameter(&_useMLock));

  options->addOption("--wal.directory", "logfile directory",
                     new StringParameter(&_directory));

  options->addOption(
      "--wal.historic-logfiles",
      "maximum number of historic logfiles to keep after collection",
      new UInt32Parameter(&_historicLogfiles));

  options->addOption(
      "--wal.ignore-logfile-errors",
      "ignore logfile errors. this will read recoverable data from corrupted "
      "logfiles but ignore any unrecoverable data",
      new BooleanParameter(&_ignoreLogfileErrors));

  options->addOption(
      "--wal.ignore-recovery-errors",
      "continue recovery even if re-applying operations fails",
      new BooleanParameter(&_ignoreRecoveryErrors));

  options->addOption("--wal.logfile-size", "size of each logfile (in bytes)",
                     new UInt32Parameter(&_filesize));

  options->addOption("--wal.open-logfiles",
                     "maximum number of parallel open logfiles",
                     new UInt32Parameter(&_maxOpenLogfiles));

  options->addOption("--wal.reserve-logfiles",
                     "maximum number of reserve logfiles to maintain",
                     new UInt32Parameter(&_reserveLogfiles));

  options->addHiddenOption("--wal.slots", "number of logfile slots to use",
                           new UInt32Parameter(&_numberOfSlots));

  options->addOption(
      "--wal.sync-interval",
      "interval for automatic, non-requested disk syncs (in milliseconds)",
      new UInt64Parameter(&_syncInterval));

  options->addHiddenOption(
      "--wal.throttle-when-pending",
      "throttle writes when at least this many operations are waiting for "
      "collection (set to 0 to deactivate write-throttling)",
      new UInt64Parameter(&_throttleWhenPending));

  options->addHiddenOption(
      "--wal.throttle-wait",
      "maximum wait time per operation when write-throttled (in milliseconds)",
      new UInt64Parameter(&_maxThrottleWait));
}

void LogfileManager::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  if (_filesize < MinFileSize()) {
    // minimum filesize per logfile
    LOG(FATAL) << "invalid value for --wal.logfile-size. Please use a value of "
                  "at least "
               << MinFileSize();
    FATAL_ERROR_EXIT();
  }

  if (_numberOfSlots < MinSlots() || _numberOfSlots > MaxSlots()) {
    // invalid number of slots
    LOG(FATAL) << "invalid value for --wal.slots. Please use a value between "
               << MinSlots() << " and " << MaxSlots();
    FATAL_ERROR_EXIT();
  }

  if (_throttleWhenPending > 0 &&
      _throttleWhenPending < MinThrottleWhenPending()) {
    LOG(FATAL) << "invalid value for --wal.throttle-when-pending. Please use a "
                  "value of at least "
               << MinThrottleWhenPending();
    FATAL_ERROR_EXIT();
  }

  if (_syncInterval < MinSyncInterval()) {
    LOG(FATAL) << "invalid value for --wal.sync-interval. Please use a value "
                  "of at least "
               << MinSyncInterval();
    FATAL_ERROR_EXIT();
  }

  // sync interval is specified in milliseconds by the user, but internally
  // we use microseconds
  _syncInterval = _syncInterval * 1000;
}
  
void LogfileManager::prepare() {
  auto databasePath = ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _databasePath = databasePath->directory();

  _shutdownFile = shutdownFilename();
  bool const shutdownFileExists = basics::FileUtils::exists(_shutdownFile);

  if (shutdownFileExists) {
    LOG(TRACE) << "shutdown file found";

    int res = readShutdownInfo();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(FATAL) << "could not open shutdown file '" << _shutdownFile
                 << "': " << TRI_errno_string(res);
      FATAL_ERROR_EXIT();
    }
  } else {
    LOG(TRACE) << "no shutdown file found";
  }
}

void LogfileManager::start() {
  Instance = this;

  // needs server initialized
  size_t pageSize = PageSizeFeature::getPageSize();
  _filesize = static_cast<uint32_t>(((_filesize + pageSize - 1) / pageSize) * pageSize);

  if (_directory.empty()) {
    // use global configuration variable
    _directory = _databasePath;

    // append "/journals"
    if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
      // append a trailing slash to directory name
      _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
    }

    _directory.append("journals");
  }

  if (_directory.empty()) {
    LOG(FATAL) << "no directory specified for WAL logfiles. Please use the "
                  "--wal.directory option";
    FATAL_ERROR_EXIT();
  }

  if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
    // append a trailing slash to directory name
    _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
  }
  
  // initialize some objects
  _slots = new Slots(this, _numberOfSlots, 0);
  _recoverState = new RecoverState(_ignoreRecoveryErrors);

  TRI_ASSERT(!_allowWrites);

  int res = inventory();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not create WAL logfile inventory: "
               << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  res = inspectLogfiles();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not inspect WAL logfiles: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  LOG(TRACE) << "WAL logfile manager configuration: historic logfiles: "
             << _historicLogfiles << ", reserve logfiles: " << _reserveLogfiles
             << ", filesize: " << _filesize
             << ", sync interval: " << _syncInterval;
}

bool LogfileManager::open() {
  // note all failed transactions that we found plus the list
  // of collections and databases that we can ignore
  {
    WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

    for (auto const& it : _recoverState->failedTransactions) {
      size_t bucket = getBucket(it.first);

      WRITE_LOCKER(locker, _transactions[bucket]._lock);

      _transactions[bucket]._failedTransactions.emplace(it.first);
    }

    _droppedDatabases = _recoverState->droppedDatabases;
    _droppedCollections = _recoverState->droppedCollections;
  }

  {
    // set every open logfile to a status of sealed
    WRITE_LOCKER(writeLocker, _logfilesLock);

    for (auto& it : _logfiles) {
      Logfile* logfile = it.second;

      if (logfile == nullptr) {
        continue;
      }

      Logfile::StatusType status = logfile->status();

      if (status == Logfile::StatusType::OPEN) {
        // set all logfiles to sealed status so they can be collected

        // we don't care about the previous status here
        logfile->forceStatus(Logfile::StatusType::SEALED);

        MUTEX_LOCKER(mutexLocker, _idLock);

        if (logfile->id() > _lastSealedId) {
          _lastSealedId = logfile->id();
        }
      }
    }
  }

  // now start allocator and synchronizer
  int res = startAllocatorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not start WAL allocator thread: "
               << TRI_errno_string(res);
    return false;
  }

  res = startSynchronizerThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not start WAL synchronizer thread: "
               << TRI_errno_string(res);
    return false;
  }

  // from now on, we allow writes to the logfile
  allowWrites(true);

  // explicitly abort any open transactions found in the logs
  res = _recoverState->abortOpenTransactions();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not abort open transactions: " << TRI_errno_string(res);
    return false;
  }

  // remove all empty logfiles
  _recoverState->removeEmptyLogfiles();

  // now fill secondary indexes of all collections used in the recovery
  _recoverState->fillIndexes();

  // remove usage locks for databases and collections
  _recoverState->releaseResources();

  // write the current state into the shutdown file
  writeShutdownInfo(false);

  // finished recovery
  _inRecovery = false;

  res = startCollectorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not start WAL collector thread: "
               << TRI_errno_string(res);
    return false;
  }

  TRI_ASSERT(_collectorThread != nullptr);

  res = startRemoverThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not start WAL remover thread: " << TRI_errno_string(res);
    return false;
  }

  // tell the allocator that the recovery is over now
  _allocatorThread->recoveryDone();

  // start compactor threads etc.
  auto databaseFeature = ApplicationServer::getFeature<DatabaseFeature>("Database");
  res = databaseFeature->recoveryDone();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "could not initialize databases: " << TRI_errno_string(res);
    return false;
  }

  return true;
}
    
void LogfileManager::stop() { 
  // deactivate write-throttling (again) on shutdown in case it was set again
  // after beginShutdown
  throttleWhenPending(0); 
}

void LogfileManager::beginShutdown() {
  throttleWhenPending(0); // deactivate write-throttling on shutdown
}

void LogfileManager::unprepare() {
  // deactivate write-throttling (again) on shutdown in case it was set again
  // after beginShutdown
  throttleWhenPending(0); 
  
  _shutdown = 1;

  LOG(TRACE) << "shutting down WAL";

  // set WAL to read-only mode
  allowWrites(false);

  // notify slots that we're shutting down
  _slots->shutdown();

  // finalize allocator thread
  // this prevents creating new (empty) WAL logfile once we flush
  // the current logfile
  stopAllocatorThread();

  if (_allocatorThread != nullptr) {
    LOG(TRACE) << "stopping allocator thread";
    while (_allocatorThread->isRunning()) {
      usleep(10000);
    }
    delete _allocatorThread;
    _allocatorThread = nullptr;
  }

  // do a final flush at shutdown
  this->flush(true, true, false);

  // stop other threads
  LOG(TRACE) << "sending shutdown request to WAL threads";
  stopRemoverThread();
  stopCollectorThread();
  stopSynchronizerThread();

  // physically destroy all threads
  if (_removerThread != nullptr) {
    LOG(TRACE) << "stopping remover thread";
    while (_removerThread->isRunning()) {
      usleep(10000);
    }
    delete _removerThread;
    _removerThread = nullptr;
  }

  if (_collectorThread != nullptr) {
    LOG(TRACE) << "stopping collector thread";
    while (_collectorThread->isRunning()) {
      usleep(10000);
    }
    delete _collectorThread;
    _collectorThread = nullptr;
  }

  if (_synchronizerThread != nullptr) {
    LOG(TRACE) << "stopping synchronizer thread";
    while (_synchronizerThread->isRunning()) {
      usleep(10000);
    }
    delete _synchronizerThread;
    _synchronizerThread = nullptr;
  }

  // close all open logfiles
  LOG(TRACE) << "closing logfiles";
  closeLogfiles();

  TRI_IF_FAILURE("LogfileManagerStop") {
    // intentionally kill the server
    TRI_SegfaultDebugging("LogfileManagerStop");
  }

  int res = writeShutdownInfo(true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "could not write WAL shutdown info: " << TRI_errno_string(res);
  }
}

// registers a transaction
int LogfileManager::registerTransaction(TRI_voc_tid_t transactionId) {
  auto lastCollectedId = _lastCollectedId.load();
  auto lastSealedId = _lastSealedId.load();

  TRI_IF_FAILURE("LogfileManagerRegisterTransactionOom") {
    // intentionally fail here
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  try {
    size_t bucket = getBucket(transactionId);
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
     
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    // insert into currently running list of transactions
    _transactions[bucket]._activeTransactions.emplace(transactionId, std::make_pair(lastCollectedId, lastSealedId));
    TRI_ASSERT(lastCollectedId <= lastSealedId);

    return TRI_ERROR_NO_ERROR;
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
}

// unregisters a transaction
void LogfileManager::unregisterTransaction(TRI_voc_tid_t transactionId,
                                           bool markAsFailed) {
  size_t bucket = getBucket(transactionId);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  _transactions[bucket]._activeTransactions.erase(transactionId);

  if (markAsFailed) {
    _transactions[bucket]._failedTransactions.emplace(transactionId);
  }
}

// return the set of failed transactions
std::unordered_set<TRI_voc_tid_t> LogfileManager::getFailedTransactions() {
  std::unordered_set<TRI_voc_tid_t> failedTransactions;

  {
    WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

    for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
      READ_LOCKER(locker, _transactions[bucket]._lock);

      for (auto const& it : _transactions[bucket]._failedTransactions) {
        failedTransactions.emplace(it);
      }
    }
  }

  return failedTransactions;
}

// return the set of dropped collections
/// this is used during recovery and not used afterwards
std::unordered_set<TRI_voc_cid_t> LogfileManager::getDroppedCollections() {
  std::unordered_set<TRI_voc_cid_t> droppedCollections;

  {
    READ_LOCKER(readLocker, _logfilesLock);
    droppedCollections = _droppedCollections;
  }

  return droppedCollections;
}

// return the set of dropped databases
/// this is used during recovery and not used afterwards
std::unordered_set<TRI_voc_tick_t> LogfileManager::getDroppedDatabases() {
  std::unordered_set<TRI_voc_tick_t> droppedDatabases;

  {
    READ_LOCKER(readLocker, _logfilesLock);
    droppedDatabases = _droppedDatabases;
  }

  return droppedDatabases;
}

// unregister a list of failed transactions
void LogfileManager::unregisterFailedTransactions(
    std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
    
  WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    std::for_each(failedTransactions.begin(), failedTransactions.end(),
                [&](TRI_voc_tid_t id) { _transactions[bucket]._failedTransactions.erase(id); });
  }
}

// whether or not it is currently allowed to create an additional
/// logfile
bool LogfileManager::logfileCreationAllowed(uint32_t size) {
  if (size + DatafileHelper::JournalOverhead() > filesize()) {
    // oversize entry. this is always allowed because otherwise everything would
    // lock
    return true;
  }

  if (_maxOpenLogfiles == 0) {
    return true;
  }

  uint32_t numberOfLogfiles = 0;

  // note: this information could also be cached instead of being recalculated
  // every time
  READ_LOCKER(readLocker, _logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    TRI_ASSERT(logfile != nullptr);

    if (logfile->status() == Logfile::StatusType::OPEN ||
        logfile->status() == Logfile::StatusType::SEAL_REQUESTED) {
      ++numberOfLogfiles;
    }
  }

  return (numberOfLogfiles <= _maxOpenLogfiles);
}

// whether or not there are reserve logfiles
bool LogfileManager::hasReserveLogfiles() {
  uint32_t numberOfLogfiles = 0;

  // note: this information could also be cached instead of being recalculated
  // every time
  READ_LOCKER(readLocker, _logfilesLock);

  // reverse-scan the logfiles map
  for (auto it = _logfiles.rbegin(); it != _logfiles.rend(); ++it) {
    Logfile* logfile = (*it).second;

    TRI_ASSERT(logfile != nullptr);

    if (logfile->freeSize() > 0 && !logfile->isSealed()) {
      if (++numberOfLogfiles >= reserveLogfiles()) {
        return true;
      }
    }
  }

  return false;
}

// signal that a sync operation is required
void LogfileManager::signalSync(bool waitForSync) {
  _synchronizerThread->signalSync(waitForSync);
}

// allocate space in a logfile for later writing
SlotInfo LogfileManager::allocate(uint32_t size) {
  TRI_ASSERT(size >= sizeof(TRI_df_marker_t));

  if (!_allowWrites) {
    // no writes allowed
    return SlotInfo(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (size > MaxEntrySize()) {
    // entry is too big
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  if (size > _filesize && !_allowOversizeEntries) {
    // entry is too big for a logfile
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  return _slots->nextUnused(size);
}

// allocate space in a logfile for later writing
SlotInfo LogfileManager::allocate(TRI_voc_tick_t databaseId,
                                  TRI_voc_cid_t collectionId, uint32_t size) {
  TRI_ASSERT(size >= sizeof(TRI_df_marker_t));

  if (!_allowWrites) {
    // no writes allowed
    return SlotInfo(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (size > MaxEntrySize()) {
    // entry is too big
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  if (size > _filesize && !_allowOversizeEntries) {
    // entry is too big for a logfile
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  return _slots->nextUnused(databaseId, collectionId, size);
}

// write data into the logfile, using database id and collection id
// this is a convenience function that combines allocate, memcpy and finalize
SlotInfoCopy LogfileManager::allocateAndWrite(TRI_voc_tick_t databaseId,
                                              TRI_voc_cid_t collectionId,
                                              Marker const* marker,
                                              bool wakeUpSynchronizer,
                                              bool waitForSyncRequested,
                                              bool waitUntilSyncDone) {
  TRI_ASSERT(marker != nullptr);
  SlotInfo slotInfo = allocate(databaseId, collectionId, marker->size());

  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    return SlotInfoCopy(slotInfo.errorCode);
  }
  
  return writeSlot(slotInfo, marker, wakeUpSynchronizer, waitForSyncRequested, waitUntilSyncDone);
}

// write data into the logfile
// this is a convenience function that combines allocate, memcpy and finalize
SlotInfoCopy LogfileManager::allocateAndWrite(Marker const* marker,
                                              bool wakeUpSynchronizer,
                                              bool waitForSyncRequested,
                                              bool waitUntilSyncDone) {
  TRI_ASSERT(marker != nullptr);
  SlotInfo slotInfo = allocate(marker->size());

  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    return SlotInfoCopy(slotInfo.errorCode);
  }

  return writeSlot(slotInfo, marker, wakeUpSynchronizer, waitForSyncRequested, waitUntilSyncDone);
}

// write marker into the logfile
// this is a convenience function with less parameters
SlotInfoCopy LogfileManager::allocateAndWrite(Marker const& marker, bool waitForSync) {
  return allocateAndWrite(&marker, true, waitForSync, waitForSync);
}

// memcpy the data into the WAL region and return the filled slot
// to the WAL logfile manager
SlotInfoCopy LogfileManager::writeSlot(SlotInfo& slotInfo,
                                       Marker const* marker,
                                       bool wakeUpSynchronizer,
                                       bool waitForSyncRequested,
                                       bool waitUntilSyncDone) {
  TRI_ASSERT(slotInfo.slot != nullptr);
  TRI_ASSERT(marker != nullptr);

  try {
    // write marker data into slot
    marker->store(static_cast<char*>(slotInfo.slot->mem()));
    slotInfo.slot->finalize(marker);

    // we must copy the slotinfo because Slots::returnUsed() will set the
    // internals of slotInfo.slot to 0 again
    SlotInfoCopy copy(slotInfo.slot);

    _slots->returnUsed(slotInfo, wakeUpSynchronizer, waitForSyncRequested, waitUntilSyncDone);
    return copy;
  } catch (...) {
    // if we don't return the slot we'll run into serious problems later
    _slots->returnUsed(slotInfo, false, false, false);

    return SlotInfoCopy(TRI_ERROR_INTERNAL);
  }
}

// wait for the collector queue to get cleared for the given collection
int LogfileManager::waitForCollectorQueue(TRI_voc_cid_t cid, double timeout) {
  double const end = TRI_microtime() + timeout;

  while (_collectorThread->hasQueuedOperations(cid)) {
    usleep(10000);

    if (TRI_microtime() > end) {
      return TRI_ERROR_LOCKED;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

// finalize and seal the currently open logfile
// this is useful to ensure that any open writes up to this point have made
// it into a logfile
int LogfileManager::flush(bool waitForSync, bool waitForCollector,
                          bool writeShutdownFile) {
  TRI_ASSERT(!_inRecovery);

  Logfile::IdType lastOpenLogfileId;
  Logfile::IdType lastSealedLogfileId;

  {
    MUTEX_LOCKER(mutexLocker, _idLock);
    lastOpenLogfileId = _lastOpenedId;
    lastSealedLogfileId = _lastSealedId;
  }

  if (lastOpenLogfileId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG(TRACE) << "about to flush active WAL logfile. currentLogfileId: "
             << lastOpenLogfileId << ", waitForSync: " << waitForSync
             << ", waitForCollector: " << waitForCollector;

  int res = _slots->flush(waitForSync);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
    LOG(ERR) << "unexpected error in WAL flush request: "
             << TRI_errno_string(res);
    return res;
  }

  if (waitForCollector) {
    double maxWaitTime = 0.0;  // this means wait forever
    if (_shutdown == 1) {
      maxWaitTime = 120.0;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      // we need to wait for the collector...
      // LOG(TRACE) << "entering waitForCollector with lastOpenLogfileId " << //
      // (unsigned long long) lastOpenLogfileId;
      res = this->waitForCollector(lastOpenLogfileId, maxWaitTime);
    } else if (res == TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
      // current logfile is empty and cannot be collected
      // we need to wait for the collector to collect the previously sealed
      // datafile

      if (lastSealedLogfileId > 0) {
        res = this->waitForCollector(lastSealedLogfileId, maxWaitTime);
      }
    }
  }

  if (writeShutdownFile &&
      (res == TRI_ERROR_NO_ERROR || res == TRI_ERROR_ARANGO_DATAFILE_EMPTY)) {
    // update the file with the last tick, last sealed etc.
    return writeShutdownInfo(false);
  }

  return res;
}

/// wait until all changes to the current logfile are synced
bool LogfileManager::waitForSync(double maxWait) {
  TRI_ASSERT(!_inRecovery);

  double const end = TRI_microtime() + maxWait;
  TRI_voc_tick_t lastAssignedTick = 0;

  while (true) {
    // fill the state
    LogfileManagerState state;
    _slots->statistics(state.lastAssignedTick, state.lastCommittedTick,
                       state.lastCommittedDataTick, state.numEvents, state.numEventsSync);

    if (lastAssignedTick == 0) {
      // get last assigned tick only once
      lastAssignedTick = state.lastAssignedTick;
    }

    // now compare last committed tick with first lastAssigned tick that we got
    if (state.lastCommittedTick >= lastAssignedTick) {
      // everything was already committed
      return true;
    }

    // not everything was committed yet. wait a bit
    usleep(10000);

    if (TRI_microtime() >= end) {
      // time's up!
      return false;
    }
  }
}

// re-inserts a logfile back into the inventory only
void LogfileManager::relinkLogfile(Logfile* logfile) {
  Logfile::IdType const id = logfile->id();

  WRITE_LOCKER(writeLocker, _logfilesLock);
  _logfiles.emplace(id, logfile);
}

// remove a logfile from the inventory only
bool LogfileManager::unlinkLogfile(Logfile* logfile) {
  Logfile::IdType const id = logfile->id();

  WRITE_LOCKER(writeLocker, _logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return false;
  }

  _logfiles.erase(it);

  return true;
}

// remove a logfile from the inventory only
Logfile* LogfileManager::unlinkLogfile(Logfile::IdType id) {
  WRITE_LOCKER(writeLocker, _logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return nullptr;
  }

  _logfiles.erase(it);

  return (*it).second;
}

// removes logfiles that are allowed to be removed
bool LogfileManager::removeLogfiles() {
  int iterations = 0;
  bool worked = false;

  while (++iterations < 6) {
    Logfile* logfile = getRemovableLogfile();

    if (logfile == nullptr) {
      break;
    }

    removeLogfile(logfile);
    worked = true;
  }

  return worked;
}

// sets the status of a logfile to open
void LogfileManager::setLogfileOpen(Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  WRITE_LOCKER(writeLocker, _logfilesLock);
  logfile->setStatus(Logfile::StatusType::OPEN);
}

// sets the status of a logfile to seal-requested
void LogfileManager::setLogfileSealRequested(Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(writeLocker, _logfilesLock);
    logfile->setStatus(Logfile::StatusType::SEAL_REQUESTED);
  }

  signalSync(true);
}

// sets the status of a logfile to sealed
void LogfileManager::setLogfileSealed(Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  setLogfileSealed(logfile->id());
}

// sets the status of a logfile to sealed
void LogfileManager::setLogfileSealed(Logfile::IdType id) {
  {
    WRITE_LOCKER(writeLocker, _logfilesLock);

    auto it = _logfiles.find(id);

    if (it == _logfiles.end()) {
      return;
    }

    (*it).second->setStatus(Logfile::StatusType::SEALED);
  }

  {
    MUTEX_LOCKER(mutexLocker, _idLock);
    _lastSealedId = id;
  }
}

// return the status of a logfile
Logfile::StatusType LogfileManager::getLogfileStatus(Logfile::IdType id) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return Logfile::StatusType::UNKNOWN;
  }

  return (*it).second->status();
}

// return the file descriptor of a logfile
int LogfileManager::getLogfileDescriptor(Logfile::IdType id) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    // error
    LOG(ERR) << "could not find logfile " << id;
    return -1;
  }

  Logfile* logfile = (*it).second;
  TRI_ASSERT(logfile != nullptr);

  return logfile->fd();
}

// get the current open region of a logfile
/// this uses the slots lock
void LogfileManager::getActiveLogfileRegion(Logfile* logfile,
                                            char const*& begin,
                                            char const*& end) {
  _slots->getActiveLogfileRegion(logfile, begin, end);
}

// garbage collect expired logfile barriers
void LogfileManager::collectLogfileBarriers() {
  auto now = TRI_microtime();

  WRITE_LOCKER(barrierLock, _barriersLock);

  for (auto it = _barriers.begin(); it != _barriers.end(); /* no hoisting */) {
    auto logfileBarrier = (*it).second;

    if (logfileBarrier->expires <= now) {
      LOG_TOPIC(TRACE, Logger::REPLICATION)
          << "garbage-collecting expired WAL logfile barrier "
          << logfileBarrier->id;

      it = _barriers.erase(it);
      delete logfileBarrier;
    } else {
      ++it;
    }
  }
}

// returns a list of all logfile barrier ids
std::vector<TRI_voc_tick_t> LogfileManager::getLogfileBarriers() {
  std::vector<TRI_voc_tick_t> result;

  {
    READ_LOCKER(barrierLock, _barriersLock);
    result.reserve(_barriers.size());

    for (auto& it : _barriers) {
      result.emplace_back(it.second->id);
    }
  }

  return result;
}

// remove a specific logfile barrier
bool LogfileManager::removeLogfileBarrier(TRI_voc_tick_t id) {
  LogfileBarrier* logfileBarrier = nullptr;
  {
    WRITE_LOCKER(barrierLock, _barriersLock);

    auto it = _barriers.find(id);

    if (it == _barriers.end()) {
      return false;
    }

    logfileBarrier = (*it).second;
    _barriers.erase(it);
  }

  TRI_ASSERT(logfileBarrier != nullptr);
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "removing WAL logfile barrier "
                                        << logfileBarrier->id;

  delete logfileBarrier;

  return true;
}

// adds a barrier that prevents removal of logfiles
TRI_voc_tick_t LogfileManager::addLogfileBarrier(TRI_voc_tick_t minTick,
                                                 double ttl) {
  TRI_voc_tick_t id = TRI_NewTickServer();
  double expires = TRI_microtime() + ttl;

  auto logfileBarrier = std::make_unique<LogfileBarrier>(id, expires, minTick);
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "adding WAL logfile barrier "
                                        << logfileBarrier->id
                                        << ", minTick: " << minTick;

  {
    WRITE_LOCKER(barrierLock, _barriersLock);
    _barriers.emplace(id, logfileBarrier.get());
  }

  logfileBarrier.release();

  return id;
}

// extend the lifetime of a logfile barrier
bool LogfileManager::extendLogfileBarrier(TRI_voc_tick_t id, double ttl,
                                          TRI_voc_tick_t tick) {
  WRITE_LOCKER(barrierLock, _barriersLock);

  auto it = _barriers.find(id);

  if (it == _barriers.end()) {
    return false;
  }

  auto logfileBarrier = (*it).second;
  logfileBarrier->expires = TRI_microtime() + ttl;

  if (tick > 0 && tick > logfileBarrier->minTick) {
    // patch tick
    logfileBarrier->minTick = tick;
  }

  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "extending WAL logfile barrier " << logfileBarrier->id
      << ", minTick: " << logfileBarrier->minTick;

  return true;
}

// get minimum tick value from all logfile barriers
TRI_voc_tick_t LogfileManager::getMinBarrierTick() {
  TRI_voc_tick_t value = 0;

  READ_LOCKER(barrierLock, _barriersLock);

  for (auto const& it : _barriers) {
    auto logfileBarrier = it.second;
    LOG_TOPIC(TRACE, Logger::REPLICATION)
        << "server has WAL logfile barrier " << logfileBarrier->id
        << ", minTick: " << logfileBarrier->minTick;

    if (value == 0 || value < logfileBarrier->minTick) {
      value = logfileBarrier->minTick;
    }
  }

  return value;
}

// get logfiles for a tick range
std::vector<Logfile*> LogfileManager::getLogfilesForTickRange(
    TRI_voc_tick_t minTick, TRI_voc_tick_t maxTick, bool& minTickIncluded) {
  std::vector<Logfile*> temp;
  std::vector<Logfile*> matching;

  minTickIncluded = false;

  // we need a two step logfile qualification procedure
  // this is to avoid holding the lock on _logfilesLock and then acquiring the
  // mutex on the _slots. If we hold both locks, we might deadlock with other
  // threads

  {
    READ_LOCKER(readLocker, _logfilesLock);
    temp.reserve(_logfiles.size());
    matching.reserve(_logfiles.size());

    for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
      Logfile* logfile = (*it).second;

      if (logfile == nullptr ||
          logfile->status() == Logfile::StatusType::EMPTY) {
        continue;
      }

      // found a datafile
      temp.emplace_back(logfile);

      // mark it as being used so it isn't deleted
      logfile->use();
    }
  }

  // now go on without the lock
  for (auto it = temp.begin(); it != temp.end(); ++it) {
    Logfile* logfile = (*it);

    TRI_voc_tick_t logMin;
    TRI_voc_tick_t logMax;
    _slots->getActiveTickRange(logfile, logMin, logMax);

    if (logMin <= minTick && logMin > 0) {
      minTickIncluded = true;
    }

    if (minTick > logMax || maxTick < logMin) {
      // datafile is older than requested range
      // or: datafile is newer than requested range

      // release the logfile, so it can be deleted
      logfile->release();
      continue;
    }

    // finally copy all qualifying logfiles into the result
    matching.push_back(logfile);
  }

  // all qualifying locks are marked as used now
  return matching;
}

// return logfiles for a tick range
void LogfileManager::returnLogfiles(std::vector<Logfile*> const& logfiles) {
  for (auto& logfile : logfiles) {
    logfile->release();
  }
}

// get a logfile by id
Logfile* LogfileManager::getLogfile(Logfile::IdType id) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it != _logfiles.end()) {
    return (*it).second;
  }

  return nullptr;
}

// get a logfile and its status by id
Logfile* LogfileManager::getLogfile(Logfile::IdType id,
                                    Logfile::StatusType& status) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it != _logfiles.end()) {
    status = (*it).second->status();
    return (*it).second;
  }

  status = Logfile::StatusType::UNKNOWN;

  return nullptr;
}

// get a logfile for writing. this may return nullptr
int LogfileManager::getWriteableLogfile(uint32_t size,
                                        Logfile::StatusType& status,
                                        Logfile*& result) {
  static uint64_t const SleepTime = 10 * 1000;
  static uint64_t const MaxIterations = 1500;
  size_t iterations = 0;
  bool haveSignalled = false;

  // always initialize the result
  result = nullptr;

  TRI_IF_FAILURE("LogfileManagerGetWriteableLogfile") {
    // intentionally don't return a logfile
    return TRI_ERROR_DEBUG;
  }

  while (++iterations < MaxIterations) {
    {
      WRITE_LOCKER(writeLocker, _logfilesLock);
      auto it = _logfiles.begin();

      while (it != _logfiles.end()) {
        Logfile* logfile = (*it).second;

        TRI_ASSERT(logfile != nullptr);

        if (logfile->isWriteable(size)) {
          // found a logfile, update the status variable and return the logfile

          {
            // LOG(TRACE) << "setting lastOpenedId " << // logfile->id();
            MUTEX_LOCKER(mutexLocker, _idLock);
            _lastOpenedId = logfile->id();
          }

          result = logfile;
          status = logfile->status();

          return TRI_ERROR_NO_ERROR;
        }

        if (logfile->status() == Logfile::StatusType::EMPTY &&
            !logfile->isWriteable(size)) {
          // we found an empty logfile, but the entry won't fit

          // delete the logfile from the sequence of logfiles
          _logfiles.erase(it++);

          // and physically remove the file
          // note: this will also delete the logfile object!
          removeLogfile(logfile);
        } else {
          ++it;
        }
      }
    }

    // signal & sleep outside the lock
    if (!haveSignalled) {
      _allocatorThread->signal(size);
      haveSignalled = true;
    }

    int res = _allocatorThread->waitForResult(SleepTime);

    if (res != TRI_ERROR_LOCK_TIMEOUT && res != TRI_ERROR_NO_ERROR) {
      TRI_ASSERT(result == nullptr);

      // some error occurred
      return res;
    }
  }

  TRI_ASSERT(result == nullptr);
  LOG(WARN) << "unable to acquire writeable WAL logfile after "
            << (MaxIterations * SleepTime) / 1000 << " ms";

  return TRI_ERROR_LOCK_TIMEOUT;
}

// get a logfile to collect. this may return nullptr
Logfile* LogfileManager::getCollectableLogfile() {
  // iterate over all active readers and find their minimum used logfile id
  Logfile::IdType minId = UINT64_MAX;

  { 
    WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

    // iterate over all active transactions and find their minimum used logfile
    // id
    for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
      READ_LOCKER(locker, _transactions[bucket]._lock);

      for (auto const& it : _transactions[bucket]._activeTransactions) {
        Logfile::IdType lastWrittenId = it.second.second;

        if (lastWrittenId < minId && lastWrittenId != 0) {
          minId = lastWrittenId;
        }
      }
    }
  }

  {
    READ_LOCKER(readLocker, _logfilesLock);

    for (auto& it : _logfiles) {
      auto logfile = it.second;

      if (logfile == nullptr) {
        continue;
      }

      if (logfile->id() <= minId && logfile->canBeCollected()) {
        return logfile;
      }

      if (logfile->id() > minId) {
        // abort early
        break;
      }
    }
  }

  return nullptr;
}

// get a logfile to remove. this may return nullptr
/// if it returns a logfile, the logfile is removed from the list of available
/// logfiles
Logfile* LogfileManager::getRemovableLogfile() {
  TRI_ASSERT(!_inRecovery);

  // take all barriers into account
  Logfile::IdType const minBarrierTick = getMinBarrierTick();

  Logfile::IdType minId = UINT64_MAX;

  {
    WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

    // iterate over all active readers and find their minimum used logfile id
    for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
      READ_LOCKER(locker, _transactions[bucket]._lock);

      for (auto const& it : _transactions[bucket]._activeTransactions) {
        Logfile::IdType lastCollectedId = it.second.first;

        if (lastCollectedId < minId && lastCollectedId != 0) {
          minId = lastCollectedId;
        }
      }
    }
  }

  {
    uint32_t numberOfLogfiles = 0;
    uint32_t const minHistoricLogfiles = historicLogfiles();
    Logfile* first = nullptr;

    WRITE_LOCKER(writeLocker, _logfilesLock);

    for (auto& it : _logfiles) {
      Logfile* logfile = it.second;

      // find the first logfile that can be safely removed
      if (logfile == nullptr) {
        continue;
      }

      if (logfile->id() <= minId && logfile->canBeRemoved() &&
          (minBarrierTick == 0 || (logfile->df()->_tickMin < minBarrierTick &&
                                   logfile->df()->_tickMax < minBarrierTick))) {
        // only check those logfiles that are outside the ranges specified by
        // barriers

        if (first == nullptr) {
          // note the oldest of the logfiles (_logfiles is a map, thus sorted)
          first = logfile;
        }

        if (++numberOfLogfiles > minHistoricLogfiles) {
          TRI_ASSERT(first != nullptr);
          _logfiles.erase(first->id());

          TRI_ASSERT(_logfiles.find(first->id()) == _logfiles.end());

          return first;
        }
      }
    }
  }

  return nullptr;
}

// increase the number of collect operations for a logfile
void LogfileManager::increaseCollectQueueSize(Logfile* logfile) {
  logfile->increaseCollectQueueSize();
}

// decrease the number of collect operations for a logfile
void LogfileManager::decreaseCollectQueueSize(Logfile* logfile) {
  logfile->decreaseCollectQueueSize();
}

// mark a file as being requested for collection
void LogfileManager::setCollectionRequested(Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(writeLocker, _logfilesLock);

    if (logfile->status() == Logfile::StatusType::COLLECTION_REQUESTED) {
      // the collector already asked for this file, but couldn't process it
      // due to some exception
      return;
    }

    logfile->setStatus(Logfile::StatusType::COLLECTION_REQUESTED);
  }

  if (!_inRecovery) {
    // to start collection
    _collectorThread->signal();
  }
}

// mark a file as being done with collection
void LogfileManager::setCollectionDone(Logfile* logfile) {
  TRI_IF_FAILURE("setCollectionDone") {
    return;
  }
  
  TRI_ASSERT(logfile != nullptr);
  Logfile::IdType id = logfile->id();

  // LOG(ERR) << "setCollectionDone setting lastCollectedId to " << (unsigned
  // long long) id;
  {
    WRITE_LOCKER(writeLocker, _logfilesLock);
    logfile->setStatus(Logfile::StatusType::COLLECTED);

    if (_useMLock) {
      logfile->unlockFromMemory();
    }
  }

  {
    MUTEX_LOCKER(mutexLocker, _idLock);
    _lastCollectedId = id;
  }

  if (!_inRecovery) {
    // to start removal of unneeded datafiles
    _collectorThread->signal();
    writeShutdownInfo(false);
  }
}

// force the status of a specific logfile
void LogfileManager::forceStatus(Logfile* logfile, Logfile::StatusType status) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(writeLocker, _logfilesLock);
    logfile->forceStatus(status);
  }
}

// return the current state
LogfileManagerState LogfileManager::state() {
  LogfileManagerState state;

  // now fill the state
  _slots->statistics(state.lastAssignedTick, state.lastCommittedTick,
                     state.lastCommittedDataTick, state.numEvents, state.numEventsSync);
  state.timeString = utilities::timeString();

  return state;
}

// return the current available logfile ranges
LogfileRanges LogfileManager::ranges() {
  LogfileRanges result;

  READ_LOCKER(readLocker, _logfilesLock);

  for (auto const& it : _logfiles) {
    Logfile* logfile = it.second;

    if (logfile == nullptr) {
      continue;
    }

    auto df = logfile->df();
    if (df->_tickMin == 0 && df->_tickMax == 0) {
      continue;
    }

    result.emplace_back(LogfileRange(it.first, logfile->filename(),
                                     logfile->statusText(), df->_tickMin,
                                     df->_tickMax));
  }

  return result;
}

// get information about running transactions
std::tuple<size_t, Logfile::IdType, Logfile::IdType>
LogfileManager::runningTransactions() {
  size_t count = 0;
  Logfile::IdType lastCollectedId = UINT64_MAX;
  Logfile::IdType lastSealedId = UINT64_MAX;

  {
    Logfile::IdType value;
    WRITE_LOCKER(readLocker, _allTransactionsLock);

    for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
      READ_LOCKER(locker, _transactions[bucket]._lock);

      count += _transactions[bucket]._activeTransactions.size();
      for (auto const& it : _transactions[bucket]._activeTransactions) {

        value = it.second.first;
        if (value < lastCollectedId && value != 0) {
          lastCollectedId = value;
        }

        value = it.second.second;
        if (value < lastSealedId && value != 0) {
          lastSealedId = value;
        }
      }
    }
  }

  return std::tuple<size_t, Logfile::IdType, Logfile::IdType>(
      count, lastCollectedId, lastSealedId);
}

// remove a logfile in the file system
void LogfileManager::removeLogfile(Logfile* logfile) {
  // old filename
  Logfile::IdType const id = logfile->id();
  std::string const filename = logfileName(id);

  LOG(TRACE) << "removing logfile '" << filename << "'";

  // now close the logfile
  delete logfile;

  int res = TRI_ERROR_NO_ERROR;
  // now physically remove the file

  if (!basics::FileUtils::remove(filename, &res)) {
    LOG(ERR) << "unable to remove logfile '" << filename
             << "': " << TRI_errno_string(res);
  }
}

void LogfileManager::waitForCollector() {
  if (_collectorThread == nullptr) {
    return;
  }

  while (_collectorThread->hasQueuedOperations()) {
    LOG(TRACE) << "waiting for WAL collector";
    usleep(50000);
  }
}

// wait until a specific logfile has been collected
int LogfileManager::waitForCollector(Logfile::IdType logfileId,
                                     double maxWaitTime) {
  static int64_t const SingleWaitPeriod = 50 * 1000;

  int64_t maxIterations = INT64_MAX;  // wait forever
  if (maxWaitTime > 0.0) {
    // if specified, wait for a shorter period of time
    maxIterations = static_cast<int64_t>(maxWaitTime * 1000000.0 /
                                         (double)SingleWaitPeriod);
    LOG(TRACE) << "will wait for max. " << maxWaitTime
               << " seconds for collector to finish";
  }

  LOG(TRACE) << "waiting for collector thread to collect logfile " << logfileId;

  // wait for the collector thread to finish the collection
  int64_t iterations = 0;

  while (++iterations < maxIterations) {
    if (_lastCollectedId >= logfileId) {
      return TRI_ERROR_NO_ERROR;
    }

    int res = _collectorThread->waitForResult(SingleWaitPeriod);

    // LOG(TRACE) << "still waiting for collector. logfileId: " << logfileId <<
    // " lastCollected:
    // " << // _lastCollectedId << ", result: " << res;

    if (res != TRI_ERROR_LOCK_TIMEOUT && res != TRI_ERROR_NO_ERROR) {
      // some error occurred
      return res;
    }

    // try again
  }

  // waited for too long
  return TRI_ERROR_LOCK_TIMEOUT;
}

// run the recovery procedure
// this is called after the logfiles have been scanned completely and
// recovery state has been build. additionally, all databases have been
// opened already so we can use collections
int LogfileManager::runRecovery() {
  TRI_ASSERT(!_allowWrites);

  if (!_recoverState->mustRecover()) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  if (_ignoreRecoveryErrors) {
    LOG(INFO) << "running WAL recovery ("
              << _recoverState->logfilesToProcess.size()
              << " logfiles), ignoring recovery errors";
  } else {
    LOG(INFO) << "running WAL recovery ("
              << _recoverState->logfilesToProcess.size() << " logfiles)";
  }

  // now iterate over all logfiles that we found during recovery
  // we can afford to iterate the files without _logfilesLock
  // this is because all other threads competing for the lock are
  // not active yet
  {
    int res = _recoverState->replayLogfiles();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  if (_recoverState->errorCount == 0) {
    LOG(INFO) << "WAL recovery finished successfully";
  } else {
    LOG(WARN) << "WAL recovery finished, some errors ignored due to settings";
  }

  return TRI_ERROR_NO_ERROR;
}

// closes all logfiles
void LogfileManager::closeLogfiles() {
  WRITE_LOCKER(writeLocker, _logfilesLock);

  for (auto& it : _logfiles) {
    Logfile* logfile = it.second;

    if (logfile != nullptr) {
      delete logfile;
    }
  }

  _logfiles.clear();
}

// reads the shutdown information
int LogfileManager::readShutdownInfo() {
  TRI_ASSERT(!_shutdownFile.empty());
  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(_shutdownFile);
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  VPackSlice slice = builder->slice();
  if (!slice.isObject()) {
    return TRI_ERROR_INTERNAL;
  }

  uint64_t lastTick =
      arangodb::basics::VelocyPackHelper::stringUInt64(slice.get("tick"));
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(lastTick));

  if (lastTick > 0) {
    _hasFoundLastTick = true;
  }
 
  // read last assigned revision id to seed HLC value 
  uint64_t hlc =
      arangodb::basics::VelocyPackHelper::stringUInt64(slice.get("hlc"));
  TRI_HybridLogicalClock(static_cast<TRI_voc_tick_t>(hlc));

  // read id of last collected logfile (maybe 0)
  uint64_t lastCollectedId = arangodb::basics::VelocyPackHelper::stringUInt64(
      slice.get("lastCollected"));

  // read if of last sealed logfile (maybe 0)
  uint64_t lastSealedId =
      arangodb::basics::VelocyPackHelper::stringUInt64(slice.get("lastSealed"));

  if (lastSealedId < lastCollectedId) {
    // should not happen normally
    lastSealedId = lastCollectedId;
  }

  std::string const shutdownTime =
      arangodb::basics::VelocyPackHelper::getStringValue(slice, "shutdownTime",
                                                         "");
  if (shutdownTime.empty()) {
    LOG(TRACE) << "no previous shutdown time found";
  } else {
    LOG(TRACE) << "previous shutdown was at '" << shutdownTime << "'";
  }

  {
    MUTEX_LOCKER(mutexLocker, _idLock);
    _lastCollectedId = static_cast<Logfile::IdType>(lastCollectedId);
    _lastSealedId = static_cast<Logfile::IdType>(lastSealedId);

    LOG(TRACE) << "initial values for WAL logfile manager: tick: " << lastTick
               << ", hlc: " << hlc 
               << ", lastCollected: " << _lastCollectedId.load()
               << ", lastSealed: " << _lastSealedId.load();
  }

  return TRI_ERROR_NO_ERROR;
}

// writes the shutdown information
/// this function is called at shutdown and at every logfile flush request
int LogfileManager::writeShutdownInfo(bool writeShutdownTime) {
  TRI_IF_FAILURE("LogfileManagerWriteShutdown") { return TRI_ERROR_DEBUG; }

  TRI_ASSERT(!_shutdownFile.empty());

  try {
    VPackBuilder builder;
    builder.openObject();

    // create local copies of the instance variables while holding the read lock
    Logfile::IdType lastCollectedId;
    Logfile::IdType lastSealedId;

    {
      MUTEX_LOCKER(mutexLocker, _idLock);
      lastCollectedId = _lastCollectedId;
      lastSealedId = _lastSealedId;
    }

    builder.add("tick", VPackValue(basics::StringUtils::itoa(TRI_CurrentTickServer())));
    builder.add("hlc", VPackValue(basics::StringUtils::itoa(TRI_HybridLogicalClock())));
    builder.add("lastCollected", VPackValue(basics::StringUtils::itoa(lastCollectedId)));
    builder.add("lastSealed", VPackValue(basics::StringUtils::itoa(lastSealedId)));

    if (writeShutdownTime) {
      std::string const t(utilities::timeString());
      builder.add("shutdownTime", VPackValue(t));
    }
    builder.close();

    bool ok;
    {
      // grab a lock so no two threads can write the shutdown info at the same
      // time
      MUTEX_LOCKER(mutexLocker, _shutdownFileLock);
      ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
          _shutdownFile, builder.slice(), true);
    }

    if (!ok) {
      LOG(ERR) << "unable to write WAL state file '" << _shutdownFile << "'";
      return TRI_ERROR_CANNOT_WRITE_FILE;
    }
  } catch (...) {
    LOG(ERR) << "unable to write WAL state file '" << _shutdownFile << "'";

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

// start the synchronizer thread
int LogfileManager::startSynchronizerThread() {
  _synchronizerThread = new SynchronizerThread(this, _syncInterval);

  if (!_synchronizerThread->start()) {
    delete _synchronizerThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the synchronizer thread
void LogfileManager::stopSynchronizerThread() {
  if (_synchronizerThread != nullptr) {
    LOG(TRACE) << "stopping WAL synchronizer thread";

    _synchronizerThread->beginShutdown();
  }
}

// start the allocator thread
int LogfileManager::startAllocatorThread() {
  _allocatorThread = new AllocatorThread(this);

  if (!_allocatorThread->start()) {
    delete _allocatorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the allocator thread
void LogfileManager::stopAllocatorThread() {
  if (_allocatorThread != nullptr) {
    LOG(TRACE) << "stopping WAL allocator thread";

    _allocatorThread->beginShutdown();
  }
}

// start the collector thread
int LogfileManager::startCollectorThread() {
  _collectorThread = new CollectorThread(this);

  if (!_collectorThread->start()) {
    delete _collectorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the collector thread
void LogfileManager::stopCollectorThread() {
  if (_collectorThread == nullptr) {
    return;
  }

  LOG(TRACE) << "stopping WAL collector thread";

  // wait for at most 5 seconds for the collector 
  // to catch up
  double const end = TRI_microtime() + 5.0;  
  while (TRI_microtime() < end) {
    bool canAbort = true;
    {
      READ_LOCKER(readLocker, _logfilesLock);
      for (auto& it : _logfiles) {
        Logfile* logfile = it.second;

        if (logfile == nullptr) {
          continue;
        }

        Logfile::StatusType status = logfile->status();

        if (status == Logfile::StatusType::SEAL_REQUESTED) {
          canAbort = false;
        }
      }
    }

    if (canAbort) {
      MUTEX_LOCKER(mutexLocker, _idLock);
      if (_lastSealedId == _lastCollectedId) {
        break;
      }
    }

    usleep(50000);
  }

  _collectorThread->beginShutdown();
}

// start the remover thread
int LogfileManager::startRemoverThread() {
  _removerThread = new RemoverThread(this);

  if (!_removerThread->start()) {
    delete _removerThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the remover thread
void LogfileManager::stopRemoverThread() {
  if (_removerThread != nullptr) {
    LOG(TRACE) << "stopping WAL remover thread";

    _removerThread->beginShutdown();
  }
}

// check which logfiles are present in the log directory
int LogfileManager::inventory() {
  int res = ensureDirectory();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  LOG(TRACE) << "scanning WAL directory: '" << _directory << "'";

  std::vector<std::string> files = basics::FileUtils::listFiles(_directory);

  for (auto it = files.begin(); it != files.end(); ++it) {
    std::string const file = (*it);

    if (StringUtils::isPrefix(file, "logfile-") &&
        StringUtils::isSuffix(file, ".db")) {
      Logfile::IdType const id =
          basics::StringUtils::uint64(file.substr(8, file.size() - 8 - 3));

      if (id == 0) {
        LOG(WARN) << "encountered invalid id for logfile '" << file
                  << "'. ids must be > 0";
      } else {
        // update global tick
        TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

        WRITE_LOCKER(writeLocker, _logfilesLock);
        _logfiles.emplace(id, nullptr);
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

// inspect the logfiles in the log directory
int LogfileManager::inspectLogfiles() {
  LOG(TRACE) << "inspecting WAL logfiles";

  WRITE_LOCKER(writeLocker, _logfilesLock);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // print an inventory
  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr) {
      std::string const logfileName = logfile->filename();
      LOG(DEBUG) << "logfile " << logfile->id() << ", filename '" << logfileName
                 << "', status " << logfile->statusText();
    }
  }
#endif

  for (auto it = _logfiles.begin(); it != _logfiles.end(); /* no hoisting */) {
    Logfile::IdType const id = (*it).first;
    std::string const filename = logfileName(id);

    TRI_ASSERT((*it).second == nullptr);

    int res = Logfile::judge(filename);

    if (res == TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
      _recoverState->emptyLogfiles.push_back(filename);
      _logfiles.erase(it++);
      continue;
    }

    bool const wasCollected = (id <= _lastCollectedId);
    Logfile* logfile =
        Logfile::openExisting(filename, id, wasCollected, _ignoreLogfileErrors);

    if (logfile == nullptr) {
      // an error happened when opening a logfile
      if (!_ignoreLogfileErrors) {
        // we don't ignore errors, so we abort here
        int res = TRI_errno();

        if (res == TRI_ERROR_NO_ERROR) {
          // must have an error!
          res = TRI_ERROR_ARANGO_DATAFILE_UNREADABLE;
        }
        return res;
      }

      _logfiles.erase(it++);
      continue;
    }

    if (logfile->status() == Logfile::StatusType::OPEN ||
        logfile->status() == Logfile::StatusType::SEALED) {
      _recoverState->logfilesToProcess.push_back(logfile);
    }

    LOG(TRACE) << "inspecting logfile " << logfile->id() << " ("
               << logfile->statusText() << ")";

    TRI_datafile_t* df = logfile->df();
    df->sequentialAccess();

    // update the tick statistics
    if (!TRI_IterateDatafile(df, &RecoverState::InitialScanMarker,
                             static_cast<void*>(_recoverState))) {
      std::string const logfileName = logfile->filename();
      LOG(WARN) << "WAL inspection failed when scanning logfile '"
                << logfileName << "'";
      return TRI_ERROR_ARANGO_RECOVERY;
    }

    LOG(TRACE) << "inspected logfile " << logfile->id() << " ("
               << logfile->statusText()
               << "), tickMin: " << df->_tickMin
               << ", tickMax: " << df->_tickMax;

    if (logfile->status() == Logfile::StatusType::SEALED) {
      // If it is sealed, switch to random access:
      df->randomAccess();
    }

    {
      MUTEX_LOCKER(mutexLocker, _idLock);
      if (logfile->status() == Logfile::StatusType::SEALED &&
          id > _lastSealedId) {
        _lastSealedId = id;
      }

      if ((logfile->status() == Logfile::StatusType::SEALED ||
           logfile->status() == Logfile::StatusType::OPEN) &&
          id > _lastOpenedId) {
        _lastOpenedId = id;
      }
    }

    (*it).second = logfile;
    ++it;
  }

  // update the tick with the max tick we found in the WAL
  TRI_UpdateTickServer(_recoverState->lastTick);
    
  // use maximum revision value found from WAL to adjust HLC value
  // should it be lower
  LOG(TRACE) << "setting max HLC value to " << _recoverState->maxRevisionId;
  TRI_HybridLogicalClock(_recoverState->maxRevisionId);

  return TRI_ERROR_NO_ERROR;
}

// allocates a new reserve logfile
int LogfileManager::createReserveLogfile(uint32_t size) {
  Logfile::IdType const id = nextId();
  std::string const filename = logfileName(id);

  LOG(TRACE) << "creating empty logfile '" << filename << "' with size "
             << size;

  uint32_t realsize;
  if (size > 0 && size > filesize()) {
    // create a logfile with the requested size
    realsize = size + DatafileHelper::JournalOverhead();
  } else {
    // create a logfile with default size
    realsize = filesize();
  }

  std::unique_ptr<Logfile> logfile(Logfile::createNew(filename, id, realsize));

  if (logfile == nullptr) {
    int res = TRI_errno();

    LOG(ERR) << "unable to create logfile: " << TRI_errno_string(res);
    return res;
  }

  if (_useMLock) {
    logfile->lockInMemory();
  }

  {
    WRITE_LOCKER(writeLocker, _logfilesLock);
    _logfiles.emplace(id, logfile.get());
  }
  logfile.release();

  return TRI_ERROR_NO_ERROR;
}

// get an id for the next logfile
Logfile::IdType LogfileManager::nextId() {
  return static_cast<Logfile::IdType>(TRI_NewTickServer());
}

// ensure the wal logfiles directory is actually there
int LogfileManager::ensureDirectory() {
  // strip directory separator from path
  // this is required for Windows
  std::string directory(_directory);

  TRI_ASSERT(!directory.empty());

  if (directory[directory.size() - 1] == TRI_DIR_SEPARATOR_CHAR) {
    directory = directory.substr(0, directory.size() - 1);
  }

  if (!basics::FileUtils::isDirectory(directory)) {
    LOG(INFO) << "WAL directory '" << directory
              << "' does not exist. creating it...";

    int res;
    if (!basics::FileUtils::createDirectory(directory, &res)) {
      LOG(ERR) << "could not create WAL directory: '" << directory
               << "': " << TRI_last_error();
      return TRI_ERROR_SYS_ERROR;
    }
  }

  if (!basics::FileUtils::isDirectory(directory)) {
    LOG(ERR) << "WAL directory '" << directory << "' does not exist";
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

// return the absolute name of the shutdown file
std::string LogfileManager::shutdownFilename() const {
  return _databasePath + TRI_DIR_SEPARATOR_STR + "SHUTDOWN";
}

// return an absolute filename for a logfile id
std::string LogfileManager::logfileName(Logfile::IdType id) const {
  return _directory + std::string("logfile-") + basics::StringUtils::itoa(id) +
         std::string(".db");
}
