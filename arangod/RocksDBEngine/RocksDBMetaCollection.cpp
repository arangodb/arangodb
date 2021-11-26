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

#include "RocksDBMetaCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>

using namespace arangodb;

namespace {

rocksdb::SequenceNumber forceWrite(RocksDBEngine& engine) {
  auto* sm = engine.settingsManager();
  if (sm) {
    sm->sync(true);  // force
  }
  return engine.db()->GetLatestSequenceNumber();
}

}  // namespace

RocksDBMetaCollection::RocksDBMetaCollection(LogicalCollection& collection,
                                             VPackSlice const& info)
    : PhysicalCollection(collection, info),
      _objectId(basics::VelocyPackHelper::stringUInt64(info, StaticStrings::ObjectId)),
      _revisionTreeApplied(0),
      _revisionTreeCreationSeq(0),
      _revisionTreeSerializedSeq(0),
      _revisionTreeSerializedTime(std::chrono::steady_clock::now()) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  TRI_ASSERT(_logicalCollection.isAStub() || _objectId != 0);
  collection.vocbase()
      .server()
      .getFeature<EngineSelectorFeature>()
      .engine<RocksDBEngine>()
      .addCollectionMapping(_objectId, _logicalCollection.vocbase().id(),
                            _logicalCollection.id());
}

std::string const& RocksDBMetaCollection::path() const {
  return StaticStrings::Empty;  // we do not have any path
}

void RocksDBMetaCollection::deferDropCollection(std::function<bool(LogicalCollection&)> const&) {
  TRI_ASSERT(!_logicalCollection.syncByRevision());
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  _revisionTree.reset();
}

RevisionId RocksDBMetaCollection::revision(transaction::Methods* trx) const {
  auto* state = RocksDBTransactionState::toState(trx);
  auto trxCollection = static_cast<RocksDBTransactionCollection*>(
                                                                  state->findCollection(_logicalCollection.id()));
  
  TRI_ASSERT(trxCollection != nullptr);
  
  return trxCollection->revision();
}

uint64_t RocksDBMetaCollection::numberDocuments(transaction::Methods* trx) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto* state = RocksDBTransactionState::toState(trx);
  auto trxCollection = static_cast<RocksDBTransactionCollection*>(
                                                                  state->findCollection(_logicalCollection.id()));
  
  TRI_ASSERT(trxCollection != nullptr);
  
  return trxCollection->numberDocuments();
}

/// @brief write locks a collection, with a timeout
ErrorCode RocksDBMetaCollection::lockWrite(double timeout) {
  return doLock(timeout, AccessMode::Type::WRITE);
}

/// @brief write unlocks a collection
void RocksDBMetaCollection::unlockWrite() noexcept { _exclusiveLock.unlockWrite(); }

/// @brief read locks a collection, with a timeout
ErrorCode RocksDBMetaCollection::lockRead(double timeout) {
  return doLock(timeout, AccessMode::Type::READ);
}

/// @brief read unlocks a collection
void RocksDBMetaCollection::unlockRead() { _exclusiveLock.unlockRead(); }

void RocksDBMetaCollection::trackWaitForSync(arangodb::transaction::Methods* trx,
                                             OperationOptions& options) {
  if (_logicalCollection.waitForSync() && !options.isRestore) {
    options.waitForSync = true;
  }
  
  if (options.waitForSync) {
    trx->state()->waitForSync(true);
  }
}

// rescans the collection to update document count
uint64_t RocksDBMetaCollection::recalculateCounts() {
  std::unique_lock<std::mutex> guard(_recalculationLock);

  RocksDBEngine& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  const rocksdb::Snapshot* snapshot = nullptr;
  // start transaction to get a collection lock
  TRI_vocbase_t& vocbase = _logicalCollection.vocbase();
  if (!vocbase.use()) {  // someone dropped the database
    return _meta.numberDocuments();
  }
  auto useGuard = scopeGuard([&]() noexcept {
    // cppcheck-suppress knownConditionTrueFalse
    if (snapshot) {
      db->ReleaseSnapshot(snapshot);
    }
    vocbase.release();
  });

  // makes sure collection doesn't get unloaded
  CollectionGuard collGuard(&vocbase, _logicalCollection.id());

  RocksDBBlockerGuard blocker(&_logicalCollection);

  uint64_t snapNumberOfDocuments = 0;
  {
    // fetch number docs and snapshot under exclusive lock
    // this should enable us to correct the count later
    auto lockGuard = scopeGuard([this]() noexcept { unlockWrite(); });
    auto res = lockWrite(transaction::Options::defaultLockTimeout);
    if (res != TRI_ERROR_NO_ERROR) {
      lockGuard.cancel();
      THROW_ARANGO_EXCEPTION(res);
    }
 
    // place a blocker. will be removed by blockerGuard automatically
    blocker.placeBlocker();

    snapshot = engine.db()->GetSnapshot();
    snapNumberOfDocuments = _meta.numberDocuments();
    TRI_ASSERT(snapshot);
  }

  auto snapSeq = snapshot->GetSequenceNumber();

  auto bounds = RocksDBKeyBounds::Empty();
  bool set = false;
  {
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
    for (auto it : _indexes) {
      if (it->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
        RocksDBIndex const* rix = static_cast<RocksDBIndex const*>(it.get());
        bounds = RocksDBKeyBounds::PrimaryIndex(rix->objectId());
        set = true;
        break;
      }
    }
  }
  if (!set) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "did not find primary index");
  }
  
  // count documents
  rocksdb::Slice upper(bounds.end());
  
  rocksdb::ReadOptions ro;
  ro.snapshot = snapshot;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.verify_checksums = false;
  ro.fill_cache = false;
  
  rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(ro, cf));
  std::size_t count = 0;
 

  application_features::ApplicationServer& server = vocbase.server();

  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    TRI_ASSERT(it->key().compare(upper) < 0);
    ++count;

    if (count % 4096 == 0 &&
        server.isStopping()) {
      // check for server shutdown
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
  }
  
  int64_t adjustment = count - snapNumberOfDocuments;
  TRI_IF_FAILURE("disableCountAdjustment") {
    adjustment = 0;
  }
  if (adjustment != 0) {
    LOG_TOPIC("ad613", WARN, Logger::REPLICATION)
      << "inconsistent collection count detected for "
      << vocbase.name() << "/" << _logicalCollection.name()
      << ": counted value: " << count << ", snapshot value: " << snapNumberOfDocuments 
      << ", current value: " << _meta.numberDocuments()
      << ", an offset of " << adjustment << " will be applied";
    auto adjustSeq = engine.db()->GetLatestSequenceNumber();
    if (adjustSeq <= snapSeq) {
      adjustSeq = ::forceWrite(engine);
      TRI_ASSERT(adjustSeq > snapSeq);
    }
    _meta.adjustNumberDocuments(adjustSeq, RevisionId::none(), adjustment);
  } else {
    LOG_TOPIC("55df5", INFO, Logger::REPLICATION)
      << "no collection count adjustment needs to be applied for "
      << vocbase.name() << "/" << _logicalCollection.name()
      << ": counted value: " << count;
  }
  
  return _meta.numberDocuments();
}

void RocksDBMetaCollection::compact() {
  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  engine.compactRange(bounds());
  
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (std::shared_ptr<Index> i : _indexes) {
    RocksDBIndex* index = static_cast<RocksDBIndex*>(i.get());
    index->compact();
  }
}

void RocksDBMetaCollection::estimateSize(velocypack::Builder& builder) {
  TRI_ASSERT(!builder.isOpenObject() && !builder.isOpenArray());

  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  RocksDBKeyBounds bounds = this->bounds();
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out = 0, total = 0;
  db->GetApproximateSizes(bounds.columnFamily(), &r, 1, &out,
                          static_cast<uint8_t>(
                                               rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
                                               rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));
  total += out;
  
  builder.openObject();
  builder.add("documents", VPackValue(out));
  builder.add("indexes", VPackValue(VPackValueType::Object));
  
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (std::shared_ptr<Index> i : _indexes) {
    RocksDBIndex* index = static_cast<RocksDBIndex*>(i.get());
    out = index->memory();
    builder.add(std::to_string(index->id().id()), VPackValue(out));
    total += out;
  }
  builder.close();
  builder.add("total", VPackValue(total));
  builder.close();
}

