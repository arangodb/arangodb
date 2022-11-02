////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBRecoveryManager.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEdgeIndex.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBRecoveryHelper.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBVPackIndex.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include <atomic>

using namespace arangodb::application_features;

namespace arangodb {

/// Constructor needs to be called synchronously,
/// will load counts from the db and scan the WAL
RocksDBRecoveryManager::RocksDBRecoveryManager(Server& server)
    : ArangodFeature{server, *this},
      _currentSequenceNumber(0),
      _recoveryState(RecoveryState::BEFORE) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<DatabaseFeature>();
  startsAfter<RocksDBEngine>();
  startsAfter<ServerIdFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<SystemDatabaseFeature>();

  onlyEnabledWith<RocksDBEngine>();
}

void RocksDBRecoveryManager::start() {
  TRI_ASSERT(isEnabled());

  // synchronizes with acquire inRecovery()
  _recoveryState.store(RecoveryState::IN_PROGRESS, std::memory_order_release);

  // start recovery
  runRecovery();

  // synchronizes with acquire inRecovery()
  _recoveryState.store(RecoveryState::DONE, std::memory_order_release);

  // notify everyone that recovery is now done
  auto& databaseFeature = server().getFeature<DatabaseFeature>();
  databaseFeature.recoveryDone();
}

/// parse recent RocksDB WAL entries and notify the
/// DatabaseFeature about the successful recovery
void RocksDBRecoveryManager::runRecovery() {
  auto res = parseRocksWAL();
  if (res.fail()) {
    LOG_TOPIC("be0ce", FATAL, Logger::ENGINES)
        << "failed during rocksdb WAL recovery: " << res.errorMessage();
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_RECOVERY);
  }

  // now restore collection counts into collections
}

RecoveryState RocksDBRecoveryManager::recoveryState() const noexcept {
  return _recoveryState.load(std::memory_order_acquire);
}

rocksdb::SequenceNumber RocksDBRecoveryManager::recoverySequenceNumber()
    const noexcept {
  return _currentSequenceNumber.load(std::memory_order_relaxed);
}

class WBReader final : public rocksdb::WriteBatch::Handler {
 private:
  WBReader(WBReader const&) = delete;
  WBReader const& operator=(WBReader const&) = delete;

  ArangodServer& _server;

  struct ProgressState {
    // sequence number from which we start recovering
    rocksdb::SequenceNumber const recoveryStartSequence;
    // latest sequence in WAL
    rocksdb::SequenceNumber const latestSequence;

    // informational section, used only for progress reporting
    rocksdb::SequenceNumber sequenceRange = 0;
    rocksdb::SequenceNumber rangeBegin = 0;
    int reportTicker = 0;
    int progressValue = 0;
  } _progressState;

  // minimum server tick we are going to accept (initialized to
  // TRI_NewTickServer())
  uint64_t const _minimumServerTick;
  // max tick value found in WAL
  uint64_t _maxTickFound;
  // max HLC value found in WAL
  uint64_t _maxHLCFound;
  // number of WAL entries scanned
  uint64_t _entriesScanned;
  // last document removed
  RevisionId _lastRemovedDocRid = RevisionId::none();
  // start of batch sequence number (currently only set, but not read back -
  // can be used for debugging later)
  rocksdb::SequenceNumber _batchStartSequence;
  // current sequence number
  std::atomic<rocksdb::SequenceNumber>& _currentSequence;

  RocksDBEngine& _engine;
  // whether we are currently at the start of a batch
  bool _startOfBatch = false;

 public:
  /// @param seqs sequence number from which to count operations
  explicit WBReader(ArangodServer& server,
                    rocksdb::SequenceNumber recoveryStartSequence,
                    rocksdb::SequenceNumber latestSequence,
                    std::atomic<rocksdb::SequenceNumber>& currentSequence)
      : _server(server),
        _progressState{recoveryStartSequence, latestSequence},
        _minimumServerTick(TRI_NewTickServer()),
        _maxTickFound(0),
        _maxHLCFound(0),
        _entriesScanned(0),
        _lastRemovedDocRid(0),
        _batchStartSequence(0),
        _currentSequence(currentSequence),
        _engine(_server.getFeature<EngineSelectorFeature>()
                    .engine<RocksDBEngine>()) {}

