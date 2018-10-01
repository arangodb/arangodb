////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/NumberUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/exitcodes.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEdgeIndex.h"
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
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::application_features;

namespace arangodb {

RocksDBRecoveryManager* RocksDBRecoveryManager::instance() {
  return ApplicationServer::getFeature<RocksDBRecoveryManager>(featureName());
}

/// Constructor needs to be called synchrunously,
/// will load counts from the db and scan the WAL
RocksDBRecoveryManager::RocksDBRecoveryManager(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, featureName()),
      _db(nullptr),
      _inRecovery(true) {
  setOptional(true);
  startsAfter("BasicsPhase");

  startsAfter("Database");
  startsAfter("SystemDatabase");
  startsAfter("RocksDBEngine");
  startsAfter("ServerId");
  startsAfter("StorageEngine");

  onlyEnabledWith("RocksDBEngine");
}

void RocksDBRecoveryManager::start() {
  if (!isEnabled()) {
    return;
  }

  _db = ApplicationServer::getFeature<RocksDBEngine>("RocksDBEngine")->db();
  runRecovery();
  _inRecovery.store(false);

  // notify everyone that recovery is now done
  auto databaseFeature =
      ApplicationServer::getFeature<DatabaseFeature>("Database");
  databaseFeature->recoveryDone();
}

/// parse recent RocksDB WAL entries and notify the
/// DatabaseFeature about the successful recovery
void RocksDBRecoveryManager::runRecovery() {
  auto res = parseRocksWAL();
  if (res.fail()) {
    LOG_TOPIC(FATAL, Logger::ENGINES)
      << "failed during rocksdb WAL recovery: "
      << res.errorMessage();
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_RECOVERY);
  }
}

bool RocksDBRecoveryManager::inRecovery() const { return _inRecovery.load(); }

class WBReader final : public rocksdb::WriteBatch::Handler {
 public:
  
  struct Operations {
    Operations(rocksdb::SequenceNumber seq) : startSequenceNumber(seq) {}
    Operations(Operations const&) = delete;
    Operations& operator=(Operations const&) = delete;
    Operations(Operations&&) = default;
    Operations& operator=(Operations&&) = default;

    rocksdb::SequenceNumber startSequenceNumber;
    rocksdb::SequenceNumber lastSequenceNumber = 0;
    uint64_t added = 0;
    uint64_t removed = 0;
    TRI_voc_rid_t lastRevisionId = 0;
    bool mustTruncate = false;
  };
  
  std::unordered_map<uint64_t, WBReader::Operations> deltas;
  rocksdb::SequenceNumber currentSeqNum;

 private:
  // used to track used IDs for key-generators
  std::unordered_map<uint64_t, uint64_t> _generators;

  uint64_t _maxTick = 0;
  uint64_t _maxHLC = 0;
  /// @brief last document removed
  TRI_voc_rid_t _lastRemovedDocRid = 0;

 public:
  
  /// @param seqs sequence number from which to count operations
  explicit WBReader(std::unordered_map<uint64_t, rocksdb::SequenceNumber> const& seqs)
      : currentSeqNum(0) {
        for (auto const& pair : seqs) {
          try {
            deltas.emplace(pair.first, Operations(pair.second));
          } catch(...) {}
        }
      }

  Result shutdownWBReader() {
    Result rv = basics::catchVoidToResult([&]() -> void {
      // update ticks after parsing wal
      LOG_TOPIC(TRACE, Logger::ENGINES) << "max tick found in WAL: " << _maxTick
                                        << ", last HLC value: " << _maxHLC;

      TRI_UpdateTickServer(_maxTick);
      TRI_HybridLogicalClock(_maxHLC);

      // TODO update generators
      auto dbfeature = ApplicationServer::getFeature<DatabaseFeature>("Database");
      for (auto gen : _generators) {
        if (gen.second > 0) {
          auto dbColPair = rocksutils::mapObjectToCollection(gen.first);
          if (dbColPair.second == 0 && dbColPair.first == 0) {
            // collection with this objectID not known.Skip.
            continue;
          }
          auto vocbase = dbfeature->useDatabase(dbColPair.first);
          if (vocbase == nullptr) {
            continue;
          }
          TRI_DEFER(vocbase->release());

          auto collection = vocbase->lookupCollection(dbColPair.second);
          if (collection == nullptr) {
            continue;
          }
          std::string k(basics::StringUtils::itoa(gen.second));
          collection->keyGenerator()->track(k.data(), k.size());
        }
      }
    });
    return rv;
  }

