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
////////////////////////////////////////////////////////////////////////////////

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "RocksDBMetadata.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-compiler.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"

#include <chrono>
#include <thread>

using namespace arangodb;

RocksDBMetadata::DocCount::DocCount(VPackSlice const& slice)
    : _committedSeq(0), _added(0), _removed(0), _revisionId(0) {
  if (!slice.isArray()) {
    // got a somewhat invalid slice. probably old data from before the key
    // structure changes
    return;
  }

  VPackArrayIterator array(slice);
  if (array.valid()) {
    this->_committedSeq = (*array).getUInt();
    // versions pre 3.4 stored only a single "count" value
    // 3.4 and higher store "added" and "removed" seperately
    this->_added = (*(++array)).getUInt();
    if (array.size() > 3) {
      TRI_ASSERT(array.size() == 4);
      this->_removed = (*(++array)).getUInt();
    }
    this->_revisionId = RevisionId{(*(++array)).getUInt()};
  }
}

void RocksDBMetadata::DocCount::toVelocyPack(VPackBuilder& b) const {
  b.openArray();
  b.add(VPackValue(_committedSeq));
  b.add(VPackValue(_added));
  b.add(VPackValue(_removed));
  b.add(VPackValue(_revisionId.id()));
  b.close();
}

RocksDBMetadata::RocksDBMetadata()
    : _maxBlockersSequenceNumber(0),
      _count(0, 0, 0, RevisionId::none()),
      _numberDocuments(0),
      _revisionId(RevisionId::none()) {}

/**
 * @brief Place a blocker to allow proper commit/serialize semantics
 *
 * Should be called immediately prior to beginning an internal trx. If the
 * trx commit succeeds, any inserts/removals should be buffered, then the
 * blocker updated (intermediate) or removed (final); otherwise simply remove
 * the blocker.
 *
 * @param  trxId The identifier for the active transaction
 * @param  seq   The sequence number immediately prior to call
 */
rocksdb::SequenceNumber RocksDBMetadata::placeBlocker(TransactionId trxId, rocksdb::SequenceNumber seq) {
  {
    WRITE_LOCKER(locker, _blockerLock);

    seq = std::max(seq, _maxBlockersSequenceNumber);

    TRI_ASSERT(_blockers.end() == _blockers.find(trxId));
    TRI_ASSERT(_blockersBySeq.end() == _blockersBySeq.find(std::make_pair(seq, trxId)));

    auto insert = _blockers.try_emplace(trxId, seq);
    if (!insert.second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "duplicate sequence number in placeBlocker");
    }
    try {
      if (!_blockersBySeq.emplace(seq, trxId).second) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "duplicate sequence number for crosslist in placeBlocker");
      }
    } catch (...) {
      _blockers.erase(trxId);
      throw;
    }

    _maxBlockersSequenceNumber = seq;
  }

  LOG_TOPIC("1587a", TRACE, Logger::ENGINES)
      << "[" << this << "] placed blocker (" << trxId.id() << ", " << seq << ")";
  return seq;
}

/**
 * @brief Update a blocker to allow proper commit/serialize semantics
 *
 * Should be called after initializing an internal trx.
 *
 * @param  trxId The identifier for the active transaction (should match input
 *               to earlier `placeBlocker` call)
 * @param  seq   The sequence number from the internal snapshot
 * @return       May return error if we fail to allocate and place blocker
 */
Result RocksDBMetadata::updateBlocker(TransactionId trxId, rocksdb::SequenceNumber seq) {
  return basics::catchToResult([&]() -> Result {
    Result res;
    WRITE_LOCKER(locker, _blockerLock);

    auto previous = _blockers.find(trxId);
    if (_blockers.end() == previous ||
        _blockersBySeq.end() ==
            _blockersBySeq.find(std::make_pair(previous->second, trxId))) {
      res.reset(TRI_ERROR_INTERNAL);
    }

    auto removed = _blockersBySeq.erase(std::make_pair(previous->second, trxId));
    if (!removed) {
      return res.reset(TRI_ERROR_INTERNAL);
    }

    TRI_ASSERT(seq >= _blockers[trxId]);
    _blockers[trxId] = seq;
    auto crosslist = _blockersBySeq.emplace(seq, trxId);
    if (!crosslist.second) {
      return res.reset(TRI_ERROR_INTERNAL);
    }
    
    _maxBlockersSequenceNumber = std::max(seq, _maxBlockersSequenceNumber);

    LOG_TOPIC("1587c", TRACE, Logger::ENGINES)
        << "[" << this << "] updated blocker (" << trxId.id() << ", " << seq << ")";
    return res;
  });
}

