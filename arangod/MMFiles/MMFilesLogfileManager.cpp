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

#include "MMFilesLogfileManager.h"

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
#include "RestServer/TransactionManagerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "MMFiles/MMFilesAllocatorThread.h"
#include "MMFiles/MMFilesCollectorThread.h"
#include "MMFiles/MMFilesRemoverThread.h"
#include "MMFiles/MMFilesWalMarker.h"
#include "MMFiles/MMFilesWalRecoverState.h"
#include "MMFiles/MMFilesWalSlots.h"
#include "MMFiles/MMFilesSynchronizerThread.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

// the logfile manager singleton
MMFilesLogfileManager* MMFilesLogfileManager::Instance = nullptr;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
bool MMFilesLogfileManager::SafeToUseInstance = false;
#endif

// whether or not there was a SHUTDOWN file with a last tick at
// server start
int MMFilesLogfileManager::FoundLastTick = -1;

namespace {
// minimum value for --wal.throttle-when-pending
static constexpr uint64_t MinThrottleWhenPending() { return 1024 * 1024; }

// minimum value for --wal.sync-interval
static constexpr uint64_t MinSyncInterval() { return 5; }

// minimum value for --wal.logfile-size
static constexpr uint32_t MinFileSize() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // this allows testing with smaller logfile-sizes
  return 1 * 1024 * 1024;
#else
  return 8 * 1024 * 1024;
#endif
}

// get the maximum size of a logfile entry
static constexpr uint32_t MaxEntrySize() {
  return 2 << 30;  // 2 GB
}

// minimum number of slots
static constexpr uint32_t MinSlots() { return 1024 * 8; }

// maximum number of slots
static constexpr uint32_t MaxSlots() { return 1024 * 1024 * 16; }
}

// create the logfile manager
MMFilesLogfileManager::MMFilesLogfileManager(ApplicationServer* server)
    : ApplicationFeature(server, "MMFilesLogfileManager"),
      _allowWrites(false),  // start in read-only mode
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
  TRI_ASSERT(!_allowWrites);

  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("DatabasePath");
  startsAfter("EngineSelector");
  startsAfter("FeatureCache");
  startsAfter("MMFilesEngine");
  
  startsBefore("Aql");
  startsBefore("Bootstrap");
  startsBefore("GeneralServer");
  startsBefore("QueryRegistry");
  startsBefore("TraverserEngineRegistry");
  
  onlyEnabledWith("MMFilesEngine");
}

// destroy the logfile manager
MMFilesLogfileManager::~MMFilesLogfileManager() {
  for (auto& it : _barriers) {
    delete it.second;
  }

  _barriers.clear();

  delete _slots;

  for (auto& it : _logfiles) {
    if (it.second != nullptr) {
      delete it.second;
    }
  }
  
  Instance = nullptr;
}

void MMFilesLogfileManager::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection(
      Section("wal", "Configure the WAL of the MMFiles engine", "wal", false, false));

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
  
  options->addHiddenOption("--wal.flush-timeout", "flush timeout (in milliseconds)",
                     new UInt64Parameter(&_flushTimeout));

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

void MMFilesLogfileManager::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  if (_filesize < MinFileSize()) {
    // minimum filesize per logfile
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for --wal.logfile-size. Please use a value of "
                  "at least "
               << MinFileSize();
    FATAL_ERROR_EXIT();
  }

  if (_numberOfSlots < MinSlots() || _numberOfSlots > MaxSlots()) {
    // invalid number of slots
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for --wal.slots. Please use a value between "
               << MinSlots() << " and " << MaxSlots();
    FATAL_ERROR_EXIT();
  }

  if (_throttleWhenPending > 0 &&
      _throttleWhenPending < MinThrottleWhenPending()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for --wal.throttle-when-pending. Please use a "
                  "value of at least "
               << MinThrottleWhenPending();
    FATAL_ERROR_EXIT();
  }

  if (_syncInterval < MinSyncInterval()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for --wal.sync-interval. Please use a value "
                  "of at least "
               << MinSyncInterval();
    FATAL_ERROR_EXIT();
  }

  // sync interval is specified in milliseconds by the user, but internally
  // we use microseconds
  _syncInterval = _syncInterval * 1000;
}
  
void MMFilesLogfileManager::prepare() {
  Instance = this;
  FoundLastTick = 0; // initialize the last found tick value to "not found"

  auto databasePath = ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _databasePath = databasePath->directory();

  _shutdownFile = shutdownFilename();
  bool const shutdownFileExists = basics::FileUtils::exists(_shutdownFile);

  if (shutdownFileExists) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "shutdown file found";

    int res = readShutdownInfo();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not open shutdown file '" << _shutdownFile
                 << "': " << TRI_errno_string(res);
      FATAL_ERROR_EXIT();
    }
  } else {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "no shutdown file found";
  }
}

void MMFilesLogfileManager::start() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  SafeToUseInstance = true;
#endif

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
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "no directory specified for WAL logfiles. Please use the "
                  "'--wal.directory' option";
    FATAL_ERROR_EXIT();
  }

  if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
    // append a trailing slash to directory name
    _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
  }
  
  // initialize some objects
  _slots = new MMFilesWalSlots(this, _numberOfSlots, 0);
  _recoverState.reset(new MMFilesWalRecoverState(_ignoreRecoveryErrors));

  TRI_ASSERT(!_allowWrites);

  int res = inventory();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not create WAL logfile inventory: "
               << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  res = inspectLogfiles();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not inspect WAL logfiles: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "WAL logfile manager configuration: historic logfiles: "
             << _historicLogfiles << ", reserve logfiles: " << _reserveLogfiles
             << ", filesize: " << _filesize
             << ", sync interval: " << _syncInterval;
}