  bool shouldHandleCollection(uint64_t objectId, Operations** ops) {
    auto it = deltas.find(objectId);
    if (it != deltas.end()) {
      *ops = &(it->second);
      return it->second.startSequenceNumber <= currentSeqNum;
    }
    auto res = deltas.emplace(objectId, currentSeqNum); // do not ignore unknown counters
    *ops = &(res.first->second);
    return true;
  }

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

  void storeLastKeyValue(uint64_t objectId, uint64_t keyValue) {
    if (keyValue == 0) {
      return;
    }

    auto it = _generators.find(objectId);
    if (it == _generators.end()) {
      try {
        _generators.emplace(objectId, keyValue);
      } catch (...) {
      }
      return;
    }

    if (keyValue > (*it).second) {
      (*it).second = keyValue;
    }
  }

  RocksDBCuckooIndexEstimator<uint64_t>* findEstimator(uint64_t objectId) {
    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    RocksDBEngine::IndexTriple triple = engine->mapObjectToIndex(objectId);
    if (std::get<0>(triple) == 0 && std::get<1>(triple) == 0) {
      return nullptr;
    }

    DatabaseFeature* df = DatabaseFeature::DATABASE;
    TRI_vocbase_t* vb = df->useDatabase(std::get<0>(triple));
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
    return static_cast<RocksDBIndex*>(index.get())->estimator();
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

    if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
      storeMaxHLC(RocksDBKey::documentId(key).id());
      storeLastKeyValue(RocksDBKey::objectId(key), RocksDBValue::keyValue(value));
    } else if (column_family_id == RocksDBColumnFamily::primary()->GetID()) {
      // document key
      StringRef ref = RocksDBKey::primaryKey(key);
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
          if (tick > _maxTick && (_maxTick == 0 || tick - _maxTick < 2048)) {
            storeMaxTick(tick);
          }
        }
        // else we got a non-numeric key. simply ignore it
      }
    } else if (column_family_id ==
               RocksDBColumnFamily::definitions()->GetID()) {
      auto const type = RocksDBKey::type(key);

      if (type == RocksDBEntryType::Collection) {
        storeMaxTick(RocksDBKey::collectionId(key));
        auto slice = RocksDBValue::data(value);
        storeMaxTick(basics::VelocyPackHelper::stringUInt64(slice, "objectId"));
        VPackSlice indexes = slice.get("indexes");
        for (VPackSlice const& idx : VPackArrayIterator(indexes)) {
          storeMaxTick(
              std::max(basics::VelocyPackHelper::stringUInt64(idx, "objectId"),
                       basics::VelocyPackHelper::stringUInt64(idx, "id")));
        }
      } else if (type == RocksDBEntryType::Database) {
        storeMaxTick(RocksDBKey::databaseId(key));
      } else if (type == RocksDBEntryType::View) {
        storeMaxTick(
            std::max(RocksDBKey::databaseId(key), RocksDBKey::viewId(key)));
      }
    }
  }

  rocksdb::Status PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                        const rocksdb::Slice& value) override {
    updateMaxTick(column_family_id, key, value);
    if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
      uint64_t objectId = RocksDBKey::objectId(key);
      Operations* ops = nullptr;
      if (shouldHandleCollection(objectId, &ops)) {
        TRI_ASSERT(ops != nullptr);
        ops->lastSequenceNumber = currentSeqNum;
        ops->added++;
        ops->lastRevisionId = transaction::helpers::extractRevFromDocument(RocksDBValue::data(value));
      }
    } else {
      // We have to adjust the estimate with an insert
      uint64_t hash = 0;
      if (column_family_id == RocksDBColumnFamily::vpack()->GetID()) {
        hash = RocksDBVPackIndex::HashForKey(key);
      } else if (column_family_id == RocksDBColumnFamily::edge()->GetID()) {
        hash = RocksDBEdgeIndex::HashForKey(key);
      }

      if (hash != 0) {
        uint64_t objectId = RocksDBKey::objectId(key);
        auto est = findEstimator(objectId);
        if (est != nullptr && est->commitSeq() < currentSeqNum) {
          // We track estimates for this index
          est->insert(hash);
        }
      }
    }

    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    for (auto helper : engine->recoveryHelpers()) {
      helper->PutCF(column_family_id, key, value);
    }

    return rocksdb::Status();
  }
  
  void handleDeleteCF(uint32_t cfId,
                      const rocksdb::Slice& key) {
    if (cfId == RocksDBColumnFamily::documents()->GetID()) {
      uint64_t objectId = RocksDBKey::objectId(key);
      Operations* ops = nullptr;
      if (shouldHandleCollection(objectId, &ops)) {
        TRI_ASSERT(ops != nullptr);
        ops->lastSequenceNumber = currentSeqNum;
        ops->removed++;
        if (_lastRemovedDocRid != 0) {
          ops->lastRevisionId = _lastRemovedDocRid;
        }
      }
      _lastRemovedDocRid = 0; // reset in any case
    } else {
      // We have to adjust the estimate with an insert
      uint64_t hash = 0;
      if (cfId == RocksDBColumnFamily::vpack()->GetID()) {
        hash = RocksDBVPackIndex::HashForKey(key);
      } else if (cfId == RocksDBColumnFamily::edge()->GetID()) {
        hash = RocksDBEdgeIndex::HashForKey(key);
      }
      
      if (hash != 0) {
        uint64_t objectId = RocksDBKey::objectId(key);
        auto est = findEstimator(objectId);
        if (est != nullptr && est->commitSeq() < currentSeqNum) {
          // We track estimates for this index
          est->remove(hash);
        }
      }
    }
  }

  rocksdb::Status DeleteCF(uint32_t column_family_id,
                           const rocksdb::Slice& key) override {
    handleDeleteCF(column_family_id, key);

    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    for (auto helper : engine->recoveryHelpers()) {
      helper->DeleteCF(column_family_id, key);
    }

    return rocksdb::Status();
  }

  rocksdb::Status SingleDeleteCF(uint32_t column_family_id,
                                 const rocksdb::Slice& key) override {
    handleDeleteCF(column_family_id, key);

    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    for (auto helper : engine->recoveryHelpers()) {
      helper->SingleDeleteCF(column_family_id, key);
    }

    return rocksdb::Status();
  }
  
  rocksdb::Status DeleteRangeCF(uint32_t column_family_id,
                                const rocksdb::Slice& begin_key,
                                const rocksdb::Slice& end_key) override {
    // drop and truncate can use this, truncate is handled via a Log marker
    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    for (auto helper : engine->recoveryHelpers()) {
      helper->DeleteRangeCF(column_family_id, begin_key, end_key);
    }
    
    return rocksdb::Status(); // make WAL iterator happy
  }

  void LogData(const rocksdb::Slice& blob) override {
    // a delete log message appears directly before a Delete
    RocksDBLogType type = RocksDBLogValue::type(blob);
    switch(type) {
      case RocksDBLogType::DocumentRemoveV2: // remove within a trx
        TRI_ASSERT(_lastRemovedDocRid == 0);
        _lastRemovedDocRid = RocksDBLogValue::revisionId(blob);
        break;
      case RocksDBLogType::SingleRemoveV2: // single remove
        TRI_ASSERT(_lastRemovedDocRid == 0);
        _lastRemovedDocRid = RocksDBLogValue::revisionId(blob);
        break;
      case RocksDBLogType::CollectionTruncate: {
        uint64_t objectId = RocksDBLogValue::objectId(blob);
        Operations* ops = nullptr;
        if (shouldHandleCollection(objectId, &ops)) {
          TRI_ASSERT(ops != nullptr);
          ops->lastSequenceNumber = currentSeqNum;
          ops->removed = 0;
          ops->added = 0;
          ops->mustTruncate = true;
          auto est = findEstimator(objectId);
          if (est != nullptr && est->commitSeq() < currentSeqNum) {
            // We track estimates for this index
            est->bufferTruncate(currentSeqNum + 1);
          }
        }
        
        _lastRemovedDocRid = 0; // reset in any other case
        break;
      }
      default:
        _lastRemovedDocRid = 0; // reset in any other case
        break;
    }
    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    for (auto helper : engine->recoveryHelpers()) {
      helper->LogData(blob);
    }
  }
};