/**
 * @brief Removes an existing transaction blocker
 *
 * Should be called after transaction abort/rollback, or after buffering any
 * updates in case of successful commit. If no blocker exists with the
 * specified transaction identifier, then this will simply do nothing.
 *
 * @param trxId Identifier for active transaction (should match input to
 *              earlier `placeBlocker` call)
 */
void RocksDBMetadata::removeBlocker(TransactionId trxId) noexcept try {
  WRITE_LOCKER(locker, _blockerLock);
  auto it = _blockers.find(trxId);

  if (ADB_LIKELY(_blockers.end() != it)) {
    auto cross = _blockersBySeq.find(std::make_pair(it->second, it->first));
    TRI_ASSERT(_blockersBySeq.end() != cross);
    if (ADB_LIKELY(_blockersBySeq.end() != cross)) {
      _blockersBySeq.erase(cross);
    }
    _blockers.erase(it);
    LOG_TOPIC("1587b", TRACE, Logger::ENGINES)
        << "[" << this << "] removed blocker (" << trxId.id() << ")";
  }
} catch (...) {}

/// @brief check if there is blocker with a seq number lower or equal to
/// the specified number
bool RocksDBMetadata::hasBlockerUpTo(rocksdb::SequenceNumber seq) const noexcept {
  READ_LOCKER(locker, _blockerLock);

  if (_blockersBySeq.empty()) {
    return false;
  }

  // _blockersBySeq is sorted by sequence number first, then transaction id
  // if the seq no in the first item is already less equal to our search
  // value, we can abort the search. all following items in _blockersBySeq
  // will only have the same or higher sequence numbers.
  return _blockersBySeq.begin()->first <= seq;
}

/// @brief returns the largest safe seq to squash updates against
rocksdb::SequenceNumber RocksDBMetadata::committableSeq(rocksdb::SequenceNumber maxCommitSeq) const {
  rocksdb::SequenceNumber committable = maxCommitSeq;

  {
    READ_LOCKER(locker, _blockerLock);
    // if we have a blocker use the lowest counter
    if (!_blockersBySeq.empty()) {
      auto it = _blockersBySeq.begin();
      committable = std::min(it->first, maxCommitSeq);
    }
  }
  LOG_TOPIC("1587d", TRACE, Logger::ENGINES)
      << "[" << this << "] committableSeq determined to be " << committable;
  return committable;
}

bool RocksDBMetadata::applyAdjustments(rocksdb::SequenceNumber commitSeq) {
  bool didWork = false;
  decltype(_bufferedAdjs) swapper;
  {
    std::lock_guard<std::mutex> guard(_bufferLock);
    if (_stagedAdjs.empty()) {
      _stagedAdjs.swap(_bufferedAdjs);
    } else {
      swapper.swap(_bufferedAdjs);
    }
  }
  if (!swapper.empty()) {
    _stagedAdjs.insert(swapper.begin(), swapper.end());
  }

  auto it = _stagedAdjs.begin();
  while (it != _stagedAdjs.end() && it->first <= commitSeq) {
    LOG_TOPIC("1487a", TRACE, Logger::ENGINES)
        << "[" << this << "] applying counter adjustment (" << it->first << ", "
        << it->second.adjustment << ", " << it->second.revisionId.id() << ")";
    if (it->second.adjustment > 0) {
      _count._added += it->second.adjustment;
    } else if (it->second.adjustment < 0) {
      _count._removed += -(it->second.adjustment);
    }
    TRI_ASSERT(_count._added >= _count._removed);
    if (it->second.revisionId.isSet()) {
      _count._revisionId = it->second.revisionId;
    }
    it = _stagedAdjs.erase(it);
    didWork = true;
  }
  
  std::lock_guard<std::mutex> guard(_bufferLock);
  _count._committedSeq = commitSeq;
  return didWork;
}

