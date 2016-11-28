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

#ifndef ARANGOD_WAL_LOGFILE_MANAGER_H
#define ARANGOD_WAL_LOGFILE_MANAGER_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"
#include "Wal/Logfile.h"
#include "Wal/Marker.h"
#include "Wal/Slots.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}

namespace wal {

class AllocatorThread;
class CollectorThread;
struct RecoverState;
class RemoverThread;
class Slot;
class SynchronizerThread;

struct LogfileRange {
  LogfileRange(Logfile::IdType id, std::string const& filename,
               std::string const& state, TRI_voc_tick_t tickMin,
               TRI_voc_tick_t tickMax)
      : id(id),
        filename(filename),
        state(state),
        tickMin(tickMin),
        tickMax(tickMax) {}

  Logfile::IdType id;
  std::string filename;
  std::string state;
  TRI_voc_tick_t tickMin;
  TRI_voc_tick_t tickMax;
};

typedef std::vector<LogfileRange> LogfileRanges;

struct LogfileManagerState {
  TRI_voc_tick_t lastAssignedTick;
  TRI_voc_tick_t lastCommittedTick;
  TRI_voc_tick_t lastCommittedDataTick;
  uint64_t numEvents;
  uint64_t numEventsSync;
  std::string timeString;
};

struct LogfileBarrier {
  LogfileBarrier() = delete;

  LogfileBarrier(TRI_voc_tick_t id, double expires, TRI_voc_tick_t minTick)
      : id(id), expires(expires), minTick(minTick) {}

  TRI_voc_tick_t const id;
  double expires;
  TRI_voc_tick_t minTick;
};

class LogfileManager final : public application_features::ApplicationFeature {
  friend class AllocatorThread;
  friend class CollectorThread;

  LogfileManager(LogfileManager const&) = delete;
  LogfileManager& operator=(LogfileManager const&) = delete;

  static constexpr size_t numBuckets = 16;

 public:
  explicit LogfileManager(application_features::ApplicationServer* server);

  // destroy the logfile manager
  ~LogfileManager();

  // get the logfile manager instance
  static LogfileManager* instance() {
    TRI_ASSERT(Instance != nullptr);
    return Instance;
  }

 private:
  static LogfileManager* Instance;

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

  // whether or not there was a SHUTDOWN file with a tick value
  /// at server start
  inline bool hasFoundLastTick() const { return _hasFoundLastTick; }

  // whether or not we are in the recovery phase
  inline bool isInRecovery() const { return _inRecovery; }

  // whether or not we are in the shutdown phase
  inline bool isInShutdown() const { return (_shutdown != 0); }

  // return the slots manager
  Slots* slots() { return _slots; }

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
  int registerTransaction(TRI_voc_tid_t);

  // unregisters a transaction
  void unregisterTransaction(TRI_voc_tid_t, bool);

  // return the set of failed transactions
  std::unordered_set<TRI_voc_tid_t> getFailedTransactions();

  // return the set of dropped collections
  /// this is used during recovery and not used afterwards
  std::unordered_set<TRI_voc_cid_t> getDroppedCollections();

  // return the set of dropped databases
  /// this is used during recovery and not used afterwards
  std::unordered_set<TRI_voc_tick_t> getDroppedDatabases();

  // unregister a list of failed transactions
  void unregisterFailedTransactions(std::unordered_set<TRI_voc_tid_t> const&);

  // whether or not it is currently allowed to create an additional
  /// logfile
  bool logfileCreationAllowed(uint32_t);

  // whether or not there are reserve logfiles
  bool hasReserveLogfiles();

  // signal that a sync operation is required
  void signalSync(bool);

  // write data into the logfile, using database id and collection id
  /// this is a convenience function that combines allocate, memcpy and finalize
  SlotInfoCopy allocateAndWrite(TRI_voc_tick_t databaseId, 
                                TRI_voc_cid_t collectionId, 
                                Marker const*, bool wakeUpSynchronizer,
                                bool waitForSyncRequested, bool waitUntilSyncDone);

