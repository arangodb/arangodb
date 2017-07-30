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

#ifndef ARANGOD_MMFILES_LOGFILE_MANAGER_H
#define ARANGOD_MMFILES_LOGFILE_MANAGER_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "MMFiles/MMFilesWalLogfile.h"
#include "MMFiles/MMFilesWalSlots.h"
#include "StorageEngine/TransactionManager.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class MMFilesAllocatorThread;
class MMFilesCollectorThread;
class MMFilesRemoverThread;
class MMFilesSynchronizerThread;
class MMFilesWalMarker;
struct MMFilesWalRecoverState;

namespace options {
class ProgramOptions;
}

struct MMFilesTransactionData final : public TransactionData {
  MMFilesTransactionData() = delete;
  MMFilesTransactionData(MMFilesWalLogfile::IdType lastCollectedId, MMFilesWalLogfile::IdType lastSealedId) :
      lastCollectedId(lastCollectedId), lastSealedId(lastSealedId) {}
  MMFilesWalLogfile::IdType const lastCollectedId;
  MMFilesWalLogfile::IdType const lastSealedId;
};

struct MMFilesLogfileManagerState {
  TRI_voc_tick_t lastAssignedTick;
  TRI_voc_tick_t lastCommittedTick;
  TRI_voc_tick_t lastCommittedDataTick;
  uint64_t numEvents;
  uint64_t numEventsSync;
  std::string timeString;
};

class MMFilesLogfileManager final : public application_features::ApplicationFeature {
  friend class MMFilesAllocatorThread;
  friend class MMFilesCollectorThread;

  MMFilesLogfileManager(MMFilesLogfileManager const&) = delete;
  MMFilesLogfileManager& operator=(MMFilesLogfileManager const&) = delete;

 public:
  explicit MMFilesLogfileManager(application_features::ApplicationServer* server);

  // destroy the logfile manager
  ~MMFilesLogfileManager();

  // get the logfile manager instance
  static MMFilesLogfileManager* instance() {
    TRI_ASSERT(Instance != nullptr);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(SafeToUseInstance);
#endif
    return Instance;
  }

 private:
  // logfile manager instance
  static MMFilesLogfileManager* Instance;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // whether or not it is safe to retrieve the instance yet
  static bool SafeToUseInstance;
#endif

  // status of whether the last tick value was found on startup
  static int FoundLastTick;

  struct LogfileBarrier {
    LogfileBarrier() = delete;

    LogfileBarrier(TRI_voc_tick_t id, double expires, TRI_voc_tick_t minTick)
        : id(id), expires(expires), minTick(minTick) {}

    TRI_voc_tick_t const id;
    double expires;
    TRI_voc_tick_t minTick;
  };

  struct LogfileRange {
    LogfileRange(MMFilesWalLogfile::IdType id, std::string const& filename,
                std::string const& state, TRI_voc_tick_t tickMin,
                TRI_voc_tick_t tickMax)
        : id(id),
          filename(filename),
          state(state),
          tickMin(tickMin),
          tickMax(tickMax) {}

    MMFilesWalLogfile::IdType id;
    std::string filename;
    std::string state;
    TRI_voc_tick_t tickMin;
    TRI_voc_tick_t tickMax;
  };

  typedef std::vector<LogfileRange> LogfileRanges;

 public:
  void collectOptions(
      std::shared_ptr<options::ProgramOptions> options) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

 public:
  void logStatus();
  // run the recovery procedure
  // this is called after the logfiles have been scanned completely and
  // recovery state has been build. additionally, all databases have been
  // opened already so we can use collections
  int runRecovery();

  // called by recovery feature once after runRecovery()
  bool open();

  // get the logfile directory
  inline std::string directory() const { return _directory; }

  // get the logfile size
  inline uint32_t filesize() const { return _filesize; }

  // set the logfile size
  inline void filesize(uint32_t value) { _filesize = value; }

  // get the sync interval
  inline uint64_t syncInterval() const { return _syncInterval / 1000; }

  // set the sync interval
  inline void syncInterval(uint64_t value) { _syncInterval = value * 1000; }

  // get the number of reserve logfiles
  inline uint32_t reserveLogfiles() const { return _reserveLogfiles; }

  // set the number of reserve logfiles
  inline void reserveLogfiles(uint32_t value) { _reserveLogfiles = value; }

  // get the number of historic logfiles to keep
  inline uint32_t historicLogfiles() const { return _historicLogfiles; }

  // set the number of historic logfiles
  inline void historicLogfiles(uint32_t value) { _historicLogfiles = value; }