bool MMFilesLogfileManager::open() {
  // note all failed transactions that we found plus the list
  // of collections and databases that we can ignore
 
  std::unordered_set<TRI_voc_tid_t> failedTransactions;
  for (auto const& it : _recoverState->failedTransactions) {
    failedTransactions.emplace(it.first);
  }
  TransactionManagerFeature::manager()->registerFailedTransactions(failedTransactions);
  _droppedDatabases = _recoverState->droppedDatabases;
  _droppedCollections = _recoverState->droppedCollections;

  {
    // set every open logfile to a status of sealed
    WRITE_LOCKER(writeLocker, _logfilesLock);

    for (auto& it : _logfiles) {
      MMFilesWalLogfile* logfile = it.second;

      if (logfile == nullptr) {
        continue;
      }

      MMFilesWalLogfile::StatusType status = logfile->status();

      if (status == MMFilesWalLogfile::StatusType::OPEN) {
        // set all logfiles to sealed status so they can be collected

        // we don't care about the previous status here
        logfile->forceStatus(MMFilesWalLogfile::StatusType::SEALED);

        MUTEX_LOCKER(mutexLocker, _idLock);

        if (logfile->id() > _lastSealedId) {
          _lastSealedId = logfile->id();
        }
      }
    }
  }

  // now start allocator and synchronizer
  int res = startMMFilesAllocatorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not start WAL allocator thread: "
               << TRI_errno_string(res);
    return false;
  }

  res = startMMFilesSynchronizerThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not start WAL synchronizer thread: "
               << TRI_errno_string(res);
    return false;
  }

  // from now on, we allow writes to the logfile
  allowWrites(true);

  // explicitly abort any open transactions found in the logs
  res = _recoverState->abortOpenTransactions();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not abort open transactions: " << TRI_errno_string(res);
    return false;
  }

  // remove all empty logfiles
  _recoverState->removeEmptyLogfiles();

  // now fill secondary indexes of all collections used in the recovery
  _recoverState->fillIndexes();

  // remove usage locks for databases and collections
  _recoverState->releaseResources();

  // not needed anymore
  _recoverState.reset();

  // write the current state into the shutdown file
  writeShutdownInfo(false);

  // finished recovery
  _inRecovery = false;

  res = startMMFilesCollectorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not start WAL collector thread: "
               << TRI_errno_string(res);
    return false;
  }

  TRI_ASSERT(_collectorThread != nullptr);

  res = startMMFilesRemoverThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not start WAL remover thread: " << TRI_errno_string(res);
    return false;
  }

  // tell the allocator that the recovery is over now
  _allocatorThread->recoveryDone();

  return true;
}

void MMFilesLogfileManager::beginShutdown() {
  if (!isEnabled()) {
    return;
  }
  throttleWhenPending(0); // deactivate write-throttling on shutdown
}

void MMFilesLogfileManager::stop() { 
  if (!isEnabled()) {
    return;
  }
  // deactivate write-throttling (again) on shutdown in case it was set again
  // after beginShutdown
  throttleWhenPending(0); 
}

void MMFilesLogfileManager::unprepare() {
  if (!isEnabled()) {
    return;
  }

  // deactivate write-throttling (again) on shutdown in case it was set again
  // after beginShutdown
  throttleWhenPending(0); 
  
  _shutdown = 1;

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "shutting down WAL";

  // set WAL to read-only mode
  allowWrites(false);

  // notify slots that we're shutting down
  _slots->shutdown();

  // finalize allocator thread
  // this prevents creating new (empty) WAL logfile once we flush
  // the current logfile
  stopMMFilesAllocatorThread();

  if (_allocatorThread != nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping allocator thread";
    while (_allocatorThread->isRunning()) {
      usleep(10000);
    }
    delete _allocatorThread;
    _allocatorThread = nullptr;
  }

  // do a final flush at shutdown
  if (!_inRecovery) {
    flush(true, true, false);
  }

  // stop other threads
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "sending shutdown request to WAL threads";
  stopMMFilesRemoverThread();
  stopMMFilesCollectorThread();
  stopMMFilesSynchronizerThread();

  // physically destroy all threads
  if (_removerThread != nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping remover thread";
    while (_removerThread->isRunning()) {
      usleep(10000);
    }
    delete _removerThread;
    _removerThread = nullptr;
  }

  {
    WRITE_LOCKER(locker, _collectorThreadLock);

    if (_collectorThread != nullptr) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping collector thread";
      _collectorThread->forceStop();
      while (_collectorThread->isRunning()) {
        usleep(10000);
      }
      delete _collectorThread;
      _collectorThread = nullptr;
    }
  }

  if (_synchronizerThread != nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping synchronizer thread";
    while (_synchronizerThread->isRunning()) {
      usleep(10000);
    }
    delete _synchronizerThread;
    _synchronizerThread = nullptr;
  }

  // close all open logfiles
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "closing logfiles";
  closeLogfiles();

  TRI_IF_FAILURE("LogfileManagerStop") {
    // intentionally kill the server
    TRI_SegfaultDebugging("MMFilesLogfileManagerStop");
  }

  int res = writeShutdownInfo(true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not write WAL shutdown info: " << TRI_errno_string(res);
  }
}

