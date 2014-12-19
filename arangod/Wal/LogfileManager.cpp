////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log logfile manager
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

#include "LogfileManager.h"
#include "Basics/hashes.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/JsonHelper.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Utils/Exception.h"
#include "VocBase/server.h"
#include "Wal/AllocatorThread.h"
#include "Wal/CollectorThread.h"
#include "Wal/RecoverState.h"
#include "Wal/RemoverThread.h"
#include "Wal/Slots.h"
#include "Wal/SynchroniserThread.h"

using namespace triagens::wal;

////////////////////////////////////////////////////////////////////////////////
/// @brief the logfile manager singleton
////////////////////////////////////////////////////////////////////////////////

static LogfileManager* Instance = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum value for --wal.throttle-when-pending
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t MinThrottleWhenPending () {
  return 1024 * 1024;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum value for --wal.sync-interval
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t MinSyncInterval () {
  return 5;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum value for --wal.logfile-size
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t MinFileSize () {
#ifdef TRI_ENABLE_MAINTAINER_MODE  
  // this allows testing with smaller logfile-sizes
  return 1 * 1024 * 1024;
#else
  return 8 * 1024 * 1024;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the maximum size of a logfile entry
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t MaxEntrySize () {
  return 2 << 30; // 2 GB
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum number of slots
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t MinSlots () {
  return 1024 * 8;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of slots
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t MaxSlots () {
  return 1024 * 1024 * 16;
}


// -----------------------------------------------------------------------------
// --SECTION--                                              class LogfileManager
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::LogfileManager (TRI_server_t* server,
                                std::string* databasePath)
  : ApplicationFeature("logfile-manager"),
    _server(server),
    _databasePath(databasePath),
    _directory(),
    _recoverState(nullptr),
    _filesize(32 * 1024 * 1024),
    _reserveLogfiles(4),
    _historicLogfiles(10),
    _maxOpenLogfiles(0),
    _numberOfSlots(1048576),
    _syncInterval(100),
    _maxThrottleWait(15000),
    _throttleWhenPending(0),
    _allowOversizeEntries(true),
    _ignoreLogfileErrors(false),
    _ignoreRecoveryErrors(false),
    _suppressShapeInformation(false),
    _allowWrites(false), // start in read-only mode
    _hasFoundLastTick(false),
    _inRecovery(true),
    _slots(nullptr),
    _synchroniserThread(nullptr),
    _allocatorThread(nullptr),
    _collectorThread(nullptr),
    _removerThread(nullptr),
    _logfilesLock(),
    _lastOpenedId(0),
    _lastCollectedId(0),
    _lastSealedId(0),
    _logfiles(),
    _transactions(),
    _failedTransactions(),
    _droppedCollections(),
    _droppedDatabases(),
    _writeThrottled(0),
    _filenameRegex(),
    _shutdown(0) {

  LOG_TRACE("creating WAL logfile manager");
  TRI_ASSERT(! _allowWrites);

  int res = regcomp(&_filenameRegex, "^logfile-([0-9][0-9]*)\\.db$", REG_EXTENDED);

  if (res != 0) {
    THROW_INTERNAL_ERROR("could not compile regex");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the logfile manager
////////////////////////////////////////////////////////////////////////////////

LogfileManager::~LogfileManager () {
  LOG_TRACE("shutting down WAL logfile manager");

  stop();

  regfree(&_filenameRegex);

  if (_recoverState != nullptr) {
    delete _recoverState;
    _recoverState = nullptr;
  }

  if (_slots != nullptr) {
    delete _slots;
    _slots = nullptr;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the logfile manager instance
////////////////////////////////////////////////////////////////////////////////

LogfileManager* LogfileManager::instance () {
  TRI_ASSERT(Instance != nullptr);
  return Instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the logfile manager instance
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::initialise (std::string* path,
                                 TRI_server_t* server) {
  TRI_ASSERT(Instance == nullptr);

  Instance = new LogfileManager(server, path);
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setupOptions (std::map<std::string, triagens::basics::ProgramOptionsDescription>& options) {
  options["Write-ahead log options:help-wal"]
    ("wal.allow-oversize-entries", &_allowOversizeEntries, "allow entries that are bigger than --wal.logfile-size")
    ("wal.directory", &_directory, "logfile directory")
    ("wal.historic-logfiles", &_historicLogfiles, "maximum number of historic logfiles to keep after collection")
    ("wal.ignore-logfile-errors", &_ignoreLogfileErrors, "ignore logfile errors. this will read recoverable data from corrupted logfiles but ignore any unrecoverable data")
    ("wal.ignore-recovery-errors", &_ignoreRecoveryErrors, "continue recovery even if re-applying operations fails")
    ("wal.logfile-size", &_filesize, "size of each logfile (in bytes)")
    ("wal.open-logfiles", &_maxOpenLogfiles, "maximum number of parallel open logfiles")
    ("wal.reserve-logfiles", &_reserveLogfiles, "maximum number of reserve logfiles to maintain")
    ("wal.slots", &_numberOfSlots, "number of logfile slots to use")
    ("wal.suppress-shape-information", &_suppressShapeInformation, "do not write shape information for markers (saves a lot of disk space, but effectively disables using the write-ahead log for replication)")
    ("wal.sync-interval", &_syncInterval, "interval for automatic, non-requested disk syncs (in milliseconds)")
    ("wal.throttle-when-pending", &_throttleWhenPending, "throttle writes when at least this many operations are waiting for collection (set to 0 to deactivate write-throttling)")
    ("wal.throttle-wait", &_maxThrottleWait, "maximum wait time per operation when write-throttled (in milliseconds)")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::prepare () {
  static bool Prepared = false;

  if (Prepared) {
    return true;
  }

  Prepared = true;

  if (_directory.empty()) {
    // use global configuration variable
    _directory = (*_databasePath);

    if (! basics::FileUtils::isDirectory(_directory)) {
      LOG_FATAL_AND_EXIT("database directory '%s' does not exist.", _directory.c_str());
    }

    // append "/journals"
    if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
      // append a trailing slash to directory name
      _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
    }
    _directory.append("journals");
  }

  if (_directory.empty()) {
    LOG_FATAL_AND_EXIT("no directory specified for WAL logfiles. Please use the --wal.directory option");
  }

  if (_directory[_directory.size() - 1] != TRI_DIR_SEPARATOR_CHAR) {
    // append a trailing slash to directory name
    _directory.push_back(TRI_DIR_SEPARATOR_CHAR);
  }

  if (_filesize < MinFileSize()) {
    // minimum filesize per logfile
    LOG_FATAL_AND_EXIT("invalid value for --wal.logfile-size. Please use a value of at least %lu", (unsigned long) MinFileSize());
  }

  _filesize = (uint32_t) (((_filesize + PageSize - 1) / PageSize) * PageSize);

  if (_numberOfSlots < MinSlots() || _numberOfSlots > MaxSlots()) {
    // invalid number of slots
    LOG_FATAL_AND_EXIT("invalid value for --wal.slots. Please use a value between %lu and %lu", (unsigned long) MinSlots(), (unsigned long) MaxSlots());
  }

  if (_throttleWhenPending > 0 && _throttleWhenPending < MinThrottleWhenPending()) {
    LOG_FATAL_AND_EXIT("invalid value for --wal.throttle-when-pending. Please use a value of at least %llu", (unsigned long long) MinThrottleWhenPending());
  }

  if (_syncInterval < MinSyncInterval()) {
    LOG_FATAL_AND_EXIT("invalid value for --wal.sync-interval. Please use a value of at least %llu", (unsigned long long) MinSyncInterval());
  }

  // sync interval is specified in milliseconds by the user, but internally
  // we use microseconds
  _syncInterval = _syncInterval * 1000;

  // initialise some objects
  _slots = new Slots(this, _numberOfSlots, 0);
  _recoverState = new RecoverState(_server, _ignoreRecoveryErrors);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::start () {
  static bool started = false;

  if (started) {
    // we were already started
    return true;
  }

  TRI_ASSERT(! _allowWrites);

  int res = inventory();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not create WAL logfile inventory: %s", TRI_errno_string(res));
    return false;
  }

  std::string const shutdownFile = shutdownFilename();
  bool const shutdownFileExists = basics::FileUtils::exists(shutdownFile);

  if (shutdownFileExists) {
    LOG_TRACE("shutdown file found");

    res = readShutdownInfo();

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("could not open shutdown file '%s': %s",
                shutdownFile.c_str(),
                TRI_errno_string(res));
      return false;
    }
  }
  else {
    LOG_TRACE("no shutdown file found");
  }

  res = inspectLogfiles();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not inspect WAL logfiles: %s", TRI_errno_string(res));
    return false;
  }

  started = true;

  LOG_TRACE("WAL logfile manager configuration: historic logfiles: %lu, reserve logfiles: %lu, filesize: %lu, sync interval: %lu",
            (unsigned long) _historicLogfiles,
            (unsigned long) _reserveLogfiles,
            (unsigned long) _filesize,
            (unsigned long) _syncInterval);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::open () {
  static bool opened = false;

  if (opened) {
    // we were already started
    return true;
  }
  
  opened = true;
  
  int res = runRecovery();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("unable to finish WAL recovery: %s", TRI_errno_string(res));
    return false;
  }
  
  // note all failed transactions that we found plus the list
  // of collections and databases that we can ignore
  {
    WRITE_LOCKER(_logfilesLock);
    for (auto it = _recoverState->failedTransactions.begin(); it != _recoverState->failedTransactions.end(); ++it) {
      _failedTransactions.insert((*it).first);
    }

    _droppedDatabases   = _recoverState->droppedDatabases;
    _droppedCollections = _recoverState->droppedCollections;
  }

  
  {
    // set every open logfile to a status of sealed
    WRITE_LOCKER(_logfilesLock);

    for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
      Logfile* logfile = (*it).second;

      if (logfile != nullptr) {
        Logfile::StatusType status = logfile->status();

        if (status == Logfile::StatusType::OPEN) { 
          // set all logfiles to sealed status so they can be collected

          // we don't care about the previous status here
          logfile->forceStatus(Logfile::StatusType::SEALED);
          if (logfile->id() > _lastSealedId) {
            _lastSealedId = logfile->id();
          }
        }
      }
    }
  }
 

  // now start allocator and synchroniser
  res = startAllocatorThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start WAL allocator thread: %s", TRI_errno_string(res));
    return false;
  }

  res = startSynchroniserThread();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start WAL synchroniser thread: %s", TRI_errno_string(res));
    return false;
  }
  
  // from now on, we allow writes to the logfile
  allowWrites(true);

  // explicitly abort any open transactions found in the logs
  res = _recoverState->abortOpenTransactions();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not abort open transactions: %s", TRI_errno_string(res));
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
    LOG_ERROR("could not start WAL collector thread: %s", TRI_errno_string(res));
    return false;
  }

  TRI_ASSERT(_collectorThread != nullptr);

  res = startRemoverThread();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not start WAL remover thread: %s", TRI_errno_string(res));
    return false;
  }

  // tell the allocator that the recovery is over now
  _allocatorThread->recoveryDone();


  // unload all collections to reset statistics, start compactor threads etc.
  res = TRI_InitDatabasesServer(_server);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not initialise databases: %s", TRI_errno_string(res));
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::close () {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stop () {
  if (_shutdown > 0) {
    return;
  }

  _shutdown = 1;

  LOG_TRACE("shutting down WAL");

  // set WAL to read-only mode
  allowWrites(false);

  // do a final flush at shutdown
  this->flush(true, true, false);

  // stop threads
  LOG_TRACE("stopping remover thread");
  stopRemoverThread();

  LOG_TRACE("stopping collector thread");
  stopCollectorThread();

  LOG_TRACE("stopping allocator thread");
  stopAllocatorThread();

  LOG_TRACE("stopping synchroniser thread");
  stopSynchroniserThread();

  // close all open logfiles
  LOG_TRACE("closing logfiles");
  closeLogfiles();

  TRI_IF_FAILURE("LogfileManagerStop") {
    // intentionally kill the server
    TRI_SegfaultDebugging("LogfileManagerStop");
  }

  int res = writeShutdownInfo(true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("could not write WAL shutdown info: %s", TRI_errno_string(res));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a transaction
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::registerTransaction (TRI_voc_tid_t transactionId) {
  {
    WRITE_LOCKER(_logfilesLock);

    _transactions.emplace(std::make_pair(transactionId, std::make_pair(_lastCollectedId, _lastSealedId)));
    TRI_ASSERT(_lastCollectedId <= _lastSealedId);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a transaction
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::unregisterTransaction (TRI_voc_tid_t transactionId,
                                            bool markAsFailed) {
  {
    WRITE_LOCKER(_logfilesLock);
    _transactions.erase(transactionId);

    if (markAsFailed) {
      _failedTransactions.insert(transactionId);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the set of failed transactions
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<TRI_voc_tid_t> LogfileManager::getFailedTransactions () {
  std::unordered_set<TRI_voc_tid_t> failedTransactions;

  {
    READ_LOCKER(_logfilesLock);
    failedTransactions = _failedTransactions;
  }

  return failedTransactions;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the set of dropped collections
/// this is used during recovery and not used afterwards
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<TRI_voc_cid_t> LogfileManager::getDroppedCollections () {
  std::unordered_set<TRI_voc_cid_t> droppedCollections;

  {
    READ_LOCKER(_logfilesLock);
    droppedCollections = _droppedCollections;
  }

  return droppedCollections;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the set of dropped databases
/// this is used during recovery and not used afterwards
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<TRI_voc_tick_t> LogfileManager::getDroppedDatabases () {
  std::unordered_set<TRI_voc_tick_t> droppedDatabases;

  {
    READ_LOCKER(_logfilesLock);
    droppedDatabases = _droppedDatabases;
  }

  return droppedDatabases;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister a list of failed transactions
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::unregisterFailedTransactions (std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
  WRITE_LOCKER(_logfilesLock);

  std::for_each(failedTransactions.begin(), failedTransactions.end(), 
                [&] (TRI_voc_tid_t id) { _failedTransactions.erase(id);
                });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not it is currently allowed to create an additional
/// logfile
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::logfileCreationAllowed (uint32_t size) {
  if (size + Logfile::overhead() > filesize()) {
    // oversize entry. this is always allowed because otherwise everything would
    // lock
    return true;
  }

  if (_maxOpenLogfiles == 0) {
    return true;
  }

  uint32_t numberOfLogfiles = 0;

  // note: this information could also be cached instead of being recalculated
  // everytime
  READ_LOCKER(_logfilesLock);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not there are reserve logfiles
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::hasReserveLogfiles () {
  uint32_t numberOfLogfiles = 0;

  // note: this information could also be cached instead of being recalculated
  // everytime
  READ_LOCKER(_logfilesLock);

  // reverse-scan the logfiles map
  for (auto it = _logfiles.rbegin(); it != _logfiles.rend(); ++it) {
    Logfile* logfile = (*it).second;

    TRI_ASSERT(logfile != nullptr);

    if (logfile->freeSize() > 0 && ! logfile->isSealed()) {
      if (++numberOfLogfiles >= reserveLogfiles()) {
        return true;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal that a sync operation is required
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::signalSync () {
  _synchroniserThread->signalSync();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate space in a logfile for later writing
////////////////////////////////////////////////////////////////////////////////

SlotInfo LogfileManager::allocate (uint32_t size) {
  if (! _allowWrites) {
    // no writes allowed
    return SlotInfo(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (size > MaxEntrySize()) {
    // entry is too big
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  if (size > _filesize && ! _allowOversizeEntries) {
    // entry is too big for a logfile
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  return _slots->nextUnused(size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate space in a logfile for later writing, version for legends
///
/// See explanations about legends in the corresponding allocateAndWrite
/// convenience function.
////////////////////////////////////////////////////////////////////////////////

SlotInfo LogfileManager::allocate (uint32_t size,
                                   TRI_voc_cid_t cid,
                                   TRI_shape_sid_t sid,
                                   uint32_t legendOffset,
                                   void*& oldLegend) {
  if (! _allowWrites) {
    // no writes allowed
#ifdef TRI_ENABLE_MAINTAINER_MODE    
    TRI_ASSERT(false);
#endif
     
    return SlotInfo(TRI_ERROR_ARANGO_READ_ONLY);
  }

  if (size > MaxEntrySize()) {
    // entry is too big
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  if (size > _filesize && ! _allowOversizeEntries) {
    // entry is too big for a logfile
    return SlotInfo(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE);
  }

  return _slots->nextUnused(size, cid, sid, legendOffset, oldLegend);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalise a log entry
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::finalise (SlotInfo& slotInfo,
                               bool waitForSync) {
  _slots->returnUsed(slotInfo, waitForSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data into the logfile
/// this is a convenience function that combines allocate, memcpy and finalise
///
/// We need this version with cid, sid, legendOffset and oldLegend because
/// there is a cache for each WAL file keeping track which legends are
/// already in it. The decision whether or not an additional legend is
/// needed therefore has to be taken in the allocation routine. This
/// version is only used to write document or edge markers. If a previously
/// written legend is found its address is returned in oldLegend such that
/// the new marker can point to it with a relative reference.
////////////////////////////////////////////////////////////////////////////////

SlotInfoCopy LogfileManager::allocateAndWrite (void* src,
                                               uint32_t size,
                                               bool waitForSync,
                                               TRI_voc_cid_t cid,
                                               TRI_shape_sid_t sid,
                                               uint32_t legendOffset,
                                               void*& oldLegend) {

  SlotInfo slotInfo = allocate(size, cid, sid, legendOffset, oldLegend);

  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    return SlotInfoCopy(slotInfo.errorCode);
  }

  TRI_ASSERT(slotInfo.slot != nullptr);

  try {
    slotInfo.slot->fill(src, size);

    // we must copy the slotinfo because finalise() will set its internal to 0 again
    SlotInfoCopy copy(slotInfo.slot);

    finalise(slotInfo, waitForSync);
    return copy;
  }
  catch (...) {
    // if we don't return the slot we'll run into serious problems later
    finalise(slotInfo, false);

    return SlotInfoCopy(TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data into the logfile
/// this is a convenience function that combines allocate, memcpy and finalise
////////////////////////////////////////////////////////////////////////////////

SlotInfoCopy LogfileManager::allocateAndWrite (void* src,
                                               uint32_t size,
                                               bool waitForSync) {

  SlotInfo slotInfo = allocate(size);

  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    return SlotInfoCopy(slotInfo.errorCode);
  }

  TRI_ASSERT(slotInfo.slot != nullptr);

  try {
    slotInfo.slot->fill(src, size);

    // we must copy the slotinfo because finalise() will set its internal to 0 again
    SlotInfoCopy copy(slotInfo.slot);

    finalise(slotInfo, waitForSync);
    return copy;
  }
  catch (...) {
    // if we don't return the slot we'll run into serious problems later
    finalise(slotInfo, false);

    return SlotInfoCopy(TRI_ERROR_INTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data into the logfile
/// this is a convenience function that combines allocate, memcpy and finalise
////////////////////////////////////////////////////////////////////////////////

SlotInfoCopy LogfileManager::allocateAndWrite (Marker const& marker,
                                               bool waitForSync) {
  return allocateAndWrite(marker.mem(), marker.size(), waitForSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalise and seal the currently open logfile
/// this is useful to ensure that any open writes up to this point have made
/// it into a logfile
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::flush (bool waitForSync,
                           bool waitForCollector,
                           bool writeShutdownFile) {
  TRI_ASSERT(! _inRecovery);

  Logfile::IdType lastOpenLogfileId;
  Logfile::IdType lastSealedLogfileId;

  {
    READ_LOCKER(_logfilesLock);
    lastOpenLogfileId   = _lastOpenedId;
    lastSealedLogfileId = _lastSealedId;
  }

  if (lastOpenLogfileId == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG_TRACE("about to flush active WAL logfile. currentLogfileId: %llu, waitForSync: %d, waitForCollector: %d",
            (unsigned long long) lastOpenLogfileId,
            (int) waitForSync,
            (int) waitForCollector);

  int res = _slots->flush(waitForSync);

  if (res != TRI_ERROR_NO_ERROR &&
      res != TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
    LOG_ERROR("unexpected error in WAL flush request: %s", TRI_errno_string(res));
    return res;
  }

  if (waitForCollector) {
    double maxWaitTime = 0.0; // this means wait forever
    if (_shutdown == 1) {
      maxWaitTime = 120.0;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      // we need to wait for the collector...
      this->waitForCollector(lastOpenLogfileId, maxWaitTime);
    }
    else if (res == TRI_ERROR_ARANGO_DATAFILE_EMPTY) {
      // current logfile is empty and cannot be collected
      // we need to wait for the collector to collect the previously sealed datafile

      if (lastSealedLogfileId > 0) {
        this->waitForCollector(lastSealedLogfileId, maxWaitTime);
      }
    }
  }

  if (writeShutdownFile) {
    // update the file with the last tick, last sealed etc.
    return writeShutdownInfo(false);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief re-inserts a logfile back into the inventory only
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::relinkLogfile (Logfile* logfile) {
  Logfile::IdType const id = logfile->id();

  WRITE_LOCKER(_logfilesLock);
  _logfiles.emplace(std::make_pair(id, logfile));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a logfile from the inventory only
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::unlinkLogfile (Logfile* logfile) {
  Logfile::IdType const id = logfile->id();

  WRITE_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return false;
  }

  _logfiles.erase(it);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a logfile from the inventory only
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::unlinkLogfile (Logfile::IdType id) {
  WRITE_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return nullptr;
  }

  _logfiles.erase(it);

  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes logfiles that are allowed to be removed
////////////////////////////////////////////////////////////////////////////////

bool LogfileManager::removeLogfiles () {
  int iterations = 0;
  bool worked = false;

  while (++iterations < 6) {
    Logfile* logfile = getRemovableLogfile();

    if (logfile == nullptr) {
      break;
    }

    removeLogfile(logfile, true);
    worked = true;
  }

  return worked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to open
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileOpen (Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  WRITE_LOCKER(_logfilesLock);
  logfile->setStatus(Logfile::StatusType::OPEN);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to seal-requested
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileSealRequested (Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  WRITE_LOCKER(_logfilesLock);
  logfile->setStatus(Logfile::StatusType::SEAL_REQUESTED);
  signalSync();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to sealed
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileSealed (Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  setLogfileSealed(logfile->id());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the status of a logfile to sealed
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setLogfileSealed (Logfile::IdType id) {
  WRITE_LOCKER(_logfilesLock);

  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return;
  }

  (*it).second->setStatus(Logfile::StatusType::SEALED);
  _lastSealedId = id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of a logfile
////////////////////////////////////////////////////////////////////////////////

Logfile::StatusType LogfileManager::getLogfileStatus (Logfile::IdType id) {
  READ_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    return Logfile::StatusType::UNKNOWN;
  }
  return (*it).second->status();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the file descriptor of a logfile
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::getLogfileDescriptor (Logfile::IdType id) {
  READ_LOCKER(_logfilesLock);
  auto it = _logfiles.find(id);

  if (it == _logfiles.end()) {
    // error
    LOG_ERROR("could not find logfile %llu", (unsigned long long) id);
    return -1;
  }

  Logfile* logfile = (*it).second;
  TRI_ASSERT(logfile != nullptr);

  return logfile->fd();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current open region of a logfile
/// this uses the slots lock
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::getActiveLogfileRegion (Logfile* logfile,
                                             char const*& begin,
                                             char const*& end) {
  _slots->getActiveLogfileRegion(logfile, begin, end);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get logfiles for a tick range
////////////////////////////////////////////////////////////////////////////////

std::vector<Logfile*> LogfileManager::getLogfilesForTickRange (TRI_voc_tick_t minTick,
                                                               TRI_voc_tick_t maxTick) {
  std::vector<Logfile*> temp;
  std::vector<Logfile*> matching;
 
  // we need a two step logfile qualification procedure
  // this is to avoid holding the lock on _logfilesLock and then acquiring the
  // mutex on the _slots. If we hold both locks, we might deadlock with other
  // threads

  {   
    READ_LOCKER(_logfilesLock);
    temp.reserve(_logfiles.size());
    matching.reserve(_logfiles.size());

    for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
      Logfile* logfile = (*it).second;

      if (logfile == nullptr || logfile->status() == Logfile::StatusType::EMPTY) {
        continue;
      }
    
      // found a datafile
      temp.push_back(logfile);
    
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

////////////////////////////////////////////////////////////////////////////////
/// @brief return logfiles for a tick range
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::returnLogfiles (std::vector<Logfile*> const& logfiles) {
  for (auto it = logfiles.begin(); it != logfiles.end(); ++it) {
    Logfile* logfile = (*it);
    
    logfile->release();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile by id
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getLogfile (Logfile::IdType id) {
  READ_LOCKER(_logfilesLock);

  auto it = _logfiles.find(id);
  if (it != _logfiles.end()) {
    return (*it).second;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile and its status by id
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getLogfile (Logfile::IdType id,
                                     Logfile::StatusType& status) {
  READ_LOCKER(_logfilesLock);

  auto it = _logfiles.find(id);
  if (it != _logfiles.end()) {
    status = (*it).second->status();
    return (*it).second;
  }

  status = Logfile::StatusType::UNKNOWN;

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile for writing. this may return nullptr
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getWriteableLogfile (uint32_t size,
                                              Logfile::StatusType& status) {
  static const uint64_t SleepTime = 10 * 1000;
  static const uint64_t MaxIterations = 1500;
  size_t iterations = 0;
  bool haveSignalled = false;

  TRI_IF_FAILURE("LogfileManagerGetWriteableLogfile") {
    // intentionally don't return a logfile
    return nullptr;
  }

  while (++iterations < MaxIterations) {
    {
      WRITE_LOCKER(_logfilesLock);
      auto it = _logfiles.begin();

      while (it != _logfiles.end()) {
        Logfile* logfile = (*it).second;

        TRI_ASSERT(logfile != nullptr);

        if (logfile->isWriteable(size)) {
          // found a logfile, update the status variable and return the logfile
          _lastOpenedId = logfile->id();
          status = logfile->status();
          return logfile;
        }

        if (logfile->status() == Logfile::StatusType::EMPTY &&
            ! logfile->isWriteable(size)) {
          // we found an empty logfile, but the entry won't fit

          // delete the logfile from the sequence of logfiles
          _logfiles.erase(it++);

          // and physically remove the file
          // note: this will also delete the logfile object!
          removeLogfile(logfile, false);
        }
        else {
          ++it;
        }
      }
    }

    // signal & sleep outside the lock
    if (! haveSignalled) {
      _allocatorThread->signal(size);
      haveSignalled = true;
    }
    usleep(SleepTime);
  }

  LOG_WARNING("unable to acquire writeable WAL logfile after %llu ms", (unsigned long long) (MaxIterations * SleepTime) / 1000);

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile to collect. this may return nullptr
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getCollectableLogfile () {
  // iterate over all active readers and find their minimum used logfile id
  Logfile::IdType minId = UINT64_MAX;

  READ_LOCKER(_logfilesLock);

  // iterate over all active transactions and find their minimum used logfile id
  for (auto it = _transactions.begin(); it != _transactions.end(); ++it) {
    Logfile::IdType lastWrittenId = (*it).second.second;

    if (lastWrittenId < minId) {
      minId = lastWrittenId;
    }
  }

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr) {
      if (logfile->id() <= minId && logfile->canBeCollected()) {
        return logfile;
      }
      else if (logfile->id() > minId) {
        // abort early
        break;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a logfile to remove. this may return nullptr
////////////////////////////////////////////////////////////////////////////////

Logfile* LogfileManager::getRemovableLogfile () {
  TRI_ASSERT(! _inRecovery);

  Logfile::IdType minId = UINT64_MAX;

  uint32_t numberOfLogfiles = 0;
  Logfile* first = nullptr;

  READ_LOCKER(_logfilesLock);

  // iterate over all active readers and find their minimum used logfile id
  for (auto it = _transactions.begin(); it != _transactions.end(); ++it) {
    Logfile::IdType lastCollectedId = (*it).second.first;

    if (lastCollectedId < minId) {
      minId = lastCollectedId;
    }
  }

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    // find the first logfile that can be safely removed
    if (logfile != nullptr && logfile->id() <= minId && logfile->canBeRemoved()) {
      if (first == nullptr) {
        first = logfile;
      }

      if (++numberOfLogfiles > historicLogfiles()) {
        TRI_ASSERT(first != nullptr);
        return first;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the number of collect operations for a logfile
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::increaseCollectQueueSize (Logfile* logfile) {
  logfile->increaseCollectQueueSize();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the number of collect operations for a logfile
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::decreaseCollectQueueSize (Logfile* logfile) {
  logfile->decreaseCollectQueueSize();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a file as being requested for collection
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setCollectionRequested (Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(_logfilesLock);
    if (logfile->status() == Logfile::StatusType::COLLECTION_REQUESTED) {
      // the collector already asked for this file, but couldn't process it
      // due to some exception
      return;
    }

    logfile->setStatus(Logfile::StatusType::COLLECTION_REQUESTED);
  }

  if (! _inRecovery) {
    // to start collection
    _collectorThread->signal();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a file as being done with collection
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::setCollectionDone (Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  {
    WRITE_LOCKER(_logfilesLock);
    logfile->setStatus(Logfile::StatusType::COLLECTED);

    _lastCollectedId = logfile->id();
  }

  if (! _inRecovery) {
    // to start removal of unneeded datafiles
    _collectorThread->signal();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the current state
////////////////////////////////////////////////////////////////////////////////

LogfileManagerState LogfileManager::state () {
  LogfileManagerState state;

  // now fill the state
  _slots->statistics(state.lastTick, state.lastDataTick, state.numEvents);
  state.timeString = getTimeString();

  return state;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a logfile from the inventory and in the file system
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::removeLogfile (Logfile* logfile,
                                    bool unlink) {
  if (unlink) {
    unlinkLogfile(logfile);
  }

  // old filename
  Logfile::IdType const id = logfile->id();
  std::string const filename = logfileName(id);

  LOG_TRACE("removing logfile '%s'", filename.c_str());

  // now close the logfile
  delete logfile;

  int res = TRI_ERROR_NO_ERROR;
  // now physically remove the file

  if (! basics::FileUtils::remove(filename, &res)) {
    LOG_ERROR("unable to remove logfile '%s': %s",
              filename.c_str(),
              TRI_errno_string(res));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait until a specific logfile has been collected
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::waitForCollector (Logfile::IdType logfileId,
                                       double maxWaitTime) {
  static const int64_t SingleWaitPeriod = 50 * 1000;
 
  int64_t maxIterations = INT64_MAX; // wait forever
  if (maxWaitTime > 0.0) {
    // if specified, wait for a shorter period of time
    maxIterations = static_cast<int64_t>(maxWaitTime * 1000000.0 / (double) SingleWaitPeriod); 
    LOG_TRACE("will wait for max. %f seconds for collector to finish", maxWaitTime);
  }

  LOG_TRACE("waiting for collector thread to collect logfile %llu", (unsigned long long) logfileId);

  // wait for the collector thread to finish the collection
  int64_t iterations = 0;
  while (++iterations < maxIterations) {
    {
      READ_LOCKER(_logfilesLock);

      if (_lastCollectedId >= logfileId) {
        return;
      }
    }

    LOG_TRACE("waiting for collector");
    usleep(SingleWaitPeriod);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the recovery procedure
/// this is called after the logfiles have been scanned completely and
/// recovery state has been build. additionally, all databases have been
/// opened already so we can use collections
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::runRecovery () {
  TRI_ASSERT(! _allowWrites);

  if (! _recoverState->mustRecover()) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }
    
  LOG_INFO("running WAL recovery");
  
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
  
  LOG_INFO("WAL recovery finished successfully");
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all logfiles
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::closeLogfiles () {
  WRITE_LOCKER(_logfilesLock);

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr) {
      delete logfile;
    }
  }

  _logfiles.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads the shutdown information
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::readShutdownInfo () {
  std::string const filename = shutdownFilename();

  TRI_json_t* json = TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, filename.c_str(), nullptr);

  if (json == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  uint64_t lastTick = basics::JsonHelper::stringUInt64(json, "tick");
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(lastTick));

  if (lastTick > 0) {
    _hasFoundLastTick = true;
  }

  // read id of last collected logfile (maybe 0)
  uint64_t lastCollectedId = basics::JsonHelper::stringUInt64(json, "lastCollected");

  // read if of last sealed logfile (maybe 0)
  uint64_t lastSealedId = basics::JsonHelper::stringUInt64(json, "lastSealed");

  if (lastSealedId < lastCollectedId) {
    // should not happen normally
    lastSealedId = lastCollectedId;
  }

  std::string const shutdownTime(basics::JsonHelper::getStringValue(json, "shutdownTime"));
  if (shutdownTime.empty()) {
    LOG_TRACE("no previous shutdown time found");
  }
  else {
    LOG_TRACE("previous shutdown was at '%s'", shutdownTime.c_str());
  }
  
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  {
    WRITE_LOCKER(_logfilesLock);
    _lastCollectedId = static_cast<Logfile::IdType>(lastCollectedId);
    _lastSealedId = static_cast<Logfile::IdType>(lastSealedId);

    LOG_TRACE("initial values for WAL logfile manager: tick: %llu, lastCollected: %llu, lastSealed: %llu",
              (unsigned long long) lastTick,
              (unsigned long long) _lastCollectedId,
              (unsigned long long) _lastSealedId);

  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the shutdown information
/// this function is called at shutdown and at every logfile flush request
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::writeShutdownInfo (bool writeShutdownTime) {
  TRI_IF_FAILURE("LogfileManagerWriteShutdown") {
    return TRI_ERROR_DEBUG;
  }

  std::string const filename = shutdownFilename();

  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == nullptr) {
    LOG_ERROR("unable to write WAL state file '%s'", filename.c_str());
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  std::string val;

  val = basics::StringUtils::itoa(TRI_CurrentTickServer());
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "tick", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, val.c_str(), val.size()));

  val = basics::StringUtils::itoa(_lastCollectedId);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "lastCollected", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, val.c_str(), val.size()));

  val = basics::StringUtils::itoa(_lastSealedId);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "lastSealed", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, val.c_str(), val.size()));

  if (writeShutdownTime) {
    std::string const t(getTimeString());
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "shutdownTime", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, t.c_str(), t.size()));
  }

  if (! TRI_SaveJson(filename.c_str(), json, true)) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    LOG_ERROR("unable to write WAL state file '%s'", filename.c_str());

    return TRI_ERROR_CANNOT_WRITE_FILE;
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startSynchroniserThread () {
  _synchroniserThread = new SynchroniserThread(this, _syncInterval);

  if (_synchroniserThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _synchroniserThread->start()) {
    delete _synchroniserThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the synchroniser thread
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stopSynchroniserThread () {
  if (_synchroniserThread != nullptr) {
    LOG_TRACE("stopping WAL synchroniser thread");

    _synchroniserThread->stop();
    _synchroniserThread->shutdown();

    delete _synchroniserThread;
    _synchroniserThread = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the allocator thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startAllocatorThread () {
  _allocatorThread = new AllocatorThread(this);

  if (_allocatorThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _allocatorThread->start()) {
    delete _allocatorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the allocator thread
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stopAllocatorThread () {
  if (_allocatorThread != nullptr) {
    LOG_TRACE("stopping WAL allocator thread");

    _allocatorThread->stop();
    _allocatorThread->shutdown();

    delete _allocatorThread;
    _allocatorThread = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the collector thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startCollectorThread () {
  _collectorThread = new CollectorThread(this, _server);

  if (_collectorThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _collectorThread->start()) {
    delete _collectorThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the collector thread
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stopCollectorThread () {
  if (_collectorThread != nullptr) {
    LOG_TRACE("stopping WAL collector thread");

    _collectorThread->stop();
    _collectorThread->shutdown();

    delete _collectorThread;
    _collectorThread = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the remover thread
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::startRemoverThread () {
  _removerThread = new RemoverThread(this);

  if (_removerThread == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  if (! _removerThread->start()) {
    delete _removerThread;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the remover thread
////////////////////////////////////////////////////////////////////////////////

void LogfileManager::stopRemoverThread () {
  if (_removerThread != nullptr) {
    LOG_TRACE("stopping WAL remover thread");

    _removerThread->stop();
    _removerThread->shutdown();

    delete _removerThread;
    _removerThread = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check which logfiles are present in the log directory
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::inventory () {
  int res = ensureDirectory();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  LOG_TRACE("scanning WAL directory: '%s'", _directory.c_str());

  std::vector<std::string> files = basics::FileUtils::listFiles(_directory);

  for (auto it = files.begin(); it != files.end(); ++it) {
    regmatch_t matches[2];
    std::string const file = (*it);
    char const* s = file.c_str();

    if (regexec(&_filenameRegex, s, sizeof(matches) / sizeof(matches[1]), matches, 0) == 0) {
      Logfile::IdType const id = basics::StringUtils::uint64(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);

      if (id == 0) {
        LOG_WARNING("encountered invalid id for logfile '%s'. ids must be > 0", file.c_str());
      }
      else {
        // update global tick
        TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

        WRITE_LOCKER(_logfilesLock);
        _logfiles.emplace(std::make_pair(id, nullptr));
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inspect the logfiles in the log directory
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::inspectLogfiles () {
  LOG_TRACE("inspecting WAL logfiles");

  WRITE_LOCKER(_logfilesLock);

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // print an inventory
  for (auto it = _logfiles.begin(); it != _logfiles.end(); ++it) {
    Logfile* logfile = (*it).second;

    if (logfile != nullptr) {
      LOG_DEBUG("logfile %llu, filename '%s', status %s", 
                (unsigned long long) logfile->id(),
                logfile->filename().c_str(),
                logfile->statusText().c_str());
    }
  }
#endif

  for (auto it = _logfiles.begin(); it != _logfiles.end(); ) {
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
    Logfile* logfile = Logfile::openExisting(filename, id, wasCollected, _ignoreLogfileErrors);

    if (logfile == nullptr) {
      // an error happened when opening a logfile
      if (! _ignoreLogfileErrors) {
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
        
    LOG_TRACE("inspecting logfile %llu (%s)", (unsigned long long) logfile->id(), logfile->statusText().c_str());

    // update the tick statistics  
    if (! TRI_IterateDatafile(logfile->df(), &RecoverState::InitialScanMarker, static_cast<void*>(_recoverState))) {
      LOG_WARNING("WAL inspection failed when scanning logfile '%s'", logfile->filename().c_str());
      return TRI_ERROR_ARANGO_RECOVERY;
    }

    if (logfile->status() == Logfile::StatusType::SEALED &&
        id > _lastSealedId) {
      _lastSealedId = id;
    }

    if ((logfile->status() == Logfile::StatusType::SEALED || logfile->status() == Logfile::StatusType::OPEN) &&
         id > _lastOpenedId) {
      _lastOpenedId = id;
    }


    (*it).second = logfile;
    ++it;
  }
 
  
  // update the tick with the max tick we found in the WAL
  TRI_UpdateTickServer(_recoverState->lastTick);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocates a new reserve logfile
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::createReserveLogfile (uint32_t size) {
  Logfile::IdType const id = nextId();
  std::string const filename = logfileName(id);

  LOG_TRACE("creating empty logfile '%s' with size %lu",
            filename.c_str(),
            (unsigned long) size);

  uint32_t realsize;
  if (size > 0 && size > filesize()) {
    // create a logfile with the requested size
    realsize = size + Logfile::overhead();
  }
  else {
    // create a logfile with default size
    realsize = filesize();
  }

  Logfile* logfile = Logfile::createNew(filename.c_str(), id, realsize);

  if (logfile == nullptr) {
    int res = TRI_errno();

    LOG_ERROR("unable to create logfile: %s", TRI_errno_string(res));
    return res;
  }

  WRITE_LOCKER(_logfilesLock);
  _logfiles.emplace(std::make_pair(id, logfile));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an id for the next logfile
////////////////////////////////////////////////////////////////////////////////

Logfile::IdType LogfileManager::nextId () {
  return static_cast<Logfile::IdType>(TRI_NewTickServer());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure the wal logfiles directory is actually there
////////////////////////////////////////////////////////////////////////////////

int LogfileManager::ensureDirectory () {
  // strip directory separator from path
  // this is required for Windows
  std::string directory(_directory);

  TRI_ASSERT(! directory.empty());

  if (directory[directory.size() - 1] == TRI_DIR_SEPARATOR_CHAR) {
    directory = directory.substr(0, directory.size() - 1);
  }

  if (! basics::FileUtils::isDirectory(directory)) {
    int res;
    
    LOG_INFO("WAL directory '%s' does not exist. creating it...", directory.c_str());

    if (! basics::FileUtils::createDirectory(directory, &res)) {
      LOG_ERROR("could not create WAL directory: '%s': %s", directory.c_str(), TRI_errno_string(res));
      return res;
    }
  }

  if (! basics::FileUtils::isDirectory(directory)) {
    LOG_ERROR("WAL directory '%s' does not exist", directory.c_str());
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the absolute name of the shutdown file
////////////////////////////////////////////////////////////////////////////////

std::string LogfileManager::shutdownFilename () const {
  return (*_databasePath) + TRI_DIR_SEPARATOR_STR + std::string("SHUTDOWN");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an absolute filename for a logfile id
////////////////////////////////////////////////////////////////////////////////

std::string LogfileManager::logfileName (Logfile::IdType id) const {
  return _directory + std::string("logfile-") + basics::StringUtils::itoa(id) + std::string(".db");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the current time as a string
////////////////////////////////////////////////////////////////////////////////

std::string LogfileManager::getTimeString () {
  char buffer[32];
  size_t len;
  time_t tt = time(0);
  struct tm tb;
  TRI_gmtime(tt, &tb);
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);

  return std::string(buffer, len);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