/// @brief buffer a counter adjustment
void RocksDBMetadata::adjustNumberDocuments(rocksdb::SequenceNumber seq,
                                            RevisionId revId, int64_t adj) {
  TRI_ASSERT(seq != 0 && (adj || revId.isSet()));
  
  std::lock_guard<std::mutex> guard(_bufferLock);
  TRI_ASSERT(seq > _count._committedSeq);
  _bufferedAdjs.try_emplace(seq, Adjustment{revId, adj});
  LOG_TOPIC("1587e", TRACE, Logger::ENGINES)
      << "[" << this << "] buffered adjustment (" << seq << ", " << adj << ", "
      << revId.id() << ")";
  // update immediately to ensure the user sees a correct value
  if (revId.isSet()) {
    _revisionId.store(revId);
  }
  if (adj < 0) {
    TRI_ASSERT(_numberDocuments >= static_cast<uint64_t>(-adj));
    _numberDocuments.fetch_sub(static_cast<uint64_t>(-adj));
  } else if (adj > 0) {
    _numberDocuments.fetch_add(static_cast<uint64_t>(adj));
  }
}

/// @brief buffer a counter adjustment ONLY in recovery, optimized to use less memory
void RocksDBMetadata::adjustNumberDocumentsInRecovery(rocksdb::SequenceNumber seq,
                                                      RevisionId revId, int64_t adj) {
  TRI_ASSERT(seq != 0 && (adj || revId.isSet()));
  if (seq <= _count._committedSeq) {
    // already incorporated into counter
    return;
  }
  bool updateRev = true;
  TRI_ASSERT(seq > _count._committedSeq);
  if (_bufferedAdjs.empty()) {
    _bufferedAdjs.try_emplace(seq, Adjustment{revId, adj});
  } else {
    // in recovery we should only maintain one adjustment which combines
    TRI_ASSERT(_bufferedAdjs.size() == 1);
    auto old = _bufferedAdjs.begin();
    TRI_ASSERT(old != _bufferedAdjs.end());
    if (old->first <= seq) {
      old->second.adjustment += adj;
      // just adjust counter, not rev
      updateRev = false;
    } else {
      _bufferedAdjs.try_emplace(seq, Adjustment{revId, adj + old->second.adjustment});
      _bufferedAdjs.erase(old);
    }
  }
  TRI_ASSERT(_bufferedAdjs.size() == 1);
  LOG_TOPIC("1587f", TRACE, Logger::ENGINES)
      << "[" << this << "] buffered adjustment (" << seq << ", " << adj << ", "
      << revId.id() << ") in recovery";

  // update immediately to ensure the user sees a correct value
  if (revId.isSet() && updateRev) {
    _revisionId.store(revId);
  }
  if (adj < 0) {
    TRI_ASSERT(_numberDocuments >= static_cast<uint64_t>(-adj));
    _numberDocuments.fetch_sub(static_cast<uint64_t>(-adj));
  } else if (adj > 0) {
    _numberDocuments.fetch_add(static_cast<uint64_t>(adj));
  }
}