/// parse the WAL with the above handler parser class
Result RocksDBRecoveryManager::parseRocksWAL() {
  Result shutdownRv;

  Result res = basics::catchToResult([&]() -> Result {
    Result rv;
    RocksDBEngine* engine =
        static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
    for (auto& helper : engine->recoveryHelpers()) {
      helper->prepare();
    }

    // Tell the WriteBatch reader the transaction markers to look for
    WBReader handler(engine->settingsManager()->counterSeqs());

    auto minTick = std::min(engine->settingsManager()->earliestSeqNeeded(),
                            engine->releasedTick());
    std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
    rocksdb::Status s = _db->GetUpdatesSince(
        minTick, &iterator, rocksdb::TransactionLogIterator::ReadOptions(true));

    rv = rocksutils::convertStatus(s);

    if (rv.ok()) {
      while (iterator->Valid()) {
        s = iterator->status();
        if (s.ok()) {
          rocksdb::BatchResult batch = iterator->GetBatch();
          handler.currentSeqNum = batch.sequence;
          s = batch.writeBatchPtr->Iterate(&handler);
        }


        if (!s.ok()) {
          rv = rocksutils::convertStatus(s);
          std::string msg = "error during WAL scan: " + rv.errorMessage();
          LOG_TOPIC(ERR, Logger::ENGINES) << msg;
          rv.reset(rv.errorNumber(), std::move(msg)); // update message
          break;
        }

        iterator->Next();
      }

      if (rv.ok()) {
        LOG_TOPIC(TRACE, Logger::ENGINES)
            << "finished WAL scan with " << handler.deltas.size();
        
        RocksDBSettingsManager* mgr = engine->settingsManager();
        for (auto& pair : handler.deltas) {
          WBReader::Operations const& ops = pair.second;
          if (ops.mustTruncate) {
            mgr->setAbsoluteCounter(pair.first, ops.lastSequenceNumber, 0);
          }
          RocksDBSettingsManager::CounterAdjustment adj{};
          adj._sequenceNum = ops.lastSequenceNumber;
          adj._added = ops.added;
          adj._removed = ops.removed;
          adj._revisionId = ops.lastRevisionId;
          
          mgr->updateCounter(pair.first, adj);
          LOG_TOPIC(TRACE, Logger::ENGINES)
              << "WAL recovered " << adj.added() << " PUTs and "
              << adj.removed() << " DELETEs for objectID " << pair.first;
        }
      }
    }

    shutdownRv = handler.shutdownWBReader();

    return rv;
  });

  if (res.ok()) {
    res = std::move(shutdownRv);
  } else {
    if (shutdownRv.fail()){
      res.reset(res.errorNumber(), res.errorMessage() + " - " + shutdownRv.errorMessage());
    }
  }

  return res;
}

} // arangodb