void RocksDBMetaCollection::setRevisionTree(std::unique_ptr<containers::RevisionTree>&& tree,
                                            uint64_t seq) {
  TRI_ASSERT(_logicalCollection.useSyncByRevision());
  TRI_ASSERT(_logicalCollection.syncByRevision());
  TRI_ASSERT(tree != nullptr);
  TRI_ASSERT(tree->depth() == revisionTreeDepth);
  
  std::unique_lock<std::mutex> guard(_revisionTreeLock);

  _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(tree), _logicalCollection);
  _revisionTreeApplied = seq;
  _revisionTreeCreationSeq = seq;
  _revisionTreeSerializedSeq = seq;
  _revisionTreeSerializedTime = std::chrono::steady_clock::time_point();
  _revisionTreeCanBeSerialized = true;
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(
    rocksdb::SequenceNumber notAfter,
    std::function<std::unique_ptr<containers::RevisionTree>(std::unique_ptr<containers::RevisionTree>, std::unique_lock<std::mutex>&)> const& callback) {
  if (!_logicalCollection.useSyncByRevision()) {
    return nullptr;
  }

  RocksDBEngine& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  rocksdb::DB* db = engine.db()->GetRootDB();

  std::unique_lock<std::mutex> lock(_revisionTreeLock);
  
  // first apply any updates that can be safely applied
  rocksdb::SequenceNumber safeSeq = meta().committableSeq(db->GetLatestSequenceNumber());
  if (notAfter < safeSeq) {
    safeSeq = notAfter;
  }

  if (!_revisionTree && !haveBufferedOperations(lock)) {
    // we only need to return an empty tree here.
    lock.unlock();
    return allocateEmptyRevisionTree(revisionTreeDepth); 
  }

  // If the tree has already moved past `notAfter`, then all is lost and we can
  // only give up:
  if (_revisionTreeApplied > notAfter) {
    return nullptr;
  }

  applyUpdates(safeSeq, lock);  // will not go beyond `notAfter` because code above!
  TRI_ASSERT(_revisionTree != nullptr);
  TRI_ASSERT(_revisionTree->depth() == revisionTreeDepth);

  // now clone the tree so we can apply all updates consistent with our ongoing trx
  auto tree = _revisionTree->clone();
  TRI_ASSERT(tree != nullptr);

  return callback(std::move(tree), lock);
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(transaction::Methods& trx) {
  rocksdb::SequenceNumber trxSeq = RocksDBTransactionState::toState(&trx)->beginSeq();
  TRI_ASSERT(trxSeq != 0);

  return revisionTree(trxSeq, [this, &trx, trxSeq](std::unique_ptr<containers::RevisionTree> tree, std::unique_lock<std::mutex>& lock) -> std::unique_ptr<containers::RevisionTree> {
    TRI_ASSERT(lock.owns_lock());
    // apply any which are buffered and older than our ongoing transaction start
    Result res = applyUpdatesForTransaction(*tree, trxSeq, lock);
    if (res.fail()) {
      return nullptr;
    }

    // now peek at updates buffered inside transaction and apply those too
    auto const& operations = RocksDBTransactionState::toState(&trx)->trackedOperations(
        _logicalCollection.id());
        
    // this revision tree exists only temporarily, so we do not track its memory usage
    tree->insert(operations.inserts);
    tree->remove(operations.removals);
  
    return tree;
  });
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(uint64_t batchId) {
  RocksDBEngine& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  RocksDBReplicationManager* manager = engine.replicationManager();
  RocksDBReplicationContext* ctx = batchId == 0 ? nullptr : manager->find(batchId);
  if (!ctx) {
    return nullptr;
  }
  auto guard = scopeGuard([manager, ctx]() noexcept -> void { manager->release(ctx); });
  rocksdb::SequenceNumber trxSeq = ctx->snapshotTick();
  TRI_ASSERT(trxSeq != 0);

  return revisionTree(trxSeq, [this, trxSeq](std::unique_ptr<containers::RevisionTree> tree, std::unique_lock<std::mutex>& lock) -> std::unique_ptr<containers::RevisionTree> {
    TRI_ASSERT(lock.owns_lock());
    // apply any which are buffered and older than our ongoing transaction start
    Result res = applyUpdatesForTransaction(*tree, trxSeq, lock);
    if (res.fail()) {
      return nullptr;
    }
    
    return tree;
  });
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::computeRevisionTree(uint64_t batchId) {
  std::unique_ptr<ReplicationIterator> iter
    = _logicalCollection.getPhysical()->getReplicationIterator(ReplicationIterator::Ordering::Revision, batchId);
  if (!iter) {
    LOG_TOPIC("52617", WARN, arangodb::Logger::ENGINES)
        << "failed to retrieve replication iterator to build revision tree "
           "for collection '"
        << _logicalCollection.id().id() << "'";
    return nullptr;
  }

  RevisionReplicationIterator& it =
      *static_cast<RevisionReplicationIterator*>(iter.get());
 
  // the tree may only be there temporarily, so we intentionally do
  // not track its memory usage here, but rather at some indirect call sites
  // only.
  return buildTreeFromIterator(it);
}

Result RocksDBMetaCollection::takeCareOfRevisionTreePersistence(
    LogicalCollection& coll, RocksDBEngine& engine,
    rocksdb::WriteBatch& batch, rocksdb::ColumnFamilyHandle* const cf,
    rocksdb::SequenceNumber maxCommitSeq,
    bool force, std::string const& context, std::string& scratch,
    rocksdb::SequenceNumber& appliedSeq) {

  TRI_ASSERT(coll.useSyncByRevision());

  // might lower `appliedSeq`!

  // Get lock on revision tree:
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  if (maxCommitSeq < _revisionTreeApplied) {
    // this function is called indirectly from RocksDBSettingsManager::sync which passes
    // in a seq nr that it has previously obtained, but it can happen that in the meantime
    // the tree has been moved forward or a tree was rebuilt and _revisionTreeApplied is
    // now greater than the seq nr we get passed in. In this case we have to bump the seq
    // nr forward, because we cannot generate a tree from the past.
    maxCommitSeq = _revisionTreeApplied;
  }
  
  maxCommitSeq = _meta.committableSeq(maxCommitSeq);
  if ((force || needToPersistRevisionTree(maxCommitSeq, guard)) &&
      _revisionTreeCanBeSerialized) {
    rocksdb::SequenceNumber seq = maxCommitSeq;

    TRI_ASSERT(maxCommitSeq >= _revisionTreeApplied);
    
    scratch.clear();

    try {
      seq = serializeRevisionTree(scratch, seq, force, guard);
    } catch (std::exception const& ex) {
      LOG_TOPIC("33691", WARN, Logger::ENGINES)
          << context << ": caught exception during revision tree serialization: " << ex.what();
    
      auto& engine =
          _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
      auto* db = engine.db();
      
      rocksdb::WriteOptions wo;
      
      RocksDBKey key;
      // remove revision tree from rocksdb
      key.constructRevisionTreeValue(_objectId);
      rocksdb::Status s = batch.Delete(cf, key.string());
      
      if (s.ok() || s.IsNotFound()) {
        s = db->Write(wo, &batch);
      } 
      if (!s.ok()) {
        LOG_TOPIC("af3de", WARN, Logger::ENGINES)
            << context << ": could not delete invalid revision tree: " << s.ToString();
        // continue and schedule a tree rebuild
      }

      batch.Clear();

      // if we crash now, we won't have a revision tree and can rebuild it from
      // scratch on restart.
      
      // now also prevent this revision tree from being serialized...
      _revisionTreeCanBeSerialized = false;

      // you can take our trees, but you cannot take our souls!
      // schedule a rebuild for the tree:
      engine.scheduleTreeRebuild(coll.vocbase().id(), coll.guid());

      // we cannot move beyond the seq no that we have already serialized up to.
      // this will effectively keep the background thread from making progress, 
      // but the scheduled tree rebuild operation should unblock it eventually.
      appliedSeq = std::min(appliedSeq, _revisionTreeSerializedSeq);
      return {};
    }

    guard.unlock();

    appliedSeq = std::min(appliedSeq, seq);

    if (!scratch.empty()) {
      rocksutils::uint64ToPersistent(scratch, seq);

      RocksDBKey key;
      key.constructRevisionTreeValue(objectId());
      rocksdb::Slice value(scratch);

      rocksdb::Status s = batch.Put(cf, key.string(), value);
      if (!s.ok()) {
        LOG_TOPIC("ff234", WARN, Logger::ENGINES)
            << context << ": writing revision tree failed";
        return Result(rocksutils::convertStatus(s));
      } else {
        LOG_TOPIC("92a08", TRACE, Logger::ENGINES)
            << context << ": serialized revision tree for "
            << "collection with objectId '" << objectId() << "' "
            << "through sequence number " << seq;
      }
    } else {
      LOG_TOPIC("92b07", TRACE, Logger::ENGINES)
          << context << ": skipping serialization of revision tree for "
          << "collection with objectId '" << objectId() << "'";
    }
  } else {
    LOG_TOPIC("92ba9", TRACE, Logger::ENGINES)
        << context << ": no need to serialize revision tree for "
        << "collection with objectId '" << objectId() << "'";

    TRI_IF_FAILURE("serializeMeta::delayCallToLastSerializedRevisionTree") {
      if (!coll.system()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    }

    // Find out up to which sequence the tree of this collection has been
    // persisted to RocksDB. We need this to compute the minimum over all
    // collections for the persisted `lastSync` value (in `appliedSeq`).
    // Note that on recovery after a crash (or restart) all WAL entries
    // lower than the persisted `lastSync` are ignored. Therefore it
    // is imperative that there are no pending or actual changes for
    // the revision tree in memory, which will end up in the WAL with a
    // sequence number less than the computed `seq` number here!
    rocksdb::SequenceNumber seq = lastSerializedRevisionTree(maxCommitSeq, guard);
    appliedSeq = std::min(appliedSeq, seq);

    // set the tree to sleep (note: hibernation requests may be ignored if there
    // is not yet a need to hibernate)
    hibernateRevisionTree(guard);
  }

  return Result{};
}

bool RocksDBMetaCollection::needToPersistRevisionTree(rocksdb::SequenceNumber maxCommitSeq, std::unique_lock<std::mutex> const& lock) const {
  TRI_ASSERT(lock.owns_lock());
  TRI_ASSERT(_logicalCollection.useSyncByRevision());
  
  if (!_revisionTreeCanBeSerialized) {
    return false;
  }
  
  bool checkBuffers = true;

  TRI_IF_FAILURE("needToPersistRevisionTree::checkBuffers") {
    checkBuffers = false;
  }
  
  if (checkBuffers) {
    // have a truncate to apply
    if (!_revisionTruncateBuffer.empty() && *_revisionTruncateBuffer.begin() <= maxCommitSeq) {
      return true;
    }

    // have insertions to apply
    if (!_revisionInsertBuffers.empty() && _revisionInsertBuffers.begin()->first <= maxCommitSeq) {
      return true;
    }

    // have removals to apply
    if (!_revisionRemovalBuffers.empty() && _revisionRemovalBuffers.begin()->first <= maxCommitSeq) {
      return true;
    }
  }

  // have applied updates that we haven't persisted
  if (_revisionTreeSerializedSeq < _revisionTreeApplied) {
    return true;
  }

  // tree has never been persisted
  if (_revisionTreeSerializedSeq <= _revisionTreeCreationSeq) {
    return true;
  }
      
  return false;
}

rocksdb::SequenceNumber RocksDBMetaCollection::lastSerializedRevisionTree(rocksdb::SequenceNumber seq, 
                                                                          std::unique_lock<std::mutex> const& lock) {

  TRI_ASSERT(lock.owns_lock());

  // As explained at the call site in RocksDBMetadata.cpp the purpose of
  // this function is to compute a sequence number s, which is as large
  // as possible with the property, that there are and will never be
  // any changes to the collection with a sequence number larger than
  // the sequence number of the last persisted version of the revision tree
  // *and* smaller than s. Therefore, we can in the end move the in memory
  // sequence number forward of the latest serialized revision tree.
  // See below for a proof that we do not miss any transactions!
  
  // A transaction commit proceeds as follows:
  //
  //  1. Note the latest sequence number in the RocksDB WAL, call it "beginSeq"
  //  2. Create a blocker for all collections involved with beginSeq, if this
  //     is not possible, bump beginSeq forward (by getting a new latest
  //     sequence number from RocksDB). After this step, we have blockers
  //     on all collections involved for sequence numbers, which are all
  //     at most beginSeq in its final state.
  //  3. Commit the rocksDB transaction, this will result in a WAL sequence
  //     number for the commit marker called "postCommitSeq". It and all
  //     sequence numbers of write operations for the transaction are larger
  //     than beginSeq.
  //  4. Call `bufferUpdates` on the `RocksDBMetaCollection and submit all
  //     revisions which have been inserted or removed, which in turns
  //     queues these operations in `_revisionInsertBuffers` and
  //     `_revisionRemoveBuffers` for the revision tree, filed under
  //     the sequence number `postCommitSeq`.
  //  5. Remove the blockers for the transaction.
  //
  // After that, at some stage the pending insertions and removals are
  // actually applied to the tree and its `_revisionTreeApplied` is moved
  // forward. The new tree will eventually be persisted again.
  // Note that since `beginSeq` is always smaller than any sequence number
  // involved in the transaction (including `postCommitSeq`),
  // we always have a blocker with `beginSeq` (or smaller) whenever
  // we write operations to the WAL.

  // bump sequence number forward for the tree only if this is safe
  if (_revisionTruncateBuffer.empty() && 
      _revisionInsertBuffers.empty() && 
      _revisionRemovalBuffers.empty() &&
      _revisionTreeApplied < seq &&
      _revisionTreeSerializedSeq < seq &&
      _revisionTreeCanBeSerialized &&
      !_meta.hasBlockerUpTo(seq)) {

    // If we get here, we can advance `_revisionTreeSerializedSeq` 
    // and `_revisionTreeApplied` to `seq`,
    // although we have not actually persisted the tree at `seq`.
    // All we have to ensure that no transaction has or will ever again
    // produce a change with a sequence number larger than
    // `_revisitionTreeSerializedSeq` and smaller than or equal to `seq`.
    // This is true, because of the following:
    // Assume some transaction that touches this collection would end up
    // with its commit marker at a sequence number between
    // `_revisionTreeSerializedSeq` and `seq` (i.e. its
    // `postCommitSeq`). Then it must have before got a blocker for its
    // `beginSeq` number in Step 2 above. So either, we still see the blocker,
    // or the corresponding operations have already been added to the buffers,
    // or they have already been applied to the tree, in which case
    // `_revisionTreeApplied` would be larger than `seq`. q.e.d.
    LOG_TOPIC("32d45", TRACE, Logger::ENGINES)
        << "adjusting sequence number for " << _logicalCollection.name() 
        << " from " << _revisionTreeSerializedSeq << " to " << seq;
    
    _revisionTreeSerializedSeq = seq;
    _revisionTreeApplied = seq;
    // This allows to uphold the invariants that
    // _revisionTreeCreationSeq <= _revisionTreeSerializedSeq and
    // _revisionTreeSerializedSeq <= _revisionTreeApplied.
  }
  
  return _revisionTreeSerializedSeq;
}

rocksdb::SequenceNumber RocksDBMetaCollection::serializeRevisionTree(
    std::string& output, rocksdb::SequenceNumber commitSeq, bool force,
    std::unique_lock<std::mutex> const& lock) {
  TRI_ASSERT(lock.owns_lock());
  TRI_ASSERT(_logicalCollection.useSyncByRevision());

  if (!_revisionTree && !haveBufferedOperations(lock)) {  // empty collection
    return commitSeq;
  }
  if (!_revisionTreeCanBeSerialized) {
    // previously, a revision tree was found to be bad and is now being repaired, 
    // in the meantime we return the number when it was last persisted to avoid 
    // lastSync jumping down again later
    return _revisionTreeSerializedSeq;
  }

  applyUpdates(commitSeq, lock);  // always apply updates...
  TRI_ASSERT(_revisionTreeApplied == commitSeq);

  // applyUpdates will make sure we will have a valid tree
  TRI_ASSERT(_revisionTree != nullptr);
  TRI_ASSERT(_revisionTree->depth() == revisionTreeDepth);

  bool neverDone = _revisionTreeSerializedSeq <= _revisionTreeCreationSeq;
  bool coinFlip = RandomGenerator::interval(static_cast<uint32_t>(5)) == 0;
  bool beenTooLong = 30 < std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::steady_clock::now() - _revisionTreeSerializedTime)
                              .count();
  TRI_IF_FAILURE("RocksDBMetaCollection::forceSerialization") {
    coinFlip = true;
  }

  TRI_IF_FAILURE("RocksDBMetaCollection::serializeRevisionTree") {
    return _revisionTreeSerializedSeq;
  }
  if (force || neverDone || coinFlip || beenTooLong) {  // ...but only write the tree out sometimes
    _revisionTree->serializeBinary(output);
    _revisionTreeSerializedSeq = commitSeq;
    _revisionTreeSerializedTime = std::chrono::steady_clock::now();

    if (_revisionTreeSerializedSeq < _revisionTreeCreationSeq) {
      _revisionTreeCreationSeq = _revisionTreeSerializedSeq;
    }
  }
  return _revisionTreeSerializedSeq;
}
  
ResultT<std::pair<std::unique_ptr<containers::RevisionTree>, rocksdb::SequenceNumber>> 
RocksDBMetaCollection::revisionTreeFromCollection(bool checkForBlockers) {
  auto ctxt = transaction::StandaloneContext::Create(_logicalCollection.vocbase());
  SingleCollectionTransaction trx(ctxt, _logicalCollection, AccessMode::Type::READ);

  Result res = trx.begin();
  if (res.fail()) {
    LOG_TOPIC("d1e53", WARN, arangodb::Logger::ENGINES)
        << "failed to begin transaction to rebuild revision tree "
           "for collection '"
        << _logicalCollection.id().id() << "'";
    return res;
  }

  auto* state = RocksDBTransactionState::toState(&trx);

  if (checkForBlockers &&
      _meta.hasBlockerUpTo(state->beginSeq())) {
    return Result(TRI_ERROR_LOCKED, "cannot rebuild revision tree now as there are still transaction blockers in place");
  }

  auto iter = getReplicationIterator(ReplicationIterator::Ordering::Revision, trx);
  if (!iter) {
    LOG_TOPIC("d1e54", WARN, arangodb::Logger::ENGINES)
        << "failed to retrieve replication iterator to rebuild revision tree "
           "for collection '"
        << _logicalCollection.id().id() << "'";
    return Result(TRI_ERROR_INTERNAL);
  }
  RevisionReplicationIterator& it =
      *static_cast<RevisionReplicationIterator*>(iter.get());
  
  // the tree may only be there temporarily, so we intentionally do
  // not track its memory usage here, but rather at some indirect call sites
  // only.
  auto newTree = buildTreeFromIterator(it);
  return std::make_pair(std::move(newTree), state->beginSeq());
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::buildTreeFromIterator(RevisionReplicationIterator& it) const {
  std::vector<std::uint64_t> revisions;
  revisions.reserve(1024);
  
  auto newTree = allocateEmptyRevisionTree(revisionTreeDepth);
  
  while (it.hasMore()) {
    revisions.emplace_back(it.revision().id());
    if (revisions.size() >= 4096) {  // arbitrary batch size
      newTree->insert(revisions);
      revisions.clear();
    
      if (_logicalCollection.vocbase().server().isStopping()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }
    }
    it.next();
  }
  if (!revisions.empty()) {
    newTree->insert(revisions);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  newTree->checkConsistency();
#endif

  return newTree;
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void RocksDBMetaCollection::corruptRevisionTree(uint64_t count, uint64_t hash) {
  if (!_logicalCollection.useSyncByRevision()) {
    return;
  }

  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  if (_revisionTree != nullptr) {
    _revisionTree->corrupt(count, hash);
  }
}
#endif

Result RocksDBMetaCollection::rebuildRevisionTree() {
  if (!_logicalCollection.useSyncByRevision()) {
    return {};
  }
    
  return basics::catchToResult([this]() -> Result {
    auto lockGuard = scopeGuard([this]() noexcept { unlockWrite(); });
    // get the exclusive lock on the collection, so that no new write transactions
    // can start for this collection
    auto res = lockWrite(/*timeout*/ 180.0);
    if (res != TRI_ERROR_NO_ERROR) {
      lockGuard.cancel();
      THROW_ARANGO_EXCEPTION(res);
    }
    
    // we now have the exclusive lock on the collection!
    
    // utility function to remove buffered updates up to (including) seq.
    // requires the buffer lock to be held!
    auto removeBufferedUpdatesUpTo = [this](rocksdb::SequenceNumber seq) {
      {
        auto it = _revisionTruncateBuffer.begin(); 
        while (it != _revisionTruncateBuffer.end() && *it <= seq) {
          it = _revisionTruncateBuffer.erase(it);
        }
      }
      
      {
        auto it = _revisionInsertBuffers.begin(); 
        while (it != _revisionInsertBuffers.end() && it->first <= seq) {
          it = _revisionInsertBuffers.erase(it);
        }
      }
      
      {
        auto it = _revisionRemovalBuffers.begin(); 
        while (it != _revisionRemovalBuffers.end() && it->first <= seq) {
          it = _revisionRemovalBuffers.erase(it);
        }
      }
    };
    
    // place a blocker and add a guard to remove it at the end
    TransactionId trxId = TransactionId(transaction::Context::makeTransactionId());
  
    auto blockerGuard = scopeGuard([&]() noexcept {  // remove blocker afterwards
      removeRevisionTreeBlocker(trxId);
    });
      
    TRI_IF_FAILURE("rebuildRevisionTree::sleep") {
      std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }
    
    // we now have a blocker in place that will prevent updates from being 
    // applied
    rocksdb::SequenceNumber blockerSeq = placeRevisionTreeBlocker(trxId);
    
    TRI_IF_FAILURE("rebuildRevisionTree::sleep") {
      std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }

    {
      // we may still have some buffered updates, so let's remove them.
      std::unique_lock<std::mutex> guard(_revisionTreeLock);
      removeBufferedUpdatesUpTo(blockerSeq - 1);

      _revisionTreeApplied = blockerSeq - 1;
    }
  
    // unlock the collection again
    lockGuard.fire();
    
    TRI_IF_FAILURE("rebuildRevisionTree::sleep") {
      std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }

    // now rebuild the tree from the collection data, using a
    // snapshot that will be acquired inside `revisionTreeFromCollection`.
    // note that the snapshot may have a sequence number > blockerSeq!
    auto result = revisionTreeFromCollection(false);
    if (result.fail()) {
      return result.result(); 
    }

    auto&& [newTree, beginSeq] = result.get();

    TRI_ASSERT(blockerSeq <= beginSeq);

    // update our blocker to the sequence number from the snapshot 
    _meta.updateBlocker(trxId, beginSeq);
    
    TRI_IF_FAILURE("rebuildRevisionTree::sleep") {
      std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }

    // we need to set a timeout for this operation, because if it does not complete within 
    // reasonable bounds, we need to relinquish the thread that is kept busy waiting here.
    auto const endTime = std::chrono::steady_clock::now() + std::chrono::minutes(2);

    // wait until all blockers before the snapshot have been removed
    while (true) {
      std::unique_lock<std::mutex> guard(_revisionTreeLock);
      
      if (_revisionTreeApplied < beginSeq) {
        _revisionTreeApplied = beginSeq;
      }
        
      TRI_IF_FAILURE("rebuildRevisionTree::sleep") {
        if (RandomGenerator::interval(uint32_t(2000)) > 1750) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_DEBUG, "intentional error during testing");
        }
      }

      if (_meta.hasBlockerUpTo(beginSeq - 1)) {
        guard.unlock();

        if (_logicalCollection.vocbase().server().isStopping()) {
          // react on server shutdown
          THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
        }
    
        if (std::chrono::steady_clock::now() > endTime) {
          // timeout reached. abort and later try again
          std::string msg("unable to establish new revision tree for collection '");
          msg.append(_logicalCollection.name());
          msg.append("' in time");

          LOG_TOPIC("d7560", WARN, Logger::ENGINES) << msg;
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::move(msg));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }

      TRI_ASSERT(_revisionTreeApplied <= beginSeq);
      TRI_ASSERT(_revisionTreeSerializedSeq <= beginSeq);

      // establish the new tree. this will also track the memory usage of the tree
      _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(newTree), _logicalCollection);
      _revisionTreeApplied = beginSeq;
      _revisionTreeCreationSeq = beginSeq;
      _revisionTreeSerializedSeq = beginSeq;
      _revisionTreeSerializedTime = std::chrono::steady_clock::time_point();
      _revisionTreeCanBeSerialized = true;

      // finally remove all pending updates up to including our own sequence number
      removeBufferedUpdatesUpTo(beginSeq);
      return {};
    }
  });
}

void RocksDBMetaCollection::rebuildRevisionTree(std::unique_ptr<rocksdb::Iterator>& iter) {
  auto newTree = allocateEmptyRevisionTree(revisionTreeDepth);
  
  // okay, we are in recovery and can't open a transaction, so we need to
  // read the raw RocksDB data; on the plus side, we are in recovery, so we
  // are single-threaded and don't need to worry about transactions anyway

  RocksDBKeyBounds documentBounds =
      RocksDBKeyBounds::CollectionDocuments(_objectId);
  rocksdb::Comparator const* cmp =
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
          ->GetComparator();
  rocksdb::Slice const end = documentBounds.end();

  auto& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  auto* db = engine.db();
  
  std::vector<std::uint64_t> revisions;
  revisions.reserve(1024);

  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  
  for (iter->Seek(documentBounds.start());
       iter->Valid() && cmp->Compare(iter->key(), end) < 0; iter->Next()) {
    LocalDocumentId const docId = RocksDBKey::documentId(iter->key());
    revisions.emplace_back(docId.id());
    if (revisions.size() >= 4096) {  // arbitrary batch size
      newTree->insert(revisions);
      revisions.clear();
        
      if (_logicalCollection.vocbase().server().isStopping()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }
    }
  }
  if (!revisions.empty()) {
    newTree->insert(revisions);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  newTree->checkConsistency();
#endif

  rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();
  _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(newTree), _logicalCollection);
  _revisionTreeApplied = seq;
  _revisionTreeCreationSeq = seq;
  _revisionTreeSerializedSeq = seq;
  _revisionTreeSerializedTime = std::chrono::steady_clock::time_point();
  _revisionTreeCanBeSerialized = true;
}

// returns a pair with the number of documents and the tree's seq number.
std::pair<uint64_t, uint64_t> RocksDBMetaCollection::revisionTreeInfo() const {
  TRI_ASSERT(_logicalCollection.useSyncByRevision());
    
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  if (_revisionTree != nullptr) {
    return {_revisionTree->count(), _revisionTreeApplied};
  }
  return {0, 0};
}

void RocksDBMetaCollection::revisionTreeSummary(VPackBuilder& builder, bool fromCollection) {
  if (!_logicalCollection.useSyncByRevision()) {
    return;
  }

  uint64_t count = 0;
  uint64_t hash = 0;
  uint64_t byteSize = 0;

  if (fromCollection) {
    // rebuild a temporary tree from the collection
    auto result = revisionTreeFromCollection(false);
    // the tree is only there temporarily, so we intentionally do
    // not track its memory usage!

    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result.result());
    }

    auto&& [tree, beginSeq] = result.get();

    count = tree->count();
    hash = tree->rootValue();
    byteSize = tree->byteSize();
  } else {
    // use existing revision tree
    std::unique_lock<std::mutex> guard(_revisionTreeLock);

    if (_revisionTree != nullptr) {
      count = _revisionTree->count();
      hash = _revisionTree->rootValue();
      byteSize = _revisionTree->compressedSize();
    }
  }
  
  VPackObjectBuilder obj(&builder);
  obj->add(StaticStrings::RevisionTreeCount, VPackValue(count));
  obj->add(StaticStrings::RevisionTreeHash, VPackValue(hash));
  obj->add("byteSize", VPackValue(byteSize));
}

void RocksDBMetaCollection::revisionTreePendingUpdates(VPackBuilder& builder) {
  VPackObjectBuilder obj(&builder);
  
  if (!_logicalCollection.useSyncByRevision()) {
    // empty object
    return;
  }

  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  if (_revisionTree == nullptr) {
    // no revision tree yet
    return;
  }
  
  uint64_t inserts = _revisionInsertBuffers.size();
  uint64_t removes = _revisionRemovalBuffers.size();
  uint64_t truncates = _revisionTruncateBuffer.size();

  guard.unlock();
  
  obj->add("inserts", VPackValue(inserts));
  obj->add("removes", VPackValue(removes));
  obj->add("truncates", VPackValue(truncates));
}
  
uint64_t RocksDBMetaCollection::placeRevisionTreeBlocker(TransactionId transactionId) {
  auto& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  
  // make sure that the global revision tree in _revisionTree does not move beyond
  // the sequence number we get here from RocksDB:
  std::unique_lock<std::mutex> guard(_revisionTreeLock);

  while (true) {
    rocksdb::SequenceNumber preSeq = db->GetLatestSequenceNumber();
    // Since we have the lock above, the revision tree cannot move beyond
    // this sequence number before we have actually placed the blocker.
    if (preSeq >= _revisionTreeApplied &&
        preSeq >= _revisionTreeSerializedSeq &&
        preSeq >= _meta.countCommitted()) {
      _meta.placeBlocker(transactionId, preSeq);
      return preSeq;
    }
    
    if (_logicalCollection.vocbase().server().isStopping()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
      
    std::this_thread::yield();
  }
}

void RocksDBMetaCollection::removeRevisionTreeBlocker(TransactionId transactionId) {
  _meta.removeBlocker(transactionId);
}

void RocksDBMetaCollection::bufferUpdates(rocksdb::SequenceNumber seq,
                                          std::vector<std::uint64_t>&& inserts,
                                          std::vector<std::uint64_t>&& removals) {
  if (!_logicalCollection.useSyncByRevision()) {
    return;
  }
  
  TRI_ASSERT(!inserts.empty() || !removals.empty());
  
  std::unique_lock<std::mutex> guard(_revisionTreeLock);

  if (_revisionTreeApplied >= seq) {
    LOG_TOPIC("1fe4d", TRACE, Logger::ENGINES)
        << "rejecting change with too low sequence number " << seq << " for collection " << _logicalCollection.name();

    TRI_ASSERT(_logicalCollection.vocbase()
                   .server()
                   .getFeature<EngineSelectorFeature>()
                   .engine()
                   .inRecovery());
    return;
  }
  
  TRI_IF_FAILURE("TransactionChaos::randomSleep") {
    std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(5))));
  }
    
  LOG_TOPIC("bafcf", TRACE, Logger::ENGINES) 
      << "buffering " << inserts.size() << "inserts and " << removals.size() << " removals "
      << "for collection " << _logicalCollection.name();

  if (!inserts.empty()) {
    // will default-construct an empty entry if it does not yet exist
    TRI_ASSERT(_revisionInsertBuffers.find(seq) == _revisionInsertBuffers.end());
    auto& elem = _revisionInsertBuffers[seq];
    if (elem.empty()) {
      elem = std::move(inserts);
    } else {
      // should only happen in recovery, if at all
      TRI_ASSERT(_logicalCollection.vocbase()
                     .server()
                     .getFeature<EngineSelectorFeature>()
                     .engine()
                     .inRecovery());
      elem.insert(elem.end(), inserts.begin(), inserts.end());
    }
  }
  if (!removals.empty()) {
    // will default-construct an empty entry if it does not yet exist
    TRI_ASSERT(_revisionRemovalBuffers.find(seq) == _revisionRemovalBuffers.end());
    auto& elem = _revisionRemovalBuffers[seq];
    if (elem.empty()) {
      elem = std::move(removals);
    } else {
      // should only happen in recovery, if at all
      TRI_ASSERT(_logicalCollection.vocbase()
                     .server()
                     .getFeature<EngineSelectorFeature>()
                     .engine()
                     .inRecovery());
      elem.insert(elem.end(), removals.begin(), removals.end());
    }
  }
}

