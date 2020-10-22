////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
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
      _tempObjectId(basics::VelocyPackHelper::stringUInt64(info, StaticStrings::TempObjectId)),
      _revisionTreeApplied(0),
      _revisionTreeCreationSeq(0),
      _revisionTreeSerializedSeq(0),
      _revisionTreeSerializedTime(std::chrono::steady_clock::now()) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  VPackSlice s = info.get("isVolatile");
  if (s.isBoolean() && s.getBoolean()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "volatile collections are unsupported in the RocksDB engine");
  }

  TRI_ASSERT(_logicalCollection.isAStub() || _objectId.load() != 0);
  rocksutils::globalRocksEngine()->addCollectionMapping(
      _objectId.load(), _logicalCollection.vocbase().id(), _logicalCollection.id());

  if (collection.useSyncByRevision()) {
    std::unique_lock<std::mutex> guard(_revisionTreeLock);
    _revisionTreeCreationSeq = rocksutils::globalRocksDB()->GetLatestSequenceNumber();
    _revisionTreeSerializedSeq = _revisionTreeCreationSeq;
    _revisionTree =
        std::make_unique<containers::RevisionTree>(6, collection.minRevision());
  }
}

RocksDBMetaCollection::RocksDBMetaCollection(LogicalCollection& collection,
                                             PhysicalCollection const* physical)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()),
      _objectId(static_cast<RocksDBMetaCollection const*>(physical)->_objectId.load()),
      _tempObjectId(
          static_cast<RocksDBMetaCollection const*>(physical)->_tempObjectId.load()),
      _revisionTreeApplied(0),
      _revisionTreeCreationSeq(0),
      _revisionTreeSerializedSeq(0),
      _revisionTreeSerializedTime(std::chrono::steady_clock::now()) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  rocksutils::globalRocksEngine()->addCollectionMapping(
      _objectId.load(), _logicalCollection.vocbase().id(), _logicalCollection.id());

  if (collection.useSyncByRevision()) {
    std::unique_lock<std::mutex> guard(_revisionTreeLock);
    _revisionTreeCreationSeq = rocksutils::globalRocksDB()->GetLatestSequenceNumber();
    _revisionTreeSerializedSeq = _revisionTreeCreationSeq;
    _revisionTree =
        std::make_unique<containers::RevisionTree>(6, collection.minRevision());
  }
}

std::string const& RocksDBMetaCollection::path() const {
  return StaticStrings::Empty;  // we do not have any path
}

void RocksDBMetaCollection::deferDropCollection(std::function<bool(LogicalCollection&)> const&) {
  TRI_ASSERT(!_logicalCollection.syncByRevision());
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  _revisionTree.reset();
}

TRI_voc_rid_t RocksDBMetaCollection::revision(transaction::Methods* trx) const {
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
int RocksDBMetaCollection::lockWrite(double timeout) {
  return doLock(timeout, AccessMode::Type::WRITE);
}

/// @brief write unlocks a collection
void RocksDBMetaCollection::unlockWrite() { _exclusiveLock.unlockWrite(); }

/// @brief read locks a collection, with a timeout
int RocksDBMetaCollection::lockRead(double timeout) {
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

  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  rocksdb::TransactionDB* db = engine->db();
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

  TRI_voc_tid_t trxId{0};
  auto blockerGuard = scopeGuard([&] {  // remove blocker afterwards
    if (trxId != 0) {
      _meta.removeBlocker(trxId);
    }
  });

  uint64_t snapNumberOfDocuments = 0;
  {
    // fetch number docs and snapshot under exclusive lock
    // this should enable us to correct the count later
    auto lockGuard = scopeGuard([this] { unlockWrite(); });
    int res = lockWrite(transaction::Options::defaultLockTimeout);
    if (res != TRI_ERROR_NO_ERROR) {
      lockGuard.cancel();
      THROW_ARANGO_EXCEPTION(res);
    }

    // generate a unique transaction id for a blocker
    trxId = transaction::Context::makeTransactionId();

    // place a blocker. will be removed by blockerGuard automatically
    _meta.placeBlocker(trxId, engine->db()->GetLatestSequenceNumber());

    snapshot = engine->db()->GetSnapshot();
    snapNumberOfDocuments = _meta.numberDocuments();
    TRI_ASSERT(snapshot);
  }

  auto snapSeq = snapshot->GetSequenceNumber();

  auto bounds = RocksDBKeyBounds::Empty();
  bool set = false;
  {
    READ_LOCKER(guard, _indexesLock);
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
      << ", an offet of " << adjustment << " will be applied";
    auto adjustSeq = engine->db()->GetLatestSequenceNumber();
    if (adjustSeq <= snapSeq) {
      adjustSeq = ::forceWrite(*engine);
      TRI_ASSERT(adjustSeq > snapSeq);
    }
    _meta.adjustNumberDocuments(adjustSeq, static_cast<TRI_voc_rid_t>(0), adjustment);
  }
  
  return _meta.numberDocuments();
}

Result RocksDBMetaCollection::compact() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = this->bounds();
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, bounds.columnFamily(), &b, &e);
  
  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> i : _indexes) {
    RocksDBIndex* index = static_cast<RocksDBIndex*>(i.get());
    index->compact();
  }
  
  return {};
}