/// @brief serialize the collection metadata
Result RocksDBMetadata::serializeMeta(rocksdb::WriteBatch& batch,
                                      LogicalCollection& coll, bool force,
                                      VPackBuilder& tmp, rocksdb::SequenceNumber& appliedSeq,
                                      std::string& output) {
  TRI_ASSERT(!coll.isAStub());
  TRI_ASSERT(appliedSeq != UINT64_MAX);
  TRI_ASSERT(appliedSeq > 0);
    
  TRI_ASSERT(batch.Count() == 0);
  
  Result res;
  if (coll.deleted()) {
    return res;
  }
  
  auto& engine = coll.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  std::string const context = coll.vocbase().name() + "/" + coll.name();

  rocksdb::SequenceNumber const maxCommitSeq = committableSeq(appliedSeq);
  TRI_ASSERT(maxCommitSeq <= appliedSeq);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // simulate another transaction coming along and trying to commit while
  // we are serializing
  RocksDBBlockerGuard blocker(&coll);
  
  TRI_IF_FAILURE("TransactionChaos::blockerOnSync") {
    blocker.placeBlocker();
  }
#endif

  TRI_ASSERT(maxCommitSeq <= appliedSeq);
  TRI_ASSERT(maxCommitSeq != UINT64_MAX);
  TRI_ASSERT(maxCommitSeq > 0);
  
  TRI_IF_FAILURE("TransactionChaos::randomSleep") {
    std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(5))));
  }
    
  bool didWork = applyAdjustments(maxCommitSeq);
  appliedSeq = maxCommitSeq;

  RocksDBKey key;
  rocksdb::ColumnFamilyHandle* const cf =
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions);
  
  RocksDBCollection* const rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());

  // Step 1. store the document count
  tmp.clear();
  if (didWork || force) {
    _count.toVelocyPack(tmp);
    key.constructCounterValue(rcoll->objectId());
    rocksdb::Slice value((char*)tmp.start(), tmp.size());
    rocksdb::Status s = batch.Put(cf, key.string(), value);
    if (!s.ok()) {
      LOG_TOPIC("1d7f3", WARN, Logger::ENGINES)
          << context << ": writing counter for collection with objectId '"
          << rcoll->objectId() << "' failed: " << s.ToString();
      return res.reset(rocksutils::convertStatus(s));
    } 
    LOG_TOPIC("1387a", TRACE, Logger::ENGINES)
        << context << ": wrote counter '" << tmp.toJson()
        << "' for collection with objectId '" << rcoll->objectId() << "'";
  } else {
    LOG_TOPIC("1e7f3", TRACE, Logger::ENGINES)
        << context << ": not writing counter for collection with "
        << "objectId '" << rcoll->objectId() << "', no updates applied";
  }

  // Step 2. store the key generator
  KeyGenerator* keyGen = coll.keyGenerator();
  if ((didWork || force) && keyGen->hasDynamicState()) {
    // only a key generator with dynamic data needs to be recovered
    key.constructKeyGeneratorValue(rcoll->objectId());

    tmp.clear();
    tmp.openObject();
    keyGen->toVelocyPack(tmp);
    tmp.close();

    RocksDBValue value = RocksDBValue::KeyGeneratorValue(tmp.slice());
    rocksdb::Status s = batch.Put(cf, key.string(), value.string());
    LOG_TOPIC("17610", TRACE, Logger::ENGINES)
        << context << ": writing key generator coll " << coll.name();

    if (!s.ok()) {
      LOG_TOPIC("333fe", WARN, Logger::ENGINES)
          << context << ": writing key generator data failed";
      return res.reset(rocksutils::convertStatus(s));
    }
  }

  // Step 3. store the index estimates
  auto indexes = coll.getIndexes();
  for (std::shared_ptr<arangodb::Index>& index : indexes) {
    RocksDBIndex* idx = static_cast<RocksDBIndex*>(index.get());
    RocksDBCuckooIndexEstimatorType* est = idx->estimator();
    if (est == nullptr) {  // does not have an estimator
      LOG_TOPIC("ab329", TRACE, Logger::ENGINES)
          << context << ": index '" << idx->objectId()
          << "' does not have an estimator";
      continue;
    }

    if (est->needToPersist() || force) {
      LOG_TOPIC("82a07", TRACE, Logger::ENGINES)
          << context << ": beginning estimate serialization for index '"
          << idx->objectId() << "'";
      output.clear();

      auto format = RocksDBCuckooIndexEstimatorType::SerializeFormat::COMPRESSED;
      est->serialize(output, maxCommitSeq, format);
      TRI_ASSERT(output.size() > sizeof(uint64_t));

      LOG_TOPIC("6b761", TRACE, Logger::ENGINES)
          << context << ": serialized estimate for index '"
          << idx->objectId() << "' with estimate " << est->computeEstimate()
          << " valid through seq " << appliedSeq;

      key.constructIndexEstimateValue(idx->objectId());
      rocksdb::Slice value(output);
      rocksdb::Status s = batch.Put(cf, key.string(), value);
      if (!s.ok()) {
        LOG_TOPIC("ff233", WARN, Logger::ENGINES)
            << context << ": writing index estimates failed";
        return res.reset(rocksutils::convertStatus(s));
      }
    } else {
      LOG_TOPIC("ab328", TRACE, Logger::ENGINES)
          << context << ": index '" << idx->objectId()
          << "' estimator does not need to be persisted";
    }
  }

  if (!coll.useSyncByRevision()) {
    return Result{};
  }

  // Step 4. Take care of revision tree, either serialize or persist
  // it, or at least check if we can move forward the seq number when
  // it was last serialized (in case there have been no writes to the
  // collection for some time). In either case, the resulting sequence
  // number is incorporated into the minimum calculation for lastSync
  // (via `appliedSeq`), such that recovery only has to look at the WAL
  // from this sequence number on to be able to recover the tree from
  // its last persisted state.
  return rcoll->takeCareOfRevisionTreePersistence(
            coll, engine, batch, cf, maxCommitSeq, force, context,
            output, appliedSeq);
}