Result RocksDBMetaCollection::bufferTruncate(rocksdb::SequenceNumber seq) {
  if (!_logicalCollection.useSyncByRevision()) {
    return Result();
  }

  Result res = basics::catchVoidToResult([&]() -> void {
    std::unique_lock<std::mutex> guard(_revisionTreeLock);
    if (_revisionTreeApplied >= seq) {
      return;
    }
    _revisionTruncateBuffer.emplace(seq);
  });
  return res;
}

void RocksDBMetaCollection::hibernateRevisionTree(std::unique_lock<std::mutex> const& lock) {
  TRI_ASSERT(lock.owns_lock());
  if (_revisionTree && !haveBufferedOperations(lock)) {
    _revisionTree->hibernate(/*force*/ false);
  }
}

void RocksDBMetaCollection::applyUpdates(rocksdb::SequenceNumber commitSeq,
                                         std::unique_lock<std::mutex> const& lock) {
  TRI_ASSERT(lock.owns_lock());
  TRI_ASSERT(_logicalCollection.useSyncByRevision());
  TRI_ASSERT(_revisionTree || haveBufferedOperations(lock));

  TRI_IF_FAILURE("applyUpdates::forceHibernation1") {
    if (_revisionTree != nullptr) {
      _revisionTree->hibernate(/*force*/ true);
    }
  }

  // make sure we will have a _revisionTree ready after this
  ensureRevisionTree();
  TRI_ASSERT(_revisionTree != nullptr);
  TRI_ASSERT(_revisionTree->depth() == revisionTreeDepth);
    
  if (!_revisionTreeCanBeSerialized) {
    return;
  }

  auto oldMemoryUsage = _revisionTree->memoryUsage();
  
  Result res = basics::catchVoidToResult([&]() -> void {
    // bump sequence number upwards
    auto bumpSequence = [this](rocksdb::SequenceNumber seq) noexcept {
      if (seq > _revisionTreeApplied) {
        _revisionTreeApplied = seq;
      }
    };
  
    TRI_IF_FAILURE("TransactionChaos::randomSleep") {
      std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(5))));
    }

    auto insertIt = _revisionInsertBuffers.begin();
    auto removeIt = _revisionRemovalBuffers.begin();

    auto checkIterators = [&]() {
      TRI_ASSERT(insertIt == _revisionInsertBuffers.begin() || 
                 _revisionInsertBuffers.empty() || _revisionInsertBuffers.begin()->first > commitSeq);
      TRI_ASSERT(removeIt == _revisionRemovalBuffers.begin() || 
                 _revisionRemovalBuffers.empty() || _revisionRemovalBuffers.begin()->first > commitSeq);
    };

    auto it = _revisionTruncateBuffer.begin();  // sorted ASC
    
    {
      // check for a truncate marker
      rocksdb::SequenceNumber ignoreSeq = 0;  // truncate will increase this sequence
      bool foundTruncate = false;

      while (it != _revisionTruncateBuffer.end() && *it <= commitSeq) {
        ignoreSeq = *it;
        TRI_ASSERT(ignoreSeq > _revisionTreeApplied);
        TRI_ASSERT(ignoreSeq != 0);
        foundTruncate = true;
        it = _revisionTruncateBuffer.erase(it);
      }

      if (foundTruncate) {
        TRI_ASSERT(ignoreSeq <= commitSeq);
        while (insertIt != _revisionInsertBuffers.end() && insertIt->first <= ignoreSeq) {
          insertIt = _revisionInsertBuffers.erase(insertIt);
        }
        while (removeIt != _revisionRemovalBuffers.end() && removeIt->first <= ignoreSeq) {
          removeIt = _revisionRemovalBuffers.erase(removeIt);
        }
 
        checkIterators();

        // clear out any revision structure, now empty
        _revisionTree->clear();

        checkIterators();

        // we have applied all changes up to here
        bumpSequence(ignoreSeq);
      }
    }

    // still holding the mutex here

    while (true) {
      checkIterators();

      // find out if we still have buffers to apply
      bool haveInserts = insertIt != _revisionInsertBuffers.end() &&
                         insertIt->first <= commitSeq;
      bool haveRemovals = removeIt != _revisionRemovalBuffers.end() &&
                          removeIt->first <= commitSeq;

      bool applyInserts =
            haveInserts && (!haveRemovals || removeIt->first >= insertIt->first);
      bool applyRemovals = haveRemovals && !applyInserts;
  
      // no inserts or removals left to apply, drop out of loop
      if (!applyInserts && !applyRemovals) {
        // we have applied all changes up to including commitSeq
        TRI_IF_FAILURE("TransactionChaos::randomSleep") {
          std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(5))));
        }
        bumpSequence(commitSeq);
        return;
      }
     
      // another concurrent thread may insert new elements into _revisionInsertBuffers or 
      // _revisionRemovalBuffers while we are here.
      // it is safe for us to access the elements pointed to by insertIt and removeIt here 
      // without holding the lock on these containers though, because of std::map's
      // iterator invalidation rules:
      // - emplace / insert: No iterators or references are invalidated.
      // - erase: References and iterators to the erased elements are invalidated. Other references and iterators are not affected.
      
      // check for inserts
      if (applyInserts) {
        TRI_ASSERT(!applyRemovals);
      
        TRI_IF_FAILURE("RevisionTree::applyInserts") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
          
        TRI_ASSERT(insertIt->first > _revisionTreeApplied);

        // apply inserts, without holding the lock
        // if this throws we will not have modified _revisionInsertBuffers
        try {
          _revisionTree->insert(insertIt->second);
        } catch (std::exception const& ex) {
          LOG_TOPIC("27811", ERR, Logger::ENGINES)
              << "unable to apply revision tree inserts for " 
              << _logicalCollection.vocbase().name() << "/" << _logicalCollection.name() 
              << ": " << ex.what();
          // it is pretty bad if this fails, so we want to see it in our tests
          // and not overlook it.
          TRI_ASSERT(false);

          // if an exception escapes from here, the insert will not have happened.
          // it is safe to retry it next time.
          throw;
        }
    
        TRI_IF_FAILURE("TransactionChaos::randomSleep") {
          std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(5))));
        }

        // if the insert succeeded, we remove it from the list of operations, 
        // so it won't be reapplied even if subsequent operations fail.
        insertIt = _revisionInsertBuffers.erase(insertIt);
      }

      // check for removals
      if (applyRemovals) {
        TRI_ASSERT(!applyInserts);
        
        TRI_ASSERT(removeIt->first > _revisionTreeApplied);
        
        TRI_IF_FAILURE("RevisionTree::applyRemoves") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        // apply removals, without holding the lock
        // if this throws we will not have modified _revisionRemovalBuffers
        try {
          _revisionTree->remove(removeIt->second);
        } catch (std::exception const& ex) {
          // this should never fail, anyway log...
          LOG_TOPIC("a5ba8", ERR, Logger::ENGINES)
              << "unable to apply revision tree removals for " 
              << _logicalCollection.vocbase().name() << "/" << _logicalCollection.name() 
              << ": " << ex.what();
          // it is pretty bad if this fails in real life, but we would trigger this assertion 
          // during failure testing as well, so we cannot enable it
          // TRI_ASSERT(false);
          
          // if an exception escapes from here, the same remove will be retried next time.
          throw;
        }
    
        TRI_IF_FAILURE("TransactionChaos::randomSleep") {
          std::this_thread::sleep_for(std::chrono::milliseconds(RandomGenerator::interval(uint32_t(5))));
        }

        // if the remove succeeded, we remove it from the list of operations, 
        // so it won't be reapplied even if subsequent operations fail.
        removeIt = _revisionRemovalBuffers.erase(removeIt);
      }
    }
  });

  // if memory usage changed, track the adjustment
  auto newMemoryUsage = _revisionTree->memoryUsage();
  if (oldMemoryUsage != newMemoryUsage) {
    auto& engine =
        _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    if (oldMemoryUsage > newMemoryUsage) {
      engine.trackRevisionTreeMemoryDecrease(oldMemoryUsage - newMemoryUsage);
    } else {
      engine.trackRevisionTreeMemoryIncrease(newMemoryUsage - oldMemoryUsage);
    }
  }

  if (res.fail()) {
    LOG_TOPIC("fdfa7", ERR, Logger::ENGINES)
        << "unable to apply revision tree updates for " 
        << _logicalCollection.vocbase().name() << "/" << _logicalCollection.name() 
        << ": " << res.errorMessage();
    THROW_ARANGO_EXCEPTION(res);
  }
  
  TRI_IF_FAILURE("applyUpdates::forceHibernation2") {
    if (_revisionTree != nullptr) {
      _revisionTree->hibernate(/*force*/ true);
    }
  }
}

