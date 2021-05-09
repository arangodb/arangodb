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

  TRI_ASSERT(_logicalCollection.isAStub() || _objectId.load() != 0);
  collection.vocbase()
      .server()
      .getFeature<EngineSelectorFeature>()
      .engine<RocksDBEngine>()
      .addCollectionMapping(_objectId.load(), _logicalCollection.vocbase().id(),
                            _logicalCollection.id());
}

RocksDBMetaCollection::RocksDBMetaCollection(LogicalCollection& collection,
                                             PhysicalCollection const* physical)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
      _objectId(static_cast<RocksDBMetaCollection const*>(physical)->_objectId.load()),
      _revisionTreeApplied(0),
      _revisionTreeCreationSeq(0),
      _revisionTreeSerializedSeq(0),
      _revisionTreeSerializedTime(std::chrono::steady_clock::now()) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  collection.vocbase()
      .server()
      .getFeature<EngineSelectorFeature>()
      .engine<RocksDBEngine>()
      .addCollectionMapping(_objectId.load(), _logicalCollection.vocbase().id(),
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
void RocksDBMetaCollection::unlockWrite() { _exclusiveLock.unlockWrite(); }

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
  auto useGuard = scopeGuard([&] {
    // cppcheck-suppress knownConditionTrueFalse
    if (snapshot) {
      db->ReleaseSnapshot(snapshot);
    }
    vocbase.release();
  });

  // makes sure collection doesn't get unloaded
  CollectionGuard collGuard(&vocbase, _logicalCollection.id());

  TransactionId trxId{0};
  auto blockerGuard = scopeGuard([&] {  // remove blocker afterwards
    if (trxId.isSet()) {
      _meta.removeBlocker(trxId);
    }
  });

  uint64_t snapNumberOfDocuments = 0;
  {
    // fetch number docs and snapshot under exclusive lock
    // this should enable us to correct the count later
    auto lockGuard = scopeGuard([this] { unlockWrite(); });
    auto res = lockWrite(transaction::Options::defaultLockTimeout);
    if (res != TRI_ERROR_NO_ERROR) {
      lockGuard.cancel();
      THROW_ARANGO_EXCEPTION(res);
    }
 
    // generate a unique transaction id for a blocker
    trxId = TransactionId(transaction::Context::makeTransactionId());

    // place a blocker. will be removed by blockerGuard automatically
    _meta.placeBlocker(trxId, engine.db()->GetLatestSequenceNumber());

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
  if (adjustment != 0) {
    LOG_TOPIC("ad613", WARN, Logger::REPLICATION)
      << "inconsistent collection count detected for "
      << vocbase.name() << "/" << _logicalCollection.name()
      << ": counted value: " << count << ", snapshot value: " << snapNumberOfDocuments 
      << ", current value: " << _meta.numberDocuments()
      << ",  an offet of " << adjustment << " will be applied";
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
  TRI_ASSERT(tree->maxDepth() == revisionTreeDepth);
  
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(tree));
  _revisionTreeApplied.store(seq);
  _revisionTreeCreationSeq = seq;
  _revisionTreeSerializedSeq = seq;
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(
    std::function<std::unique_ptr<containers::RevisionTree>(std::unique_ptr<containers::RevisionTree>)> const& callback) {
  if (!_logicalCollection.useSyncByRevision()) {
    return nullptr;
  }

  RocksDBEngine& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  rocksdb::DB* db = engine.db()->GetRootDB();

  // first apply any updates that can be safely applied
  rocksdb::SequenceNumber safeSeq = meta().committableSeq(db->GetLatestSequenceNumber());

  std::unique_lock<std::mutex> guard(_revisionTreeLock);

  if (!_revisionTree && !haveBufferedOperations()) {
    // we only need to return an empty tree here.
    // TODO: check if we can get away with returning a very small tree here
    return allocateEmptyRevisionTree(2); 
  }

  applyUpdates(safeSeq);
  TRI_ASSERT(_revisionTree != nullptr);
  TRI_ASSERT(_revisionTree->maxDepth() == revisionTreeDepth);

  // now clone the tree so we can apply all updates consistent with our ongoing trx
  auto tree = _revisionTree->clone();
  if (!tree) {
    return nullptr;
  }

  return callback(std::move(tree));
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(transaction::Methods& trx) {
  return revisionTree([this, &trx](std::unique_ptr<containers::RevisionTree> tree) -> std::unique_ptr<containers::RevisionTree> {
    // apply any which are buffered and older than our ongoing transaction start
    rocksdb::SequenceNumber trxSeq = RocksDBTransactionState::toState(&trx)->beginSeq();
    TRI_ASSERT(trxSeq != 0);
    Result res = applyUpdatesForTransaction(*tree, trxSeq);
    if (res.fail()) {
      return nullptr;
    }

    // now peek at updates buffered inside transaction and apply those too
    auto operations = RocksDBTransactionState::toState(&trx)->trackedOperations(
        _logicalCollection.id());
        
    tree->insert(operations.inserts);
    tree->remove(operations.removals);

    return tree;
  });
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(uint64_t batchId) {
  return revisionTree([this, batchId](std::unique_ptr<containers::RevisionTree> tree) -> std::unique_ptr<containers::RevisionTree> {
    RocksDBEngine& engine =
        _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();

    // apply any which are buffered and older than our ongoing transaction start
    RocksDBReplicationManager* manager = engine.replicationManager();
    RocksDBReplicationContext* ctx = batchId == 0 ? nullptr : manager->find(batchId);
    if (!ctx) {
      return nullptr;
    }
    auto guard = scopeGuard([manager, ctx]() -> void { manager->release(ctx); });
    rocksdb::SequenceNumber trxSeq = ctx->snapshotTick();
    TRI_ASSERT(trxSeq != 0);
    Result res = applyUpdatesForTransaction(*tree, trxSeq);
    if (res.fail()) {
      return nullptr;
    }

    return tree;
  });
}

bool RocksDBMetaCollection::needToPersistRevisionTree(rocksdb::SequenceNumber maxCommitSeq) const {
  if (!_logicalCollection.useSyncByRevision()) {
    return maxCommitSeq > _revisionTreeApplied.load();
  }

  std::unique_lock<std::mutex> guard(_revisionBufferLock);

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

  // have applied updates that we haven't persisted
  if (_revisionTreeSerializedSeq < _revisionTreeApplied.load()) {
    return true;
  }

  // tree has never been persisted
  if (_revisionTreeSerializedSeq <= _revisionTreeCreationSeq) {
    return true;
  }

  return false;
}

rocksdb::SequenceNumber RocksDBMetaCollection::lastSerializedRevisionTree(rocksdb::SequenceNumber maxCommitSeq) {
  // first update so we don't under-report
  std::unique_lock<std::mutex> guard(_revisionBufferLock);
  rocksdb::SequenceNumber seq = maxCommitSeq;

  // limit to before any pending buffered updates

  if (!_revisionTruncateBuffer.empty() && *_revisionTruncateBuffer.begin() - 1 < seq) {
    seq = *_revisionTruncateBuffer.begin() - 1;
  }

  if (!_revisionInsertBuffers.empty() && _revisionInsertBuffers.begin()->first - 1 < seq) {
    seq = _revisionInsertBuffers.begin()->first - 1;
  }

  if (!_revisionRemovalBuffers.empty() && _revisionRemovalBuffers.begin()->first - 1 <= seq) {
    seq = _revisionRemovalBuffers.begin()->first - 1;
  }

  // limit to before the last thing we applied, since we haven't persisted it
  rocksdb::SequenceNumber applied = _revisionTreeApplied.load();
  if (applied > _revisionTreeSerializedSeq && applied - 1 < seq) {
    seq = applied - 1;
  }

  // now actually advance it if we can
  if (seq > _revisionTreeSerializedSeq) {
    _revisionTreeSerializedSeq = seq;
  }

  return _revisionTreeSerializedSeq;
}

rocksdb::SequenceNumber RocksDBMetaCollection::serializeRevisionTree(
    std::string& output, rocksdb::SequenceNumber commitSeq, bool force) {
  std::unique_lock<std::mutex> guard(_revisionTreeLock);

  if (_logicalCollection.useSyncByRevision()) {
    if (!_revisionTree && !haveBufferedOperations()) {  // empty collection
      return commitSeq;
    }
    applyUpdates(commitSeq);  // always apply updates...
 
    // applyUpdates will make sure we will have a valid tree
    TRI_ASSERT(_revisionTree != nullptr);
    TRI_ASSERT(_revisionTree->maxDepth() == revisionTreeDepth);

    bool neverDone = _revisionTreeSerializedSeq == _revisionTreeCreationSeq;
    bool coinFlip = RandomGenerator::interval(static_cast<uint32_t>(5)) == 0;
    bool beenTooLong = 30 < std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - _revisionTreeSerializedTime)
                                .count();
    TRI_IF_FAILURE("RocksDBMetaCollection::serializeRevisionTree") {
      return _revisionTreeSerializedSeq;
    }
    if (force || neverDone || coinFlip || beenTooLong) {  // ...but only write the tree out sometimes
      _revisionTree->serializeBinary(output);
      _revisionTreeSerializedSeq = commitSeq;
      _revisionTreeSerializedTime = std::chrono::steady_clock::now();
    }
    return _revisionTreeSerializedSeq;
  }
  // if we get here, we aren't using the trees;
  // mark as don't persist again, tree should be deleted now
  _revisionTreeApplied.store(std::numeric_limits<rocksdb::SequenceNumber>::max());
  return commitSeq;
}

Result RocksDBMetaCollection::rebuildRevisionTree() {
  return basics::catchToResult([this]() -> Result {
    std::unique_lock<std::mutex> guard(_revisionTreeLock);

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
    
    std::vector<std::uint64_t> revisions;
    revisions.resize(1024);
    
    auto newTree = allocateEmptyRevisionTree(revisionTreeDepth);
    
    while (it.hasMore()) {
      revisions.emplace_back(it.revision().id());
      if (revisions.size() >= 4096) {  // arbitrary batch size
        newTree->insert(revisions);
        revisions.clear();
      }
      it.next();
    }
    if (!revisions.empty()) {
      newTree->insert(revisions);
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    newTree->checkConsistency();
#endif

    _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(newTree));
    _revisionTreeApplied = state->beginSeq();
    _revisionTreeCreationSeq = state->beginSeq();
    _revisionTreeSerializedSeq = state->beginSeq();
    return Result();
  });
}

void RocksDBMetaCollection::rebuildRevisionTree(std::unique_ptr<rocksdb::Iterator>& iter) {
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  
  auto newTree = allocateEmptyRevisionTree(revisionTreeDepth);

  // okay, we are in recovery and can't open a transaction, so we need to
  // read the raw RocksDB data; on the plus side, we are in recovery, so we
  // are single-threaded and don't need to worry about transactions anyway

  RocksDBKeyBounds documentBounds =
      RocksDBKeyBounds::CollectionDocuments(_objectId.load());
  rocksdb::Comparator const* cmp =
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
          ->GetComparator();
  rocksdb::ReadOptions ro;
  rocksdb::Slice const end = documentBounds.end();
  ro.iterate_upper_bound = &end;
  ro.fill_cache = false;

  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  auto* db = engine.db();
  
  std::vector<std::uint64_t> revisions;
  revisions.reserve(1024);
  
  for (iter->Seek(documentBounds.start());
       iter->Valid() && cmp->Compare(iter->key(), end) < 0; iter->Next()) {
    LocalDocumentId const docId = RocksDBKey::documentId(iter->key());
    revisions.emplace_back(docId.id());
    if (revisions.size() >= 4096) {  // arbitrary batch size
      newTree->insert(revisions);
      revisions.clear();
    }
  }
  if (!revisions.empty()) {
    newTree->insert(revisions);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    newTree->checkConsistency();
#endif

  rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();
  _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(newTree));
  _revisionTreeApplied.store(seq);
  _revisionTreeCreationSeq = seq;
  _revisionTreeSerializedSeq = seq;
}

void RocksDBMetaCollection::revisionTreeSummary(VPackBuilder& builder) {
  if (!_logicalCollection.useSyncByRevision()) {
    return;
  }

  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  if (_revisionTree) {
    VPackObjectBuilder obj(&builder);
    obj->add(StaticStrings::RevisionTreeCount, VPackValue(_revisionTree->count()));
    obj->add(StaticStrings::RevisionTreeHash, VPackValue(_revisionTree->rootValue()));
  }
}

void RocksDBMetaCollection::placeRevisionTreeBlocker(TransactionId transactionId) {
  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  rocksdb::SequenceNumber preSeq = db->GetLatestSequenceNumber();
  _meta.placeBlocker(transactionId, preSeq);
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

  if (_revisionTreeApplied.load() > seq) {
    TRI_ASSERT(_logicalCollection.vocbase()
                   .server()
                   .getFeature<EngineSelectorFeature>()
                   .engine()
                   .inRecovery());
    return;
  }


  TRI_ASSERT(!inserts.empty() || !removals.empty());

  std::unique_lock<std::mutex> guard(_revisionBufferLock);
  if (!inserts.empty()) {
    _revisionInsertBuffers.emplace(seq, std::move(inserts));
  }
  if (!removals.empty()) {
    _revisionRemovalBuffers.emplace(seq, std::move(removals));
  }
}

Result RocksDBMetaCollection::bufferTruncate(rocksdb::SequenceNumber seq) {
  if (!_logicalCollection.useSyncByRevision()) {
    return Result();
  }

  Result res = basics::catchVoidToResult([&]() -> void {
    if (_revisionTreeApplied.load() > seq) {
      return;
    }
    std::unique_lock<std::mutex> guard(_revisionBufferLock);
    _revisionTruncateBuffer.emplace(seq);
  });
  return res;
}

void RocksDBMetaCollection::hibernateRevisionTree() {
  std::unique_lock<std::mutex> guard(_revisionTreeLock);

  if (_revisionTree && !haveBufferedOperations()) {
    _revisionTree->hibernate();
  }
}

void RocksDBMetaCollection::applyUpdates(rocksdb::SequenceNumber commitSeq) {
  // should have _revisionTreeLock held outside
    
  TRI_ASSERT(_logicalCollection.useSyncByRevision());
  TRI_ASSERT(_revisionTree || haveBufferedOperations());

  // make sure we will have a _revisionTree ready after this
  ensureRevisionTree();
  TRI_ASSERT(_revisionTree != nullptr);
  TRI_ASSERT(_revisionTree->maxDepth() == revisionTreeDepth);

  Result res = basics::catchVoidToResult([&]() -> void {
    std::unique_lock<std::mutex> guard(_revisionBufferLock);

    auto insertIt = _revisionInsertBuffers.begin();
    auto removeIt = _revisionRemovalBuffers.begin();

    auto it = _revisionTruncateBuffer.begin();  // sorted ASC
    
    {
      // check for a truncate marker
      rocksdb::SequenceNumber ignoreSeq = 0;  // truncate will increase this sequence
      bool foundTruncate = false;

      while (it != _revisionTruncateBuffer.end() && *it <= commitSeq) {
        ignoreSeq = *it;
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
      
        TRI_ASSERT(insertIt == _revisionInsertBuffers.begin() || insertIt == _revisionInsertBuffers.end());
        TRI_ASSERT(removeIt == _revisionRemovalBuffers.begin() || removeIt == _revisionRemovalBuffers.end());

        // we can clear the revision tree without holding the mutex here
        guard.unlock();
        // clear out any revision structure, now empty
        _revisionTree->clear();

        guard.lock();
      }
    }

    // still holding the mutex here

    while (true) {
      // find out if we still have buffers to apply
      TRI_ASSERT(insertIt == _revisionInsertBuffers.begin() || insertIt == _revisionInsertBuffers.end());
      TRI_ASSERT(removeIt == _revisionRemovalBuffers.begin() || removeIt == _revisionRemovalBuffers.end());

      bool haveInserts = insertIt != _revisionInsertBuffers.end() &&
                         insertIt->first <= commitSeq;
      bool haveRemovals = removeIt != _revisionRemovalBuffers.end() &&
                          removeIt->first <= commitSeq;

      bool applyInserts =
            haveInserts && (!haveRemovals || removeIt->first >= insertIt->first);
      bool applyRemovals = haveRemovals && !applyInserts;
  
      // no inserts or removals left to apply, drop out of loop
      if (!applyInserts && !applyRemovals) {
        break;
      }
     
      // another concurrent thread may insert new elements into _revisionInsertBuffers or 
      // _revisionRemovalBuffers while we are here.
      // it is safe for us to access the elements pointed to by insertIt and removeIt here 
      // without holding the lock on these containers though, because of std::multimap's
      // iterator invalidation rules:
      // - emplace / insert: No iterators or references are invalidated.
      // - erase: References and iterators to the erased elements are invalidated. Other references and iterators are not affected.
      
      // check for inserts
      if (applyInserts) {
        TRI_ASSERT(!applyRemovals);
      
        // release the mutex while we modify the tree. this is safe (see above)
        guard.unlock();

        // apply inserts, without holding the lock
        // if this throws we will not have modified _revisionInsertBuffers
        _revisionTree->insert(insertIt->second);

        // move iterator forward, we need the mutex for this
        guard.lock();
        insertIt = _revisionInsertBuffers.erase(insertIt);
      }

      // check for removals
      if (applyRemovals) {
        TRI_ASSERT(!applyInserts);
        
        // release the mutex while we modify the tree. this is safe (see above)
        guard.unlock();

        // apply removals, without holding the lock
        // if this throws we will not have modified _revisionRemovalBuffers
        _revisionTree->remove(removeIt->second);

        // move iterator forward, we need the mutex for this
        guard.lock();
        removeIt = _revisionRemovalBuffers.erase(removeIt);
      }
    }
  });

  if (res.ok()) {
    rocksdb::SequenceNumber applied = _revisionTreeApplied.load();
    while (commitSeq > applied) {
      _revisionTreeApplied.compare_exchange_strong(applied, commitSeq);
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // TODO: remove this cherk because it is very expensive
    _revisionTree->checkConsistency();
#endif
  } else {
    LOG_TOPIC("fdfa7", WARN, Logger::ENGINES)
        << "unable to apply updates: " << res.errorMessage();
  }
}

Result RocksDBMetaCollection::applyUpdatesForTransaction(containers::RevisionTree& tree,
                                                         rocksdb::SequenceNumber commitSeq) const {
  if (!_logicalCollection.useSyncByRevision()) {
    return Result();
  }

  Result res = basics::catchVoidToResult([&]() -> void {
    decltype(_revisionInsertBuffers)::const_iterator insertIt;
    decltype(_revisionRemovalBuffers)::const_iterator removeIt;
    {
      std::unique_lock<std::mutex> guard(_revisionBufferLock);
      insertIt = _revisionInsertBuffers.begin();
      removeIt = _revisionRemovalBuffers.begin();
    }

    {
      rocksdb::SequenceNumber ignoreSeq = 0;  // truncate will increase this sequence
      bool foundTruncate = false;
      // check for a truncate marker
      std::unique_lock<std::mutex> guard(_revisionBufferLock);
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
          std::unique_lock<std::mutex> guard(_revisionBufferLock);
          inserts = &insertIt->second;
          ++insertIt;
        }
        // check for removals
        if (applyRemovals) {
          std::unique_lock<std::mutex> guard(_revisionBufferLock);
          removals = &removeIt->second;
          ++removeIt;
        }
      }

      // no inserts or removals left to apply, drop out of loop
      if (!inserts && !removals) {
        break;
      }

      // apply inserts
      if (inserts) {
        tree.insert(*inserts);
      }

      // apply removals
      if (removals) {
        tree.remove(*removals);
      }
    }  // </while(true)>
  });
  return res;
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

bool RocksDBMetaCollection::haveBufferedOperations() const {
  TRI_ASSERT(_logicalCollection.useSyncByRevision());

  std::unique_lock<std::mutex> guard(_revisionBufferLock);

  // have a truncate to apply
  if (!_revisionTruncateBuffer.empty()) {
    return true;
  }

  // have insertions to apply
  if (!_revisionInsertBuffers.empty()) {
    return true;
  }

  // have removals to apply
  if (!_revisionRemovalBuffers.empty()) {
    return true;
  }

  return false;
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::allocateEmptyRevisionTree(std::size_t depth) const {
  // should have _revisionTreeLock held outside
  return std::make_unique<containers::RevisionTree>(depth, _logicalCollection.minRevision().id());
}

void RocksDBMetaCollection::ensureRevisionTree() {
  // should have _revisionTreeLock held outside
  if (_revisionTree == nullptr) {
    auto& selector =
        _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
    auto& engine = selector.engine<RocksDBEngine>();

    auto newTree = allocateEmptyRevisionTree(revisionTreeDepth);
    TRI_ASSERT(newTree->maxDepth() == revisionTreeDepth);

    _revisionTree = std::make_unique<RevisionTreeAccessor>(std::move(newTree));
    _revisionTreeCreationSeq = engine.db()->GetLatestSequenceNumber();
    _revisionTreeSerializedSeq = _revisionTreeCreationSeq;
  }

  TRI_ASSERT(_revisionTree != nullptr);
  TRI_ASSERT(_revisionTree->maxDepth() == revisionTreeDepth);
}

/// @brief construct from revision tree
RocksDBMetaCollection::RevisionTreeAccessor::RevisionTreeAccessor(std::unique_ptr<containers::RevisionTree> tree) 
    : _tree(std::move(tree)),
      _maxDepth(_tree->maxDepth()),
      _compressible(true) {
  TRI_ASSERT(_maxDepth == revisionTreeDepth);
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
  _tree->clear();
  _compressible = true;
}
    
std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::RevisionTreeAccessor::clone() const {
  ensureTree();
  return _tree->clone();
}

std::uint64_t RocksDBMetaCollection::RevisionTreeAccessor::count() const {
  ensureTree();
  return _tree->count();
}

std::uint64_t RocksDBMetaCollection::RevisionTreeAccessor::rootValue() const {
  ensureTree();
  return _tree->rootValue();
}

std::uint64_t RocksDBMetaCollection::RevisionTreeAccessor::maxDepth() const {
  return _maxDepth;
}

void RocksDBMetaCollection::RevisionTreeAccessor::checkConsistency() const {
  ensureTree();
  return _tree->checkConsistency();
}

void RocksDBMetaCollection::RevisionTreeAccessor::hibernate() {
  if (_tree == nullptr) {
    // already compressed, nothing to do
    TRI_ASSERT(!_compressed.empty());
    return;
  }

  std::uint64_t count = _tree->count();

  if (count >= 5'000'000) {
    // we have so many values in the tree that compressibility
    // will likely be bad
    return;
  }
  
  if (count >= 1'000'000 && !_compressible) {
    // for whatever reason this collection is not well compressible
    return;
  }
  
  _compressed.clear();
  _tree->serializeBinary(_compressed, true);
  TRI_ASSERT(!_compressed.empty());
  
  if (_compressed.size() * 2 < _tree->byteSize()) {
    // we would like to see at least 50% compressibility
    _tree.reset();
    _compressible = true;
  } else {
    // otherwise we'll keep the uncompressed tree, and will
    // not try compressing again soon
    _compressed.clear();
    _compressed.shrink_to_fit();
    _compressible = false;
  }
}

void RocksDBMetaCollection::RevisionTreeAccessor::serializeBinary(std::string& output) const {
  if (_tree != nullptr) {
    // compress tree into output
    _tree->serializeBinary(output, true);
  } else {
    // append our already compressed state
    output.append(_compressed);
  }
}

// unfortunately we need to declare this method const although it can
// modify internal (mutable) state
void RocksDBMetaCollection::RevisionTreeAccessor::ensureTree() const {
  if (_tree == nullptr) {
    // build tree from compressed state
    TRI_ASSERT(!_compressed.empty());
    _tree = containers::RevisionTree::fromBuffer(std::string_view(_compressed));

    if (_tree == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to uncompress tree");
    }
    TRI_ASSERT(_tree != nullptr);
    TRI_ASSERT(_tree->maxDepth() == _maxDepth);

    // clear the compressed state and free the associated memory
    _compressed.clear();
    _compressed.shrink_to_fit();
  }
  TRI_ASSERT(_tree != nullptr);
}