/// @brief deserialize collection metadata, only called on startup
Result RocksDBMetadata::deserializeMeta(rocksdb::DB* db, LogicalCollection& coll) {
  TRI_ASSERT(!coll.isAStub());
  std::string const context = coll.vocbase().name() + "/" + coll.name();

  RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());

  auto& engine = coll.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  rocksdb::SequenceNumber globalSeq = engine.settingsManager()->earliestSeqNeeded();

  // Step 1. load the counter
  auto cf = RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions);
  rocksdb::ReadOptions ro;
  ro.fill_cache = false;

  RocksDBKey key;
  key.constructCounterValue(rcoll->objectId());

  rocksdb::PinnableSlice value;
  rocksdb::Status s = db->Get(ro, cf, key.string(), &value);
  if (s.ok()) {
    VPackSlice countSlice = RocksDBValue::data(value);
    _count = RocksDBMetadata::DocCount(countSlice);
    LOG_TOPIC("1387b", TRACE, Logger::ENGINES)
        << context << ": recovered counter '" << countSlice.toJson()
        << "' for collection with objectId '" << rcoll->objectId() << "'";
  } else if (!s.IsNotFound()) {
    LOG_TOPIC("1397c", TRACE, Logger::ENGINES)
        << context << ": error while recovering counter for collection with objectId '"
        << rcoll->objectId() << "': " << rocksutils::convertStatus(s).errorMessage();
    return rocksutils::convertStatus(s);
  } else {
    LOG_TOPIC("1387c", TRACE, Logger::ENGINES)
        << context << ": no counter found for collection with objectId '"
        << rcoll->objectId() << "'";
  }
 
  // setting the cached version of the counts
  loadInitialNumberDocuments();

  // Step 2. load the key generator
  KeyGenerator* keyGen = coll.keyGenerator();
  if (keyGen->hasDynamicState()) {
    // only a key generator with dynamic data needs to be recovered
    key.constructKeyGeneratorValue(rcoll->objectId());
    s = db->Get(ro, cf, key.string(), &value);
    if (s.ok()) {
      VPackSlice keyGenProps = RocksDBValue::data(value);
      TRI_ASSERT(keyGenProps.isObject());
      // simon: wtf who decided this is a good deserialization routine ?!
      VPackSlice val = keyGenProps.get(StaticStrings::LastValue);
      if (val.isString()) {
        VPackValueLength size;
        const char* data = val.getString(size);
        keyGen->track(data, size);
      } else if (val.isInteger()) {
        uint64_t lastValue = val.getUInt();
        std::string str = std::to_string(lastValue);
        keyGen->track(str.data(), str.size());
      }

    } else if (!s.IsNotFound()) {
      return rocksutils::convertStatus(s);
    }
  }

  // Step 3. load the index estimates
  auto indexes = coll.getIndexes();
  for (std::shared_ptr<arangodb::Index> const& index : indexes) {
    RocksDBIndex* idx = static_cast<RocksDBIndex*>(index.get());
    if (idx->estimator() == nullptr) {
      continue;
    }

    key.constructIndexEstimateValue(idx->objectId());
    s = db->Get(ro, cf, key.string(), &value);
    if (!s.ok() && !s.IsNotFound()) {
      return rocksutils::convertStatus(s);
    } else if (s.IsNotFound()) {  // expected with nosync recovery tests
      LOG_TOPIC("ecdbb", INFO, Logger::ENGINES)
          << context << ": no index estimate found for index of "
          << "type '" << idx->typeName() << "', id '" << idx->id().id() << "', recalculating...";
      idx->recalculateEstimates();
      continue;
    }

    std::string_view estimateInput(value.data(), value.size());
    if (RocksDBCuckooIndexEstimatorType::isFormatSupported(estimateInput)) {
      TRI_ASSERT(rocksutils::uint64FromPersistent(value.data()) <= db->GetLatestSequenceNumber());

      auto est = std::make_unique<RocksDBCuckooIndexEstimatorType>(estimateInput);
      LOG_TOPIC("63f3b", DEBUG, Logger::ENGINES)
          << context << ": found index estimator for objectId '"
          << idx->objectId() << "' committed seqNr '" << est->appliedSeq()
          << "' with estimate " << est->computeEstimate();

      idx->setEstimator(std::move(est));
    } else {
      LOG_TOPIC("dcd98", ERR, Logger::ENGINES)
          << context << ": unsupported index estimator format in index "
          << "with objectId '" << idx->objectId() << "'";
    }
  }

  // Step 4. load the revision tree
  if (!coll.useSyncByRevision()) {
    LOG_TOPIC("92ca9", TRACE, Logger::ENGINES)
        << context << ": no need to recover revision tree for "
        << "collection with objectId '" << rcoll->objectId() << "', "
        << "it is not configured to sync by revision";
    // nothing to do
    return {};
  }

  // look for a persisted Merkle tree in RocksDB
  key.constructRevisionTreeValue(rcoll->objectId());
  s = db->Get(ro, cf, key.string(), &value);
  if (!s.ok() && !s.IsNotFound()) {
    LOG_TOPIC("92caa", TRACE, Logger::ENGINES)
        << context << ": error while recovering revision tree for "
        << "collection with objectId '" << rcoll->objectId()
        << "': " << rocksutils::convertStatus(s).errorMessage();
    return rocksutils::convertStatus(s);
  }

  bool const treeFound = !s.IsNotFound();

  if (treeFound) {
    // we do have a persisted tree.
    TRI_ASSERT(value.size() > sizeof(uint64_t));

    try {
      auto tree = containers::RevisionTree::fromBuffer(
          std::string_view(value.data(), value.size() - sizeof(uint64_t)));

      if (tree) {
        // may throw
        tree->checkConsistency();

        uint64_t seq = rocksutils::uint64FromPersistent(
            value.data() + value.size() - sizeof(uint64_t));
        // we may have skipped writing out the tree because it hadn't changed,
        // but we had already applied everything through the global released
        // seq anyway, so take the max
  
        rocksdb::SequenceNumber useSeq = std::max(globalSeq, seq);
        rcoll->setRevisionTree(std::move(tree), useSeq);

        LOG_TOPIC("92cab", TRACE, Logger::ENGINES)
            << context << ": recovered revision tree for "
            << "collection with objectId '" << rcoll->objectId() << "', "
            << "valid through " << useSeq << ", seq: " << seq << ", globalSeq: " << globalSeq;

        return {};
      }
      
      LOG_TOPIC("dcd99", WARN, Logger::ENGINES)
          << context << ": unsupported revision tree format";

      // we intentionally fall through to the tree rebuild process
    } catch (std::exception const& ex) {
      // error during tree processing.
      // the tree is either invalid or some other exception happened
      LOG_TOPIC("84247", ERR, Logger::ENGINES)
          << context << ": caught exception while loading revision tree in collection "
          << coll.name() << ": " << ex.what();
    }
  }

  // no tree, or we read an invalid tree from persistence

  // no tree, check if collection is non-empty
  auto bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
  auto cmp = RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
                 ->GetComparator();
  std::unique_ptr<rocksdb::Iterator> it{
      db->NewIterator(ro, RocksDBColumnFamilyManager::get(
                              RocksDBColumnFamilyManager::Family::Documents))};
  it->Seek(bounds.start());
  if (it->Valid() && cmp->Compare(it->key(), bounds.end()) < 0) {
    if (treeFound) {
      LOG_TOPIC("ecdbc", WARN, Logger::ENGINES)
          << context << ": invalid revision tree found for collection, "
          << "rebuilding from collection data...";
    } else {
      LOG_TOPIC("ecdba", INFO, Logger::ENGINES)
          << context << ": no revision tree found for collection, "
          << "rebuilding from collection data...";
    }
  } else {
    LOG_TOPIC("ecdbe", DEBUG, Logger::ENGINES)
        << context << ": no revision tree found for collection, "
        << ", but collection appears empty";
  }
  rcoll->rebuildRevisionTree(it);
    
  auto [countInTree, treeSeq] = rcoll->revisionTreeInfo();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    std::lock_guard<std::mutex> guard(_bufferLock);
    TRI_ASSERT(_bufferedAdjs.empty());
  }