Result RocksDBMetaCollection::applyUpdatesForTransaction(containers::RevisionTree& tree,
                                                         rocksdb::SequenceNumber commitSeq,
                                                         std::unique_lock<std::mutex>& lock) const {
  TRI_ASSERT(lock.owns_lock());
  TRI_ASSERT(_logicalCollection.useSyncByRevision());
  
  return basics::catchVoidToResult([&]() -> void {
    auto insertIt = _revisionInsertBuffers.begin();
    auto removeIt = _revisionRemovalBuffers.begin();

    {
      rocksdb::SequenceNumber ignoreSeq = 0;  // truncate will increase this sequence
      bool foundTruncate = false;
      // check for a truncate marker
      auto it = _revisionTruncateBuffer.begin();  // sorted ASC
      while (it != _revisionTruncateBuffer.end() && *it <= commitSeq) {
        ignoreSeq = *it;
        TRI_ASSERT(ignoreSeq != 0);
        foundTruncate = true;
        ++it;
      }
      if (foundTruncate) {
        TRI_ASSERT(ignoreSeq <= commitSeq);
        while (insertIt != _revisionInsertBuffers.end() && insertIt->first <= ignoreSeq) {
          ++insertIt;
        }
        while (removeIt != _revisionRemovalBuffers.end() && removeIt->first <= ignoreSeq) {
          ++removeIt;
        }

        tree.clear();  // clear estimates
      }
    }

    while (true) {
      std::vector<std::uint64_t> const* inserts = nullptr;
      std::vector<std::uint64_t> const* removals = nullptr;

      // find out if we have buffers to apply
      {
        bool haveInserts = insertIt != _revisionInsertBuffers.end() &&
                           insertIt->first <= commitSeq;
        bool haveRemovals = removeIt != _revisionRemovalBuffers.end() &&
                            removeIt->first <= commitSeq;

        bool applyInserts =
            haveInserts && (!haveRemovals || removeIt->first >= insertIt->first);
        bool applyRemovals = haveRemovals && !applyInserts;

        // check for inserts
        if (applyInserts) {
          inserts = &insertIt->second;
          ++insertIt;
        }
        // check for removals
        if (applyRemovals) {
          removals = &removeIt->second;
          ++removeIt;
        }
      }

      // no inserts or removals left to apply, drop out of loop
      if (!inserts && !removals) {
        break;
      }

      TRI_ASSERT(inserts == nullptr || removals == nullptr);

      // apply inserts
      if (inserts) {
        tree.insert(*inserts);
      }

      // apply removals
      if (removals) {
        tree.remove(*removals);
      }
    } 
  });
}

