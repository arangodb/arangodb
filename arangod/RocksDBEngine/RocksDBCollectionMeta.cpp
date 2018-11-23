////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBCollectionMeta.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

RocksDBCollectionMeta::DocCount::DocCount(VPackSlice const& slice)
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
    this->_revisionId = (*(++array)).getUInt();
  }
}

void RocksDBCollectionMeta::DocCount::toVelocyPack(VPackBuilder& b) const {
  b.openArray();
  b.add(VPackValue(_committedSeq));
  b.add(VPackValue(_added));
  b.add(VPackValue(_removed));
  b.add(VPackValue(_revisionId));
  b.close();
}

RocksDBCollectionMeta::RocksDBCollectionMeta()
  : _count(0,0,0,0) {}

/**
 * @brief Place a blocker to allow proper commit/serialize semantics
 *
 * Should be called immediately prior to internal RocksDB commit. If the
 * commit succeeds, any inserts/removals should be buffered, then the blocker
 * removed; otherwise simply remove the blocker.
 *
 * @param  trxId The identifier for the active transaction
 * @param  seq   The sequence number immediately prior to call
 * @return       May return error if we fail to allocate and place blocker
 */
Result RocksDBCollectionMeta::placeBlocker(uint64_t trxId, rocksdb::SequenceNumber seq) {
  return basics::catchToResult([&]() -> Result {
    Result res;
    WRITE_LOCKER(locker, _blockerLock);
    
    TRI_ASSERT(_blockers.end() == _blockers.find(trxId));
    TRI_ASSERT(_blockersBySeq.end() ==
               _blockersBySeq.find(std::make_pair(seq, trxId)));
    
    auto insert = _blockers.emplace(trxId, seq);
    auto crosslist = _blockersBySeq.emplace(seq, trxId);
    if (!insert.second || !crosslist.second) {
      return res.reset(TRI_ERROR_INTERNAL);
    }
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
void RocksDBCollectionMeta::removeBlocker(uint64_t trxId) {
  WRITE_LOCKER(locker, _blockerLock);
  auto it = _blockers.find(trxId);
  if (ADB_LIKELY(_blockers.end() != it)) {
    auto cross = _blockersBySeq.find(std::make_pair(it->second, it->first));
    TRI_ASSERT(_blockersBySeq.end() != cross);
    if (ADB_LIKELY(_blockersBySeq.end() != cross)) {
      _blockersBySeq.erase(cross);
    }
    _blockers.erase(it);
  }
}

/// @brief updates and returns the largest safe seq to squash updated against
rocksdb::SequenceNumber RocksDBCollectionMeta::committableSeq() const {
  READ_LOCKER(locker, _blockerLock);
  // if we have a blocker use the lowest counter
  if (!_blockersBySeq.empty()) {
    auto it = _blockersBySeq.begin();
    return it->first;
  }
  return std::numeric_limits<rocksdb::SequenceNumber>::max();
}

rocksdb::SequenceNumber RocksDBCollectionMeta::applyAdjustments(rocksdb::SequenceNumber commitSeq,
                                                                bool& didWork) {
  rocksdb::SequenceNumber appliedSeq = _count._committedSeq;
  
  decltype(_bufferedAdjs) swapper;
  {
    std::lock_guard<std::mutex> guard(_countLock);
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
  while (it != _stagedAdjs.end() && it->first < commitSeq) {
    appliedSeq = std::max(appliedSeq, it->first);
    if (it->second.adjustment > 0) {
      _count._added += it->second.adjustment;
    } else if (it->second.adjustment < 0) {
      _count._removed += -(it->second.adjustment);
    }
    if (it->second.revisionId != 0) {
      _count._revisionId = it->second.revisionId;
    }
    it = _stagedAdjs.erase(it);
    didWork = true;
  }
  _count._committedSeq = appliedSeq;
  return appliedSeq;
}

/// @brief get the current count
RocksDBCollectionMeta::DocCount RocksDBCollectionMeta::currentCount() {
  
  bool didWork = false;
  const rocksdb::SequenceNumber commitSeq = committableSeq();
  rocksdb::SequenceNumber seq = applyAdjustments(commitSeq, didWork);
  if (didWork) { // make sure serializeMeta has something to do
    std::lock_guard<std::mutex> guard(_countLock);
    _bufferedAdjs.emplace(seq, Adjustment{0, 0});
  }
  
  return _count;
}

/// @brief buffer a counter adjustment
void RocksDBCollectionMeta::adjustNumberDocuments(rocksdb::SequenceNumber seq,
                                                  TRI_voc_rid_t revId, int64_t adj) {
  TRI_ASSERT(seq != 0 && (adj || revId));
  std::lock_guard<std::mutex> guard(_countLock);
  _bufferedAdjs.emplace(seq, Adjustment{revId, adj});
}

/// @brief serialize the collection metadata
Result RocksDBCollectionMeta::serializeMeta(rocksdb::WriteBatch& batch, LogicalCollection& coll,
                                            bool force, VPackBuilder& tmp,
                                            rocksdb::SequenceNumber& appliedSeq) {
  Result res;
  
  bool didWork = false;
  const rocksdb::SequenceNumber maxCommitSeq = committableSeq();
  rocksdb::SequenceNumber seq = applyAdjustments(maxCommitSeq, didWork);
  if (didWork) {
    appliedSeq = std::min(appliedSeq, seq);
  } else { // maxCommitSeq is == UINT64_MAX without any blockers
    appliedSeq = std::min(appliedSeq, maxCommitSeq);
  }
  
  RocksDBKey key;
  rocksdb::ColumnFamilyHandle* const cf = RocksDBColumnFamily::definitions();
  RocksDBCollection* const rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());
  
  // Step 1. store the document count
  tmp.clear();
  if (didWork || force) {
    _count.toVelocyPack(tmp);
    key.constructCounterValue(rcoll->objectId());
    rocksdb::Slice value((char*)tmp.start(), tmp.size());
    rocksdb::Status s = batch.Put(cf, key.string(), value);
    if (!s.ok()) {
      LOG_TOPIC(WARN, Logger::ENGINES)
      << "writing counter for collection with objectId '" << rcoll->objectId()
      << "' failed: " << s.ToString();
      return res.reset(rocksutils::convertStatus(s));
    }
  }
  
  if (coll.deleted()) {
    return Result();
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
    LOG_TOPIC(TRACE, Logger::ENGINES) << "writing key generator coll " << coll.name();

    if (!s.ok()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << "writing key generator data failed";
      return res.reset(rocksutils::convertStatus(s));
    }
  }
  
  if (coll.deleted()) {
    return Result();
  }
  
  // Step 3. store the index estimates
  std::string output;
  auto indexes = coll.getIndexes();
  for (std::shared_ptr<arangodb::Index>& index : indexes) {
    RocksDBIndex* idx = static_cast<RocksDBIndex*>(index.get());
    RocksDBCuckooIndexEstimator<uint64_t>* est = idx->estimator();
    if (est == nullptr) { // does not have an estimator
      continue;
    }
    if (coll.deleted()) {
      return Result();
    }
    
    if (est->needToPersist() || force) {
      LOG_TOPIC(TRACE, Logger::ENGINES)
      << "beginning estimate serialization for index '" << idx->objectId() << "'";
      output.clear();

      seq = est->serialize(output, maxCommitSeq);
      // calculate retention sequence number
      appliedSeq = std::min(appliedSeq, seq);
      TRI_ASSERT(output.size() > sizeof(uint64_t));
      
      LOG_TOPIC(TRACE, Logger::ENGINES)
      << "serialized estimate for index '" << idx->objectId()
      << "' valid through seq " << seq;
      
      key.constructIndexEstimateValue(idx->objectId());
      rocksdb::Slice value(output);
      rocksdb::Status s = batch.Put(cf, key.string(), value);
      if (!s.ok()) {
        LOG_TOPIC(WARN, Logger::ENGINES) << "writing index estimates failed";
        return res.reset(rocksutils::convertStatus(s));
      }
    }
  }
  
  return res;
}

/// @brief deserialize collection metadata, only called on startup
Result RocksDBCollectionMeta::deserializeMeta(rocksdb::DB* db, LogicalCollection& coll) {
  RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());
  
  // Step 1. load the counter
  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::ReadOptions ro;
  ro.fill_cache = false;
  
  RocksDBKey key;
  key.constructCounterValue(rcoll->objectId());
  
  rocksdb::PinnableSlice value;
  rocksdb::Status s = db->Get(ro, cf, key.string(), &value);
  if (s.ok()) {
    VPackSlice countSlice = RocksDBValue::data(value);
    _count = RocksDBCollectionMeta::DocCount(countSlice);
  } else if (!s.IsNotFound()) {
    return rocksutils::convertStatus(s);
  }
  
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
  for (std::shared_ptr<arangodb::Index>& index : indexes) {
    RocksDBIndex* idx = static_cast<RocksDBIndex*>(index.get());
    if (idx->estimator() == nullptr) {
      continue;
    }
    
    key.constructIndexEstimateValue(idx->objectId());
    s = db->Get(ro, cf, key.string(), &value);
    if (!s.ok() && !s.IsNotFound()) {
      return rocksutils::convertStatus(s);
    } else if (s.IsNotFound()) { // expected with nosync recovery tests
      LOG_TOPIC(WARN, Logger::ROCKSDB) << "recalculating index estimate for index "
      << "type '" << idx->typeName() << "' with id '" << idx->id() << "'";
      idx->recalculateEstimates();
      continue;
    }
    
    StringRef estimateInput(value.data() + sizeof(uint64_t),
                            value.size() - sizeof(uint64_t));
    
    uint64_t committedSeq = rocksutils::uint64FromPersistent(value.data());
    if (RocksDBCuckooIndexEstimator<uint64_t>::isFormatSupported(estimateInput)) {
      TRI_ASSERT(committedSeq <= db->GetLatestSequenceNumber());
      
      auto est = std::make_unique<RocksDBCuckooIndexEstimator<uint64_t>>(committedSeq, estimateInput);
      LOG_TOPIC(DEBUG, Logger::ENGINES)
      << "found index estimator for objectId '" << idx->objectId()
      << "' committed seqNr '" << committedSeq << "' with estimate "
      << est->computeEstimate();
      
      idx->setEstimator(std::move(est));
    } else {
      LOG_TOPIC(ERR, Logger::ENGINES) << "unsupported index estimator format in index "
      << "with objectId '" << idx->objectId() << "'";
    }
  }
  
  return Result();
}

/// @brief load collection
/*static*/ RocksDBCollectionMeta::DocCount
    RocksDBCollectionMeta::loadCollectionCount(rocksdb::DB* db, uint64_t objectId) {
      
  auto cf = RocksDBColumnFamily::definitions();
  rocksdb::ReadOptions ro;
  ro.fill_cache = false;
  
  RocksDBKey key;
  key.constructCounterValue(objectId);
  
  rocksdb::PinnableSlice value;
  rocksdb::Status s = db->Get(ro, cf, key.string(), &value);
  if (s.ok()) {
    VPackSlice countSlice = RocksDBValue::data(value);
    return RocksDBCollectionMeta::DocCount(countSlice);
  }
  return DocCount(0,0,0,0);
}

/// @brief remove collection metadata
/*static*/ Result RocksDBCollectionMeta::deleteCollectionMeta(rocksdb::DB* db, uint64_t objectId) {
  

  rocksdb::ColumnFamilyHandle* const cf = RocksDBColumnFamily::definitions();
  rocksdb::WriteOptions wo;
  
  // Step 1. delete the document count
  RocksDBKey key;
  key.constructCounterValue(objectId);
  rocksdb::Status s = db->Delete(wo, cf, key.string());
  if (!s.ok()) {
    LOG_TOPIC(ERR, Logger::ENGINES) << "could not delete counter value: " << s.ToString();
    // try to remove the key generator value regardless
  }
  
  key.constructKeyGeneratorValue(objectId);
  s = db->Delete(wo, cf, key.string());
  if (!s.ok() && !s.IsNotFound()) {
    LOG_TOPIC(ERR, Logger::ENGINES) << "could not delete key generator value: " << s.ToString();
    return rocksutils::convertStatus(s);
  }
  
  return Result();
}

/// @brief remove collection index estimate
/*static*/ Result RocksDBCollectionMeta::deleteIndexEstimate(rocksdb::DB* db, uint64_t objectId) {
  
  rocksdb::ColumnFamilyHandle* const cf = RocksDBColumnFamily::definitions();
  rocksdb::WriteOptions wo;
  
  RocksDBKey key;
  key.constructIndexEstimateValue(objectId);
  rocksdb::Status s = db->Delete(wo, cf, key.string());
  if (!s.ok() && !s.IsNotFound()) {
    return rocksutils::convertStatus(s);
  }
  return Result();
}