#endif
  uint64_t stored = _numberDocuments.load();
  if (stored != countInTree && treeSeq != 0) {
    // patch the document count to the correct value
    adjustNumberDocumentsInRecovery(treeSeq, RevisionId{0}, static_cast<int64_t>(countInTree) - static_cast<int64_t>(stored));
    // also patch the counter's sequence number, so that
    // any changes encountered by the recovery do not modify
    // the counter once more
    _count._committedSeq = treeSeq;

    TRI_ASSERT(_numberDocuments.load() == countInTree);
    
    LOG_TOPIC("f3f38", INFO, Logger::ENGINES)
          << context << ": rebuilt revision tree for collection with objectId '"
          << rcoll->objectId() << "', seqNr " << treeSeq << ", count: " << countInTree;
  }

  return {};
}

void RocksDBMetadata::loadInitialNumberDocuments() {
  TRI_ASSERT(_count._added >= _count._removed);
  _numberDocuments.store(_count._added - _count._removed);
  _revisionId.store(_count._revisionId);
}

// static helper methods to modify collection meta entries in rocksdb

/// @brief load collection
/*static*/ RocksDBMetadata::DocCount RocksDBMetadata::loadCollectionCount(
    rocksdb::DB* db, uint64_t objectId) {
  auto cf = RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions);
  rocksdb::ReadOptions ro;
  ro.fill_cache = false;

  RocksDBKey key;
  key.constructCounterValue(objectId);

  rocksdb::PinnableSlice value;
  rocksdb::Status s = db->Get(ro, cf, key.string(), &value);
  if (s.ok()) {
    VPackSlice countSlice = RocksDBValue::data(value);
    LOG_TOPIC("1387e", TRACE, Logger::ENGINES)
        << "loaded counter '" << countSlice.toJson()
        << "' for collection with objectId '" << objectId << "'";
    return RocksDBMetadata::DocCount(countSlice);
  }
  LOG_TOPIC("1387f", TRACE, Logger::ENGINES)
      << "loaded default zero counter for collection with objectId '" << objectId << "'";
  return DocCount(0, 0, 0, RevisionId::none());
}

