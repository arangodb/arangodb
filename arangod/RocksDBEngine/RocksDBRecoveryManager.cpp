////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/NumberUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
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
#include "StorageEngine/StorageEngineFeature.h"
#include "Transaction/Helpers.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::application_features;

namespace arangodb {

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBRecoveryManager::RocksDBRecoveryManager(application_features::ApplicationServer& server)
    : ApplicationFeature(server, featureName()),
      _db(nullptr),
      _tick(0),
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
  _db = server().getFeature<RocksDBEngine>().db();
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

class WBReader final : public rocksdb::WriteBatch::Handler {
 private:
  WBReader(WBReader const&) = delete;
  WBReader const& operator=(WBReader const&) = delete;

  application_features::ApplicationServer& _server;

  // used to track used IDs for key-generators
  std::map<uint64_t, uint64_t> _generators;

  // max tick found
  uint64_t _maxTick;
  uint64_t _maxHLC;
  /// @brief last document removed
  RevisionId _lastRemovedDocRid = RevisionId::none();

  rocksdb::SequenceNumber _startSequence;    /// start of batch sequence nr
  rocksdb::SequenceNumber& _currentSequence;  /// current sequence nr
  bool _startOfBatch = false;

 public:
  /// @param seqs sequence number from which to count operations
  explicit WBReader(application_features::ApplicationServer& server,
                    rocksdb::SequenceNumber& currentSequence)
      : _server(server),
        _maxTick(TRI_NewTickServer()),
        _maxHLC(0),
        _lastRemovedDocRid(0),
        _startSequence(0),
        _currentSequence(currentSequence) {}

  void startNewBatch(rocksdb::SequenceNumber startSequence) {
    // starting new write batch
    _startSequence = startSequence;
    _currentSequence = startSequence;
    _startOfBatch = true;
    TRI_ASSERT(_maxTick > 0);
  }

  Result shutdownWBReader() {
    Result rv = basics::catchVoidToResult([&]() -> void {
      // update ticks after parsing wal
      LOG_TOPIC("a4ec8", TRACE, Logger::ENGINES)
          << "max tick found in WAL: " << _maxTick << ", last HLC value: " << _maxHLC;

      TRI_UpdateTickServer(_maxTick);
      TRI_HybridLogicalClock(_maxHLC);
    });
    return rv;
  }

 private:
  void storeMaxHLC(uint64_t hlc) {
    if (hlc > _maxHLC) {
      _maxHLC = hlc;
    }
  }

  void storeMaxTick(uint64_t tick) {
    if (tick > _maxTick) {
      _maxTick = tick;
    }
  }

  // find estimator for index
  RocksDBCollection* findCollection(uint64_t objectId) {
    RocksDBEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    // now adjust the counter in collections which are already loaded
    RocksDBEngine::CollectionPair dbColPair = engine.mapObjectToCollection(objectId);
    if (dbColPair.second.empty() || dbColPair.first == 0) {
      // collection with this objectID not known.Skip.
      return nullptr;
    }
    DatabaseFeature& df = _server.getFeature<DatabaseFeature>();
    TRI_vocbase_t* vocbase = df.useDatabase(dbColPair.first);
    if (vocbase == nullptr) {
      return nullptr;
    }
    TRI_DEFER(vocbase->release());
    return static_cast<RocksDBCollection*>(
        vocbase->lookupCollection(dbColPair.second)->getPhysical());
  }