/// @brief lock a collection, with a timeout
ErrorCode RocksDBMetaCollection::doLock(double timeout, AccessMode::Type mode) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;

  // user read operations don't require any lock in RocksDB, so we won't get here.
  // user write operations will acquire the R/W lock in read mode, and
  // user exclusive operations will acquire the R/W lock in write mode.
  TRI_ASSERT(mode == AccessMode::Type::READ || mode == AccessMode::Type::WRITE);
  
  while (true) {
    bool gotLock = false;
    if (mode == AccessMode::Type::WRITE) {
      gotLock = _exclusiveLock.tryLockWrite();
    } else if (mode == AccessMode::Type::READ) {
      gotLock = _exclusiveLock.tryLockRead();
    } else {
      // we should never get here
      TRI_ASSERT(false);
      return TRI_ERROR_INTERNAL;
    }

    if (gotLock) {
      // keep the lock and exit the loop
      return TRI_ERROR_NO_ERROR;
    }

    double now = TRI_microtime();
    if (waitTime == 0) {  // initialize times
      // set end time for lock waiting
      if (timeout <= 0.0) {
        timeout = defaultLockTimeout;
      }
      
      startTime = now;
      waitTime = 1;
    } else {
      TRI_ASSERT(startTime > 0.0);
    
      if (now > startTime + timeout) {
        LOG_TOPIC("d1e52", TRACE, arangodb::Logger::ENGINES)
          << "timed out after " << timeout << " s waiting for " 
          << AccessMode::typeString(mode) << " lock on collection '"
          << _logicalCollection.name() << "'";
      
        return TRI_ERROR_LOCK_TIMEOUT;
      }
    }
    
    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(waitTime));

      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