/// @brief remove collection metadata
/*static*/ Result RocksDBMetadata::deleteCollectionMeta(rocksdb::DB* db,
                                                        uint64_t objectId) {
  rocksdb::ColumnFamilyHandle* const cf =
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions);
  rocksdb::WriteOptions wo;

  // Step 1. delete the document count
  RocksDBKey key;
  key.constructCounterValue(objectId);
  rocksdb::Status s = db->Delete(wo, cf, key.string());
  if (!s.ok()) {
    LOG_TOPIC("93718", ERR, Logger::ENGINES)
        << "could not delete counter value for collection with objectId '"
        << objectId << "': " << s.ToString();
    // try to remove the key generator value regardless
  } else {
    LOG_TOPIC("93719", TRACE, Logger::ENGINES)
        << "deleted counter for collection with objectId '" << objectId << "'";
  }

  key.constructKeyGeneratorValue(objectId);
  s = db->Delete(wo, cf, key.string());
  if (!s.ok() && !s.IsNotFound()) {
    LOG_TOPIC("af3dc", ERR, Logger::ENGINES)
        << "could not delete key generator value: " << s.ToString();
    return rocksutils::convertStatus(s);
  }

  key.constructRevisionTreeValue(objectId);
  s = db->Delete(wo, cf, key.string());
  if (!s.ok() && !s.IsNotFound()) {
    LOG_TOPIC("af3dd", ERR, Logger::ENGINES)
        << "could not delete revision tree value: " << s.ToString();
    return rocksutils::convertStatus(s);
  }

  return Result();
}