  RocksDBIndex* findIndex(uint64_t objectId) {
    RocksDBEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    RocksDBEngine::IndexTriple triple = engine.mapObjectToIndex(objectId);
    if (std::get<0>(triple) == 0 && std::get<1>(triple).empty()) {
      return nullptr;
    }

    DatabaseFeature& df = _server.getFeature<DatabaseFeature>();
    TRI_vocbase_t* vb = df.useDatabase(std::get<0>(triple));
    if (vb == nullptr) {
      return nullptr;
    }
    TRI_DEFER(vb->release());

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
    // RETURN (side-effect): update _maxTick
    //
    // extract max tick from Markers and store them as side-effect in
    // _maxTick member variable that can be used later (dtor) to call
    // TRI_UpdateTickServer (ticks.h)
    // Markers: - collections (id,objectid) as tick and max tick in indexes
    // array
    //          - documents - _rev (revision as maxtick)
    //          - databases

    if (column_family_id ==
        RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
            ->GetID()) {
      storeMaxHLC(RocksDBKey::documentId(key).id());
    } else if (column_family_id ==
               RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::PrimaryIndex)
                   ->GetID()) {
      // document key
      arangodb::velocypack::StringRef ref = RocksDBKey::primaryKey(key);
      TRI_ASSERT(!ref.empty());
      // check if the key is numeric
      if (ref[0] >= '1' && ref[0] <= '9') {
        // numeric start byte. looks good
        bool valid;
        uint64_t tick =
            NumberUtils::atoi<uint64_t>(ref.data(), ref.data() + ref.size(), valid);
        if (valid) {
          // if no previous _maxTick set or the numeric value found is
          // "near" our previous _maxTick, then we update it
          if (tick > _maxTick && (tick - _maxTick) < 2048) {
            storeMaxTick(tick);
          }
        }
        // else we got a non-numeric key. simply ignore it
      }

      RocksDBIndex* idx = findIndex(RocksDBKey::objectId(key));
      if (idx) {
        KeyGenerator* keyGen = idx->collection().keyGenerator();
        if (keyGen) {
          keyGen->track(ref.data(), ref.size());
        }
      }

    } else if (column_family_id ==
               RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions)
                   ->GetID()) {
      auto const type = RocksDBKey::type(key);

      if (type == RocksDBEntryType::Collection) {
        storeMaxTick(RocksDBKey::collectionId(key).id());
        auto slice = RocksDBValue::data(value);
        storeMaxTick(basics::VelocyPackHelper::stringUInt64(slice, "objectId"));
        VPackSlice indexes = slice.get("indexes");
        for (VPackSlice idx : VPackArrayIterator(indexes)) {
          storeMaxTick(
              std::max(basics::VelocyPackHelper::stringUInt64(idx, "objectId"),
                       basics::VelocyPackHelper::stringUInt64(idx, "id")));
        }
      } else if (type == RocksDBEntryType::Database) {
        storeMaxTick(RocksDBKey::databaseId(key));
      } else if (type == RocksDBEntryType::View) {
        storeMaxTick(
            std::max(RocksDBKey::databaseId(key), RocksDBKey::viewId(key).id()));
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
      ++_currentSequence;
    }
  }

 public:
  rocksdb::Status PutCF(uint32_t column_family_id,
                        const rocksdb::Slice& key,
                        const rocksdb::Slice& value) override {
    LOG_TOPIC("3e5c5", TRACE, Logger::ENGINES)
        << "recovering PUT @ " << _currentSequence << " " << RocksDBKey(key);
    incTick();

    updateMaxTick(column_family_id, key, value);
    if (column_family_id ==
        RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
            ->GetID()) {
      auto coll = findCollection(RocksDBKey::objectId(key));
      if (coll) {
        coll->meta().adjustNumberDocumentsInRecovery(_currentSequence,
                                                     transaction::helpers::extractRevFromDocument(
                                                         RocksDBValue::data(value)),
                                                     1);

        std::vector<std::uint64_t> inserts;
        std::vector<std::uint64_t> removes;
        inserts.emplace_back(RocksDBKey::documentId(key).id());
        coll->bufferUpdates(_currentSequence, std::move(inserts), std::move(removes));
      }
    } else {
      // We have to adjust the estimate with an insert
      uint64_t hashval = 0;
      if (column_family_id ==
          RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::VPackIndex)
              ->GetID()) {
        hashval = RocksDBVPackIndex::HashForKey(key);
      } else if (column_family_id ==
                 RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::EdgeIndex)
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

    RocksDBEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    for (auto helper : engine.recoveryHelpers()) {
      helper->PutCF(column_family_id, key, value, _currentSequence);
    }

    return rocksdb::Status();
  }