bool RocksDBMetaCollection::haveBufferedOperations(std::unique_lock<std::mutex> const& lock) const {
  TRI_ASSERT(lock.owns_lock());
  TRI_ASSERT(_logicalCollection.useSyncByRevision());

  return (!_revisionTruncateBuffer.empty() || !_revisionInsertBuffers.empty() || !_revisionRemovalBuffers.empty());
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::allocateEmptyRevisionTree(std::size_t depth) const {
  // determine the minimum revision id for the collection.
  // for smart edge collection children collections, the _rev values are determined
  // via the agency's uniqid() function. they can be anything, so we need to assume
  // 0 as the lower bound.
  // for other collections, it is mostly safe to assume that there will be no 
  // data inserted with non-recent revisions. So a minRevision id that is relatively
  // recent (HLC value from January 2021) will mostly work for these cases. There are
  // some exceptions (e.g. some system collections are never dropped by the replication
  // on the follower but only truncated, which may lead to collections on the follower 
  // being "older" than on the leader, plus because of DC2DC, which may insert 
  // arbitrary data into a collection). For these special cases no current minRevision
  // value would do, but we would like to avoid using a value very much in the past.
  // thus we still go with a HLC from January 2021 and in addition allow the 
  // minRevision to decrease in the revision trees.
  RevisionId minRevision = _logicalCollection.isSmartChild() 
                            ? RevisionId::none()
                            : RevisionId::lowerBound();
  return std::make_unique<containers::RevisionTree>(depth, minRevision.id());
}

void RocksDBMetaCollection::ensureRevisionTree() {
  // should have _revisionTreeLock held outside
  if (_revisionTree == nullptr) {
    auto& engine =
        _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();

    auto newTree = allocateEmptyRevisionTree(revisionTreeDepth);
    TRI_ASSERT(newTree->depth() == revisionTreeDepth);

    _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(newTree), _logicalCollection);
    _revisionTreeCreationSeq = engine.db()->GetLatestSequenceNumber();
    _revisionTreeSerializedSeq = _revisionTreeCreationSeq;
    _revisionTreeCanBeSerialized = true;

    LOG_TOPIC("7630e", TRACE, Logger::ENGINES)
        << "created revision tree for " << _logicalCollection.name()
        << ", sequence number: " << _revisionTreeCreationSeq;
  }

  TRI_ASSERT(_revisionTree != nullptr);
  TRI_ASSERT(_revisionTree->depth() == revisionTreeDepth);
}