  void startNewBatch(rocksdb::SequenceNumber startSequence) {
    TRI_ASSERT(_currentSequence <= startSequence);

    if (_batchStartSequence == 0) {
      // for the first call, initialize the [from - to] recovery range values
      _progressState.rangeBegin = startSequence;
      _progressState.sequenceRange =
          _progressState.latestSequence - _progressState.rangeBegin;
    }

    // progress reporting. only do this every 100 iterations to avoid the
    // overhead of the calculations for every new sequence number
    if (_progressState.sequenceRange > 0 &&
        ++_progressState.reportTicker >= 100) {
      _progressState.reportTicker = 0;

      auto progress =
          static_cast<int>(100.0 * (startSequence - _progressState.rangeBegin) /
                           _progressState.sequenceRange);

      // report only every 5%, so that we don't flood the log with micro
      // progress
      if (progress >= 5 && progress >= _progressState.progressValue + 5) {
        LOG_TOPIC("fb20c", INFO, Logger::ENGINES)
            << "Recovering from sequence number " << startSequence << " ("
            << progress << "% of WAL)...";

        _progressState.progressValue = progress;
      }
    }

    // starting new write batch
    _batchStartSequence = startSequence;
    _currentSequence = startSequence;
    _startOfBatch = true;
  }

  Result shutdownWBReader() {
    Result rv = basics::catchVoidToResult([&]() -> void {
      if (_engine.dbExisted()) {
        LOG_TOPIC("a4ec8", INFO, Logger::ENGINES)
            << "RocksDB recovery finished, "
            << "WAL entries scanned: " << _entriesScanned
            << ", recovery start sequence number: "
            << _progressState.recoveryStartSequence
            << ", latest WAL sequence number: "
            << _engine.db()->GetLatestSequenceNumber()
            << ", max tick value found in WAL: " << _maxTickFound
            << ", last HLC value found in WAL: " << _maxHLCFound;
      }

      // update ticks after parsing wal
      TRI_UpdateTickServer(_maxTickFound);

      TRI_HybridLogicalClock(_maxHLCFound);
    });
    return rv;
  }

 private:
  void storeMaxHLC(uint64_t hlc) {
    if (hlc > _maxHLCFound) {
      _maxHLCFound = hlc;
    }
  }

  void storeMaxTick(uint64_t tick) {
    if (tick > _maxTickFound) {
      _maxTickFound = tick;
    }
  }

  // find estimator for index
  RocksDBCollection* findCollection(uint64_t objectId) {
    // now adjust the counter in collections which are already loaded
    RocksDBEngine::CollectionPair dbColPair =
        _engine.mapObjectToCollection(objectId);
    if (dbColPair.second.empty() || dbColPair.first == 0) {
      // collection with this objectID not known.Skip.
      return nullptr;
    }
    DatabaseFeature& df = _server.getFeature<DatabaseFeature>();
    TRI_vocbase_t* vocbase = df.useDatabase(dbColPair.first);
    if (vocbase == nullptr) {
      return nullptr;
    }
    auto sg = arangodb::scopeGuard([&]() noexcept { vocbase->release(); });
    return static_cast<RocksDBCollection*>(
        vocbase->lookupCollection(dbColPair.second)->getPhysical());
  }

  RocksDBIndex* findIndex(uint64_t objectId) {
    RocksDBEngine::IndexTriple triple = _engine.mapObjectToIndex(objectId);
    if (std::get<0>(triple) == 0 && std::get<1>(triple).empty()) {
      return nullptr;
    }

    DatabaseFeature& df = _server.getFeature<DatabaseFeature>();
    TRI_vocbase_t* vb = df.useDatabase(std::get<0>(triple));
    if (vb == nullptr) {
      return nullptr;
    }
    auto sg = arangodb::scopeGuard([&]() noexcept { vb->release(); });

    auto coll = vb->lookupCollection(std::get<1>(triple));

    if (coll == nullptr) {
      return nullptr;
    }

    std::shared_ptr<Index> index = coll->lookupIndex(std::get<2>(triple));
    if (index == nullptr) {
      return nullptr;
    }
    return static_cast<RocksDBIndex*>(index.get());
  }