  // whether or not we are in the recovery phase
  inline bool isInRecovery() const { return _inRecovery; }

  // whether or not we are in the shutdown phase
  inline bool isInShutdown() const { return (_shutdown != 0); }

  // whether or not there was a SHUTDOWN file with a last tick at
  // server start
  static bool hasFoundLastTick() { 
    // validate that the value is already initialized
    // -1 = uninitialized
    //  0 = last tick not found
    //  1 = last tick found
    TRI_ASSERT(FoundLastTick != -1);
    return (FoundLastTick == 1); 
  }

  // return the slots manager
  MMFilesWalSlots* slots() { return _slots; }

  // whether or not oversize entries are allowed
  inline bool allowOversizeEntries() const { return _allowOversizeEntries; }

  // sets the "allowOversizeEntries" value
  inline void allowOversizeEntries(bool value) {
    _allowOversizeEntries = value;
  }

  // whether or not write-throttling can be enabled
  inline bool canBeThrottled() const { return (_throttleWhenPending > 0); }

  // maximum wait time when write-throttled (in milliseconds)
  inline uint64_t maxThrottleWait() const { return _maxThrottleWait; }

  // maximum wait time when write-throttled (in milliseconds)
  inline void maxThrottleWait(uint64_t value) { _maxThrottleWait = value; }

  // whether or not write-throttling is currently enabled
  inline bool isThrottled() { return _writeThrottled; }

  // activate write-throttling
  void activateWriteThrottling() { _writeThrottled = true; }

  // deactivate write-throttling
  void deactivateWriteThrottling() { _writeThrottled = false; }

  // allow or disallow writes to the WAL
  inline void allowWrites(bool value) { _allowWrites = value; }
  inline bool allowWrites() const { return _allowWrites; }

  // get the value of --wal.throttle-when-pending
  inline uint64_t throttleWhenPending() const { return _throttleWhenPending; }

  // set the value of --wal.throttle-when-pending
  inline void throttleWhenPending(uint64_t value) {
    _throttleWhenPending = value;

    if (_throttleWhenPending == 0) {
      deactivateWriteThrottling();
    }
  }

  // registers a transaction
  int registerTransaction(TRI_voc_tid_t id, bool isReadOnlyTransaction);

  // return the set of dropped collections
  /// this is used during recovery and not used afterwards
  std::unordered_set<TRI_voc_cid_t> getDroppedCollections();

  // return the set of dropped databases
  /// this is used during recovery and not used afterwards
  std::unordered_set<TRI_voc_tick_t> getDroppedDatabases();

  // whether or not it is currently allowed to create an additional
  /// logfile
  bool logfileCreationAllowed(uint32_t);

  // whether or not there are reserve logfiles
  bool hasReserveLogfiles();

  // signal that a sync operation is required
  void signalSync(bool);

  // write data into the logfile, using database id and collection id
  /// this is a convenience function that combines allocate, memcpy and finalize
  MMFilesWalSlotInfoCopy allocateAndWrite(TRI_voc_tick_t databaseId, 
                                TRI_voc_cid_t collectionId, 
                                MMFilesWalMarker const*, bool wakeUpSynchronizer,
                                bool waitForSyncRequested, bool waitUntilSyncDone);

  // write data into the logfile
  /// this is a convenience function that combines allocate, memcpy and finalize
  MMFilesWalSlotInfoCopy allocateAndWrite(MMFilesWalMarker const* marker, bool wakeUpSynchronizer, 
                                bool waitForSyncRequested, bool waitUntilSyncDone);

  // write marker into the logfile
  // this is a convenience function with less parameters
  MMFilesWalSlotInfoCopy allocateAndWrite(MMFilesWalMarker const& marker, bool waitForSync);

  // wait for the collector queue to get cleared for the given
  /// collection
  int waitForCollectorQueue(TRI_voc_cid_t, double);

  // finalize and seal the currently open logfile
  /// this is useful to ensure that any open writes up to this point have made
  /// it into a logfile
  int flush(bool, bool, bool);

  /// wait until all changes to the current logfile are synced
  bool waitForSync(double);

  // re-inserts a logfile back into the inventory only
  void relinkLogfile(MMFilesWalLogfile*);

  // remove a logfile from the inventory only
  bool unlinkLogfile(MMFilesWalLogfile*);

  // remove a logfile from the inventory only
  MMFilesWalLogfile* unlinkLogfile(MMFilesWalLogfile::IdType);

  // removes logfiles that are allowed to be removed
  bool removeLogfiles();