// registers a transaction
int MMFilesLogfileManager::registerTransaction(TRI_voc_tid_t transactionId, bool isReadOnlyTransaction) {
  auto lastCollectedId = _lastCollectedId.load();
  auto lastSealedId = _lastSealedId.load();

  TRI_IF_FAILURE("LogfileManagerRegisterTransactionOom") {
    // intentionally fail here
    return TRI_ERROR_OUT_OF_MEMORY;
  }
    
  TRI_ASSERT(lastCollectedId <= lastSealedId);

  if (isReadOnlyTransaction) {
    // in case this is a read-only transaction, we are sure that the transaction can
    // only see committed data (as itself it will not write anything, and write transactions
    // run exclusively). we thus can allow the WAL collector to already seal and collect
    // logfiles. the only thing that needs to be ensured for read-only transactions is
    // that a logfile does not get thrown away while the read-only transaction is 
    // ongoing
    lastSealedId = 0;
  }

  try {
    auto data = std::make_unique<MMFilesTransactionData>(lastCollectedId, lastSealedId);
    TransactionManagerFeature::manager()->registerTransaction(transactionId, std::move(data));
    return TRI_ERROR_NO_ERROR;
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
}

// return the set of dropped collections
/// this is used during recovery and not used afterwards
std::unordered_set<TRI_voc_cid_t> MMFilesLogfileManager::getDroppedCollections() {
  std::unordered_set<TRI_voc_cid_t> droppedCollections;

  {
    READ_LOCKER(readLocker, _logfilesLock);
    droppedCollections = _droppedCollections;
  }

  return droppedCollections;
}

// return the set of dropped databases
/// this is used during recovery and not used afterwards
std::unordered_set<TRI_voc_tick_t> MMFilesLogfileManager::getDroppedDatabases() {
  std::unordered_set<TRI_voc_tick_t> droppedDatabases;

  {
    READ_LOCKER(readLocker, _logfilesLock);
    droppedDatabases = _droppedDatabases;
  }

  return droppedDatabases;
}

// whether or not it is currently allowed to create an additional
/// logfile
bool MMFilesLogfileManager::logfileCreationAllowed(uint32_t size) {
  if (size + MMFilesDatafileHelper::JournalOverhead() > filesize()) {
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
    MMFilesWalLogfile* logfile = (*it).second;

    TRI_ASSERT(logfile != nullptr);

    if (logfile->status() == MMFilesWalLogfile::StatusType::OPEN ||
        logfile->status() == MMFilesWalLogfile::StatusType::SEAL_REQUESTED) {
      ++numberOfLogfiles;
    }
  }

  return (numberOfLogfiles <= _maxOpenLogfiles);
}

// whether or not there are reserve logfiles
bool MMFilesLogfileManager::hasReserveLogfiles() {
  uint32_t numberOfLogfiles = 0;

  // note: this information could also be cached instead of being recalculated
  // every time
  READ_LOCKER(readLocker, _logfilesLock);

  // reverse-scan the logfiles map
  for (auto it = _logfiles.rbegin(); it != _logfiles.rend(); ++it) {
    MMFilesWalLogfile* logfile = (*it).second;

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
void MMFilesLogfileManager::signalSync(bool waitForSync) {
  _synchronizerThread->signalSync(waitForSync);
}

// allocate space in a logfile for later writing
MMFilesWalSlotInfo MMFilesLogfileManager::allocate(uint32_t size) {
  TRI_ASSERT(size >= sizeof(MMFilesMarker));

  if (!_allowWrites) {
    // no writes allowed
    return MMFilesWalSlotInfo(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (size > MaxEntrySize()) {
    // entry is too big
    return MMFilesWalSlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  if (size > _filesize && !_allowOversizeEntries) {
    // entry is too big for a logfile
    return MMFilesWalSlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  return _slots->nextUnused(size);
}

// allocate space in a logfile for later writing
MMFilesWalSlotInfo MMFilesLogfileManager::allocate(TRI_voc_tick_t databaseId,
                                  TRI_voc_cid_t collectionId, uint32_t size) {
  TRI_ASSERT(size >= sizeof(MMFilesMarker));

  if (!_allowWrites) {
    // no writes allowed
    return MMFilesWalSlotInfo(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (size > MaxEntrySize()) {
    // entry is too big
    return MMFilesWalSlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  if (size > _filesize && !_allowOversizeEntries) {
    // entry is too big for a logfile
    return MMFilesWalSlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  return _slots->nextUnused(databaseId, collectionId, size);
}

// write data into the logfile, using database id and collection id
// this is a convenience function that combines allocate, memcpy and finalize
MMFilesWalSlotInfoCopy MMFilesLogfileManager::allocateAndWrite(TRI_voc_tick_t databaseId,
                                              TRI_voc_cid_t collectionId,
                                              MMFilesWalMarker const* marker,
                                              bool wakeUpSynchronizer,
                                              bool waitForSyncRequested,
                                              bool waitUntilSyncDone) {
  TRI_ASSERT(marker != nullptr);
  MMFilesWalSlotInfo slotInfo = allocate(databaseId, collectionId, marker->size());

  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    return MMFilesWalSlotInfoCopy(slotInfo.errorCode);
  }
  
  return writeSlot(slotInfo, marker, wakeUpSynchronizer, waitForSyncRequested, waitUntilSyncDone);
}

// write data into the logfile
// this is a convenience function that combines allocate, memcpy and finalize
MMFilesWalSlotInfoCopy MMFilesLogfileManager::allocateAndWrite(MMFilesWalMarker const* marker,
                                              bool wakeUpSynchronizer,
                                              bool waitForSyncRequested,
                                              bool waitUntilSyncDone) {
  TRI_ASSERT(marker != nullptr);
  MMFilesWalSlotInfo slotInfo = allocate(marker->size());

  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    return MMFilesWalSlotInfoCopy(slotInfo.errorCode);
  }

  return writeSlot(slotInfo, marker, wakeUpSynchronizer, waitForSyncRequested, waitUntilSyncDone);
}

// write marker into the logfile
// this is a convenience function with less parameters
MMFilesWalSlotInfoCopy MMFilesLogfileManager::allocateAndWrite(MMFilesWalMarker const& marker, bool waitForSync) {
  return allocateAndWrite(&marker, true, waitForSync, waitForSync);
}

// memcpy the data into the WAL region and return the filled slot
// to the WAL logfile manager
MMFilesWalSlotInfoCopy MMFilesLogfileManager::writeSlot(MMFilesWalSlotInfo& slotInfo,
                                       MMFilesWalMarker const* marker,
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
    MMFilesWalSlotInfoCopy copy(slotInfo.slot);

    int res = _slots->returnUsed(slotInfo, wakeUpSynchronizer, waitForSyncRequested, waitUntilSyncDone);

    if (res == TRI_ERROR_NO_ERROR) {
      return copy;
    }
    return MMFilesWalSlotInfoCopy(res);
  } catch (...) {
    // if we don't return the slot we'll run into serious problems later
    int res = _slots->returnUsed(slotInfo, false, false, false);
    
    if (res != TRI_ERROR_NO_ERROR) {
      return MMFilesWalSlotInfoCopy(res);
    }

    return MMFilesWalSlotInfoCopy(TRI_ERROR_INTERNAL);
  }
}

// wait for the collector queue to get cleared for the given collection
int MMFilesLogfileManager::waitForCollectorQueue(TRI_voc_cid_t cid, double timeout) {
  double const end = TRI_microtime() + timeout;

  while (true) {
    READ_LOCKER(locker, _collectorThreadLock);

    if (_collectorThread == nullptr) {
      break;
    }
    
    if (!_collectorThread->hasQueuedOperations(cid)) {
      break;
    }

    // sleep without holding the lock
    locker.unlock();
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
int MMFilesLogfileManager::flush(bool waitForSync, bool waitForCollector,
                          bool writeShutdownFile) {
  TRI_ASSERT(!_inRecovery);

  MMFilesWalLogfile::IdType lastOpenLogfileId;
  MMFilesWalLogfile::IdType lastSealedLogfileId;

  {
    MUTEX_LOCKER(mutexLocker, _idLock);
    lastOpenLogfileId = _lastOpenedId;
    lastSealedLogfileId = _lastSealedId;
  }

  if (lastOpenLogfileId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "about to flush active WAL logfile. currentLogfileId: "
             << lastOpenLogfileId << ", waitForSync: " << waitForSync
             << ", waitForCollector: " << waitForCollector;

  int res = _slots->flush(waitForSync);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unexpected error in WAL flush request: "
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
      // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "entering waitForCollector with lastOpenLogfileId " << lastOpenLogfileId;
      res = this->waitForCollector(lastOpenLogfileId, maxWaitTime);

      if (res == TRI_ERROR_LOCK_TIMEOUT) {
        LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "got lock timeout when waiting for WAL flush. lastOpenLogfileId: " << lastOpenLogfileId;
      }
    } else if (res == TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
      // current logfile is empty and cannot be collected
      // we need to wait for the collector to collect the previously sealed
      // datafile

      if (lastSealedLogfileId > 0) {
        res = this->waitForCollector(lastSealedLogfileId, maxWaitTime);
      
        if (res == TRI_ERROR_LOCK_TIMEOUT) {
          LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "got lock timeout when waiting for WAL flush. lastSealedLogfileId: " << lastSealedLogfileId;
        }
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
bool MMFilesLogfileManager::waitForSync(double maxWait) {
  TRI_ASSERT(!_inRecovery);

  double const end = TRI_microtime() + maxWait;
  TRI_voc_tick_t lastAssignedTick = 0;

  while (true) {
    // fill the state
    MMFilesLogfileManagerState state;
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
void MMFilesLogfileManager::relinkLogfile(MMFilesWalLogfile* logfile) {
  MMFilesWalLogfile::IdType const id = logfile->id();

  WRITE_LOCKER(writeLocker, _logfilesLock);
  _logfiles.emplace(id, logfile);
}

// remove a logfile from the inventory only
bool MMFilesLogfileManager::unlinkLogfile(MMFilesWalLogfile* logfile) {
  MMFilesWalLogfile::IdType const id = logfile->id();

  WRITE_LOCKER(writeLocker, _logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return false;
  }

  _logfiles.erase(it);

  return true;
}

// remove a logfile from the inventory only
MMFilesWalLogfile* MMFilesLogfileManager::unlinkLogfile(MMFilesWalLogfile::IdType id) {
  WRITE_LOCKER(writeLocker, _logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return nullptr;
  }

  _logfiles.erase(it);

  return (*it).second;
}

// removes logfiles that are allowed to be removed
bool MMFilesLogfileManager::removeLogfiles() {
  int iterations = 0;
  bool worked = false;

  while (++iterations < 6) {
    MMFilesWalLogfile* logfile = getRemovableLogfile();

    if (logfile == nullptr) {
      break;
    }

    removeLogfile(logfile);
    worked = true;
  }

  return worked;
}

// sets the status of a logfile to open
void MMFilesLogfileManager::setLogfileOpen(MMFilesWalLogfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  WRITE_LOCKER(writeLocker, _logfilesLock);
  logfile->setStatus(MMFilesWalLogfile::StatusType::OPEN);
}

// sets the status of a logfile to seal-requested
void MMFilesLogfileManager::setLogfileSealRequested(MMFilesWalLogfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(writeLocker, _logfilesLock);
    logfile->setStatus(MMFilesWalLogfile::StatusType::SEAL_REQUESTED);
  }

  signalSync(true);
}

// sets the status of a logfile to sealed
void MMFilesLogfileManager::setLogfileSealed(MMFilesWalLogfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  setLogfileSealed(logfile->id());
}

// sets the status of a logfile to sealed
void MMFilesLogfileManager::setLogfileSealed(MMFilesWalLogfile::IdType id) {
  {
    WRITE_LOCKER(writeLocker, _logfilesLock);

    auto it = _logfiles.find(id);

    if (it == _logfiles.end()) {
      return;
    }

    (*it).second->setStatus(MMFilesWalLogfile::StatusType::SEALED);
  }

  {
    MUTEX_LOCKER(mutexLocker, _idLock);
    _lastSealedId = id;
  }
}

// return the status of a logfile
MMFilesWalLogfile::StatusType MMFilesLogfileManager::getLogfileStatus(MMFilesWalLogfile::IdType id) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return MMFilesWalLogfile::StatusType::UNKNOWN;
  }

  return (*it).second->status();
}

// return the file descriptor of a logfile
int MMFilesLogfileManager::getLogfileDescriptor(MMFilesWalLogfile::IdType id) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    // error
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not find logfile " << id;
    return -1;
  }

  MMFilesWalLogfile* logfile = (*it).second;
  TRI_ASSERT(logfile != nullptr);

  return logfile->fd();
}

// get the current open region of a logfile
/// this uses the slots lock
void MMFilesLogfileManager::getActiveLogfileRegion(MMFilesWalLogfile* logfile,
                                            char const*& begin,
                                            char const*& end) {
  _slots->getActiveLogfileRegion(logfile, begin, end);
}

// garbage collect expired logfile barriers
void MMFilesLogfileManager::collectLogfileBarriers() {
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
std::vector<TRI_voc_tick_t> MMFilesLogfileManager::getLogfileBarriers() {
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
bool MMFilesLogfileManager::removeLogfileBarrier(TRI_voc_tick_t id) {
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
TRI_voc_tick_t MMFilesLogfileManager::addLogfileBarrier(TRI_voc_tick_t minTick,
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
bool MMFilesLogfileManager::extendLogfileBarrier(TRI_voc_tick_t id, double ttl,
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
TRI_voc_tick_t MMFilesLogfileManager::getMinBarrierTick() {
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
std::vector<MMFilesWalLogfile*> MMFilesLogfileManager::getLogfilesForTickRange(
    TRI_voc_tick_t minTick, TRI_voc_tick_t maxTick, bool& minTickIncluded) {
  std::vector<MMFilesWalLogfile*> temp;
  std::vector<MMFilesWalLogfile*> matching;

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
      MMFilesWalLogfile* logfile = (*it).second;

      if (logfile == nullptr ||
          logfile->status() == MMFilesWalLogfile::StatusType::EMPTY) {
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
    MMFilesWalLogfile* logfile = (*it);

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
void MMFilesLogfileManager::returnLogfiles(std::vector<MMFilesWalLogfile*> const& logfiles) {
  for (auto& logfile : logfiles) {
    logfile->release();
  }
}

// get a logfile by id
MMFilesWalLogfile* MMFilesLogfileManager::getLogfile(MMFilesWalLogfile::IdType id) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it != _logfiles.end()) {
    return (*it).second;
  }

  return nullptr;
}

// get a logfile and its status by id
MMFilesWalLogfile* MMFilesLogfileManager::getLogfile(MMFilesWalLogfile::IdType id,
                                    MMFilesWalLogfile::StatusType& status) {
  READ_LOCKER(readLocker, _logfilesLock);

  auto it = _logfiles.find(id);

  if (it != _logfiles.end()) {
    status = (*it).second->status();
    return (*it).second;
  }

  status = MMFilesWalLogfile::StatusType::UNKNOWN;

  return nullptr;
}

// get a logfile for writing. this may return nullptr
int MMFilesLogfileManager::getWriteableLogfile(uint32_t size,
                                        MMFilesWalLogfile::StatusType& status,
                                        MMFilesWalLogfile*& result) {
  // always initialize the result
  result = nullptr;

  TRI_IF_FAILURE("LogfileManagerGetWriteableLogfile") {
    // intentionally don't return a logfile
    return TRI_ERROR_DEBUG;
  }
  
  size_t iterations = 0;
  double const end = TRI_microtime() + (_flushTimeout / 1000.0);

  while (true) {
    {
      WRITE_LOCKER(writeLocker, _logfilesLock);
      auto it = _logfiles.begin();

      while (it != _logfiles.end()) {
        MMFilesWalLogfile* logfile = (*it).second;

        TRI_ASSERT(logfile != nullptr);

        if (logfile->isWriteable(size)) {
          // found a logfile, update the status variable and return the logfile

          {
            // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "setting lastOpenedId " << logfile->id();
            MUTEX_LOCKER(mutexLocker, _idLock);
            _lastOpenedId = logfile->id();
          }

          result = logfile;
          status = logfile->status();

          return TRI_ERROR_NO_ERROR;
        }

        if (logfile->status() == MMFilesWalLogfile::StatusType::EMPTY &&
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
    if (++iterations % 10 == 1) {
      _allocatorThread->signal(size);
    }

    int res = _allocatorThread->waitForResult(15000);

    if (res != TRI_ERROR_LOCK_TIMEOUT && res != TRI_ERROR_NO_ERROR) {
      TRI_ASSERT(result == nullptr);

      // some error occurred
      return res;
    }

    if (TRI_microtime() > end) {
      // timeout
      break;
    }
  }

  TRI_ASSERT(result == nullptr);
  LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to acquire writeable WAL logfile after " << _flushTimeout << " ms";

  return TRI_ERROR_LOCK_TIMEOUT;
}

// get a logfile to collect. this may return nullptr
MMFilesWalLogfile* MMFilesLogfileManager::getCollectableLogfile() {
  // iterate over all active readers and find their minimum used logfile id
  MMFilesWalLogfile::IdType minId = UINT64_MAX;

  auto cb = [&minId](TRI_voc_tid_t, TransactionData const* data) {
    MMFilesWalLogfile::IdType lastWrittenId = static_cast<MMFilesTransactionData const*>(data)->lastSealedId;

    if (lastWrittenId < minId && lastWrittenId != 0) {
      minId = lastWrittenId;
    }
  };

  // iterate over all active transactions and find their minimum used logfile id
  TransactionManagerFeature::manager()->iterateActiveTransactions(cb);

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
MMFilesWalLogfile* MMFilesLogfileManager::getRemovableLogfile() {
  TRI_ASSERT(!_inRecovery);

  // take all barriers into account
  MMFilesWalLogfile::IdType const minBarrierTick = getMinBarrierTick();

  MMFilesWalLogfile::IdType minId = UINT64_MAX;
  
  // iterate over all active transactions and find their minimum used logfile id
  auto cb = [&minId](TRI_voc_tid_t, TransactionData const* data) {
    MMFilesWalLogfile::IdType lastCollectedId = static_cast<MMFilesTransactionData const*>(data)->lastCollectedId;

    if (lastCollectedId < minId && lastCollectedId != 0) {
      minId = lastCollectedId;
    }
  };

  TransactionManagerFeature::manager()->iterateActiveTransactions(cb);

  {
    uint32_t numberOfLogfiles = 0;
    uint32_t const minHistoricLogfiles = historicLogfiles();
    MMFilesWalLogfile* first = nullptr;

    WRITE_LOCKER(writeLocker, _logfilesLock);

    for (auto& it : _logfiles) {
      MMFilesWalLogfile* logfile = it.second;

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
void MMFilesLogfileManager::increaseCollectQueueSize(MMFilesWalLogfile* logfile) {
  logfile->increaseCollectQueueSize();
}

// decrease the number of collect operations for a logfile
void MMFilesLogfileManager::decreaseCollectQueueSize(MMFilesWalLogfile* logfile) {
  logfile->decreaseCollectQueueSize();
}

// mark a file as being requested for collection
void MMFilesLogfileManager::setCollectionRequested(MMFilesWalLogfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(writeLocker, _logfilesLock);

    if (logfile->status() == MMFilesWalLogfile::StatusType::COLLECTION_REQUESTED) {
      // the collector already asked for this file, but couldn't process it
      // due to some exception
      return;
    }

    logfile->setStatus(MMFilesWalLogfile::StatusType::COLLECTION_REQUESTED);
  }

  if (!_inRecovery) {
    // to start collection
    READ_LOCKER(locker, _collectorThreadLock);

    if (_collectorThread != nullptr) {
      _collectorThread->signal();
    }
  }
}

// mark a file as being done with collection
void MMFilesLogfileManager::setCollectionDone(MMFilesWalLogfile* logfile) {
  TRI_IF_FAILURE("setCollectionDone") {
    return;
  }
  
  TRI_ASSERT(logfile != nullptr);
  MMFilesWalLogfile::IdType id = logfile->id();

  // LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "setCollectionDone setting lastCollectedId to " << id
  {
    WRITE_LOCKER(writeLocker, _logfilesLock);
    logfile->setStatus(MMFilesWalLogfile::StatusType::COLLECTED);

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
    {
      READ_LOCKER(locker, _collectorThreadLock);
      if (_collectorThread != nullptr) {
        _collectorThread->signal();
      }
    }
    writeShutdownInfo(false);
  }
}

// force the status of a specific logfile
void MMFilesLogfileManager::forceStatus(MMFilesWalLogfile* logfile, MMFilesWalLogfile::StatusType status) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(writeLocker, _logfilesLock);
    logfile->forceStatus(status);
  }
}

// return the current state
MMFilesLogfileManagerState MMFilesLogfileManager::state() {
  MMFilesLogfileManagerState state;

  // now fill the state
  while (true) {
    _slots->statistics(state.lastAssignedTick, state.lastCommittedTick,
                      state.lastCommittedDataTick, state.numEvents, state.numEventsSync);

    // check if lastCommittedTick is still 0. this will be the case directly
    // after server start. in this case, we need to wait for the server to write
    // and sync at least one WAL entry so the tick increases beyond 0
    if (state.lastCommittedTick != 0) {
      break;
    }

    // don't hang forever on shutdown
    if (application_features::ApplicationServer::isStopping()) {
      break;
    }
    usleep(10000);
  }
  TRI_ASSERT(state.lastCommittedTick > 0);

  state.timeString = utilities::timeString();

  return state;
}

// return the current available logfile ranges
MMFilesLogfileManager::LogfileRanges MMFilesLogfileManager::ranges() {
  LogfileRanges result;

  READ_LOCKER(readLocker, _logfilesLock);

  for (auto const& it : _logfiles) {
    MMFilesWalLogfile* logfile = it.second;

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
std::tuple<size_t, MMFilesWalLogfile::IdType, MMFilesWalLogfile::IdType>
MMFilesLogfileManager::runningTransactions() {
  size_t count = 0;
  MMFilesWalLogfile::IdType lastCollectedId = UINT64_MAX;
  MMFilesWalLogfile::IdType lastSealedId = UINT64_MAX;

  auto cb = [&count, &lastCollectedId, &lastSealedId](TRI_voc_tid_t, TransactionData const* data) {
    ++count;
        
    MMFilesWalLogfile::IdType value = static_cast<MMFilesTransactionData const*>(data)->lastCollectedId;
    if (value < lastCollectedId && value != 0) {
      lastCollectedId = value;
    }

    value = static_cast<MMFilesTransactionData const*>(data)->lastSealedId;
    if (value < lastSealedId && value != 0) {
      lastSealedId = value;
    }
  };
  
  // iterate over all active transactions
  TransactionManagerFeature::manager()->iterateActiveTransactions(cb);

  return std::tuple<size_t, MMFilesWalLogfile::IdType, MMFilesWalLogfile::IdType>(
      count, lastCollectedId, lastSealedId);
}

// remove a logfile in the file system
void MMFilesLogfileManager::removeLogfile(MMFilesWalLogfile* logfile) {
  // old filename
  MMFilesWalLogfile::IdType const id = logfile->id();
  std::string const filename = logfileName(id);

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "removing logfile '" << filename << "'";

  // now close the logfile
  delete logfile;

  int res = TRI_ERROR_NO_ERROR;
  // now physically remove the file

  if (!basics::FileUtils::remove(filename, &res)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to remove logfile '" << filename
             << "': " << TRI_errno_string(res);
  }
}

void MMFilesLogfileManager::waitForCollector() {
  while (true) {
    READ_LOCKER(locker, _collectorThreadLock);

    if (_collectorThread == nullptr) {
      return;
    }

    if (!_collectorThread->hasQueuedOperations()) {
      return;
    }

    locker.unlock();

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "waiting for WAL collector";
    usleep(50000);
  }
}

// execute a callback during a phase in which the collector has nothing
// queued. This is used in the DatabaseManagerThread when dropping
// a database to avoid existence of ditches of type DOCUMENT.
bool MMFilesLogfileManager::executeWhileNothingQueued(std::function<void()> const& cb) {
  READ_LOCKER(locker, _collectorThreadLock);

  if (_collectorThread != nullptr) {
    return _collectorThread->executeWhileNothingQueued(cb);
  }

  locker.unlock();

  cb();
  return true;
}

// wait until a specific logfile has been collected
int MMFilesLogfileManager::waitForCollector(MMFilesWalLogfile::IdType logfileId,
                                     double maxWaitTime) {
  if (maxWaitTime <= 0.0) {
    maxWaitTime = 24.0 * 3600.0; // wait "forever"
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "waiting for collector thread to collect logfile " << logfileId;

  // wait for the collector thread to finish the collection
  double const end = TRI_microtime() + maxWaitTime;

  while (true) {
    if (_lastCollectedId >= logfileId) {
      return TRI_ERROR_NO_ERROR;
    }

    READ_LOCKER(locker, _collectorThreadLock);

    if (_collectorThread == nullptr) {
      return TRI_ERROR_NO_ERROR;
    }

    int res = _collectorThread->waitForResult(50 * 1000);

    locker.unlock();

    // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "still waiting for collector. logfileId: " << logfileId <<
    // " lastCollected: " << _lastCollectedId << ", result: " << res;

    if (res != TRI_ERROR_LOCK_TIMEOUT && res != TRI_ERROR_NO_ERROR) {
      // some error occurred
      return res;
    }

    double const now = TRI_microtime();

    if (now > end) {
      break;
    }

    usleep(20000);
    // try again
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "going into lock timeout. having waited for logfile: " << logfileId << ", maxWaitTime: " << maxWaitTime;
  logStatus();

  // waited for too long
  return TRI_ERROR_LOCK_TIMEOUT;
}
  
void MMFilesLogfileManager::logStatus() {
  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "logfile manager status report: lastCollectedId: " << _lastCollectedId.load() << ", lastSealedId: " << _lastSealedId.load();
  READ_LOCKER(locker, _logfilesLock);
  for (auto logfile : _logfiles) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "- logfile " << logfile.second->id() << ", filename '" << logfile.second->filename()
               << "', status " << logfile.second->statusText();
  }
}

// run the recovery procedure
// this is called after the logfiles have been scanned completely and
// recovery state has been build. additionally, all databases have been
// opened already so we can use collections
int MMFilesLogfileManager::runRecovery() {
  TRI_ASSERT(!_allowWrites);

  if (!_recoverState->mustRecover()) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  if (_ignoreRecoveryErrors) {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "running WAL recovery ("
              << _recoverState->logfilesToProcess.size()
              << " logfiles), ignoring recovery errors";
  } else {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "running WAL recovery ("
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
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "WAL recovery finished successfully";
  } else {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "WAL recovery finished, some errors ignored due to settings";
  }

  return TRI_ERROR_NO_ERROR;
}

// closes all logfiles
void MMFilesLogfileManager::closeLogfiles() {
  WRITE_LOCKER(writeLocker, _logfilesLock);

  for (auto& it : _logfiles) {
    MMFilesWalLogfile* logfile = it.second;

    if (logfile != nullptr) {
      delete logfile;
    }
  }

  _logfiles.clear();
}

// reads the shutdown information
int MMFilesLogfileManager::readShutdownInfo() {
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
    FoundLastTick = 1;
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
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "no previous shutdown time found";
  } else {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "previous shutdown was at '" << shutdownTime << "'";
  }

  {
    MUTEX_LOCKER(mutexLocker, _idLock);
    _lastCollectedId = static_cast<MMFilesWalLogfile::IdType>(lastCollectedId);
    _lastSealedId = static_cast<MMFilesWalLogfile::IdType>(lastSealedId);

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "initial values for WAL logfile manager: tick: " << lastTick
               << ", hlc: " << hlc 
               << ", lastCollected: " << _lastCollectedId.load()
               << ", lastSealed: " << _lastSealedId.load();
  }

  return TRI_ERROR_NO_ERROR;
}

// writes the shutdown information
/// this function is called at shutdown and at every logfile flush request
int MMFilesLogfileManager::writeShutdownInfo(bool writeShutdownTime) {
  TRI_IF_FAILURE("LogfileManagerWriteShutdown") { return TRI_ERROR_DEBUG; }

  TRI_ASSERT(!_shutdownFile.empty());

  try {
    VPackBuilder builder;
    builder.openObject();

    // create local copies of the instance variables while holding the read lock
    MMFilesWalLogfile::IdType lastCollectedId;
    MMFilesWalLogfile::IdType lastSealedId;

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
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to write WAL state file '" << _shutdownFile << "'";
      return TRI_ERROR_CANNOT_WRITE_FILE;
    }
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to write WAL state file '" << _shutdownFile << "'";

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

// start the synchronizer thread
int MMFilesLogfileManager::startMMFilesSynchronizerThread() {
  _synchronizerThread = new MMFilesSynchronizerThread(this, _syncInterval);

  if (!_synchronizerThread->start()) {
    delete _synchronizerThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the synchronizer thread
void MMFilesLogfileManager::stopMMFilesSynchronizerThread() {
  if (_synchronizerThread != nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping WAL synchronizer thread";

    _synchronizerThread->beginShutdown();
  }
}

// start the allocator thread
int MMFilesLogfileManager::startMMFilesAllocatorThread() {
  _allocatorThread = new MMFilesAllocatorThread(this);

  if (!_allocatorThread->start()) {
    delete _allocatorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the allocator thread
void MMFilesLogfileManager::stopMMFilesAllocatorThread() {
  if (_allocatorThread != nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping WAL allocator thread";

    _allocatorThread->beginShutdown();
  }
}

// start the collector thread
int MMFilesLogfileManager::startMMFilesCollectorThread() {
  WRITE_LOCKER(locker, _collectorThreadLock);

  _collectorThread = new MMFilesCollectorThread(this);

  if (!_collectorThread->start()) {
    delete _collectorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the collector thread
void MMFilesLogfileManager::stopMMFilesCollectorThread() {
  if (_collectorThread == nullptr) {
    return;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping WAL collector thread";

  // wait for at most 5 seconds for the collector 
  // to catch up
  double const end = TRI_microtime() + 5.0;  
  while (TRI_microtime() < end) {
    bool canAbort = true;
    {
      READ_LOCKER(readLocker, _logfilesLock);
      for (auto& it : _logfiles) {
        MMFilesWalLogfile* logfile = it.second;

        if (logfile == nullptr) {
          continue;
        }

        MMFilesWalLogfile::StatusType status = logfile->status();

        if (status == MMFilesWalLogfile::StatusType::SEAL_REQUESTED) {
          canAbort = false;
          break;
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
int MMFilesLogfileManager::startMMFilesRemoverThread() {
  _removerThread = new MMFilesRemoverThread(this);

  if (!_removerThread->start()) {
    delete _removerThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

// stop the remover thread
void MMFilesLogfileManager::stopMMFilesRemoverThread() {
  if (_removerThread != nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "stopping WAL remover thread";

    _removerThread->beginShutdown();
  }
}

// check which logfiles are present in the log directory
int MMFilesLogfileManager::inventory() {
  int res = ensureDirectory();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "scanning WAL directory: '" << _directory << "'";

  std::vector<std::string> files = basics::FileUtils::listFiles(_directory);

  for (auto it = files.begin(); it != files.end(); ++it) {
    std::string const file = (*it);

    if (StringUtils::isPrefix(file, "logfile-") &&
        StringUtils::isSuffix(file, ".db")) {
      MMFilesWalLogfile::IdType const id =
          basics::StringUtils::uint64(file.substr(8, file.size() - 8 - 3));

      if (id == 0) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "encountered invalid id for logfile '" << file
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
int MMFilesLogfileManager::inspectLogfiles() {
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "inspecting WAL logfiles";

  WRITE_LOCKER(writeLocker, _logfilesLock);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // print an inventory
  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    MMFilesWalLogfile* logfile = (*it).second;

    if (logfile != nullptr) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "logfile " << logfile->id() << ", filename '" << logfile->filename()
                 << "', status " << logfile->statusText();
    }
  }
#endif

  for (auto it = _logfiles.begin(); it != _logfiles.end(); /* no hoisting */) {
    MMFilesWalLogfile::IdType const id = (*it).first;
    std::string const filename = logfileName(id);

    TRI_ASSERT((*it).second == nullptr);

    int res = MMFilesDatafile::judge(filename);

    if (res == TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
      _recoverState->emptyLogfiles.push_back(filename);
      _logfiles.erase(it++);
      continue;
    }

    bool const wasCollected = (id <= _lastCollectedId);
    std::unique_ptr<MMFilesWalLogfile> logfile(MMFilesWalLogfile::openExisting(filename, id, wasCollected, _ignoreLogfileErrors));

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

    if (logfile->status() == MMFilesWalLogfile::StatusType::OPEN ||
        logfile->status() == MMFilesWalLogfile::StatusType::SEALED) {
      _recoverState->logfilesToProcess.push_back(logfile.get());
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "inspecting logfile " << logfile->id() << " ("
               << logfile->statusText() << ")";

    MMFilesDatafile* df = logfile->df();
    df->sequentialAccess();

    // update the tick statistics
    if (!TRI_IterateDatafile(df, &MMFilesWalRecoverState::InitialScanMarker,
                             static_cast<void*>(_recoverState.get()))) {
      std::string const logfileName = logfile->filename();
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "WAL inspection failed when scanning logfile '"
                << logfileName << "'";
      return TRI_ERROR_ARANGO_RECOVERY;
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "inspected logfile " << logfile->id() << " ("
               << logfile->statusText()
               << "), tickMin: " << df->_tickMin
               << ", tickMax: " << df->_tickMax;

    if (logfile->status() == MMFilesWalLogfile::StatusType::SEALED) {
      // If it is sealed, switch to random access:
      df->randomAccess();
    }

    {
      MUTEX_LOCKER(mutexLocker, _idLock);
      if (logfile->status() == MMFilesWalLogfile::StatusType::SEALED &&
          id > _lastSealedId) {
        _lastSealedId = id;
      }

      if ((logfile->status() == MMFilesWalLogfile::StatusType::SEALED ||
           logfile->status() == MMFilesWalLogfile::StatusType::OPEN) &&
          id > _lastOpenedId) {
        _lastOpenedId = id;
      }
    }

    (*it).second = logfile.release();
    ++it;
  }

  // update the tick with the max tick we found in the WAL
  TRI_UpdateTickServer(_recoverState->lastTick);

  TRI_ASSERT(_slots != nullptr);
  // set the last ticks we found in existing logfile data
  _slots->setLastTick(_recoverState->lastTick);
    
  // use maximum revision value found from WAL to adjust HLC value
  // should it be lower
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "setting max HLC value to " << _recoverState->maxRevisionId;
  TRI_HybridLogicalClock(_recoverState->maxRevisionId);

  return TRI_ERROR_NO_ERROR;
}

// allocates a new reserve logfile
int MMFilesLogfileManager::createReserveLogfile(uint32_t size) {
  MMFilesWalLogfile::IdType const id = nextId();
  std::string const filename = logfileName(id);

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "creating empty logfile '" << filename << "' with size "
             << size;

  uint32_t realsize;
  if (size > 0 && size > filesize()) {
    // create a logfile with the requested size
    realsize = size + MMFilesDatafileHelper::JournalOverhead();
  } else {
    // create a logfile with default size
    realsize = filesize();
  }

  std::unique_ptr<MMFilesWalLogfile> logfile(MMFilesWalLogfile::createNew(filename, id, realsize));

  if (logfile == nullptr) {
    int res = TRI_errno();

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "unable to create logfile: " << TRI_errno_string(res);
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
MMFilesWalLogfile::IdType MMFilesLogfileManager::nextId() {
  return static_cast<MMFilesWalLogfile::IdType>(TRI_NewTickServer());
}

// ensure the wal logfiles directory is actually there
int MMFilesLogfileManager::ensureDirectory() {
  // strip directory separator from path
  // this is required for Windows
  std::string directory(_directory);

  TRI_ASSERT(!directory.empty());

  if (directory[directory.size() - 1] == TRI_DIR_SEPARATOR_CHAR) {
    directory = directory.substr(0, directory.size() - 1);
  }

  if (!basics::FileUtils::isDirectory(directory)) {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "WAL directory '" << directory
              << "' does not exist. creating it...";

    int res;
    if (!basics::FileUtils::createDirectory(directory, &res)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not create WAL directory: '" << directory
               << "': " << TRI_last_error();
      return TRI_ERROR_SYS_ERROR;
    }
  }

  if (!basics::FileUtils::isDirectory(directory)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "WAL directory '" << directory << "' does not exist";
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

// return the absolute name of the shutdown file
std::string MMFilesLogfileManager::shutdownFilename() const {
  return _databasePath + TRI_DIR_SEPARATOR_STR + "SHUTDOWN";
}

// return an absolute filename for a logfile id
std::string MMFilesLogfileManager::logfileName(MMFilesWalLogfile::IdType id) const {
  return _directory + std::string("logfile-") + basics::StringUtils::itoa(id) +
         std::string(".db");
}