/// @brief construct from revision tree
RocksDBMetaCollection::RevisionTreeAccessor::RevisionTreeAccessor(std::unique_ptr<containers::RevisionTree> tree,
                                                                  LogicalCollection const& collection) 
    : _tree(std::move(tree)),
      _logicalCollection(collection),
      _depth(_tree->depth()),
      _hibernationRequests(0),
      _compressible(true) {
  TRI_ASSERT(_depth == revisionTreeDepth);
  TRI_ASSERT(_tree != nullptr);
        
  auto& engine = _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  engine.trackRevisionTreeMemoryIncrease(_tree->memoryUsage());
}

RocksDBMetaCollection::RevisionTreeAccessor::~RevisionTreeAccessor() {
  TRI_ASSERT((_tree == nullptr && !_compressed.empty()) || (_tree != nullptr && _compressed.empty()));

  auto& engine = _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  if (_tree != nullptr) {
    engine.trackRevisionTreeMemoryDecrease(_tree->memoryUsage());
  } else if (!_compressed.empty()) {
    engine.trackRevisionTreeMemoryDecrease(_compressed.size());
  }
}

void RocksDBMetaCollection::RevisionTreeAccessor::insert(std::vector<std::uint64_t> const& keys) {
  ensureTree();
  _tree->insert(keys);
}