  void handleDeleteCF(uint32_t cfId, const rocksdb::Slice& key) {
    incTick();

    if (cfId == RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
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
        coll->bufferUpdates(_currentSequence, std::move(inserts), std::move(removes));
      }
      _lastRemovedDocRid = RevisionId::none();  // reset in any case

    } else {
      // We have to adjust the estimate with an insert
      uint64_t hashval = 0;
      if (cfId == RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::VPackIndex)
                      ->GetID()) {
        hashval = RocksDBVPackIndex::HashForKey(key);
      } else if (cfId == RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::EdgeIndex)
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

  rocksdb::Status DeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
    LOG_TOPIC("5f341", TRACE, Logger::ENGINES)
        << "recovering DELETE @ " << _currentSequence << " " << RocksDBKey(key);
    handleDeleteCF(column_family_id, key);
    RocksDBEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    for (auto helper : engine.recoveryHelpers()) {
      helper->DeleteCF(column_family_id, key, _currentSequence);
    }

    return rocksdb::Status();
  }

  rocksdb::Status SingleDeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
    LOG_TOPIC("aa997", TRACE, Logger::ENGINES)
        << "recovering SINGLE DELETE @ " << _currentSequence << " " << RocksDBKey(key);
    handleDeleteCF(column_family_id, key);

    RocksDBEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    for (auto helper : engine.recoveryHelpers()) {
      helper->SingleDeleteCF(column_family_id, key, _currentSequence);
    }

    return rocksdb::Status();
  }

  rocksdb::Status DeleteRangeCF(uint32_t column_family_id, const rocksdb::Slice& begin_key,
                                const rocksdb::Slice& end_key) override {
    LOG_TOPIC("ed6f5", TRACE, Logger::ENGINES)
        << "recovering DELETE RANGE @ " << _currentSequence << " from "
        << RocksDBKey(begin_key) << " to " << RocksDBKey(end_key);
    incTick();
    // drop and truncate can use this, truncate is handled via a Log marker
    RocksDBEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    for (auto helper : engine.recoveryHelpers()) {
      helper->DeleteRangeCF(column_family_id, begin_key, end_key, _currentSequence);
    }

    // check for a range-delete of the primary index
    if (column_family_id ==
        RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
            ->GetID()) {
      uint64_t objectId = RocksDBKey::objectId(begin_key);
      TRI_ASSERT(objectId == RocksDBKey::objectId(end_key));

      auto coll = findCollection(objectId);
      if (!coll) {
        return rocksdb::Status();
      }

      uint64_t currentCount = coll->meta().numberDocuments();
      if (currentCount != 0) {
        coll->meta().adjustNumberDocumentsInRecovery(_currentSequence, RevisionId::none(),
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
    RocksDBEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    for (auto helper : engine.recoveryHelpers()) {
      helper->LogData(blob, _currentSequence);
    }
  }

  // MergeCF is not used
};

/// parse the WAL with the above handler parser class
Result RocksDBRecoveryManager::parseRocksWAL() {
  Result shutdownRv;

  Result res = basics::catchToResult([&, &server = server()]() -> Result {
    Result rv;
    RocksDBEngine& engine =
        server.getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    for (auto& helper : engine.recoveryHelpers()) {
      helper->prepare();
    }

    // Tell the WriteBatch reader the transaction markers to look for
    WBReader handler(server, _tick);
    rocksdb::SequenceNumber earliest = engine.settingsManager()->earliestSeqNeeded();
    auto minTick = std::min(earliest, engine.releasedTick());

    LOG_TOPIC("fe333", DEBUG, Logger::ENGINES)
        << "recovery scanning wal starting from seq " << minTick;

    // prevent purging of WAL files while we are in here
    RocksDBFilePurgePreventer purgePreventer(engine.disallowPurging());

    std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
    rocksdb::Status s =
        _db->GetUpdatesSince(minTick, &iterator,
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
          std::string msg = basics::StringUtils::concatT("error during WAL scan: ", rv.errorMessage());
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