  void updateMaxTick(uint32_t column_family_id, const rocksdb::Slice& key,
                     const rocksdb::Slice& value) {
    // RETURN (side-effect): update _maxTickFound
    //
    // extract max tick from Markers and store them as side-effect in
    // _maxTickFound member variable that can be used later (dtor) to call
    // TRI_UpdateTickServer (ticks.h)
    // Markers: - collections (id,objectid) as tick and max tick in indexes
    // array
    //          - documents - _rev (revision as maxtick)
    //          - databases

    if (column_family_id == RocksDBColumnFamilyManager::get(
                                RocksDBColumnFamilyManager::Family::Documents)
                                ->GetID()) {
      storeMaxHLC(RocksDBKey::documentId(key).id());
    } else if (column_family_id ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::PrimaryIndex)
                   ->GetID()) {
      // document key
      std::string_view ref = RocksDBKey::primaryKey(key);
      TRI_ASSERT(!ref.empty());
      // check if the key is numeric
      if (ref[0] >= '1' && ref[0] <= '9') {
        // numeric start byte. looks good
        bool valid;
        uint64_t tick = NumberUtils::atoi<uint64_t>(
            ref.data(), ref.data() + ref.size(), valid);
        if (valid && tick > _maxTickFound) {
          uint64_t compareTick = _maxTickFound;
          if (compareTick == 0) {
            compareTick = _minimumServerTick;
          }
          // if no previous _maxTickFound set or the numeric value found is
          // "near" our previous _maxTickFound, then we update it
          if (tick > compareTick && (tick - compareTick) < 2048) {
            storeMaxTick(tick);
          }
        }
        // else we got a non-numeric key. simply ignore it
      }

      RocksDBIndex* idx = findIndex(RocksDBKey::objectId(key));
      if (idx) {
        idx->collection().keyGenerator().track(ref);
      }

    } else if (column_family_id ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::Definitions)
                   ->GetID()) {
      auto const type = RocksDBKey::type(key);

      if (type == RocksDBEntryType::Collection) {
        storeMaxTick(RocksDBKey::collectionId(key).id());
        auto slice = RocksDBValue::data(value);
        storeMaxTick(basics::VelocyPackHelper::stringUInt64(
            slice, StaticStrings::ObjectId));
        VPackSlice indexes = slice.get("indexes");
        for (VPackSlice idx : VPackArrayIterator(indexes)) {
          storeMaxTick(std::max(basics::VelocyPackHelper::stringUInt64(
                                    idx, StaticStrings::ObjectId),
                                basics::VelocyPackHelper::stringUInt64(
                                    idx, StaticStrings::IndexId)));
        }
      } else if (type == RocksDBEntryType::Database) {
        storeMaxTick(RocksDBKey::databaseId(key));
      } else if (type == RocksDBEntryType::View) {
        storeMaxTick(std::max(RocksDBKey::databaseId(key),
                              RocksDBKey::viewId(key).id()));
      }
    }
  }

  // tick function that is called before each new WAL entry
  void incTick() {
    if (_startOfBatch) {
      // we are at the start of a batch. do NOT increase sequence number
      _startOfBatch = false;
    } else {
      // we are inside a batch already. now increase sequence number
      _currentSequence.fetch_add(1, std::memory_order_relaxed);
    }
  }

 public:
  rocksdb::Status PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                        const rocksdb::Slice& value) override {
    ++_entriesScanned;

    LOG_TOPIC("3e5c5", TRACE, Logger::ENGINES)
        << "recovering PUT @ " << _currentSequence << " " << RocksDBKey(key);
    incTick();

    updateMaxTick(column_family_id, key, value);
    if (column_family_id == RocksDBColumnFamilyManager::get(
                                RocksDBColumnFamilyManager::Family::Documents)
                                ->GetID()) {
      auto coll = findCollection(RocksDBKey::objectId(key));
      if (coll) {
        coll->meta().adjustNumberDocumentsInRecovery(
            _currentSequence,
            transaction::helpers::extractRevFromDocument(
                RocksDBValue::data(value)),
            1);

        std::vector<std::uint64_t> inserts;
        std::vector<std::uint64_t> removes;
        inserts.emplace_back(RocksDBKey::documentId(key).id());
        coll->bufferUpdates(_currentSequence, std::move(inserts),
                            std::move(removes));
      }
    } else {
      // We have to adjust the estimate with an insert
      uint64_t hashval = 0;
      if (column_family_id ==
          RocksDBColumnFamilyManager::get(
              RocksDBColumnFamilyManager::Family::VPackIndex)
              ->GetID()) {
        hashval = RocksDBVPackIndex::HashForKey(key);
      } else if (column_family_id ==
                 RocksDBColumnFamilyManager::get(
                     RocksDBColumnFamilyManager::Family::EdgeIndex)
                     ->GetID()) {
        hashval = RocksDBEdgeIndex::HashForKey(key);
      }

      if (hashval != 0) {
        auto* idx = findIndex(RocksDBKey::objectId(key));
        if (idx) {
          RocksDBCuckooIndexEstimatorType* est = idx->estimator();
          if (est && est->appliedSeq() < _currentSequence) {
            // We track estimates for this index
            est->insert(hashval);
          }
        }
      }
    }

    for (auto helper : _engine.recoveryHelpers()) {
      helper->PutCF(column_family_id, key, value, _currentSequence);
    }

    return rocksdb::Status();
  }

  void handleDeleteCF(uint32_t cfId, const rocksdb::Slice& key) {
    incTick();

    if (cfId == RocksDBColumnFamilyManager::get(
                    RocksDBColumnFamilyManager::Family::Documents)
                    ->GetID()) {
      uint64_t objectId = RocksDBKey::objectId(key);

      storeMaxHLC(RocksDBKey::documentId(key).id());
      storeMaxTick(objectId);

      auto coll = findCollection(RocksDBKey::objectId(key));

      if (coll) {
        coll->meta().adjustNumberDocumentsInRecovery(_currentSequence,
                                                     _lastRemovedDocRid, -1);

        std::vector<std::uint64_t> inserts;
        std::vector<std::uint64_t> removes;
        removes.emplace_back(RocksDBKey::documentId(key).id());
        coll->bufferUpdates(_currentSequence, std::move(inserts),
                            std::move(removes));
      }
      _lastRemovedDocRid = RevisionId::none();  // reset in any case

    } else {
      // We have to adjust the estimate with an insert
      uint64_t hashval = 0;
      if (cfId == RocksDBColumnFamilyManager::get(
                      RocksDBColumnFamilyManager::Family::VPackIndex)
                      ->GetID()) {
        hashval = RocksDBVPackIndex::HashForKey(key);
      } else if (cfId == RocksDBColumnFamilyManager::get(
                             RocksDBColumnFamilyManager::Family::EdgeIndex)
                             ->GetID()) {
        hashval = RocksDBEdgeIndex::HashForKey(key);
      }

      if (hashval != 0) {
        auto* idx = findIndex(RocksDBKey::objectId(key));
        if (idx) {
          RocksDBCuckooIndexEstimatorType* est = idx->estimator();
          if (est && est->appliedSeq() < _currentSequence) {
            // We track estimates for this index
            est->remove(hashval);
          }
        }
      }
    }
  }

  rocksdb::Status DeleteCF(uint32_t column_family_id,
                           const rocksdb::Slice& key) override {
    ++_entriesScanned;

    LOG_TOPIC("5f341", TRACE, Logger::ENGINES)
        << "recovering DELETE @ " << _currentSequence << " " << RocksDBKey(key);
    handleDeleteCF(column_family_id, key);
    for (auto helper : _engine.recoveryHelpers()) {
      helper->DeleteCF(column_family_id, key, _currentSequence);
    }

    return rocksdb::Status();
  }

  rocksdb::Status SingleDeleteCF(uint32_t column_family_id,
                                 const rocksdb::Slice& key) override {
    ++_entriesScanned;

    LOG_TOPIC("aa997", TRACE, Logger::ENGINES)
        << "recovering SINGLE DELETE @ " << _currentSequence << " "
        << RocksDBKey(key);
    handleDeleteCF(column_family_id, key);
    for (auto helper : _engine.recoveryHelpers()) {
      helper->SingleDeleteCF(column_family_id, key, _currentSequence);
    }

    return rocksdb::Status();
  }

  rocksdb::Status DeleteRangeCF(uint32_t column_family_id,
                                const rocksdb::Slice& begin_key,
                                const rocksdb::Slice& end_key) override {
    ++_entriesScanned;

    LOG_TOPIC("ed6f5", TRACE, Logger::ENGINES)
        << "recovering DELETE RANGE @ " << _currentSequence << " from "
        << RocksDBKey(begin_key) << " to " << RocksDBKey(end_key);
    incTick();
    // drop and truncate can use this, truncate is handled via a Log marker
    for (auto helper : _engine.recoveryHelpers()) {
      helper->DeleteRangeCF(column_family_id, begin_key, end_key,
                            _currentSequence);
    }

    // check for a range-delete of the primary index
    if (column_family_id == RocksDBColumnFamilyManager::get(
                                RocksDBColumnFamilyManager::Family::Documents)
                                ->GetID()) {
      uint64_t objectId = RocksDBKey::objectId(begin_key);
      TRI_ASSERT(objectId == RocksDBKey::objectId(end_key));

      auto coll = findCollection(objectId);
      if (!coll) {
        return rocksdb::Status();
      }

      uint64_t currentCount = coll->meta().numberDocuments();
      if (currentCount != 0) {
        coll->meta().adjustNumberDocumentsInRecovery(
            _currentSequence, RevisionId::none(),
            -static_cast<int64_t>(currentCount));
      }
      for (std::shared_ptr<arangodb::Index> const& idx : coll->getIndexes()) {
        RocksDBIndex* ridx = static_cast<RocksDBIndex*>(idx.get());
        RocksDBCuckooIndexEstimatorType* est = ridx->estimator();
        TRI_ASSERT(ridx->type() != Index::TRI_IDX_TYPE_EDGE_INDEX || est);
        if (est) {
          est->clearInRecovery(_currentSequence);
        }
      }
      coll->bufferTruncate(_currentSequence);
    }

    return rocksdb::Status();  // make WAL iterator happy
  }

  void LogData(const rocksdb::Slice& blob) override {
    ++_entriesScanned;

    // a delete log message appears directly before a Delete
    RocksDBLogType type = RocksDBLogValue::type(blob);
    switch (type) {
      case RocksDBLogType::DocumentRemoveV2:  // remove within a trx
      case RocksDBLogType::SingleRemoveV2:    // single remove
        TRI_ASSERT(_lastRemovedDocRid.empty());
        _lastRemovedDocRid = RocksDBLogValue::revisionId(blob);
        break;
      default:
        _lastRemovedDocRid = RevisionId::none();  // reset in any other case
        break;
    }
    for (auto helper : _engine.recoveryHelpers()) {
      helper->LogData(blob, _currentSequence);
    }
  }

  rocksdb::Status MarkBeginPrepare(bool = false) override {
    TRI_ASSERT(false);
    return rocksdb::Status::InvalidArgument(
        "MarkBeginPrepare() handler not defined.");
  }

  rocksdb::Status MarkEndPrepare(rocksdb::Slice const& /*xid*/) override {
    TRI_ASSERT(false);
    return rocksdb::Status::InvalidArgument(
        "MarkEndPrepare() handler not defined.");
  }

  rocksdb::Status MarkNoop(bool /*empty_batch*/) override {
    return rocksdb::Status::OK();
  }

  rocksdb::Status MarkRollback(rocksdb::Slice const& /*xid*/) override {
    TRI_ASSERT(false);
    return rocksdb::Status::InvalidArgument(
        "MarkRollbackPrepare() handler not defined.");
  }

  rocksdb::Status MarkCommit(rocksdb::Slice const& /*xid*/) override {
    TRI_ASSERT(false);
    return rocksdb::Status::InvalidArgument(
        "MarkCommit() handler not defined.");
  }

  // MergeCF is not used
};