void RocksDBMetaCollection::RevisionTreeAccessor::remove(std::vector<std::uint64_t> const& keys) {
  ensureTree();
  _tree->remove(keys);
}

void RocksDBMetaCollection::RevisionTreeAccessor::clear() {
  ensureTree();
 
  // do not track memory usage here. the caller has to do this
  _tree->clear();
  TRI_ASSERT(_tree->memoryUsage() == 0);
  _compressed.clear();
  _compressible = true;
  
  TRI_ASSERT(_tree != nullptr && _compressed.empty());
}
    
std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::RevisionTreeAccessor::clone() const {
  ensureTree();
  return _tree->clone();
}

std::uint64_t RocksDBMetaCollection::RevisionTreeAccessor::compressedSize() const {
  if (_tree == nullptr) {
    TRI_ASSERT(!_compressed.empty());
    return _compressed.size();
  }
  std::string output;
  _tree->serializeBinary(output, arangodb::containers::MerkleTreeBase::BinaryFormat::Optimal);
  return output.size();
}

std::uint64_t RocksDBMetaCollection::RevisionTreeAccessor::count() const {
  ensureTree();
  return _tree->count();
}

std::uint64_t RocksDBMetaCollection::RevisionTreeAccessor::rootValue() const {
  ensureTree();
  return _tree->rootValue();
}

std::uint64_t RocksDBMetaCollection::RevisionTreeAccessor::depth() const {
  return _depth;
}

std::size_t RocksDBMetaCollection::RevisionTreeAccessor::memoryUsage() const {
  ensureTree();
  return _tree->memoryUsage();
}

void RocksDBMetaCollection::RevisionTreeAccessor::checkConsistency() const {
  ensureTree();
  return _tree->checkConsistency();
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void RocksDBMetaCollection::RevisionTreeAccessor::corrupt(uint64_t count, uint64_t hash) {
  ensureTree();

  // track memory usage differences
  auto oldMemoryUsage = _tree->memoryUsage();
  // corrupt will _insert_ intentionally broken data into the tree. it does not _remove_ data 
  _tree->corrupt(count, hash);
  
  RocksDBEngine& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  engine.trackRevisionTreeMemoryIncrease(_tree->memoryUsage() - oldMemoryUsage);
}
#endif

void RocksDBMetaCollection::RevisionTreeAccessor::hibernate(bool force) {
  TRI_ASSERT((_tree == nullptr && !_compressed.empty()) || (_tree != nullptr && _compressed.empty()));

  if (_tree == nullptr) {
    // already compressed, nothing to do
    TRI_ASSERT(!_compressed.empty());
    return;
  }
  
  TRI_ASSERT(_compressed.empty());

  std::uint64_t count = _tree->count();
  auto oldMemoryUsage = _tree->memoryUsage();

  if (!force) {
    if (count >= 5'000'000) {
      // we have so many values in the tree that compressibility
      // will likely be bad
      return;
    }
    
    if (count >= 1'000'000 && !_compressible) {
      // for whatever reason this collection is not well compressible
      return;
    }
  
    if (oldMemoryUsage <= 64) {
      // tree uses so little memory that it won't be worth to compress it
      return;
    }
    
    if (++_hibernationRequests < 10) {
      // sit out the first few hibernation requests before we
      // actually work (10 is just an arbitrary value here to avoid
      // some pathologic hibernation/resurrection cycles, e.g. by
      // the statistics collections)
      return;
    }
  }

  double start = TRI_microtime();
 
  _compressed.clear();
  _tree->serializeBinary(_compressed, arangodb::containers::MerkleTreeBase::BinaryFormat::Optimal);

  TRI_ASSERT(!_compressed.empty());
  
  LOG_TOPIC("45eae", DEBUG, Logger::REPLICATION) 
      << "hibernating revision tree for "
      << _logicalCollection.vocbase().name() << "/" << _logicalCollection.name()
      << " with " << count << " entries and size " 
      << _tree->byteSize() << ", hibernated size: " << _compressed.size() 
      << ", hibernation took: " << (TRI_microtime() - start);;

  // we would like to see at least 50% compressibility
  if (_compressed.size() * 2 < _tree->byteSize()) {
    // compression ratio ok.
    // remove tree from memory. now we only have _compressed
    RocksDBEngine& engine =
        _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    engine.trackRevisionTreeHibernation();
    engine.trackRevisionTreeMemoryIncrease(_compressed.size());
    engine.trackRevisionTreeMemoryDecrease(oldMemoryUsage);
    _tree.reset();
    _compressible = true;
    TRI_ASSERT(_tree == nullptr && !_compressed.empty());
  } else {
    // otherwise we'll keep the uncompressed tree, and will
    // not try compressing again soon
    _compressed = std::string();
    _compressible = false;
    TRI_ASSERT(_tree != nullptr && _compressed.empty());
  }
}

void RocksDBMetaCollection::RevisionTreeAccessor::serializeBinary(std::string& output) const {
  if (_tree != nullptr) {
    // compress tree into output
    _tree->serializeBinary(output, arangodb::containers::MerkleTreeBase::BinaryFormat::Optimal);
  } else {
    // append our already compressed state
    output.append(_compressed);
  }
}

// unfortunately we need to declare this method const although it can
// modify internal (mutable) state
void RocksDBMetaCollection::RevisionTreeAccessor::ensureTree() const {
  TRI_ASSERT((_tree == nullptr && !_compressed.empty()) || (_tree != nullptr && _compressed.empty()));

  if (_tree == nullptr) {
    // build tree from compressed state
    TRI_ASSERT(!_compressed.empty());

    double start = TRI_microtime();

    _tree = containers::RevisionTree::fromBuffer(std::string_view(_compressed));

    if (_tree == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to uncompress tree");
    }
  
    RocksDBEngine& engine =
        _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
    engine.trackRevisionTreeResurrection();
    engine.trackRevisionTreeMemoryIncrease(_tree->memoryUsage());
    engine.trackRevisionTreeMemoryDecrease(_compressed.size());

    // reset hibernation counter
    _hibernationRequests = 0;

    TRI_ASSERT(_tree->depth() == _depth);
    
    LOG_TOPIC("e9c29", DEBUG, Logger::REPLICATION) 
        << "resurrected revision tree for " 
        << _logicalCollection.vocbase().name() << "/" << _logicalCollection.name()
        << " with " << _tree->count() << " entries. resurrection took: " 
        << (TRI_microtime() - start);

    // clear the compressed state and free the associated memory
    _compressed = std::string();
  }
  TRI_ASSERT(_tree != nullptr);
  TRI_ASSERT(_compressed.empty());
}