  // write data into the logfile
  /// this is a convenience function that combines allocate, memcpy and finalize
  SlotInfoCopy allocateAndWrite(Marker const* marker, bool wakeUpSynchronizer, 
                                bool waitForSyncRequested, bool waitUntilSyncDone);

  // write marker into the logfile
  // this is a convenience function with less parameters
  SlotInfoCopy allocateAndWrite(Marker const& marker, bool waitForSync);

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
  void relinkLogfile(Logfile*);

  // remove a logfile from the inventory only
  bool unlinkLogfile(Logfile*);

  // remove a logfile from the inventory only
  Logfile* unlinkLogfile(Logfile::IdType);

  // removes logfiles that are allowed to be removed
  bool removeLogfiles();

  // sets the status of a logfile to open
  void setLogfileOpen(Logfile*);

  // sets the status of a logfile to seal-requested
  void setLogfileSealRequested(Logfile*);

  // sets the status of a logfile to sealed
  void setLogfileSealed(Logfile*);

  // sets the status of a logfile to sealed
  void setLogfileSealed(Logfile::IdType);

  // return the status of a logfile
  Logfile::StatusType getLogfileStatus(Logfile::IdType);

  // return the file descriptor of a logfile
  int getLogfileDescriptor(Logfile::IdType);

  // get the current open region of a logfile
  /// this uses the slots lock
  void getActiveLogfileRegion(Logfile*, char const*&, char const*&);

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
  std::vector<Logfile*> getLogfilesForTickRange(TRI_voc_tick_t, TRI_voc_tick_t,
                                                bool& minTickIncluded);

  // return logfiles for a tick range
  void returnLogfiles(std::vector<Logfile*> const&);

  // get a logfile by id
  Logfile* getLogfile(Logfile::IdType);

  // get a logfile and its status by id
  Logfile* getLogfile(Logfile::IdType, Logfile::StatusType&);

  // get a logfile for writing. this may return nullptr
  int getWriteableLogfile(uint32_t, Logfile::StatusType&, Logfile*&);

  // get a logfile to collect. this may return nullptr
  Logfile* getCollectableLogfile();

  // get a logfile to remove. this may return nullptr
  /// if it returns a logfile, the logfile is removed from the list of available
  /// logfiles
  Logfile* getRemovableLogfile();

  // increase the number of collect operations for a logfile
  void increaseCollectQueueSize(Logfile*);

  // decrease the number of collect operations for a logfile
  void decreaseCollectQueueSize(Logfile*);

  // mark a file as being requested for collection
  void setCollectionRequested(Logfile*);

  // mark a file as being done with collection
  void setCollectionDone(Logfile*);

  // force the status of a specific logfile
  void forceStatus(Logfile*, Logfile::StatusType);

  // return the current state
  LogfileManagerState state();

  // return the current available logfile ranges
  LogfileRanges ranges();

  // get information about running transactions
  std::tuple<size_t, Logfile::IdType, Logfile::IdType> runningTransactions();
  
  void waitForCollector();

 private:
  // hashes the transaction id into a bucket
  size_t getBucket(TRI_voc_tid_t id) const { return std::hash<TRI_voc_cid_t>()(id) % numBuckets; }
  
  // reserve space in a logfile
  SlotInfo allocate(uint32_t);

  // reserve space in a logfile
  SlotInfo allocate(TRI_voc_tick_t, TRI_voc_cid_t, uint32_t);

  // memcpy the data into the WAL region and return the filled slot
  // to the WAL logfile manager
  SlotInfoCopy writeSlot(SlotInfo& slotInfo,
                         Marker const* marker,
                         bool wakeUpSynchronizer,
                         bool waitForSyncRequested,
                         bool waitUntilSyncDone);

  // remove a logfile in the file system
  void removeLogfile(Logfile*);