  // sets the status of a logfile to open
  void setLogfileOpen(MMFilesWalLogfile*);

  // sets the status of a logfile to seal-requested
  void setLogfileSealRequested(MMFilesWalLogfile*);

  // sets the status of a logfile to sealed
  void setLogfileSealed(MMFilesWalLogfile*);

  // sets the status of a logfile to sealed
  void setLogfileSealed(MMFilesWalLogfile::IdType);

  // return the status of a logfile
  MMFilesWalLogfile::StatusType getLogfileStatus(MMFilesWalLogfile::IdType);

  // return the file descriptor of a logfile
  int getLogfileDescriptor(MMFilesWalLogfile::IdType);

  // get the current open region of a logfile
  /// this uses the slots lock
  void getActiveLogfileRegion(MMFilesWalLogfile*, char const*&, char const*&);

  // garbage collect expires logfile barriers
  void collectLogfileBarriers();

  // returns a list of all logfile barrier ids
  std::vector<TRI_voc_tick_t> getLogfileBarriers();

  // remove a specific logfile barrier
  bool removeLogfileBarrier(TRI_voc_tick_t);

  // adds a barrier that prevents removal of logfiles
  TRI_voc_tick_t addLogfileBarrier(TRI_voc_tick_t, double);

  // extend the lifetime of a logfile barrier
  bool extendLogfileBarrier(TRI_voc_tick_t, double, TRI_voc_tick_t);

  // get minimum tick value from all logfile barriers
  TRI_voc_tick_t getMinBarrierTick();

  // get logfiles for a tick range
  std::vector<MMFilesWalLogfile*> getLogfilesForTickRange(TRI_voc_tick_t, TRI_voc_tick_t,
                                                bool& minTickIncluded);

  // return logfiles for a tick range
  void returnLogfiles(std::vector<MMFilesWalLogfile*> const&);

  // get a logfile by id
  MMFilesWalLogfile* getLogfile(MMFilesWalLogfile::IdType);

  // get a logfile and its status by id
  MMFilesWalLogfile* getLogfile(MMFilesWalLogfile::IdType, MMFilesWalLogfile::StatusType&);

  // get a logfile for writing. this may return nullptr
  int getWriteableLogfile(uint32_t, MMFilesWalLogfile::StatusType&, MMFilesWalLogfile*&);

  // get a logfile to collect. this may return nullptr
  MMFilesWalLogfile* getCollectableLogfile();

  // get a logfile to remove. this may return nullptr
  /// if it returns a logfile, the logfile is removed from the list of available
  /// logfiles
  MMFilesWalLogfile* getRemovableLogfile();

  // increase the number of collect operations for a logfile
  void increaseCollectQueueSize(MMFilesWalLogfile*);

  // decrease the number of collect operations for a logfile
  void decreaseCollectQueueSize(MMFilesWalLogfile*);

  // mark a file as being requested for collection
  void setCollectionRequested(MMFilesWalLogfile*);

  // mark a file as being done with collection
  void setCollectionDone(MMFilesWalLogfile*);

  // force the status of a specific logfile
  void forceStatus(MMFilesWalLogfile*, MMFilesWalLogfile::StatusType);

  // return the current state
  MMFilesLogfileManagerState state();

  // return the current available logfile ranges
  LogfileRanges ranges();

  // get information about running transactions
  std::tuple<size_t, MMFilesWalLogfile::IdType, MMFilesWalLogfile::IdType> runningTransactions();
  
  void waitForCollector();

  // execute a callback during a phase in which the collector has nothing
  // queued. This is used in the DatabaseManagerThread when dropping
  // a database to avoid existence of ditches of type DOCUMENT.
  bool executeWhileNothingQueued(std::function<void()> const& cb);

 private:
  // reserve space in a logfile
  MMFilesWalSlotInfo allocate(uint32_t);

  // reserve space in a logfile
  MMFilesWalSlotInfo allocate(TRI_voc_tick_t, TRI_voc_cid_t, uint32_t);

  // memcpy the data into the WAL region and return the filled slot
  // to the WAL logfile manager
  MMFilesWalSlotInfoCopy writeSlot(MMFilesWalSlotInfo& slotInfo,
                         MMFilesWalMarker const* marker,
                         bool wakeUpSynchronizer,
                         bool waitForSyncRequested,
                         bool waitUntilSyncDone);

  // remove a logfile in the file system
  void removeLogfile(MMFilesWalLogfile*);

  // wait for the collector thread to collect a specific logfile
  int waitForCollector(MMFilesWalLogfile::IdType, double);