/// @brief remove collection index estimate
/*static*/ Result RocksDBMetadata::deleteIndexEstimate(rocksdb::DB* db, uint64_t objectId) {
  rocksdb::ColumnFamilyHandle* const cf =
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Definitions);
  rocksdb::WriteOptions wo;

  RocksDBKey key;
  key.constructIndexEstimateValue(objectId);
  rocksdb::Status s = db->Delete(wo, cf, key.string());
  if (!s.ok() && !s.IsNotFound()) {
    return rocksutils::convertStatus(s);
  }
  return Result();
}
  
RocksDBBlockerGuard::RocksDBBlockerGuard(LogicalCollection* collection)
    : _collection(collection),
      _trxId(TransactionId::none()) {
  TRI_ASSERT(_collection != nullptr);
}

RocksDBBlockerGuard::~RocksDBBlockerGuard() {
  releaseBlocker();
}

RocksDBBlockerGuard::RocksDBBlockerGuard(RocksDBBlockerGuard&& other) noexcept
    : _collection(other._collection),
      _trxId(other._trxId) {
  other._trxId = TransactionId::none();
}

RocksDBBlockerGuard& RocksDBBlockerGuard::operator=(RocksDBBlockerGuard&& other) noexcept {
  releaseBlocker();

  _collection = other._collection;
  _trxId = other._trxId;
  other._trxId = TransactionId::none();
  return *this;
}

rocksdb::SequenceNumber RocksDBBlockerGuard::placeBlocker() {
  TransactionId trxId = TransactionId(transaction::Context::makeTransactionId());
  // generated trxId must be > 0
  TRI_ASSERT(trxId.isSet());
  return placeBlocker(trxId);
}

rocksdb::SequenceNumber RocksDBBlockerGuard::placeBlocker(TransactionId trxId) {
  // note: input trxId can be 0 during unit tests, so we cannot assert trxId.isSet() here!
  
  TRI_ASSERT(!_trxId.isSet());
  
  auto* rcoll = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
  rocksdb::SequenceNumber blockerSeq = rcoll->placeRevisionTreeBlocker(trxId);

  // only set _trxId if placing the blocker succeeded
  _trxId = trxId;
  return blockerSeq;
}

void RocksDBBlockerGuard::releaseBlocker() noexcept {
  if (_trxId.isSet()) {
    auto* rcoll = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
    rcoll->meta().removeBlocker(_trxId);
    _trxId = TransactionId::none();
  }
}