  // wait for the collector thread to collect a specific logfile
  int waitForCollector(Logfile::IdType, double);

  // closes all logfiles
  void closeLogfiles();

  // reads the shutdown information
  int readShutdownInfo();

  // writes the shutdown information
  int writeShutdownInfo(bool);

  // start the synchronizer thread
  int startSynchronizerThread();

  // stop the synchronizer thread
  void stopSynchronizerThread();

  // start the allocator thread
  int startAllocatorThread();

  // stop the allocator thread
  void stopAllocatorThread();

  // start the collector thread
  int startCollectorThread();

  // stop the collector thread
  void stopCollectorThread();

  // start the remover thread
  int startRemoverThread();

  // stop the remover thread
  void stopRemoverThread();

  // check which logfiles are present in the log directory
  int inventory();

  // inspect all found WAL logfiles
  /// this searches for the max tick in the logfiles and builds up the initial
  /// transaction state
  int inspectLogfiles();

  // allocate a new reserve logfile
  int createReserveLogfile(uint32_t);

  // get an id for the next logfile
  Logfile::IdType nextId();

  // ensure the wal logfiles directory is actually there
  int ensureDirectory();

  // return the absolute name of the shutdown file
  std::string shutdownFilename() const;

  // return an absolute filename for a logfile id
  std::string logfileName(Logfile::IdType) const;

 private:
  // the arangod config variable containing the database path
  std::string _databasePath;

  // state during recovery
  RecoverState* _recoverState;

  bool _allowOversizeEntries = true;
  bool _useMLock = false;
  std::string _directory = "";
  uint32_t _historicLogfiles = 10;
  bool _ignoreLogfileErrors = false;
  bool _ignoreRecoveryErrors = false;
  uint32_t _filesize = 32 * 1024 * 1024;
  uint32_t _maxOpenLogfiles = 0;
  uint32_t _reserveLogfiles = 3;
  uint32_t _numberOfSlots = 1048576;
  uint64_t _syncInterval = 100;
  uint64_t _throttleWhenPending = 0;
  uint64_t _maxThrottleWait = 15000;

  // whether or not writes to the WAL are allowed
  bool _allowWrites;

  // this is true if there was a SHUTDOWN file with a last tick at
  /// server start
  bool _hasFoundLastTick;

  // whether or not the recovery procedure is running
  bool _inRecovery;

  // a lock protecting the _logfiles map and the logfiles' statuses
  basics::ReadWriteLock _logfilesLock;

  // the logfiles
  std::map<Logfile::IdType, Logfile*> _logfiles;

  // the slots manager
  Slots* _slots;

  // the synchronizer thread
  SynchronizerThread* _synchronizerThread;

  // the allocator thread
  AllocatorThread* _allocatorThread;

  // the collector thread
  CollectorThread* _collectorThread;

  // the logfile remover thread
  RemoverThread* _removerThread;

  // last opened logfile id. note: writing to this variable is protected
  /// by the _idLock
  std::atomic<Logfile::IdType> _lastOpenedId;

  // last fully collected logfile id. note: writing to this variable is
  /// protected by the_idLock
  std::atomic<Logfile::IdType> _lastCollectedId;

  // last fully sealed logfile id. note: writing to this variable is
  /// protected by the _idLock
  std::atomic<Logfile::IdType> _lastSealedId;

  // a lock protecting the shutdown file
  Mutex _shutdownFileLock;

  std::string _shutdownFile;

  // a lock protecting ALL buckets in _transactions
  basics::ReadWriteLock _allTransactionsLock;

  struct {
    // a lock protecting _activeTransactions and _failedTransactions
    basics::ReadWriteLock _lock;

    // currently ongoing transactions
    std::unordered_map<TRI_voc_tid_t, std::pair<Logfile::IdType, Logfile::IdType>>
        _activeTransactions;

    // set of failed transactions
    std::unordered_set<TRI_voc_tid_t> _failedTransactions;
  } _transactions[numBuckets];

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
}

#endif