  // closes all logfiles
  void closeLogfiles();

  // reads the shutdown information
  int readShutdownInfo();

  // writes the shutdown information
  int writeShutdownInfo(bool);

  // start the synchronizer thread
  int startMMFilesSynchronizerThread();

  // stop the synchronizer thread
  void stopMMFilesSynchronizerThread();

  // start the allocator thread
  int startMMFilesAllocatorThread();

  // stop the allocator thread
  void stopMMFilesAllocatorThread();

  // start the collector thread
  int startMMFilesCollectorThread();

  // stop the collector thread
  void stopMMFilesCollectorThread();

  // start the remover thread
  int startMMFilesRemoverThread();

  // stop the remover thread
  void stopMMFilesRemoverThread();

  // check which logfiles are present in the log directory
  int inventory();

  // inspect all found WAL logfiles
  /// this searches for the max tick in the logfiles and builds up the initial
  /// transaction state
  int inspectLogfiles();

  // allocate a new reserve logfile
  int createReserveLogfile(uint32_t);

  // get an id for the next logfile
  MMFilesWalLogfile::IdType nextId();

  // ensure the wal logfiles directory is actually there
  int ensureDirectory();

  // return the absolute name of the shutdown file
  std::string shutdownFilename() const;

  // return an absolute filename for a logfile id
  std::string logfileName(MMFilesWalLogfile::IdType) const;

 private:
  // the arangod config variable containing the database path
  std::string _databasePath;

  // state during recovery
  std::unique_ptr<MMFilesWalRecoverState> _recoverState;

  bool _allowOversizeEntries = true;
  bool _useMLock = false;
  std::string _directory;
  uint32_t _historicLogfiles = 10; 
  bool _ignoreLogfileErrors = false;
  bool _ignoreRecoveryErrors = false;
  uint64_t _flushTimeout = 15000;
  uint32_t _filesize = 32 * 1024 * 1024;
  uint32_t _maxOpenLogfiles = 0;
  uint32_t _reserveLogfiles = 3;
  uint32_t _numberOfSlots = 1048576;
  uint64_t _syncInterval = 100;
  uint64_t _throttleWhenPending = 0;
  uint64_t _maxThrottleWait = 15000;

  // whether or not writes to the WAL are allowed
  bool _allowWrites;

  // whether or not the recovery procedure is running
  bool _inRecovery;

  // a lock protecting the _logfiles map and the logfiles' statuses
  basics::ReadWriteLock _logfilesLock;

  // the logfiles
  std::map<MMFilesWalLogfile::IdType, MMFilesWalLogfile*> _logfiles;

  // the slots manager
  MMFilesWalSlots* _slots;

  // the synchronizer thread
  MMFilesSynchronizerThread* _synchronizerThread;

  // the allocator thread
  MMFilesAllocatorThread* _allocatorThread;

  // the collector thread
  MMFilesCollectorThread* _collectorThread;
  
  // lock protecting the destruction of the collector thread
  basics::ReadWriteLock _collectorThreadLock;

  // the logfile remover thread
  MMFilesRemoverThread* _removerThread;

  // last opened logfile id. note: writing to this variable is protected
  /// by the _idLock
  std::atomic<MMFilesWalLogfile::IdType> _lastOpenedId;

  // last fully collected logfile id. note: writing to this variable is
  /// protected by the_idLock
  std::atomic<MMFilesWalLogfile::IdType> _lastCollectedId;

  // last fully sealed logfile id. note: writing to this variable is
  /// protected by the _idLock
  std::atomic<MMFilesWalLogfile::IdType> _lastSealedId;

  // a lock protecting the shutdown file
  Mutex _shutdownFileLock;

  std::string _shutdownFile;

  // set of dropped collections
  /// this is populated during recovery and not used afterwards
  std::unordered_set<TRI_voc_cid_t> _droppedCollections;

  // set of dropped databases
  /// this is populated during recovery and not used afterwards
  std::unordered_set<TRI_voc_tick_t> _droppedDatabases;

  // a lock protecting the updates of _lastCollectedId, _lastSealedId,
  /// and _lastOpenedId
  Mutex _idLock;

  // whether or not write-throttling is currently enabled
  bool _writeThrottled;

  // whether or not we have been shut down already
  volatile sig_atomic_t _shutdown;

  // a lock protecting _barriers
  basics::ReadWriteLock _barriersLock;

  // barriers that prevent WAL logfiles from being collected
  std::unordered_map<TRI_voc_tick_t, LogfileBarrier*> _barriers;
};

}

#endif