/// parse the WAL with the above handler parser class
Result RocksDBRecoveryManager::parseRocksWAL() {
  Result shutdownRv;

  Result res = basics::catchToResult([&, &server = server()]() -> Result {
    RocksDBEngine& engine =
        server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();

    auto db = engine.db();

    Result rv;
    for (auto& helper : engine.recoveryHelpers()) {
      helper->prepare();
    }

    rocksdb::SequenceNumber earliest =
        engine.settingsManager()->earliestSeqNeeded();
    auto recoveryStartSequence = std::min(earliest, engine.releasedTick());

#ifdef ARANGODB_USE_GOOGLE_TESTS
    engine.recoveryStartSequence(recoveryStartSequence);
#endif

    auto latestSequenceNumber = db->GetLatestSequenceNumber();

    if (engine.dbExisted()) {
      size_t filesInArchive = 0;
      try {
        std::string archive = basics::FileUtils::buildFilename(
            db->GetOptions().wal_dir, "archive");
        filesInArchive = TRI_FilesDirectory(archive.c_str()).size();
      } catch (...) {
        // don't ever fail recovery because we can't get list of files in
        // archive
      }

      LOG_TOPIC("fe333", INFO, Logger::ENGINES)
          << "RocksDB recovery starting, scanning WAL starting from sequence "
             "number "
          << recoveryStartSequence
          << ", latest sequence number: " << latestSequenceNumber
          << ", files in archive: " << filesInArchive;
    }

    // Tell the WriteBatch reader the transaction markers to look for
    TRI_ASSERT(_currentSequenceNumber == 0);
    WBReader handler(server, recoveryStartSequence, latestSequenceNumber,
                     _currentSequenceNumber);

    // prevent purging of WAL files while we are in here
    RocksDBFilePurgePreventer purgePreventer(engine.disallowPurging());

    std::unique_ptr<rocksdb::TransactionLogIterator> iterator;
    rocksdb::Status s =
        db->GetUpdatesSince(recoveryStartSequence, &iterator,
                            rocksdb::TransactionLogIterator::ReadOptions(true));

    rv = rocksutils::convertStatus(s);

    if (rv.ok()) {
      while (iterator->Valid()) {
        s = iterator->status();
        if (s.ok()) {
          rocksdb::BatchResult batch = iterator->GetBatch();
          handler.startNewBatch(batch.sequence);
          s = batch.writeBatchPtr->Iterate(&handler);
        }

        if (!s.ok()) {
          rv = rocksutils::convertStatus(s);
          std::string msg = basics::StringUtils::concatT(
              "error during WAL scan: ", rv.errorMessage());
          LOG_TOPIC("ee333", ERR, Logger::ENGINES) << msg;
          rv.reset(rv.errorNumber(), std::move(msg));  // update message
          break;
        }

        iterator->Next();
      }
    }

    shutdownRv = handler.shutdownWBReader();

    return rv;
  });

  if (res.ok()) {
    res = std::move(shutdownRv);
  } else {
    if (shutdownRv.fail()) {
      res.reset(res.errorNumber(),
                basics::StringUtils::concatT(res.errorMessage(), " - ",
                                             shutdownRv.errorMessage()));
    }
  }

  return res;
}

}  // namespace arangodb