void RocksDBMetaCollection::estimateSize(velocypack::Builder& builder) {
  TRI_ASSERT(!builder.isOpenObject() && !builder.isOpenArray());
  
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
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
  
  READ_LOCKER(guard, _indexesLock);
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
  TRI_ASSERT(tree);
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  _revisionTree = std::move(tree);
  _revisionTreeApplied.store(seq);
  _revisionTreeCreationSeq = seq;
  _revisionTreeSerializedSeq = seq;
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(transaction::Methods& trx) {
  if (!_logicalCollection.useSyncByRevision()) {
    return nullptr;
  }
  TRI_ASSERT(_revisionTree);

  // first apply any updates that can be safely applied
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  rocksdb::DB* db = engine->db()->GetRootDB();
  rocksdb::SequenceNumber safeSeq = meta().committableSeq(db->GetLatestSequenceNumber());

  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  if (!_revisionTree) {
    return nullptr;
  }

  applyUpdates(safeSeq);

  // now clone the tree so we can apply all updates consistent with our ongoing trx
  auto tree = _revisionTree->clone();
  if (!tree) {
    return nullptr;
  }

  {
    // apply any which are buffered and older than our ongoing transaction start
    rocksdb::SequenceNumber trxSeq = RocksDBTransactionState::toState(&trx)->beginSeq();
    TRI_ASSERT(trxSeq != 0);
    Result res = applyUpdatesForTransaction(*tree, trxSeq);
    if (res.fail()) {
      return nullptr;
    }
  }

  {
    // now peek at updates buffered inside transaction and apply those too
    auto operations = RocksDBTransactionState::toState(&trx)->trackedOperations(
        _logicalCollection.id());
    tree->insert(operations.inserts);
    tree->remove(operations.removals);
  }

  return tree;
}

std::unique_ptr<containers::RevisionTree> RocksDBMetaCollection::revisionTree(uint64_t batchId) {
  if (!_logicalCollection.useSyncByRevision()) {
    return nullptr;
  }
  TRI_ASSERT(_revisionTree);

  EngineSelectorFeature& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  // first apply any updates that can be safely applied
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  rocksdb::DB* db = engine->db()->GetRootDB();
  rocksdb::SequenceNumber safeSeq = meta().committableSeq(db->GetLatestSequenceNumber());

  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  if (!_revisionTree) {
    return nullptr;
  }

  applyUpdates(safeSeq);

  // now clone the tree so we can apply all updates consistent with our ongoing trx
  auto tree = _revisionTree->clone();
  if (!tree) {
    return nullptr;
  }

  {
    // apply any which are buffered and older than our ongoing transaction start
    RocksDBEngine& engine = selector.engine<RocksDBEngine>();
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
  }

  return tree;
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
    if (!_revisionTree) {
      // should only occur temporarily during upgrade, just return last number
      return _revisionTreeSerializedSeq;
    }
    applyUpdates(commitSeq);  // always apply updates...
    bool neverDone = _revisionTreeSerializedSeq == _revisionTreeCreationSeq;
    bool coinFlip = RandomGenerator::interval(static_cast<uint32_t>(5)) == 0;
    bool beenTooLong = 30 < std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - _revisionTreeSerializedTime)
                                .count();
    TRI_IF_FAILURE("RocksDBMetaCollection::serializeRevisionTree") {
      return _revisionTreeSerializedSeq;
    }
    if (force || neverDone || coinFlip || beenTooLong) {  // ...but only write the tree out sometimes
      _revisionTree->serializeBinary(output, true);
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
  std::unique_lock<std::mutex> guard(_revisionTreeLock);
  _revisionTree =
      std::make_unique<containers::RevisionTree>(6, _logicalCollection.minRevision());

  Result res = basics::catchToResult([this]() -> Result {
    auto ctxt = transaction::StandaloneContext::Create(_logicalCollection.vocbase());
    SingleCollectionTransaction trx(ctxt, _logicalCollection, AccessMode::Type::READ);
    Result res = trx.begin();
    if (res.fail()) {
      LOG_TOPIC("d1e53", WARN, arangodb::Logger::ENGINES)
          << "failed to begin transaction to rebuild revision tree "
             "for collection '"
          << _logicalCollection.id() << "'";
      return res;
    }
    auto* state = RocksDBTransactionState::toState(&trx);

    std::vector<std::size_t> revisions;
    auto iter = getReplicationIterator(ReplicationIterator::Ordering::Revision, trx);
    if (!iter) {
      LOG_TOPIC("d1e54", WARN, arangodb::Logger::ENGINES)
          << "failed to retrieve replication iterator to rebuild revision tree "
             "for collection '"
          << _logicalCollection.id() << "'";
      return Result(TRI_ERROR_INTERNAL);
    }
    RevisionReplicationIterator& it =
        *static_cast<RevisionReplicationIterator*>(iter.get());
    while (it.hasMore()) {
      revisions.emplace_back(it.revision());
      if (revisions.size() >= 5000) {  // arbitrary batch size
        _revisionTree->insert(revisions);
        revisions.clear();
      }
      it.next();
    }
    if (!revisions.empty()) {
      _revisionTree->insert(revisions);
    }
    _revisionTreeApplied.store(state->beginSeq());
    _revisionTreeCreationSeq = state->beginSeq();
    _revisionTreeSerializedSeq = state->beginSeq();
    return Result();
  });

  if (res.fail() && res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    res.reset();
    // okay, we are in recovery and can't open a transaction, so we need to
    // read the raw RocksDB data; on the plus side, we are in recovery, so we
    // are single-threaded and don't need to worry about transactions anyway

    RocksDBKeyBounds documentBounds =
        RocksDBKeyBounds::CollectionDocuments(_objectId.load());
    rocksdb::Comparator const* cmp = RocksDBColumnFamily::documents()->GetComparator();
    rocksdb::ReadOptions ro;
    rocksdb::Slice const end = documentBounds.end();
    ro.iterate_upper_bound = &end;
    ro.fill_cache = false;

    std::vector<std::size_t> revisions;
    auto* db = rocksutils::globalRocksDB();
    std::unique_ptr<rocksdb::Iterator> iter(db->NewIterator(ro, documentBounds.columnFamily()));
    for (iter->Seek(documentBounds.start());
         iter->Valid() && cmp->Compare(iter->key(), end) < 0; iter->Next()) {
      LocalDocumentId const docId = RocksDBKey::documentId(iter->key());
      revisions.emplace_back(docId.id());
      if (revisions.size() >= 5000) {  // arbitrary batch size
        _revisionTree->insert(revisions);
        revisions.clear();
      }
    }
    if (!revisions.empty()) {
      _revisionTree->insert(revisions);
    }
    rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();
    _revisionTreeApplied.store(seq);
    _revisionTreeCreationSeq = seq;
    _revisionTreeSerializedSeq = seq;
  }

  return res;
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

void RocksDBMetaCollection::placeRevisionTreeBlocker(TRI_voc_tid_t transactionId) {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::SequenceNumber preSeq = db->GetLatestSequenceNumber();
  _meta.placeBlocker(transactionId, preSeq);
}

void RocksDBMetaCollection::removeRevisionTreeBlocker(TRI_voc_tid_t transactionId) {
  _meta.removeBlocker(transactionId);
}

void RocksDBMetaCollection::bufferUpdates(rocksdb::SequenceNumber seq,
                                          std::vector<std::size_t>&& inserts,
                                          std::vector<std::size_t>&& removals) {
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

Result RocksDBMetaCollection::setObjectIds(std::uint64_t plannedObjectId,
                                           std::uint64_t plannedTempObjectId) {
  Result res;
  auto& server = _logicalCollection.vocbase().server();
  auto& selector = server.getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();

  if (plannedObjectId == _objectId.load() && plannedTempObjectId != _tempObjectId) {
    // just temp id has changed
    std::uint64_t oldId = (plannedTempObjectId == 0) ? _tempObjectId.load() : 0;
    _tempObjectId.store(plannedTempObjectId);
    if (oldId != 0) {  // need to clean up the old range
      RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(oldId);
      res = rocksutils::removeLargeRange(engine.db(), bounds, true, true);
    }
  } else if (plannedTempObjectId != _tempObjectId) {
    TRI_ASSERT(plannedObjectId != _objectId.load());
    TRI_ASSERT(plannedObjectId != 0);
    TRI_ASSERT(plannedObjectId == _tempObjectId.load());
    // swapping in new range
    _tempObjectId.store(plannedTempObjectId);
    _objectId.store(plannedObjectId);
    engine.addCollectionMapping(_objectId, _logicalCollection.vocbase().id(),
                                _logicalCollection.id());
  }

  return res;
}

void RocksDBMetaCollection::applyUpdates(rocksdb::SequenceNumber commitSeq) {
  if (!_logicalCollection.useSyncByRevision()) {
    return;
  }
  TRI_ASSERT(_revisionTree);

  Result res = basics::catchVoidToResult([&]() -> void {
    std::multimap<rocksdb::SequenceNumber, std::vector<std::size_t>>::const_iterator insertIt;
    std::multimap<rocksdb::SequenceNumber, std::vector<std::size_t>>::const_iterator removeIt;
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
        _revisionTree->clear();  // clear out any revision structure, now empty
      }
    }

    while (true) {
      std::vector<std::size_t> inserts;
      std::vector<std::size_t> removals;
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
          inserts = std::move(insertIt->second);
          insertIt = _revisionInsertBuffers.erase(insertIt);
        }
        // check for removals
        if (applyRemovals) {
          std::unique_lock<std::mutex> guard(_revisionBufferLock);
          removals = std::move(removeIt->second);
          removeIt = _revisionRemovalBuffers.erase(removeIt);
        }
      }

      // no inserts or removals left to apply, drop out of loop
      if (inserts.empty() && removals.empty()) {
        break;
      }

      // apply inserts
      if (!inserts.empty()) {
        _revisionTree->insert(inserts);
        inserts.clear();
      }

      // apply removals
      if (!removals.empty()) {
        _revisionTree->remove(removals);
        removals.clear();
      }
    }
  });
  rocksdb::SequenceNumber applied = _revisionTreeApplied.load();
  while (commitSeq > applied) {
    _revisionTreeApplied.compare_exchange_strong(applied, commitSeq);
  }
}

Result RocksDBMetaCollection::applyUpdatesForTransaction(containers::RevisionTree& tree,
                                                         rocksdb::SequenceNumber commitSeq) const {
  if (!_logicalCollection.useSyncByRevision()) {
    return Result();
  }

  Result res = basics::catchVoidToResult([&]() -> void {
    std::multimap<rocksdb::SequenceNumber, std::vector<std::size_t>>::const_iterator insertIt;
    std::multimap<rocksdb::SequenceNumber, std::vector<std::size_t>>::const_iterator removeIt;
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
      std::vector<std::size_t> const* inserts = nullptr;
      std::vector<std::size_t> const* removals = nullptr;
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
int RocksDBMetaCollection::doLock(double timeout, AccessMode::Type mode) {
  uint64_t waitTime = 0;  // indicates that time is uninitialized
  double startTime = 0.0;
  
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
      // keep lock and exit loop
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
    }
    
    if (now > startTime + timeout) {
      LOG_TOPIC("d1e52", TRACE, arangodb::Logger::ENGINES)
      << "timed out after " << timeout << " s waiting for " 
      << AccessMode::typeString(mode) << " lock on collection '"
      << _logicalCollection.name() << "'";
      
      return TRI_ERROR_LOCK_TIMEOUT;
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
