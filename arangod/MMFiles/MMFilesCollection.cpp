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

#include "MMFilesCollection.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/PlanCache.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/PerformanceLogScope.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadUnlocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/WriteUnlocker.h"
#include "Basics/encoding.h"
#include "Basics/process-utils.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/IndexIterator.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollectionWriteLocker.h"
#include "MMFiles/MMFilesCompactorThread.h"
#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesDocumentOperation.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "MMFiles/MMFilesTransactionState.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/KeyLockInfo.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

/// @brief state during opening of a collection
namespace arangodb {
struct OpenIteratorState {
  LogicalCollection* _collection;
  arangodb::MMFilesPrimaryIndex* _primaryIndex;
  TRI_voc_tid_t _tid{0};
  TRI_voc_fid_t _fid{0};
  std::unordered_map<TRI_voc_fid_t, MMFilesDatafileStatisticsContainer*>
      _stats;
  MMFilesDatafileStatisticsContainer* _dfi{nullptr};
  transaction::Methods* _trx;
  ManagedDocumentResult _mdr;
  MMFilesIndexLookupContext _context;
  uint64_t _deletions{0};
  uint64_t _documents{0};
  int64_t _initialCount{-1};
  bool _hasAllPersistentLocalIds{true};

  OpenIteratorState(LogicalCollection* collection, transaction::Methods* trx)
      : _collection(collection),
        _primaryIndex(
            static_cast<MMFilesCollection*>(collection->getPhysical())
                ->primaryIndex()),
        _stats(),
        _trx(trx),
        _context(trx, collection, &_mdr, 1) {
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(trx != nullptr);
  }

  ~OpenIteratorState() {
    for (auto& it : _stats) {
      delete it.second;
    }
  }
};
}

namespace {

/// @brief helper class for filling indexes
class MMFilesIndexFillerTask : public basics::LocalTask {
 public:
  MMFilesIndexFillerTask(
      std::shared_ptr<basics::LocalTaskQueue> const& queue,
      transaction::Methods& trx,
      Index* idx,
      std::shared_ptr<std::vector<std::pair<LocalDocumentId, VPackSlice>>> const& documents)
      : LocalTask(queue), _trx(trx), _idx(idx), _documents(documents) {}

  void run() override {
    TRI_ASSERT(_idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

    try {
      _idx->batchInsert(_trx, *_documents.get(), _queue);
    } catch (std::exception const&) {
      _queue->setStatus(TRI_ERROR_INTERNAL);
    }

    _queue->join();
  }

 private:
  transaction::Methods& _trx;
  Index* _idx;
  std::shared_ptr<std::vector<std::pair<LocalDocumentId, VPackSlice>>> _documents;
};

/// @brief find a statistics container for a given file id
static MMFilesDatafileStatisticsContainer* FindDatafileStats(
    OpenIteratorState* state, TRI_voc_fid_t fid) {
  auto it = state->_stats.find(fid);

  if (it != state->_stats.end()) {
    return (*it).second;
  }

  auto stats = std::make_unique<MMFilesDatafileStatisticsContainer>();
  state->_stats.emplace(fid, stats.get());
  return stats.release();
}

bool countDocumentsIterator(MMFilesMarker const* marker,
                            void* counter, MMFilesDatafile*) {
  TRI_ASSERT(nullptr != counter);
  if (marker->getType() == TRI_DF_MARKER_VPACK_DOCUMENT) {
    (*static_cast<int*>(counter))++;
  }
  return true;
}

Result persistLocalDocumentIdIterator(
    MMFilesMarker const* marker, void* data, MMFilesDatafile* inputFile) {
  Result res;
  auto outputFile = static_cast<MMFilesDatafile*>(data);
  switch (marker->getType()) {
    case TRI_DF_MARKER_VPACK_DOCUMENT: {
      auto transactionId = MMFilesDatafileHelper::TransactionId(marker);

      VPackSlice const slice(
          reinterpret_cast<char const*>(marker) +
          MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
      uint8_t const* vpack = slice.begin();

      LocalDocumentId localDocumentId;
      if (marker->getSize() ==
          MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT)
          + slice.byteSize() + sizeof(LocalDocumentId::BaseType)) {
        // we do have a LocalDocumentId stored at the end of the marker
        uint8_t const* ptr = vpack + slice.byteSize();
        localDocumentId = LocalDocumentId(
            encoding::readNumber<LocalDocumentId::BaseType>(
                ptr, sizeof(LocalDocumentId::BaseType)));
      } else {
        localDocumentId = LocalDocumentId::create();
      }

      MMFilesCrudMarker updatedMarker(TRI_DF_MARKER_VPACK_DOCUMENT,
                                      transactionId, localDocumentId, slice);

      std::unique_ptr<char[]> buffer(new char[updatedMarker.size()]);
      MMFilesMarker* outputMarker =
          reinterpret_cast<MMFilesMarker*>(buffer.get());
      MMFilesDatafileHelper::InitMarker(outputMarker, updatedMarker.type(),
                                        updatedMarker.size(),
                                        marker->getTick());
      updatedMarker.store(buffer.get());

      MMFilesMarker* result;
      res = outputFile->reserveElement(outputMarker->getSize(), &result, 0);
      if (res.fail()) {
        return res;
      }
      res = outputFile->writeCrcElement(result, outputMarker);
      if (res.fail()) {
        return res;
      }
      break;
    }
    case TRI_DF_MARKER_HEADER:
    case TRI_DF_MARKER_COL_HEADER:
    case TRI_DF_MARKER_FOOTER: {
      // skip marker, either already written by createCompactor or will be
      // written by closeCompactor
      break;
    }
    default: {
      // direct copy
      MMFilesMarker* result;
      res = outputFile->reserveElement(marker->getSize(), &result, 0);
      if (res.fail()) {
        return res;
      }
      res = outputFile->writeElement(result, marker);
      if (res.fail()) {
        return res;
      }
      break;
    }
  }

  return res;
}

}  // namespace

arangodb::Result MMFilesCollection::updateProperties(VPackSlice const& slice,
                                                     bool doSync) {
  // validation
  uint32_t tmp = arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
      slice, "indexBuckets",
      2 /*Just for validation, this default Value passes*/);

  if (tmp == 0 || tmp > 1024) {
    return {TRI_ERROR_BAD_PARAMETER,
            "indexBuckets must be a two-power between 1 and 1024"};
  }

  if (isVolatile() &&
      arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "waitForSync", _logicalCollection.waitForSync()
      )) {
    // the combination of waitForSync and isVolatile makes no sense
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections do not support the waitForSync option");
  }

  if (isVolatile() != arangodb::basics::VelocyPackHelper::getBooleanValue(
                          slice, "isVolatile", isVolatile())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "isVolatile option cannot be changed at runtime");
  }
  auto journalSlice = slice.get("journalSize");

  if (journalSlice.isNone()) {
    // In some APIs maximalSize is allowed instead
    journalSlice = slice.get("maximalSize");
  }

  if (!journalSlice.isNone() && journalSlice.isNumber()) {
    uint32_t toUpdate = journalSlice.getNumericValue<uint32_t>();
    if (toUpdate < TRI_JOURNAL_MINIMAL_SIZE) {
      return {TRI_ERROR_BAD_PARAMETER, "<properties>.journalSize too small"};
    }
  }

  _indexBuckets = Helper::getNumericValue<uint32_t>(slice, "indexBuckets",
                                                    _indexBuckets);  // MMFiles

  if (slice.hasKey("journalSize")) {
    _journalSize = Helper::getNumericValue<uint32_t>(slice, "journalSize",
                                                           _journalSize);
  } else {
    _journalSize = Helper::getNumericValue<uint32_t>(slice, "maximalSize",
                                                           _journalSize);
  }
  _doCompact = Helper::getBooleanValue(slice, "doCompact", _doCompact);

  int64_t count = arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
      slice, "count", _initialCount);
  if (count != _initialCount) {
    _initialCount = count;
  }

  return {};
}

arangodb::Result MMFilesCollection::persistProperties() {
  Result res;

  try {
    auto infoBuilder = _logicalCollection.toVelocyPackIgnore(
      {"path", "statusString"}, true, true
    );
    MMFilesCollectionMarker marker(
      TRI_DF_MARKER_VPACK_CHANGE_COLLECTION,
      _logicalCollection.vocbase().id(),
      _logicalCollection.id(),
      infoBuilder.slice()
    );
    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

    res = slotInfo.errorCode;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res.fail()) {
    // TODO: what to do here
    LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
        << "could not save collection change marker in log: "
        << res.errorMessage();
  }
  return res;
}

PhysicalCollection* MMFilesCollection::clone(LogicalCollection& logical) const {
  return new MMFilesCollection(logical, this);
}

/// @brief process a document (or edge) marker when opening a collection
int MMFilesCollection::OpenIteratorHandleDocumentMarker(
    MMFilesMarker const* marker, MMFilesDatafile* datafile,
    OpenIteratorState* state) {
  LogicalCollection* collection = state->_collection;
  TRI_ASSERT(collection != nullptr);
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  transaction::Methods* trx = state->_trx;
  TRI_ASSERT(trx != nullptr);

  VPackSlice const slice(
      reinterpret_cast<char const*>(marker) +
      MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  uint8_t const* vpack = slice.begin();

  LocalDocumentId localDocumentId;
  if (marker->getSize() == MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT) + slice.byteSize() + sizeof(LocalDocumentId::BaseType)) {
    // we do have a LocalDocumentId stored at the end of the marker
    uint8_t const* ptr = vpack + slice.byteSize();
    localDocumentId = LocalDocumentId(encoding::readNumber<LocalDocumentId::BaseType>(ptr, sizeof(LocalDocumentId::BaseType)));
  } else {
    localDocumentId = LocalDocumentId::create();
    state->_hasAllPersistentLocalIds = false;
  }

  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice,
                                                     revisionId);

  physical->setRevision(revisionId, false);

  {
    // track keys
    VPackValueLength length;
    char const* p = keySlice.getString(length);
    collection->keyGenerator()->track(p, static_cast<size_t>(length));
  }

  ++state->_documents;

  TRI_voc_fid_t const fid = datafile->fid();
  if (state->_fid != fid) {
    // update the state
    state->_fid = fid;  // when we're here, we're looking at a datafile
    state->_dfi = FindDatafileStats(state, fid);
  }

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  MMFilesSimpleIndexElement* found =
      state->_primaryIndex->lookupKeyRef(trx, keySlice, state->_mdr);

  // it is a new entry
  if (found == nullptr || !found->isSet()) {
    physical->insertLocalDocumentId(localDocumentId, vpack, fid, false, false);

    // insert into primary index
    Result res = state->_primaryIndex->insertKey(trx, localDocumentId,
                                                 VPackSlice(vpack),
                                                 state->_mdr,
                                                 Index::OperationMode::normal);

    if (res.fail()) {
      physical->removeLocalDocumentId(localDocumentId, false);
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
          << "inserting document into primary index failed with error: "
          << res.errorMessage();

      return res.errorNumber();
    }

    // update the datafile info
    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive +=
        MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  // it is an update
  else {
    LocalDocumentId const oldLocalDocumentId = found->localDocumentId();
    // update the revision id in primary index
    found->updateLocalDocumentId(localDocumentId, static_cast<uint32_t>(keySlice.begin() - vpack));

    MMFilesDocumentPosition const old = physical->lookupDocument(oldLocalDocumentId);

    // remove old revision
    physical->removeLocalDocumentId(oldLocalDocumentId, false);

    // insert new revision
    physical->insertLocalDocumentId(localDocumentId, vpack, fid, false, false);

    // update the datafile info
    MMFilesDatafileStatisticsContainer* dfi;
    if (old.fid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, old.fid());
    }

    if (old.dataptr() != nullptr) {
      uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
      MMFilesMarker const* oldMarker = reinterpret_cast<MMFilesMarker const*>(vpack - MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));

      int64_t size = MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(oldMarker);
      dfi->numberAlive--;
      dfi->sizeAlive -= size;
      dfi->numberDead++;
      dfi->sizeDead += size;
    }

    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive +=
        MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief process a deletion marker when opening a collection
int MMFilesCollection::OpenIteratorHandleDeletionMarker(
    MMFilesMarker const* marker, MMFilesDatafile* datafile,
    OpenIteratorState* state) {
  LogicalCollection* collection = state->_collection;
  TRI_ASSERT(collection != nullptr);
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  transaction::Methods* trx = state->_trx;

  VPackSlice const slice(
      reinterpret_cast<char const*>(marker) +
      MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_REMOVE));

  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice, revisionId);

  physical->setRevision(revisionId, false);
  {
    // track keys
    VPackValueLength length;
    char const* p = keySlice.getString(length);
    collection->keyGenerator()->track(p, length);
  }

  ++state->_deletions;

  if (state->_fid != datafile->fid()) {
    // update the state
    state->_fid = datafile->fid();
    state->_dfi = FindDatafileStats(state, datafile->fid());
  }

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  MMFilesSimpleIndexElement found =
      state->_primaryIndex->lookupKey(trx, keySlice, state->_mdr);

  // it is a new entry, so we missed the create
  if (!found) {
    // update the datafile info
    state->_dfi->numberDeletions++;
  }

  // it is a real delete
  else {
    LocalDocumentId const oldLocalDocumentId = found.localDocumentId();

    MMFilesDocumentPosition const old = physical->lookupDocument(oldLocalDocumentId);

    // update the datafile info
    MMFilesDatafileStatisticsContainer* dfi;

    if (old.fid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, old.fid());
    }

    TRI_ASSERT(old.dataptr() != nullptr);

    uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
    MMFilesMarker const* oldMarker = reinterpret_cast<MMFilesMarker const*>(vpack - MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));

    int64_t size = MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(oldMarker);
    dfi->numberAlive--;
    dfi->sizeAlive -= size;
    dfi->numberDead++;
    dfi->sizeDead += size;
    state->_dfi->numberDeletions++;

    state->_primaryIndex->removeKey(trx, oldLocalDocumentId, VPackSlice(vpack),
                                    state->_mdr, Index::OperationMode::normal);

    physical->removeLocalDocumentId(oldLocalDocumentId, true);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief iterator for open
bool MMFilesCollection::OpenIterator(MMFilesMarker const* marker,
                                     OpenIteratorState* data,
                                     MMFilesDatafile* datafile) {
  TRI_voc_tick_t const tick = marker->getTick();
  MMFilesMarkerType const type = marker->getType();

  int res;

  if (type == TRI_DF_MARKER_VPACK_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile, data);

    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (tick > datafile->_dataMax) {
      datafile->_dataMax = tick;
    }
  } else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile, data);
  } else {
    if (type == TRI_DF_MARKER_HEADER) {
      // ensure there is a datafile info entry for each datafile of the
      // collection
      FindDatafileStats(data, datafile->fid());
    }

    LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
        << "skipping marker type " << TRI_NameMarkerDatafile(marker);
    res = TRI_ERROR_NO_ERROR;
  }

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }

  if (tick > datafile->_tickMax) {
    datafile->_tickMax = tick;
  }

  auto physical = data->_collection->getPhysical();
  auto mmfiles = static_cast<MMFilesCollection*>(physical);
  TRI_ASSERT(mmfiles);
  if (tick > mmfiles->maxTick()) {
    if (type != TRI_DF_MARKER_HEADER && type != TRI_DF_MARKER_FOOTER &&
        type != TRI_DF_MARKER_COL_HEADER && type != TRI_DF_MARKER_PROLOGUE) {
      mmfiles->maxTick(tick);
    }
  }

  return (res == TRI_ERROR_NO_ERROR);
}

MMFilesCollection::MMFilesCollection(LogicalCollection& collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info),
      _ditches(&collection),
      _initialCount(0),
      _lastRevision(0),
      _uncollectedLogfileEntries(0),
      _nextCompactionStartIndex(0),
      _lastCompactionStatus(nullptr),
      _lastCompactionStamp(0.0),
      _journalSize(Helper::readNumericValue<uint32_t>(
          info, "maximalSize",  // Backwards compatibility. Agency uses
                                // journalSize. paramters.json uses maximalSize
          Helper::readNumericValue<uint32_t>(info, "journalSize",
                                             TRI_JOURNAL_DEFAULT_SIZE))),
      _isVolatile(arangodb::basics::VelocyPackHelper::readBooleanValue(
          info, "isVolatile", false)),
      _persistentIndexes(0),
      _primaryIndex(nullptr),
      _indexBuckets(Helper::readNumericValue<uint32_t>(info, "indexBuckets",
                                                       defaultIndexBuckets)),
      _useSecondaryIndexes(true),
      _doCompact(Helper::readBooleanValue(info, "doCompact", true)),
      _maxTick(0) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  if (_isVolatile && _logicalCollection.waitForSync()) {
    // Illegal collection configuration
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections do not support the waitForSync option");
  }

  if (_journalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "<properties>.journalSize too small");
  }

  auto pathSlice = info.get("path");

  if (pathSlice.isString()) {
    _path = pathSlice.copyString();
  }

  setCompactionStatus("compaction not yet started");
}

MMFilesCollection::MMFilesCollection(
    LogicalCollection& logical,
    PhysicalCollection const* physical
)
    : PhysicalCollection(logical, VPackSlice::emptyObjectSlice()),
      _ditches(&logical),
      _isVolatile(static_cast<MMFilesCollection const*>(physical)->isVolatile()) {
  MMFilesCollection const& mmfiles = *static_cast<MMFilesCollection const*>(physical);
  _persistentIndexes = mmfiles._persistentIndexes;
  _useSecondaryIndexes = mmfiles._useSecondaryIndexes;
  _initialCount = mmfiles._initialCount;
  _lastRevision = mmfiles._lastRevision;
  _nextCompactionStartIndex = mmfiles._nextCompactionStartIndex;
  _lastCompactionStatus = mmfiles._lastCompactionStatus;
  _lastCompactionStamp = mmfiles._lastCompactionStamp;
  _journalSize = mmfiles._journalSize;
  _indexBuckets = mmfiles._indexBuckets;
  _primaryIndex = mmfiles._primaryIndex;
  _path = mmfiles._path;
  _doCompact = mmfiles._doCompact;
  _maxTick = mmfiles._maxTick;
/*
  // Copy over index definitions
  _indexes.reserve(mmfiles._indexes.size());
  for (auto const& idx : mmfiles._indexes) {
    _indexes.emplace_back(idx);
  }
*/
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  setCompactionStatus("compaction not yet started");
  //  not copied
  //  _datafiles;   // all datafiles
  //  _journals;    // all journals
  //  _compactors;  // all compactor files
  //  _uncollectedLogfileEntries = mmfiles.uncollectedLogfileEntries; //TODO
  //  FIXME
  //  _datafileStatistics;
  //  _revisionsCache;
}

MMFilesCollection::~MMFilesCollection() {}

TRI_voc_rid_t MMFilesCollection::revision(
    arangodb::transaction::Methods*) const {
  return _lastRevision;
}

TRI_voc_rid_t MMFilesCollection::revision() const { return _lastRevision; }

/// @brief update statistics for a collection
void MMFilesCollection::setRevision(TRI_voc_rid_t revision, bool force) {
  if (revision > 0 && (force || revision > _lastRevision)) {
    _lastRevision = revision;
  }
}

size_t MMFilesCollection::journalSize() const { return _journalSize; };

bool MMFilesCollection::isVolatile() const { return _isVolatile; }

/// @brief closes an open collection
int MMFilesCollection::close() {
  LOG_TOPIC(DEBUG, Logger::ENGINES) << "closing '" << _logicalCollection.name() << "'";
  if (!_logicalCollection.deleted() &&
      !_logicalCollection.vocbase().isDropped()) {
    auto primIdx = primaryIndex();
    auto idxSize = primIdx->size();

    if (_initialCount != static_cast<int64_t>(idxSize)) {
      _initialCount = idxSize;

      // save new "count" value
      StorageEngine* engine = EngineSelectorFeature::ENGINE;
      bool const doSync =
          application_features::ApplicationServer::getFeature<DatabaseFeature>(
              "Database")
              ->forceSyncProperties();

      engine->changeCollection(
        _logicalCollection.vocbase(),
        _logicalCollection.id(),
        _logicalCollection,
        doSync
      );
    }
  }

  {
    // We also have to unload the indexes.
    READ_LOCKER(guard, _indexesLock);     /// TODO - DEADLOCK!?!?
    WRITE_LOCKER(writeLocker, _dataLock);
    for (auto& idx : _indexes) {
      idx->unload();
    }
  }

  // wait until ditches have been processed fully
  while (_ditches.contains(MMFilesDitch::TRI_DITCH_DATAFILE_DROP) ||
         _ditches.contains(MMFilesDitch::TRI_DITCH_DATAFILE_RENAME) ||
         _ditches.contains(MMFilesDitch::TRI_DITCH_COMPACTION)) {
    WRITE_UNLOCKER(unlocker, _logicalCollection.lock());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  {
    WRITE_LOCKER(writeLocker, _filesLock);

    // close compactor files
    closeDatafiles(_compactors);

    for (auto& it : _compactors) {
      delete it;
    }

    _compactors.clear();

    // close journal files
    closeDatafiles(_journals);

    for (auto& it : _journals) {
      delete it;
    }

    _journals.clear();

    // close datafiles
    closeDatafiles(_datafiles);

    for (auto& it : _datafiles) {
      delete it;
    }

    _datafiles.clear();
  }

  _lastRevision = 0;

  // clear revisions lookup table
  _revisionsCache.clear();

  return TRI_ERROR_NO_ERROR;
}

/// @brief seal a datafile
int MMFilesCollection::sealDatafile(MMFilesDatafile* datafile,
                                    bool isCompactor) {
  int res = datafile->seal();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::DATAFILES) << "failed to seal journal '"
                                                << datafile->getName()
                                                << "': " << TRI_errno_string(res);
    return res;
  }

  if (!isCompactor && datafile->isPhysical()) {
    // rename the file
    std::string dname("datafile-" + std::to_string(datafile->fid()) + ".db");
    std::string filename =
        arangodb::basics::FileUtils::buildFilename(path(), dname);

    LOG_TOPIC(TRACE, arangodb::Logger::DATAFILES) << "closing and renaming journal file '"
                                                  << datafile->getName() << "'";

    res = datafile->rename(filename);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(TRACE, arangodb::Logger::DATAFILES) << "closed and renamed journal file '"
                                                    << datafile->getName() << "'";
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::DATAFILES)
          << "failed to rename datafile '" << datafile->getName() << "' to '"
          << filename << "': " << TRI_errno_string(res);
    }
  }

  return res;
}

/// @brief set the initial datafiles for the collection
void MMFilesCollection::setInitialFiles(std::vector<MMFilesDatafile*>&& datafiles,
                                        std::vector<MMFilesDatafile*>&& journals,
                                        std::vector<MMFilesDatafile*>&& compactors) {
  WRITE_LOCKER(writeLocker, _filesLock);

  _datafiles = std::move(datafiles);
  _journals = std::move(journals);
  _compactors = std::move(compactors);

  TRI_ASSERT(_journals.size() <= 1);
}

/// @brief rotate the active journal - will do nothing if there is no journal
int MMFilesCollection::rotateActiveJournal() {
  WRITE_LOCKER(writeLocker, _filesLock);

  // note: only journals need to be handled here as the journal is the
  // only place that's ever written to. if a journal is full, it will have been
  // sealed and synced already
  if (_journals.empty()) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  if (_journals.size() > 1) {
    // we should never have more than a single journal at a time
    return TRI_ERROR_INTERNAL;
  }

  MMFilesDatafile* datafile = _journals.back();
  TRI_ASSERT(datafile != nullptr);

  TRI_IF_FAILURE("CreateMultipleJournals") {
    // create an additional journal now, without sealing and renaming the old one!
    _datafiles.emplace_back(datafile);
    _journals.pop_back();

    return TRI_ERROR_NO_ERROR;
  }

  // make sure we have enough room in the target vector before we go on
  _datafiles.reserve(_datafiles.size() + 1);

  int res = sealDatafile(datafile, false);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // shouldn't throw as we reserved enough space before
  _datafiles.emplace_back(datafile);

  TRI_ASSERT(!_journals.empty());
  TRI_ASSERT(_journals.back() == datafile);
  _journals.pop_back();
  TRI_ASSERT(_journals.empty());

  return res;
}

/// @brief sync the active journal - will do nothing if there is no journal
/// or if the journal is volatile
int MMFilesCollection::syncActiveJournal() {
  WRITE_LOCKER(writeLocker, _filesLock);

  // note: only journals need to be handled here as the journal is the
  // only place that's ever written to. if a journal is full, it will have been
  // sealed and synced already
  if (_journals.empty()) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_journals.size() == 1);

  MMFilesDatafile* datafile = _journals.back();
  TRI_ASSERT(datafile != nullptr);

  return datafile->sync();
}

/// @brief reserve space in the current journal. if no create exists or the
/// current journal cannot provide enough space, close the old journal and
/// create a new one
int MMFilesCollection::reserveJournalSpace(TRI_voc_tick_t tick,
                                           uint32_t size,
                                           char*& resultPosition,
                                           MMFilesDatafile*& resultDatafile) {
  // reset results
  resultPosition = nullptr;
  resultDatafile = nullptr;

  // start with configured journal size
  uint32_t targetSize = static_cast<uint32_t>(_journalSize);

  // make sure that the document fits
  while (targetSize - 256 < size) {
    targetSize *= 2;
  }

  WRITE_LOCKER(writeLocker, _filesLock);
  TRI_ASSERT(_journals.size() <= 1);

  while (true) {
    // no need to go on if the collection is already deleted
    if (TRI_VOC_COL_STATUS_DELETED == _logicalCollection.status()) {
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
    }

    MMFilesDatafile* datafile = nullptr;

    if (_journals.empty()) {
      // create enough room in the journals vector
      _journals.reserve(_journals.size() + 1);

      try {
        std::unique_ptr<MMFilesDatafile> df(
            createDatafile(tick, targetSize, false));

        // shouldn't throw as we reserved enough space before
        _journals.emplace_back(df.get());
        df.release();
        TRI_ASSERT(_journals.size() == 1);
      } catch (basics::Exception const& ex) {
        LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: "
                                          << ex.what();
        return ex.code();
      } catch (std::exception const& ex) {
        LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: "
                                          << ex.what();
        return TRI_ERROR_INTERNAL;
      } catch (...) {
        LOG_TOPIC(ERR, Logger::COLLECTOR)
            << "cannot select journal: unknown exception";
        return TRI_ERROR_INTERNAL;
      }
    }

    // select datafile
    TRI_ASSERT(!_journals.empty());
    TRI_ASSERT(_journals.size() == 1);

    datafile = _journals.back();

    TRI_ASSERT(datafile != nullptr);

    // try to reserve space in the datafile
    MMFilesMarker* position = nullptr;
    int res = datafile->reserveElement(size, &position, targetSize);

    // found a datafile with enough space left
    if (res == TRI_ERROR_NO_ERROR) {
      // set result
      resultPosition = reinterpret_cast<char*>(position);
      resultDatafile = datafile;
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_ARANGO_DATAFILE_FULL) {
      // some other error
      LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: '"
                                        << TRI_last_error() << "'";
      return res;
    }

    // TRI_ERROR_ARANGO_DATAFILE_FULL...
    // journal is full, close it and sync
    LOG_TOPIC(DEBUG, Logger::COLLECTOR) << "closing full journal '"
                                        << datafile->getName() << "'";

    // make sure we have enough room in the target vector before we go on
    _datafiles.reserve(_datafiles.size() + 1);

    res = sealDatafile(datafile, false);

    // move journal into _datafiles vector
    // this shouldn't fail, as we have reserved space before already
    _datafiles.emplace_back(datafile);

    // and finally erase it from _journals vector
    TRI_ASSERT(!_journals.empty());
    TRI_ASSERT(_journals.back() == datafile);
    _journals.pop_back();
    TRI_ASSERT(_journals.empty());

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred, we must stop here
      return res;
    }
  }  // otherwise, next iteration!

  return TRI_ERROR_ARANGO_NO_JOURNAL;
}

/// @brief create compactor file
MMFilesDatafile* MMFilesCollection::createCompactor(
    TRI_voc_fid_t fid, uint32_t maximalSize) {
  WRITE_LOCKER(writeLocker, _filesLock);

  TRI_ASSERT(_compactors.empty());
  // reserve enough space for the later addition
  _compactors.reserve(_compactors.size() + 1);

  std::unique_ptr<MMFilesDatafile> compactor(
      createDatafile(fid, static_cast<uint32_t>(maximalSize), true));

  // should not throw, as we've reserved enough space before
  _compactors.emplace_back(compactor.get());
  TRI_ASSERT(_compactors.size() == 1);
  return compactor.release();
}

/// @brief close an existing compactor
int MMFilesCollection::closeCompactor(MMFilesDatafile* datafile) {
  WRITE_LOCKER(writeLocker, _filesLock);

  if (_compactors.size() != 1) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  MMFilesDatafile* compactor = _compactors[0];

  if (datafile != compactor) {
    // wrong compactor file specified... should not happen
    return TRI_ERROR_INTERNAL;
  }

  return sealDatafile(datafile, true);
}

/// @brief replace a datafile with a compactor
int MMFilesCollection::replaceDatafileWithCompactor(
    MMFilesDatafile* datafile, MMFilesDatafile* compactor) {
  TRI_ASSERT(datafile != nullptr);
  TRI_ASSERT(compactor != nullptr);

  WRITE_LOCKER(writeLocker, _filesLock);

  TRI_ASSERT(!_compactors.empty());

  for (size_t i = 0; i < _datafiles.size(); ++i) {
    if (_datafiles[i]->fid() == datafile->fid()) {
      // found!
      // now put the compactor in place of the datafile
      _datafiles[i] = compactor;

      // remove the compactor file from the list of compactors
      TRI_ASSERT(_compactors[0] != nullptr);
      TRI_ASSERT(_compactors[0]->fid() == compactor->fid());

      _compactors.erase(_compactors.begin());
      TRI_ASSERT(_compactors.empty());

      return TRI_ERROR_NO_ERROR;
    }
  }

  return TRI_ERROR_INTERNAL;
}

/// @brief creates a datafile
MMFilesDatafile* MMFilesCollection::createDatafile(TRI_voc_fid_t fid,
                                                   uint32_t journalSize,
                                                   bool isCompactor) {
  TRI_ASSERT(fid > 0);

  // create an entry for the new datafile
  try {
    _datafileStatistics.create(fid);
  } catch (basics::Exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    // rethrow but do not change error code
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::unique_ptr<MMFilesDatafile> datafile;

  if (isVolatile()) {
    // in-memory collection
    datafile.reset(
        MMFilesDatafile::create(StaticStrings::Empty, fid, journalSize, true));
  } else {
    // construct a suitable filename (which may be temporary at the beginning)
    std::string jname;
    if (isCompactor) {
      jname = "compaction-";
    } else {
      jname = "temp-";
    }

    jname.append(std::to_string(fid) + ".db");
    std::string filename =
        arangodb::basics::FileUtils::buildFilename(path(), jname);

    TRI_IF_FAILURE("CreateJournalDocumentCollection") {
      // simulate disk full
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
    }

    // remove an existing temporary file first
    if (TRI_ExistsFile(filename.c_str())) {
      // remove an existing file first
      TRI_UnlinkFile(filename.c_str());
    }

    datafile.reset(MMFilesDatafile::create(filename, fid, journalSize, true));
  }

  if (datafile == nullptr) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY_MMAP);
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_NO_JOURNAL);
  }

  // datafile is there now
  TRI_ASSERT(datafile != nullptr);

  if (isCompactor) {
    LOG_TOPIC(TRACE, arangodb::Logger::DATAFILES) << "created new compactor '"
                                                  << datafile->getName() << "'";
  } else {
    LOG_TOPIC(TRACE, arangodb::Logger::DATAFILES) << "created new journal '"
                                                  << datafile->getName() << "'";
  }

  // create a collection header, still in the temporary file
  MMFilesMarker* position;
  int res = datafile->reserveElement(sizeof(MMFilesCollectionHeaderMarker),
                                     &position, journalSize);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve1") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::DATAFILES)
        << "cannot create collection header in file '" << datafile->getName()
        << "': " << TRI_errno_string(res);

    // close the journal and remove it
    std::string temp(datafile->getName());
    datafile.reset();
    TRI_UnlinkFile(temp.c_str());

    THROW_ARANGO_EXCEPTION(res);
  }

  MMFilesCollectionHeaderMarker cm;
  MMFilesDatafileHelper::InitMarker(
      reinterpret_cast<MMFilesMarker*>(&cm), TRI_DF_MARKER_COL_HEADER,
      sizeof(MMFilesCollectionHeaderMarker), static_cast<TRI_voc_tick_t>(fid));

  cm._cid = _logicalCollection.id();
  res = datafile->writeCrcElement(position, &cm.base);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    int res = datafile->_lastError;
    LOG_TOPIC(ERR, arangodb::Logger::DATAFILES)
        << "cannot create collection header in file '" << datafile->getName()
        << "': " << TRI_last_error();

    // close the datafile and remove it
    std::string temp(datafile->getName());
    datafile.reset();
    TRI_UnlinkFile(temp.c_str());

    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_ASSERT(fid == datafile->fid());

  // if a physical file, we can rename it from the temporary name to the correct
  // name
  if (!isCompactor && datafile->isPhysical()) {
    // and use the correct name
    std::string oldName = datafile->getName();
    std::string jname("journal-" + std::to_string(datafile->fid()) + ".db");
    std::string filename =
        arangodb::basics::FileUtils::buildFilename(path(), jname);

    int res = datafile->rename(filename);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, arangodb::Logger::DATAFILES)
          << "failed to rename journal '" << datafile->getName() << "' to '"
          << filename << "': " << TRI_errno_string(res);

      std::string temp(datafile->getName());
      datafile.reset();
      TRI_UnlinkFile(temp.c_str());

      THROW_ARANGO_EXCEPTION(res);
    }

    LOG_TOPIC(TRACE, arangodb::Logger::DATAFILES) << "renamed journal from '"
                                                  << oldName << "' to '"
                                                  << filename << "'";
  }

  return datafile.release();
}

/// @brief remove a compactor file from the list of compactors
bool MMFilesCollection::removeCompactor(MMFilesDatafile* df) {
  TRI_ASSERT(df != nullptr);

  WRITE_LOCKER(writeLocker, _filesLock);

  for (auto it = _compactors.begin(); it != _compactors.end(); ++it) {
    if ((*it) == df) {
      // and finally remove the file from the _compactors vector
      _compactors.erase(it);
      return true;
    }
  }

  // not found
  return false;
}

/// @brief remove a datafile from the list of datafiles
bool MMFilesCollection::removeDatafile(MMFilesDatafile* df) {
  TRI_ASSERT(df != nullptr);

  WRITE_LOCKER(writeLocker, _filesLock);

  for (auto it = _datafiles.begin(); it != _datafiles.end(); ++it) {
    if ((*it) == df) {
      // and finally remove the file from the _datafiles vector
      _datafiles.erase(it);
      return true;
    }
  }

  // not found
  return false;
}

/// @brief iterates over a collection
bool MMFilesCollection::iterateDatafiles(
    std::function<bool(MMFilesMarker const*, MMFilesDatafile*)> const& cb) {
  READ_LOCKER(readLocker, _filesLock);

  if (!iterateDatafilesVector(_datafiles, cb) ||
      !iterateDatafilesVector(_compactors, cb) ||
      !iterateDatafilesVector(_journals, cb)) {
    return false;
  }
  return true;
}

/// @brief iterate over all datafiles in a vector
/// the caller must hold the _filesLock
bool MMFilesCollection::iterateDatafilesVector(
    std::vector<MMFilesDatafile*> const& files,
    std::function<bool(MMFilesMarker const*, MMFilesDatafile*)> const& cb) {
  for (auto const& datafile : files) {
    datafile->sequentialAccess();
    datafile->willNeed();

    if (!TRI_IterateDatafile(datafile, cb)) {
      return false;
    }

    if (datafile->isPhysical() && datafile->isSealed()) {
      datafile->randomAccess();
    }
  }

  return true;
}

/// @brief closes the datafiles passed in the vector
bool MMFilesCollection::closeDatafiles(
    std::vector<MMFilesDatafile*> const& files) {
  bool result = true;

  for (auto const& datafile : files) {
    TRI_ASSERT(datafile != nullptr);
    if (datafile->state() == TRI_DF_STATE_CLOSED) {
      continue;
    }

    int res = datafile->close();

    if (res != TRI_ERROR_NO_ERROR) {
      result = false;
    }
  }

  return result;
}

/// @brief export properties
void MMFilesCollection::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add("count", VPackValue(_initialCount));
  result.add("doCompact", VPackValue(_doCompact));
  result.add("indexBuckets", VPackValue(_indexBuckets));
  result.add("isVolatile", VPackValue(_isVolatile));
  result.add("journalSize", VPackValue(_journalSize));
  result.add("path", VPackValue(_path));

  TRI_ASSERT(result.isOpenObject());
}

void MMFilesCollection::figuresSpecific(
    std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  // fills in compaction status
  char const* lastCompactionStatus = "-";
  char lastCompactionStampString[21];
  lastCompactionStampString[0] = '-';
  lastCompactionStampString[1] = '\0';

  double lastCompactionStamp;

  {
    MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
    lastCompactionStatus = _lastCompactionStatus;
    lastCompactionStamp = _lastCompactionStamp;
  }

  if (lastCompactionStatus != nullptr) {
    if (lastCompactionStamp == 0.0) {
      lastCompactionStamp = TRI_microtime();
    }
    struct tm tb;
    time_t tt = static_cast<time_t>(lastCompactionStamp);
    TRI_gmtime(tt, &tb);
    strftime(&lastCompactionStampString[0], sizeof(lastCompactionStampString),
             "%Y-%m-%dT%H:%M:%SZ", &tb);
  }
  builder->add("documentReferences",
               VPackValue(_ditches.numMMFilesDocumentMMFilesDitches()));

  char const* waitingForDitch = _ditches.head();
  builder->add("waitingFor",
               VPackValue(waitingForDitch == nullptr ? "-" : waitingForDitch));

  // add datafile statistics
  MMFilesDatafileStatisticsContainer dfi = _datafileStatistics.all();
  MMFilesDatafileStatistics::CompactionStats stats = _datafileStatistics.getStats();

  builder->add("alive", VPackValue(VPackValueType::Object)); {
    builder->add("count", VPackValue(dfi.numberAlive));
    builder->add("size", VPackValue(dfi.sizeAlive));
    builder->close();  // alive
  }

  builder->add("dead", VPackValue(VPackValueType::Object)); {
    builder->add("count", VPackValue(dfi.numberDead));
    builder->add("size", VPackValue(dfi.sizeDead));
    builder->add("deletion", VPackValue(dfi.numberDeletions));
    builder->close();  // dead
  }

  builder->add("compactionStatus", VPackValue(VPackValueType::Object)); {
    builder->add("message", VPackValue(lastCompactionStatus));
    builder->add("time", VPackValue(&lastCompactionStampString[0]));

    builder->add("count", VPackValue(stats._compactionCount));
    builder->add("filesCombined", VPackValue(stats._filesCombined));
    builder->add("bytesRead", VPackValue(stats._compactionBytesRead));
    builder->add("bytesWritten", VPackValue(stats._compactionBytesWritten));
    builder->close();  // compactionStatus
  }

  // add file statistics
  READ_LOCKER(readLocker, _filesLock);

  size_t sizeDatafiles = 0;
  builder->add("datafiles", VPackValue(VPackValueType::Object));
  for (auto const& it : _datafiles) {
    sizeDatafiles += it->initSize();
  }

  builder->add("count", VPackValue(_datafiles.size()));
  builder->add("fileSize", VPackValue(sizeDatafiles));
  builder->close();  // datafiles

  size_t sizeJournals = 0;
  for (auto const& it : _journals) {
    sizeJournals += it->initSize();
  }
  builder->add("journals", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(_journals.size()));
  builder->add("fileSize", VPackValue(sizeJournals));
  builder->close();  // journals

  size_t sizeCompactors = 0;
  for (auto const& it : _compactors) {
    sizeCompactors += it->initSize();
  }
  builder->add("compactors", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(_compactors.size()));
  builder->add("fileSize", VPackValue(sizeCompactors));
  builder->close();  // compactors

  builder->add("revisions", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(_revisionsCache.size()));
  builder->add("size", VPackValue(_revisionsCache.memoryUsage()));
  builder->close();  // revisions

  builder->add("lastTick", VPackValue(_maxTick));
  builder->add("uncollectedLogfileEntries",
               VPackValue(uncollectedLogfileEntries()));
}

/// @brief iterate over a vector of datafiles and pick those with a specific
/// data range
std::vector<MMFilesCollection::DatafileDescription>
MMFilesCollection::datafilesInRange(TRI_voc_tick_t dataMin,
                                    TRI_voc_tick_t dataMax) {
  std::vector<DatafileDescription> result;

  auto apply = [&dataMin, &dataMax, &result](MMFilesDatafile const* datafile,
                                             bool isJournal) {
    DatafileDescription entry = {datafile, datafile->_dataMin,
                                 datafile->_dataMax, datafile->_tickMax,
                                 isJournal};
    LOG_TOPIC(TRACE, arangodb::Logger::DATAFILES)
        << "checking datafile " << datafile->fid() << " with data range "
        << datafile->_dataMin << " - " << datafile->_dataMax
        << ", tick max: " << datafile->_tickMax;

    LOG_TOPIC(TRACE, arangodb::Logger::REPLICATION)
        << "checking datafile " << datafile->fid() << " with data range "
        << datafile->_dataMin << " - " << datafile->_dataMax
        << ", tick max: " << datafile->_tickMax;

    if (datafile->_dataMin == 0 || datafile->_dataMax == 0) {
      // datafile doesn't have any data
      return;
    }

    TRI_ASSERT(datafile->_tickMin <= datafile->_tickMax);
    TRI_ASSERT(datafile->_dataMin <= datafile->_dataMax);

    if (dataMax < datafile->_dataMin) {
      // datafile is newer than requested range
      return;
    }

    if (dataMin > datafile->_dataMax) {
      // datafile is older than requested range
      return;
    }

    result.emplace_back(entry);
  };

  READ_LOCKER(readLocker, _filesLock);

  for (auto& it : _datafiles) {
    apply(it, false);
  }
  for (auto& it : _journals) {
    apply(it, true);
  }

  return result;
}

bool MMFilesCollection::applyForTickRange(
    TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
    std::function<bool(TRI_voc_tick_t foundTick,
                       MMFilesMarker const* marker)> const& callback) {
  LOG_TOPIC(TRACE, arangodb::Logger::DATAFILES)
      << "getting datafiles in data range " << dataMin << " - " << dataMax;

  std::vector<DatafileDescription> datafiles =
      datafilesInRange(dataMin, dataMax);
  // now we have a list of datafiles...

  size_t const n = datafiles.size();

  LOG_TOPIC(TRACE, arangodb::Logger::REPLICATION)
      << "getting datafiles in data range " << dataMin << " - " << dataMax << " produced " << n << " datafile(s)";

  for (size_t i = 0; i < n; ++i) {
    auto const& e = datafiles[i];
    MMFilesDatafile const* datafile = e._data;

    // we are reading from a journal that might be modified in parallel
    // so we must read-lock it
    CONDITIONAL_READ_LOCKER(readLocker, _filesLock, e._isJournal);

    if (!e._isJournal) {
      TRI_ASSERT(datafile->isSealed());
    }

    char const* ptr = datafile->_data;
    char const* end = ptr + datafile->currentSize();

    while (ptr < end) {
      auto const* marker = reinterpret_cast<MMFilesMarker const*>(ptr);

      if (marker->getSize() == 0) {
        // end of datafile
        break;
      }

      MMFilesMarkerType type = marker->getType();

      if (type <= TRI_DF_MARKER_MIN) {
        break;
      }

      ptr += MMFilesDatafileHelper::AlignedMarkerSize<size_t>(marker);

      if (type == TRI_DF_MARKER_BLANK) {
        // fully ignore these marker types. they don't need to be replicated,
        // but we also cannot stop iteration if we find one of these
        continue;
      }

      // get the marker's tick and check whether we should include it
      TRI_voc_tick_t foundTick = marker->getTick();

      if (foundTick <= dataMin) {
        // marker too old
        continue;
      }

      if (foundTick > dataMax) {
        // marker too new
        return false;  // hasMore = false
      }

      if (type != TRI_DF_MARKER_VPACK_DOCUMENT &&
          type != TRI_DF_MARKER_VPACK_REMOVE) {
        // found a non-data marker...

        // check if we can abort searching
        if (foundTick >= dataMax || (foundTick > e._tickMax && i == (n - 1))) {
          // fetched the last available marker
          return false;  // hasMore = false
        }

        continue;
      }

      // note the last tick we processed
      bool doAbort = false;
      if (!callback(foundTick, marker)) {
        doAbort = true;
      }

      if (foundTick >= dataMax || (foundTick >= e._tickMax && i == (n - 1))) {
        // fetched the last available marker
        return false;  // hasMore = false
      }

      if (doAbort) {
        return true;  // hasMore = true
      }
    }  // next marker in datafile
  }    // next datafile

  return false;  // hasMore = false
}

// @brief Return the number of documents in this collection
uint64_t MMFilesCollection::numberDocuments(transaction::Methods* trx) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  return primaryIndex()->size();
}

void MMFilesCollection::sizeHint(transaction::Methods* trx, int64_t hint) {
  if (hint <= 0) {
    return;
  }
  primaryIndex()->resize(trx, static_cast<size_t>(hint * 1.1));
}

/// @brief report extra memory used by indexes etc.
size_t MMFilesCollection::memory() const {
  return 0;  // TODO
}

/// @brief disallow compaction of the collection
void MMFilesCollection::preventCompaction() { _compactionLock.readLock(); }

/// @brief try disallowing compaction of the collection
bool MMFilesCollection::tryPreventCompaction() {
  return _compactionLock.tryReadLock();
}

/// @brief re-allow compaction of the collection
void MMFilesCollection::allowCompaction() { _compactionLock.unlock(); }

/// @brief exclusively lock the collection for compaction
void MMFilesCollection::lockForCompaction() { _compactionLock.writeLock(); }

/// @brief try to exclusively lock the collection for compaction
bool MMFilesCollection::tryLockForCompaction() {
  return _compactionLock.tryWriteLock();
}

/// @brief signal that compaction is finished
void MMFilesCollection::finishCompaction() { _compactionLock.unlock(); }

/// @brief iterator for index open
bool MMFilesCollection::openIndex(
    velocypack::Slice const& description,
    transaction::Methods& trx
) {
  // VelocyPack must be an index description
  if (!description.isObject()) {
    return false;
  }

  bool unused = false;
  auto idx = createIndex(trx, description, /*restore*/false, unused);

  if (idx == nullptr) {
    // error was already printed if we get here
    return false;
  }

  return true;
}

/// @brief initializes an index with a set of existing documents
void MMFilesCollection::fillIndex(
    std::shared_ptr<basics::LocalTaskQueue> queue,
    transaction::Methods& trx,
    Index* idx,
    std::shared_ptr<std::vector<std::pair<LocalDocumentId, velocypack::Slice>>> documents,
    bool skipPersistent
) {
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (!useSecondaryIndexes()) {
    return;
  }

  if (idx->isPersistent() && skipPersistent) {
    return;
  }

  try {
    // move task into thread pool
    std::shared_ptr<::MMFilesIndexFillerTask> worker;
    worker.reset(new ::MMFilesIndexFillerTask(queue, trx, idx, documents));
    queue->enqueue(worker);
  } catch (...) {
    // set error code
    queue->setStatus(TRI_ERROR_INTERNAL);
  }
}

uint32_t MMFilesCollection::indexBuckets() const { return _indexBuckets; }

int MMFilesCollection::fillAllIndexes(transaction::Methods& trx) {
  READ_LOCKER(guard, _indexesLock);
  return fillIndexes(trx, _indexes);
}

/// @brief Fill the given list of Indexes
int MMFilesCollection::fillIndexes(
    transaction::Methods& trx,
    std::vector<std::shared_ptr<arangodb::Index>> const& indexes,
    bool skipPersistent
) {
  // distribute the work to index threads plus this thread
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  size_t const n = indexes.size();

  if (n == 0 || (n == 1 &&
                 indexes[0].get()->type() ==
                     Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX)) {
    return TRI_ERROR_NO_ERROR;
  }

  bool rolledBack = false;
  auto rollbackAll = [&]() -> void {
    for (size_t i = 0; i < n; i++) {
      auto idx = indexes[i].get();
      if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
        continue;
      }
      if (idx->isPersistent()) {
        continue;
      }
      idx->unload();  // TODO: check is this safe? truncate not necessarily
                      // feasible
    }
  };

  TRI_ASSERT(n > 0);

  PerformanceLogScope logScope(
      std::string("fill-indexes-document-collection { collection: ") +
      _logicalCollection.vocbase().name() + "/" + _logicalCollection.name() +
      " }, indexes: " + std::to_string(n - 1));

  auto poster = [](std::function<void()> fn) -> void {
    SchedulerFeature::SCHEDULER->queue(RequestPriority::LOW, fn);
  };
  auto queue = std::make_shared<arangodb::basics::LocalTaskQueue>(poster);

  try {
    TRI_ASSERT(!ServerState::instance()->isCoordinator());

    // give the index a size hint
    auto primaryIdx = primaryIndex();
    auto nrUsed = primaryIdx->size();
    for (size_t i = 0; i < n; i++) {
      auto idx = indexes[i];
      if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
        continue;
      }
      idx.get()->sizeHint(trx, nrUsed);
    }

    // process documents a million at a time
    size_t blockSize = 1024 * 1024 * 1;

    if (nrUsed < blockSize) {
      blockSize = nrUsed;
    }
    if (blockSize == 0) {
      blockSize = 1;
    }

    auto documentsPtr = std::make_shared<std::vector<std::pair<LocalDocumentId, VPackSlice>>>();
    std::vector<std::pair<LocalDocumentId, VPackSlice>>& documents = *documentsPtr.get();
    documents.reserve(blockSize);

    auto insertInAllIndexes = [&]() -> void {
      for (size_t i = 0; i < n; ++i) {
        auto idx = indexes[i];
        if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
          continue;
        }
        fillIndex(queue, trx, idx.get(), documentsPtr, skipPersistent);
      }

      queue->dispatchAndWait();

      if (queue->status() != TRI_ERROR_NO_ERROR) {
        rollbackAll();
        rolledBack = true;
      }
    };

    if (nrUsed > 0) {
      arangodb::basics::BucketPosition position;
      uint64_t total = 0;

      while (true) {
        auto element = primaryIdx->lookupSequential(&trx, position, total);

        if (!element) {
          break;
        }

        LocalDocumentId const documentId = element.localDocumentId();

        uint8_t const* vpack = lookupDocumentVPack(documentId);
        if (vpack != nullptr) {
          documents.emplace_back(std::make_pair(documentId, VPackSlice(vpack)));

          if (documents.size() == blockSize) {
            // now actually fill the secondary indexes
            insertInAllIndexes();
            if (queue->status() != TRI_ERROR_NO_ERROR) {
              break;
            }
            documents.clear();
          }
        }
      }
    }

    // process the remainder of the documents
    if (queue->status() == TRI_ERROR_NO_ERROR && !documents.empty()) {
      insertInAllIndexes();
    }
  } catch (arangodb::basics::Exception const& ex) {
    queue->setStatus(ex.code());
    LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
        << "caught exception while filling indexes: " << ex.what();
  } catch (std::bad_alloc const&) {
    queue->setStatus(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
        << "caught exception while filling indexes: " << ex.what();
    queue->setStatus(TRI_ERROR_INTERNAL);
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
        << "caught unknown exception while filling indexes";
    queue->setStatus(TRI_ERROR_INTERNAL);
  }

  if (queue->status() != TRI_ERROR_NO_ERROR && !rolledBack) {
    try {
      rollbackAll();
    } catch (...) {
    }
  }

  return queue->status();
}

/// @brief opens an existing collection
int MMFilesCollection::openWorker(bool ignoreErrors) {
  auto& vocbase = _logicalCollection.vocbase();
  PerformanceLogScope logScope(
    std::string("open-collection { collection: ")
    + vocbase.name() + "/" + _logicalCollection.name() + " }"
  );

  try {
    // check for journals and datafiles
    MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
    auto res =
      engine->openCollection(&vocbase, &_logicalCollection, ignoreErrors);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
          << "cannot open '" << path() << "', check failed";

      return res;
    }

    return TRI_ERROR_NO_ERROR;
  } catch (basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "cannot load collection parameter file '" << path()
        << "': " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "cannot load collection parameter file '" << path()
        << "': " << ex.what();
    return TRI_ERROR_INTERNAL;
  }
}

void MMFilesCollection::open(bool ignoreErrors) {
  VPackBuilder builder;
  auto engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  auto& vocbase = _logicalCollection.vocbase();
  auto cid = _logicalCollection.id();

  engine->getCollectionInfo(vocbase, cid, builder, true, 0);

  VPackSlice initialCount =
      builder.slice().get(std::vector<std::string>({"parameters", "count"}));

  if (initialCount.isNumber()) {
    int64_t count = initialCount.getNumber<int64_t>();

    if (count > 0) {
      _initialCount = count;
    }
  }

  PerformanceLogScope logScope(
    std::string("open-document-collection { collection: ")
      + vocbase.name() + "/" + _logicalCollection.name() + " }"
  );

  int res = openWorker(ignoreErrors);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res,
        std::string("cannot open document collection from path '") + path() +
            "': " + TRI_errno_string(res));
  }

  arangodb::SingleCollectionTransaction trx(
    arangodb::transaction::StandaloneContext::Create(vocbase),
    _logicalCollection,
    AccessMode::Type::READ
  );

  // the underlying collections must not be locked here because the "load"
  // routine can be invoked from any other place, e.g. from an AQL query
  trx.addHint(transaction::Hints::Hint::LOCK_NEVER);

  {
    PerformanceLogScope logScope(
      std::string("iterate-markers { collection: ")
      + vocbase.name() + "/" + _logicalCollection.name() + " }"
    );

    // iterate over all markers of the collection
    res = iterateMarkersOnLoad(&trx);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res,
          std::string("cannot iterate data of document collection: ") +
              TRI_errno_string(res));
    }
  }

  // build the indexes meta-data, but do not fill the indexes yet
  {
    auto old = useSecondaryIndexes();

    // turn filling of secondary indexes off. we're now only interested in
    // getting
    // the indexes' definition. we'll fill them below ourselves.
    useSecondaryIndexes(false);

    try {
      detectIndexes(trx);
      useSecondaryIndexes(old);
    } catch (basics::Exception const& ex) {
      useSecondaryIndexes(old);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          ex.code(),
          std::string("cannot initialize collection indexes: ") + ex.what());
    } catch (std::exception const& ex) {
      useSecondaryIndexes(old);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          std::string("cannot initialize collection indexes: ") + ex.what());
    } catch (...) {
      useSecondaryIndexes(old);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "cannot initialize collection indexes: unknown exception");
    }
  }

  if (!engine->inRecovery() && !engine->upgrading()) {
    // build the index structures, and fill the indexes
    fillAllIndexes(trx);
  }

  // successfully opened collection. now adjust version number
  if (LogicalCollection::currentVersion() != _logicalCollection.version()
      && !engine->upgrading()) {
    setCurrentVersion();
    // updates have already happened elsewhere, it is safe to bump the number
  }
}

/// @brief iterate all markers of the collection
int MMFilesCollection::iterateMarkersOnLoad(transaction::Methods* trx) {
  // initialize state for iteration
  OpenIteratorState openState(&_logicalCollection, trx);

  if (_initialCount != -1) {
    _revisionsCache.sizeHint(_initialCount);
    sizeHint(trx, _initialCount);
    openState._initialCount = _initialCount;
  }

  // read all documents and fill primary index
  auto cb = [&openState](MMFilesMarker const* marker,
                         MMFilesDatafile* datafile) -> bool {
    return OpenIterator(marker, &openState, datafile);
  };

  iterateDatafiles(cb);

  LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
      << "found " << openState._documents << " document markers, "
      << openState._deletions << " deletion markers for collection '"
      << _logicalCollection.name() << "'";

  // pick up persistent id flag from state
  _hasAllPersistentLocalIds.store(openState._hasAllPersistentLocalIds);
  auto engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  LOG_TOPIC_IF(WARN, arangodb::Logger::ENGINES,
               !openState._hasAllPersistentLocalIds && !engine->upgrading())
     << "collection '" << _logicalCollection.name() << "' does not have all "
     << "persistent LocalDocumentIds; cannot be linked to an arangosearch view";

  // update the real statistics for the collection
  try {
    for (auto& it : openState._stats) {
      createStats(it.first, *(it.second));
    }
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

LocalDocumentId MMFilesCollection::lookupKey(transaction::Methods *trx,
                                                     VPackSlice const& key) const {
  MMFilesPrimaryIndex *index = primaryIndex();
  MMFilesSimpleIndexElement element = index->lookupKey(trx, key);
  return element ? LocalDocumentId(element.localDocumentId()) : LocalDocumentId();
}

Result MMFilesCollection::read(transaction::Methods* trx, VPackSlice const& key,
                               ManagedDocumentResult& result, bool lock) {
  TRI_IF_FAILURE("ReadDocumentNoLock") {
    // test what happens if no lock can be acquired
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("ReadDocumentNoLockExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
  if (lock) {
    int res = lockRead(useDeadlockDetector, trx->state());
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  TRI_DEFER(if (lock) { unlockRead(useDeadlockDetector, trx->state()); });

  Result res = lookupDocument(trx, key, result);
  if (res.fail()) {
    return res;
  }

  // we found a document
  return Result(TRI_ERROR_NO_ERROR);
}

Result MMFilesCollection::read(transaction::Methods* trx, StringRef const& key,
                               ManagedDocumentResult& result, bool lock){
  // copy string into a vpack string
  transaction::BuilderLeaser builder(trx);
  builder->add(VPackValuePair(key.data(), key.size(), VPackValueType::String));
  return read(trx, builder->slice(), result, lock);
}

bool MMFilesCollection::readDocument(transaction::Methods* trx,
                                     LocalDocumentId const& documentId,
                                     ManagedDocumentResult& result) const {
  uint8_t const* vpack = lookupDocumentVPack(documentId);
  if (vpack != nullptr) {
    result.setUnmanaged(vpack, documentId);
    return true;
  }
  return false;
}

bool MMFilesCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                 LocalDocumentId const& documentId,
                                                 IndexIterator::DocumentCallback const& cb) const {
  uint8_t const* vpack = lookupDocumentVPack(documentId);
  if (vpack != nullptr) {
    cb(documentId, VPackSlice(vpack));
    return true;
  }
  return false;
}

size_t MMFilesCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                   std::vector<std::pair<LocalDocumentId, uint8_t const*>>& documentIds,
                                                   IndexIterator::DocumentCallback const& cb){
  size_t count = 0;
  batchLookupRevisionVPack(documentIds);
  for (auto const& it : documentIds) {
    if (it.second) {
      cb(it.first, VPackSlice(it.second));
      ++count;
    }
  }
  return count;
}

bool MMFilesCollection::readDocumentConditional(
    transaction::Methods* trx, LocalDocumentId const& documentId,
    TRI_voc_tick_t maxTick, ManagedDocumentResult& result) {
  TRI_ASSERT(documentId.isSet());
  uint8_t const* vpack =
      lookupDocumentVPackConditional(documentId, maxTick, true);
  if (vpack != nullptr) {
    result.setUnmanaged(vpack, documentId);
    return true;
  }
  return false;
}

void MMFilesCollection::prepareIndexes(VPackSlice indexesSlice) {
  TRI_ASSERT(indexesSlice.isArray());

  bool foundPrimary = false;
  bool foundEdge = false;

  for (auto const& it : VPackArrayIterator(indexesSlice)) {
    auto const& s = it.get(arangodb::StaticStrings::IndexType);

    if (s.isString()) {
      if (s.isEqualString("primary")) {
        foundPrimary = true;
      } else if (s.isEqualString("edge")) {
        foundEdge = true;
      }
    }
  }

  {
    READ_LOCKER(guard, _indexesLock);
    for (auto const& idx : _indexes) {
      if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
        foundPrimary = true;
      } else if (TRI_COL_TYPE_EDGE == _logicalCollection.type() &&
                 idx->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        foundEdge = true;
      }
    }
  }

  std::vector<std::shared_ptr<arangodb::Index>> indexes;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  if (!foundPrimary ||
      (!foundEdge && TRI_COL_TYPE_EDGE == _logicalCollection.type())) {
    // we still do not have any of the default indexes, so create them now
    engine->indexFactory().fillSystemIndexes(_logicalCollection, indexes);
  }

  engine->indexFactory().prepareIndexes(
    _logicalCollection, indexesSlice, indexes
  );

  for (std::shared_ptr<Index>& idx : indexes) {
    if (ServerState::instance()->isRunningInCluster()) {
      addIndex(std::move(idx));
    } else {
      addIndexLocal(std::move(idx));
    }
  }

  {READ_LOCKER(guard, _indexesLock);
    TRI_ASSERT(!_indexes.empty());
    if (_indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
        (TRI_COL_TYPE_EDGE == _logicalCollection.type() &&
         (_indexes.size() < 2 || _indexes[1]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX))) {
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      for (auto const& it : _indexes) {
        LOG_TOPIC(ERR, arangodb::Logger::ENGINES) << "- " << it.get();
      }
  #endif
      std::string errorMsg("got invalid indexes for collection '");

      errorMsg.append(_logicalCollection.name());
      errorMsg.push_back('\'');
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    READ_LOCKER(guard, _indexesLock);
    bool foundPrimary = false;

    for (auto const& it : _indexes) {
      if (it->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
        if (foundPrimary) {
          std::string errorMsg("found multiple primary indexes for collection '");

          errorMsg.append(_logicalCollection.name());
          errorMsg.push_back('\'');
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
        }

        foundPrimary = true;
      }
    }
  }
#endif

  TRI_ASSERT(!_indexes.empty());
}

std::shared_ptr<Index> MMFilesCollection::lookupIndex(
    VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());

  // extract type
  auto value = info.get(arangodb::StaticStrings::IndexType);

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid index definition");
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  {READ_LOCKER(guard, _indexesLock);
    for (auto const& idx : _indexes) {
      if (idx->type() == type) {
        // Only check relevant indices
        if (idx->matchesDefinition(info)) {
          // We found an index for this definition.
          return idx;
        }
      }
    }
  }
  return nullptr;
}

std::shared_ptr<Index> MMFilesCollection::createIndex(arangodb::velocypack::Slice const& info,
                                                      bool restore, bool& created) {
  SingleCollectionTransaction trx(
    transaction::StandaloneContext::Create(_logicalCollection.vocbase()),
    _logicalCollection,
    AccessMode::Type::EXCLUSIVE
  );
  Result res = trx.begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto idx = createIndex(trx, info, restore, created);

  if (idx) {
    res = trx.commit();
  }

  return idx;
}

std::shared_ptr<Index> MMFilesCollection::createIndex(
    transaction::Methods& trx,
    velocypack::Slice const& info,
    bool restore,
    bool& created
) {
  // prevent concurrent dropping
//  TRI_ASSERT(trx->isLocked(&_logicalCollection, AccessMode::Type::READ));
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(info.isObject());
  std::shared_ptr<Index> idx = lookupIndex(info);

  if (idx != nullptr) { // We already have this index.
    created = false;
    return idx;
  }
  
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  // We are sure that we do not have an index of this type.
  // We also hold the lock. Create it

  bool generateKey = !restore; // Restore is not allowed to generate an id
  idx = engine->indexFactory().prepareIndexFromSlice(
    info, generateKey, _logicalCollection, false
  );
  if (!idx) {
    LOG_TOPIC(ERR, Logger::ENGINES) << "index creation failed while restoring";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED);
  }
  
  if (!restore) {
    TRI_UpdateTickServer(idx->id());
  }
  
  auto other = PhysicalCollection::lookupIndex(idx->id());
  if (other) {
    return other;
  }
  
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

  int res = saveIndex(trx, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(
      _logicalCollection->vocbase());
#endif
  // Until here no harm is done if sth fails. The shared ptr will clean up. if
  // left before

  addIndexLocal(idx);
  // trigger a rewrite
  if (!engine->inRecovery()) {
    auto builder = _logicalCollection.toVelocyPackIgnore(
      {"path", "statusString"}, true, true
    );
    _logicalCollection.properties(builder.slice(), false); // always a full-update
  }

  created = true;
  return idx;
}

/// @brief Persist an index information to file
int MMFilesCollection::saveIndex(
    transaction::Methods& trx,
    std::shared_ptr<Index> idx
) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  // we cannot persist PrimaryIndex
  TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  std::vector<std::shared_ptr<arangodb::Index>> indexListLocal;
  indexListLocal.emplace_back(idx);

  int res = fillIndexes(trx, indexListLocal, false);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  std::shared_ptr<VPackBuilder> builder;
  try {
    builder = idx->toVelocyPack(Index::makeFlags(Index::Serialize::ObjectId));
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "cannot save index definition: " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "cannot save index definition: " << ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES) << "cannot save index definition";
    return TRI_ERROR_INTERNAL;
  }
  if (builder == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES) << "cannot save index definition";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  auto& vocbase = _logicalCollection.vocbase();
  auto collectionId = _logicalCollection.id();
  VPackSlice data = builder->slice();
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  static_cast<MMFilesEngine*>(engine)->createIndex(vocbase, collectionId, idx->id(), data);

  if (!engine->inRecovery()) {
    // We need to write an index marker
    try {
      MMFilesCollectionMarker marker(
        TRI_DF_MARKER_VPACK_CREATE_INDEX, vocbase.id(), collectionId, data
      );
      MMFilesWalSlotInfoCopy slotInfo =
          MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);

      res = slotInfo.errorCode;
    } catch (arangodb::basics::Exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
          << "cannot save index definition: " << ex.what();
      res = ex.code();
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
          << "cannot save index definition: " << ex.what();
      return TRI_ERROR_INTERNAL;
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }
  }
  return res;
}

bool MMFilesCollection::addIndex(std::shared_ptr<arangodb::Index> idx) {

  WRITE_LOCKER(guard, _indexesLock);

  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return false;
    }
  }

  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

  _indexes.emplace_back(idx);
  if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
    TRI_ASSERT(idx->id() == 0);
    _primaryIndex = static_cast<MMFilesPrimaryIndex*>(idx.get());
  }
  return true;
}

void MMFilesCollection::addIndexLocal(std::shared_ptr<arangodb::Index> idx) {
  // primary index must be added at position 0
  TRI_ASSERT(idx->type() != arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
             _indexes.empty());

  if (!addIndex(idx)) {
    return;
  }

  // update statistics
  if (idx->isPersistent()) {
    ++_persistentIndexes;
  }
}

bool MMFilesCollection::dropIndex(TRI_idx_iid_t iid) {
  if (iid == 0) {
    // invalid index id or primary index
    events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
    return true;
  }

  auto& vocbase = _logicalCollection.vocbase();

  if (!removeIndex(iid)) {
    // We tried to remove an index that does not exist
    events::DropIndex("", std::to_string(iid),
                      TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    return false;
  }

  auto cid = _logicalCollection.id();
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);

  engine->dropIndex(&vocbase, cid, iid);

  {
    auto builder = _logicalCollection.toVelocyPackIgnore(
      {"path", "statusString"}, true, true
    );

    _logicalCollection.properties(builder.slice(), false); // always a full-update
  }

  if (!engine->inRecovery()) {
    int res = TRI_ERROR_NO_ERROR;

    VPackBuilder markerBuilder;
    markerBuilder.openObject();
    markerBuilder.add("id", VPackValue(std::to_string(iid)));
    markerBuilder.close();
    engine->dropIndexWalMarker(&vocbase, cid, markerBuilder.slice(), true, res);

    if (res == TRI_ERROR_NO_ERROR) {
      events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
          << "could not save index drop marker in log: "
          << TRI_errno_string(res);
      events::DropIndex("", std::to_string(iid), res);
    }
  }
  return true;
}

/// @brief removes an index by id
bool MMFilesCollection::removeIndex(TRI_idx_iid_t iid) {
  WRITE_LOCKER(guard, _indexesLock);

  size_t const n = _indexes.size();

  for (size_t i = 0; i < n; ++i) {
    auto idx = _indexes[i];

    if (!idx->canBeDropped()) {
      continue;
    }

    if (idx->id() == iid) {
      // found!
      idx->drop();

      _indexes.erase(_indexes.begin() + i);

      // update statistics
      if (idx->isPersistent()) {
        --_persistentIndexes;
      }

      return true;
    }
  }

  // not found
  return false;
}

std::unique_ptr<IndexIterator> MMFilesCollection::getAllIterator(transaction::Methods* trx) const {
  return std::unique_ptr<IndexIterator>(primaryIndex()->allIterator(trx));
}

std::unique_ptr<IndexIterator> MMFilesCollection::getAnyIterator(
    transaction::Methods* trx) const {
  return std::unique_ptr<IndexIterator>(primaryIndex()->anyIterator(trx));
}

void MMFilesCollection::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(LocalDocumentId const&)> callback) {
  primaryIndex()->invokeOnAllElements(callback);
}

/// @brief read locks a collection, with a timeout (in µseconds)
int MMFilesCollection::lockRead(bool useDeadlockDetector, TransactionState const* state, double timeout) {
  TRI_ASSERT(state != nullptr);

  if (state->isLockedShard(_logicalCollection.name())) {
    // do not lock by command
    return TRI_ERROR_NO_ERROR;
  }

  TRI_voc_tid_t tid = state->id();

  // LOCKING-DEBUG
  // std::cout << "BeginReadTimed: " << _name << std::endl;
  int iterations = 0;
  bool wasBlocked = false;
  uint64_t waitTime = 0;  // indicate that times uninitialized
  double startTime = 0.0;

  while (true) {
    TRY_READ_LOCKER(locker, _dataLock);

    if (locker.isLocked()) {
      // when we are here, we've got the read lock
      if (useDeadlockDetector) {
        _logicalCollection.vocbase()._deadlockDetector.addReader(
          tid, &_logicalCollection, wasBlocked
        );
      }

      // keep lock and exit loop
      locker.steal();

      return TRI_ERROR_NO_ERROR;
    }

    if (useDeadlockDetector) {
      try {
        if (!wasBlocked) {
          // insert reader
          wasBlocked = true;

          if (TRI_ERROR_DEADLOCK == _logicalCollection.vocbase()._deadlockDetector.setReaderBlocked(tid, &_logicalCollection)) {
            // deadlock
            LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
                << "deadlock detected while trying to acquire read-lock "
                << "on collection '" << _logicalCollection.name() << "'";

            return TRI_ERROR_DEADLOCK;
          }

          LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
              << "waiting for read-lock on collection '"
              << _logicalCollection.name() << "'";
          // intentionally falls through
        } else if (++iterations >= 5) {
          // periodically check for deadlocks
          TRI_ASSERT(wasBlocked);
          iterations = 0;

          if (TRI_ERROR_DEADLOCK == _logicalCollection.vocbase()._deadlockDetector.detectDeadlock(tid, &_logicalCollection, false)) {
            // deadlock
            _logicalCollection.vocbase()._deadlockDetector.unsetReaderBlocked(
                tid, &_logicalCollection
            );
            LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
                << "deadlock detected while trying to acquire read-lock "
                << "on collection '" << _logicalCollection.name() << "'";

            return TRI_ERROR_DEADLOCK;
          }
        }
      } catch (...) {
        // clean up!
        if (wasBlocked) {
          _logicalCollection.vocbase()._deadlockDetector.unsetReaderBlocked(
            tid, &_logicalCollection
          );
        }

        // always exit
        return TRI_ERROR_OUT_OF_MEMORY;
      }
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
      if (useDeadlockDetector) {
        _logicalCollection.vocbase()._deadlockDetector.unsetReaderBlocked(
          tid, &_logicalCollection
        );
      }

      LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
          << "timed out after " << timeout
          << " s waiting for read-lock on collection '"
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

/// @brief write locks a collection, with a timeout
int MMFilesCollection::lockWrite(bool useDeadlockDetector, TransactionState const* state, double timeout) {
  TRI_ASSERT(state != nullptr);

  if (state->isLockedShard(_logicalCollection.name())) {
    // do not lock by command
    return TRI_ERROR_NO_ERROR;
  }

  TRI_voc_tid_t tid = state->id();

  // LOCKING-DEBUG
  // std::cout << "BeginWriteTimed: " << document->_info._name << std::endl;
  int iterations = 0;
  bool wasBlocked = false;
  uint64_t waitTime = 0;  // indicate that times uninitialized
  double startTime = 0.0;

  while (true) {
    TRY_WRITE_LOCKER(locker, _dataLock);

    if (locker.isLocked()) {
      // register writer
      if (useDeadlockDetector) {
        _logicalCollection.vocbase()._deadlockDetector.addWriter(
          tid, &_logicalCollection, wasBlocked
        );
      }

      // keep lock and exit loop
      locker.steal();

      return TRI_ERROR_NO_ERROR;
    }

    if (useDeadlockDetector) {
      try {
        if (!wasBlocked) {
          // insert writer
          wasBlocked = true;

          if (TRI_ERROR_DEADLOCK == _logicalCollection.vocbase()._deadlockDetector.setWriterBlocked(tid, &_logicalCollection)) {
            // deadlock
            LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
                << "deadlock detected while trying to acquire "
                << "write-lock on collection '"
                << _logicalCollection.name() << "'";

            return TRI_ERROR_DEADLOCK;
          }

          LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
              << "waiting for write-lock on collection '"
              << _logicalCollection.name() << "'";
        } else if (++iterations >= 5) {
          // periodically check for deadlocks
          TRI_ASSERT(wasBlocked);
          iterations = 0;

          if (TRI_ERROR_DEADLOCK == _logicalCollection.vocbase()._deadlockDetector.detectDeadlock(tid, &_logicalCollection, true)) {
            // deadlock
            _logicalCollection.vocbase()._deadlockDetector.unsetWriterBlocked(
              tid, &_logicalCollection
            );
            LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
                << "deadlock detected while trying to acquire "
                   "write-lock on collection '"
                << _logicalCollection.name() << "'";

            return TRI_ERROR_DEADLOCK;
          }
        }
      } catch (...) {
        // clean up!
        if (wasBlocked) {
          _logicalCollection.vocbase()._deadlockDetector.unsetWriterBlocked(
            tid, &_logicalCollection
          );
        }

        // always exit
        return TRI_ERROR_OUT_OF_MEMORY;
      }
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
      if (useDeadlockDetector) {
        _logicalCollection.vocbase()._deadlockDetector.unsetWriterBlocked(
          tid, &_logicalCollection
        );
      }

      LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
          << "timed out after " << timeout
          << " s waiting for write-lock on collection '"
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

/// @brief read unlocks a collection
int MMFilesCollection::unlockRead(bool useDeadlockDetector, TransactionState const* state) {
  TRI_ASSERT(state != nullptr);

  if (state->isLockedShard(_logicalCollection.name())) {
    // do not lock by command
    return TRI_ERROR_NO_ERROR;
  }

  TRI_voc_tid_t tid = state->id();

  if (useDeadlockDetector) {
    // unregister reader
    try {
      _logicalCollection.vocbase()._deadlockDetector.unsetReader(
        tid, &_logicalCollection
      );
    } catch (...) {
    }
  }

  // LOCKING-DEBUG
  // std::cout << "EndRead: " << _name << std::endl;
  _dataLock.unlockRead();

  return TRI_ERROR_NO_ERROR;
}

/// @brief write unlocks a collection
int MMFilesCollection::unlockWrite(bool useDeadlockDetector, TransactionState const* state) {
  TRI_ASSERT(state != nullptr);

  if (state->isLockedShard(_logicalCollection.name())) {
    // do not lock by command
    return TRI_ERROR_NO_ERROR;
  }

  TRI_voc_tid_t tid = state->id();

  if (useDeadlockDetector) {
    // unregister writer
    try {
      _logicalCollection.vocbase()._deadlockDetector.unsetWriter(
        tid, &_logicalCollection
      );
    } catch (...) {
      // must go on here to unlock the lock
    }
  }

  // LOCKING-DEBUG
  // std::cout << "EndWrite: " << _name << std::endl;
  _dataLock.unlockWrite();

  return TRI_ERROR_NO_ERROR;
}

Result MMFilesCollection::truncate(
    transaction::Methods& trx,
    OperationOptions& options
) {
  auto primaryIdx = primaryIndex();

  options.ignoreRevs = true;

  // create remove marker
  transaction::BuilderLeaser builder(&trx);

  auto callback = [&](MMFilesSimpleIndexElement const& element) {
    LocalDocumentId const oldDocumentId = element.localDocumentId();
    uint8_t const* vpack = lookupDocumentVPack(oldDocumentId);
    if (vpack != nullptr) {
      builder->clear();
      VPackSlice oldDoc(vpack);

      LocalDocumentId const documentId = LocalDocumentId::create();
      TRI_voc_rid_t revisionId;

      newObjectForRemove(
        &trx, oldDoc, *builder.get(), options.isRestore, revisionId
      );

      Result res = removeFastPath(trx, revisionId, oldDocumentId, VPackSlice(vpack),
                                  options, documentId, builder->slice());

      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    return true;
  };
  try {
    primaryIdx->invokeOnAllElementsForRemoval(callback);
  } catch(basics::Exception const& e) {
    return Result(e.code(), e.message());
  } catch(std::exception const& e) {
    return Result(TRI_ERROR_INTERNAL, e.what());
  } catch(...) {
    return Result(TRI_ERROR_INTERNAL, "unknown error during truncate");
  }

  READ_LOCKER(guard, _indexesLock);
  auto indexes = _indexes;
  size_t const n = indexes.size();

  TRI_voc_tick_t tick = TRI_NewTickServer();
  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
    idx->afterTruncate(tick);
  }

  return Result();
}

LocalDocumentId MMFilesCollection::reuseOrCreateLocalDocumentId(OperationOptions const& options) const {
  if (options.recoveryData != nullptr) {
    auto marker = static_cast<MMFilesWalMarker*>(options.recoveryData);
    if (marker->hasLocalDocumentId()) {
      return marker->getLocalDocumentId();
    }
    // falls through intentionally
  }

  // new operation, no recovery -> generate a new LocalDocumentId
  return LocalDocumentId::create();
}

Result MMFilesCollection::insert(
    arangodb::transaction::Methods* trx, VPackSlice const slice,
    arangodb::ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_tick_t& revisionId,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock) {
  
  LocalDocumentId const documentId = reuseOrCreateLocalDocumentId(options);
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());
  transaction::BuilderLeaser builder(trx);
  VPackSlice newSlice;
  Result res(TRI_ERROR_NO_ERROR);

  if (options.recoveryData == nullptr) {
    res = newObjectForInsert(trx, slice, isEdgeCollection,
                             *builder.get(), options.isRestore, revisionId);

    if (res.fail()) {
      return res;
    }

    newSlice = builder->slice();
  } else {
    TRI_ASSERT(slice.isObject());
    // we can get away with the fast hash function here, as key values are
    // restricted to strings
    newSlice = slice;

    VPackSlice keySlice = newSlice.get(StaticStrings::KeyString);

    if (keySlice.isString()) {
      VPackValueLength l;
      char const* p = keySlice.getString(l);

      TRI_ASSERT(p != nullptr);
      _logicalCollection.keyGenerator()->track(p, l);
    }

    VPackSlice revSlice = newSlice.get(StaticStrings::RevString);

    if (revSlice.isString()) {
      VPackValueLength l;
      char const* p = revSlice.getString(l);
      TRI_ASSERT(p != nullptr);
      revisionId = TRI_StringToRid(p, l, false);
    }
  }
  
  // create marker
  MMFilesCrudMarker insertMarker(
      TRI_DF_MARKER_VPACK_DOCUMENT,
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
      documentId,
      newSlice);

  MMFilesWalMarker const* marker;
  if (options.recoveryData == nullptr) {
    marker = &insertMarker;
  } else {
    marker = static_cast<MMFilesWalMarker*>(options.recoveryData);
  }

  // now insert into indexes
  TRI_IF_FAILURE("InsertDocumentNoLock") {
    // test what happens if no lock can be acquired
    return Result(TRI_ERROR_DEBUG);
  }

  // TODO Do we need a LogicalCollection here?
  MMFilesDocumentOperation operation(
    &_logicalCollection, TRI_VOC_DOCUMENT_OPERATION_INSERT
  );

  TRI_IF_FAILURE("InsertDocumentNoHeader") {
    // test what happens if no header can be acquired
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("InsertDocumentNoHeaderExcept") {
    // test what happens if no header can be acquired
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  VPackSlice const doc(marker->vpack());
  operation.setDocumentIds(MMFilesDocumentDescriptor(),
                           MMFilesDocumentDescriptor(documentId, doc.begin()));

  try {
    insertLocalDocumentId(documentId, marker->vpack(), 0, true, true);
    // and go on with the insertion...
  } catch (basics::Exception const& ex) {
    return Result(ex.code());
  } catch (std::bad_alloc const&) {
    return Result(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }
      
  TRI_ASSERT(keyLockInfo != nullptr);
  if (keyLockInfo->shouldLock) {
    TRI_ASSERT(lock);
    lockKey(*keyLockInfo, newSlice.get(StaticStrings::KeyString));
  }

  res = Result();
  {
    // use lock?
    bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
    try {
      arangodb::MMFilesCollectionWriteLocker collectionLocker(
          this, useDeadlockDetector, trx->state(), lock);

      try {
        // insert into indexes
        res = insertDocument(
          *trx,
          documentId,
          revisionId,
          doc,
          operation,
          marker,
          options,
          options.waitForSync
        );
      } catch (basics::Exception const& ex) {
        res = Result(ex.code());
      } catch (std::bad_alloc const&) {
        res = Result(TRI_ERROR_OUT_OF_MEMORY);
      } catch (std::exception const& ex) {
        res = Result(TRI_ERROR_INTERNAL, ex.what());
      } catch (...) {
        res = Result(TRI_ERROR_INTERNAL);
      }

      if (res.ok() && callbackDuringLock != nullptr) {
        res = callbackDuringLock();
      }
    } catch (...) {
      // the collectionLocker may have thrown in its constructor...
      // if it did, then we need to manually remove the revision id
      // from the list of revisions
      try {
        removeLocalDocumentId(documentId, false);
      } catch (...) {
      }
      throw;
    }

    if (res.fail()) {
      operation.revert(trx);
    }
  }

  if (res.ok()) {
    result.setManaged(doc.begin(), documentId);
    // store the tick that was used for writing the document
    resultMarkerTick = operation.tick();
  }

  return res;
}

bool MMFilesCollection::isFullyCollected() const {
  int64_t uncollected = _uncollectedLogfileEntries.load();
  return (uncollected == 0);
}

MMFilesDocumentPosition MMFilesCollection::lookupDocument(LocalDocumentId const& documentId) const {
  TRI_ASSERT(documentId.isSet());
  MMFilesDocumentPosition const old = _revisionsCache.lookup(documentId);
  if (old) {
    return old;
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "got invalid revision value on lookup");
}

uint8_t const* MMFilesCollection::lookupDocumentVPack(LocalDocumentId const& documentId) const {
  TRI_ASSERT(documentId.isSet());

  MMFilesDocumentPosition const old = _revisionsCache.lookup(documentId);
  if (old) {
    uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
    TRI_ASSERT(VPackSlice(vpack).isObject());
    return vpack;
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "got invalid vpack value on lookup");
}

uint8_t const* MMFilesCollection::lookupDocumentVPackConditional(
    LocalDocumentId const& documentId, TRI_voc_tick_t maxTick, bool excludeWal) const {
  TRI_ASSERT(documentId.isSet());

  MMFilesDocumentPosition const old = _revisionsCache.lookup(documentId);
  if (!old) {
    return nullptr;
  }
  if (excludeWal && old.pointsToWal()) {
    return nullptr;
  }

  uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());

  if (maxTick > 0) {
    MMFilesMarker const* marker = reinterpret_cast<MMFilesMarker const*>(
        vpack -
        MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
    if (marker->getTick() > maxTick) {
      return nullptr;
    }
  }

  return vpack;
}

void MMFilesCollection::batchLookupRevisionVPack(std::vector<std::pair<LocalDocumentId, uint8_t const*>>& documentIds) const {
  _revisionsCache.batchLookup(documentIds);
}

MMFilesDocumentPosition MMFilesCollection::insertLocalDocumentId(
    LocalDocumentId const& documentId, uint8_t const* dataptr, TRI_voc_fid_t fid,
    bool isInWal, bool shouldLock) {
  TRI_ASSERT(documentId.isSet());
  TRI_ASSERT(dataptr != nullptr);
  return _revisionsCache.insert(documentId, dataptr, fid, isInWal, shouldLock);
}

void MMFilesCollection::insertLocalDocumentId(MMFilesDocumentPosition const& position,
                                              bool shouldLock) {
  return _revisionsCache.insert(position, shouldLock);
}

void MMFilesCollection::updateLocalDocumentId(LocalDocumentId const& documentId,
                                              uint8_t const* dataptr,
                                              TRI_voc_fid_t fid, bool isInWal) {
  TRI_ASSERT(documentId.isSet());
  TRI_ASSERT(dataptr != nullptr);
  _revisionsCache.update(documentId, dataptr, fid, isInWal);
}

bool MMFilesCollection::updateLocalDocumentIdConditional(
    LocalDocumentId const& documentId, MMFilesMarker const* oldPosition,
    MMFilesMarker const* newPosition, TRI_voc_fid_t newFid, bool isInWal) {
  TRI_ASSERT(documentId.isSet());
  TRI_ASSERT(newPosition != nullptr);
  return _revisionsCache.updateConditional(documentId, oldPosition, newPosition,
                                           newFid, isInWal);
}

void MMFilesCollection::removeLocalDocumentId(LocalDocumentId const& documentId,
                                              bool updateStats) {
  TRI_ASSERT(documentId.isSet());
  if (updateStats) {
    MMFilesDocumentPosition const old =
        _revisionsCache.fetchAndRemove(documentId);
    if (old && !old.pointsToWal() && old.fid() != 0) {
      TRI_ASSERT(old.dataptr() != nullptr);
      uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
      auto oldMarker = reinterpret_cast<MMFilesMarker const*>(vpack - MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
      int64_t size = MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(oldMarker);
      _datafileStatistics.increaseDead(old.fid(), 1, size);
    }
  } else {
    _revisionsCache.remove(documentId);
  }
}

bool MMFilesCollection::hasAllPersistentLocalIds() const {
  return _hasAllPersistentLocalIds.load();
}
  
Result MMFilesCollection::persistLocalDocumentIdsForDatafile(
    MMFilesCollection& collection, MMFilesDatafile& file) {
  Result res;
  // make a first pass to count documents and determine output size
  size_t numDocuments = 0;
  bool ok = TRI_IterateDatafile(&file, ::countDocumentsIterator, &numDocuments);
  if (!ok) {
    res.reset(TRI_ERROR_INTERNAL, "could not count documents");
    return res;
  }

  size_t outputSizeLimit =
      file.currentSize() + (numDocuments * sizeof(LocalDocumentId));
  MMFilesDatafile* outputFile = nullptr;
  {
    READ_UNLOCKER(unlocker, collection._filesLock);
    outputFile = collection.createCompactor(file.fid(), outputSizeLimit);
  }
  if (nullptr == outputFile) {
    return Result(TRI_ERROR_INTERNAL);
  }

  res =
      TRI_IterateDatafile(&file, ::persistLocalDocumentIdIterator, outputFile);
  if (res.fail()) {
    return res;
  }

  {
    READ_UNLOCKER(unlocker, collection._filesLock);
    res = collection.closeCompactor(outputFile);

    if (res.fail()) {
      return res;
    }

    // TODO detect error in replacement?
    MMFilesCompactorThread::RenameDatafileCallback(
        &file, outputFile, &collection._logicalCollection);
  }

  return res;
}

Result MMFilesCollection::persistLocalDocumentIds() {
  if (_logicalCollection.version() >=
      LogicalCollection::CollectionVersions::VERSION_34) {
    // already good, just continue
    return Result();
  }

  WRITE_LOCKER(dataLocker, _dataLock);
  TRI_ASSERT(_compactors.empty());

  // convert journal to datafile first
  int res = rotateActiveJournal();
  if (TRI_ERROR_NO_ERROR != res && TRI_ERROR_ARANGO_NO_JOURNAL != res) {
    return Result(res);
  }

  // now handle datafiles
  {
    READ_LOCKER(locker, _filesLock);
    for (auto file : _datafiles) {
      Result result = persistLocalDocumentIdsForDatafile(*this, *file);
      if (result.fail()) {
        return result;
      }
    }
  }

  _hasAllPersistentLocalIds.store(true);

  TRI_ASSERT(_compactors.empty());
  TRI_ASSERT(_journals.empty());

  // mark collection as upgraded so we can avoid re-checking
  setCurrentVersion();

  return Result();
}

void MMFilesCollection::setCurrentVersion() {
  _logicalCollection.setVersion(
      static_cast<LogicalCollection::CollectionVersions>(
          LogicalCollection::currentVersion()));

  bool const doSync =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database")
          ->forceSyncProperties();
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  engine->changeCollection(_logicalCollection.vocbase(),
                           _logicalCollection.id(), _logicalCollection, doSync);
}

/// @brief creates a new entry in the primary index
Result MMFilesCollection::insertPrimaryIndex(transaction::Methods* trx,
                                             LocalDocumentId const& documentId,
                                             VPackSlice const& doc,
                                             OperationOptions& options) {
  TRI_IF_FAILURE("InsertPrimaryIndex") { return Result(TRI_ERROR_DEBUG); }

  // insert into primary index
  return primaryIndex()->insertKey(trx, documentId, doc, options.indexOperationMode);
}

/// @brief deletes an entry from the primary index
Result MMFilesCollection::deletePrimaryIndex(
    arangodb::transaction::Methods* trx, LocalDocumentId const& documentId,
    VPackSlice const& doc, OperationOptions& options) {
  TRI_IF_FAILURE("DeletePrimaryIndex") { return Result(TRI_ERROR_DEBUG); }

  return primaryIndex()->removeKey(trx, documentId, doc, options.indexOperationMode);
}

/// @brief creates a new entry in the secondary indexes
Result MMFilesCollection::insertSecondaryIndexes(
    arangodb::transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_IF_FAILURE("InsertSecondaryIndexes") { return Result(TRI_ERROR_DEBUG); }

  bool const useSecondary = useSecondaryIndexes();
  if (!useSecondary && _persistentIndexes == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  Result result;

  READ_LOCKER(guard, _indexesLock);

  auto indexes = _indexes;
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

    if (!useSecondary && !idx->isPersistent()) {
      continue;
    }

    Result res = idx->insert(trx, documentId, doc, mode);

    // in case of no-memory, return immediately
    if (res.errorNumber() == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }
    if (!res.ok()) {
      if (res.errorNumber() == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
          result.ok()) {
        // "prefer" unique constraint violated
        result = res;
      }
    }
  }

  return result;
}

/// @brief deletes an entry from the secondary indexes
Result MMFilesCollection::deleteSecondaryIndexes(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  // Coordintor doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  bool const useSecondary = useSecondaryIndexes();
  if (!useSecondary && _persistentIndexes == 0) {
    return Result(TRI_ERROR_NO_ERROR);
  }

  TRI_IF_FAILURE("DeleteSecondaryIndexes") { return Result(TRI_ERROR_DEBUG); }

  Result result;

  READ_LOCKER(guard, _indexesLock);
  auto indexes = _indexes;
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

    if (!useSecondary && !idx->isPersistent()) {
      continue;
    }

    Result res = idx->remove(trx, documentId, doc, mode);

    if (res.fail()) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

/// @brief enumerate all indexes of the collection, but don't fill them yet
int MMFilesCollection::detectIndexes(transaction::Methods& trx) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  VPackBuilder builder;

  engine->getCollectionInfo(
    _logicalCollection.vocbase(),
    _logicalCollection.id(),
    builder,
    true,
    UINT64_MAX
  );

  // iterate over all index files
  for (auto const& it : VPackArrayIterator(builder.slice().get("indexes"))) {
    bool ok = openIndex(it, trx);

    if (!ok) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "cannot load index for collection '"
        << _logicalCollection.name() << "'";
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief insert a document into all indexes known to
///        this collection.
///        This function guarantees all or nothing,
///        If it returns NO_ERROR all indexes are filled.
///        If it returns an error no documents are inserted
Result MMFilesCollection::insertIndexes(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    OperationOptions& options
) {
  // insert into primary index first
  auto res = insertPrimaryIndex(&trx, documentId, doc, options);

  if (res.fail()) {
    // insert has failed
    return res;
  }

  // insert into secondary indexes
  res = insertSecondaryIndexes(trx, documentId, doc, options.indexOperationMode);

  if (res.fail()) {
    deleteSecondaryIndexes(trx, documentId, doc,
                           Index::OperationMode::rollback);
    deletePrimaryIndex(&trx, documentId, doc, options);
  }
  return res;
}

/// @brief insert a document, low level worker
/// the caller must make sure the write lock on the collection is held
Result MMFilesCollection::insertDocument(
    arangodb::transaction::Methods& trx,
    LocalDocumentId const& documentId,
    TRI_voc_rid_t revisionId,
    velocypack::Slice const& doc,
    MMFilesDocumentOperation& operation,
    MMFilesWalMarker const* marker,
    OperationOptions& options,
    bool& waitForSync
) {
  Result res = insertIndexes(trx, documentId, doc, options);

  if (res.fail()) {
    return res;
  }

  operation.indexed();

  TRI_IF_FAILURE("InsertDocumentNoOperation") { return Result(TRI_ERROR_DEBUG); }

  TRI_IF_FAILURE("InsertDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return Result(
    static_cast<MMFilesTransactionState*>(trx.state())
      ->addOperation(documentId, revisionId, operation, marker, waitForSync));
}

Result MMFilesCollection::update(
    arangodb::transaction::Methods* trx,
    VPackSlice const newSlice, ManagedDocumentResult& result,
    OperationOptions& options, TRI_voc_tick_t& resultMarkerTick, bool lock,
    TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
    VPackSlice const key,
    std::function<Result(void)> callbackDuringLock) {
  LocalDocumentId const documentId = reuseOrCreateLocalDocumentId(options);
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  TRI_IF_FAILURE("UpdateDocumentNoLock") { return Result(TRI_ERROR_DEBUG); }

  bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
  arangodb::MMFilesCollectionWriteLocker collectionLocker(
      this, useDeadlockDetector, trx->state(), lock);

  // get the previous revision
  Result res = lookupDocument(trx, key, previous);

  if (res.fail()) {
    return res;
  }

  LocalDocumentId const oldDocumentId = previous.localDocumentId();
  VPackSlice const oldDoc(previous.vpack());
  TRI_voc_rid_t const oldRevisionId =
      transaction::helpers::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  TRI_IF_FAILURE("UpdateDocumentNoMarker") {
    // test what happens when no marker can be created
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("UpdateDocumentNoMarkerExcept") {
    // test what happens when no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // Check old revision:
  if (!options.ignoreRevs) {
    TRI_voc_rid_t expectedRev = 0;
    if (newSlice.isObject()) {
      expectedRev = TRI_ExtractRevisionId(newSlice);
    }
    int res = checkRevision(trx, expectedRev, prevRev);
    if (res != TRI_ERROR_NO_ERROR) {
      return Result(res);
    }
  }

  if (newSlice.length() <= 1) {
    // no need to do anything
    result = previous;

    if (_logicalCollection.waitForSync()) {
      options.waitForSync = true;
    }

    return Result();
  }

  // merge old and new values
  TRI_voc_rid_t revisionId;
  transaction::BuilderLeaser builder(trx);
  if (options.recoveryData == nullptr) {
    res = mergeObjectsForUpdate(trx, oldDoc, newSlice, isEdgeCollection,
                                options.mergeObjects, options.keepNull,
                                *builder.get(), options.isRestore, revisionId);

    if (res.fail()) {
      return res;
    }

    if (_isDBServer) {
      // Need to check that no sharding keys have changed:
      if (arangodb::shardKeysChanged(
            _logicalCollection,
            oldDoc,
            builder->slice(),
            false
         )) {
        return Result(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
      }
    }
  } else {
    revisionId = TRI_ExtractRevisionId(VPackSlice(static_cast<MMFilesMarkerEnvelope*>(options.recoveryData)->vpack()));
  }

  // create marker
  MMFilesCrudMarker updateMarker(
      TRI_DF_MARKER_VPACK_DOCUMENT,
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
      documentId,
      builder->slice());
  MMFilesWalMarker const* marker;

  if (options.recoveryData == nullptr) {
    marker = &updateMarker;
  } else {
    marker = static_cast<MMFilesWalMarker*>(options.recoveryData);
  }

  VPackSlice const newDoc(marker->vpack());
  MMFilesDocumentOperation operation(
    &_logicalCollection, TRI_VOC_DOCUMENT_OPERATION_UPDATE
  );

  try {
    insertLocalDocumentId(documentId, marker->vpack(), 0, true, true);

    operation.setDocumentIds(MMFilesDocumentDescriptor(oldDocumentId, oldDoc.begin()),
                             MMFilesDocumentDescriptor(documentId, newDoc.begin()));

    res = updateDocument(
      *trx,
      revisionId,
      oldDocumentId,
      oldDoc,
      documentId,
      newDoc,
      operation,
      marker,
      options,
      options.waitForSync
    );

    if (res.ok() && callbackDuringLock != nullptr) {
      res = callbackDuringLock();
    }
  } catch (basics::Exception const& ex) {
    res = Result(ex.code());
  } catch (std::bad_alloc const&) {
    res = Result(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    res = Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    res = Result(TRI_ERROR_INTERNAL);
  }

  if (res.fail()) {
    operation.revert(trx);
  } else {
    result.setManaged(newDoc.begin(), documentId);

    if (options.waitForSync) {
      // store the tick that was used for writing the new document
      resultMarkerTick = operation.tick();
    }
  }

  return res;
}

Result MMFilesCollection::replace(
    transaction::Methods* trx, VPackSlice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous,
    std::function<Result(void)> callbackDuringLock) {
  LocalDocumentId const documentId = reuseOrCreateLocalDocumentId(options);
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  TRI_IF_FAILURE("ReplaceDocumentNoLock") { return Result(TRI_ERROR_DEBUG); }

  // get the previous revision
  VPackSlice key = newSlice.get(StaticStrings::KeyString);
  if (key.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
  arangodb::MMFilesCollectionWriteLocker collectionLocker(
      this, useDeadlockDetector, trx->state(), lock);

  // get the previous revision
  Result res = lookupDocument(trx, key, previous);

  if (res.fail()) {
    return res;
  }

  TRI_IF_FAILURE("ReplaceDocumentNoMarker") {
    // test what happens when no marker can be created
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("ReplaceDocumentNoMarkerExcept") {
    // test what happens when no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  uint8_t const* vpack = previous.vpack();
  LocalDocumentId const oldDocumentId = previous.localDocumentId();

  VPackSlice oldDoc(vpack);
  TRI_voc_rid_t oldRevisionId =
      transaction::helpers::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  // Check old revision:
  if (!options.ignoreRevs) {
    TRI_voc_rid_t expectedRev = 0;
    if (newSlice.isObject()) {
      expectedRev = TRI_ExtractRevisionId(newSlice);
    }
    int res = checkRevision(trx, expectedRev, prevRev);
    if (res != TRI_ERROR_NO_ERROR) {
      return Result(res);
    }
  }

  // merge old and new values
  TRI_voc_rid_t revisionId;
  transaction::BuilderLeaser builder(trx);
  res = newObjectForReplace(trx, oldDoc, newSlice, isEdgeCollection, *builder.get(),
                            options.isRestore, revisionId);

  if (res.fail()) {
    return res;
  }

  if (options.recoveryData == nullptr) {
    if (_isDBServer) {
      // Need to check that no sharding keys have changed:
      if (arangodb::shardKeysChanged(
            _logicalCollection,
            oldDoc,
            builder->slice(),
            false
        )) {
        return Result(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
      }
    }
  }

  // create marker
  MMFilesCrudMarker replaceMarker(
      TRI_DF_MARKER_VPACK_DOCUMENT,
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
      documentId,
      builder->slice());
  MMFilesWalMarker const* marker;

  if (options.recoveryData == nullptr) {
    marker = &replaceMarker;
  } else {
    marker = static_cast<MMFilesWalMarker*>(options.recoveryData);
  }

  VPackSlice const newDoc(marker->vpack());
  MMFilesDocumentOperation operation(
    &_logicalCollection, TRI_VOC_DOCUMENT_OPERATION_REPLACE
  );

  try {
    insertLocalDocumentId(documentId, marker->vpack(), 0, true, true);

    operation.setDocumentIds(MMFilesDocumentDescriptor(oldDocumentId, oldDoc.begin()),
                             MMFilesDocumentDescriptor(documentId, newDoc.begin()));

    res = updateDocument(
      *trx,
      revisionId,
      oldDocumentId,
      oldDoc,
      documentId,
      newDoc,
      operation,
      marker,
      options,
      options.waitForSync
    );

    if (res.ok() && callbackDuringLock != nullptr) {
      res = callbackDuringLock();
    }
  } catch (basics::Exception const& ex) {
    res = Result(ex.code());
  } catch (std::bad_alloc const&) {
    res = Result(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    res = Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    res = Result(TRI_ERROR_INTERNAL);
  }

  if (res.fail()) {
    operation.revert(trx);
  } else {
    result.setManaged(newDoc.begin(), documentId);

    if (options.waitForSync) {
      // store the tick that was used for writing the new document
      resultMarkerTick = operation.tick();
    }
  }

  return res;
}

Result MMFilesCollection::remove(
    transaction::Methods& trx,
    velocypack::Slice slice,
    ManagedDocumentResult& previous,
    OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    TRI_voc_rid_t& revisionId,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock
) {
  prevRev = 0;

  LocalDocumentId const documentId = LocalDocumentId::create();
  transaction::BuilderLeaser builder(&trx);

  newObjectForRemove(
    &trx, slice, *builder.get(), options.isRestore, revisionId
  );

  TRI_IF_FAILURE("RemoveDocumentNoMarker") {
    // test what happens when no marker can be created
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("RemoveDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // create marker
  MMFilesCrudMarker removeMarker(
      TRI_DF_MARKER_VPACK_REMOVE,
      static_cast<MMFilesTransactionState*>(trx.state())->idForMarker(),
      documentId,
      builder->slice());
  MMFilesWalMarker const* marker;

  if (options.recoveryData == nullptr) {
    marker = &removeMarker;
  } else {
    marker = static_cast<MMFilesWalMarker*>(options.recoveryData);
  }

  TRI_IF_FAILURE("RemoveDocumentNoLock") {
    // test what happens if no lock can be acquired
    return Result(TRI_ERROR_DEBUG);
  }

  VPackSlice key;

  if (slice.isString()) {
    key = slice;
  } else {
    key = slice.get(StaticStrings::KeyString);
  }

  TRI_ASSERT(!key.isNone());
  
  TRI_ASSERT(keyLockInfo != nullptr); 
  if (keyLockInfo->shouldLock) {
    lockKey(*keyLockInfo, key);
  }

  MMFilesDocumentOperation operation(
    &_logicalCollection, TRI_VOC_DOCUMENT_OPERATION_REMOVE
  );
  bool const useDeadlockDetector =
    (lock
     && !trx.isSingleOperationTransaction()
     && !trx.state()->hasHint(transaction::Hints::Hint::NO_DLD)
    );
  arangodb::MMFilesCollectionWriteLocker collectionLocker(
      this, useDeadlockDetector, trx.state(), lock);

  // get the previous revision
  Result res = lookupDocument(&trx, key, previous);

  if (res.fail()) {
    return res;
  }

  VPackSlice const oldDoc(previous.vpack());
  LocalDocumentId const oldDocumentId = previous.localDocumentId();
  TRI_voc_rid_t oldRevisionId = arangodb::transaction::helpers::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  // Check old revision:
  if (!options.ignoreRevs && slice.isObject()) {
    TRI_voc_rid_t expectedRevisionId = TRI_ExtractRevisionId(slice);

    res = checkRevision(&trx, expectedRevisionId, oldRevisionId);

    if (res.fail()) {
      return res;
    }
  }
  
  // we found a document to remove
  try {
    operation.setDocumentIds(MMFilesDocumentDescriptor(oldDocumentId, oldDoc.begin()),
                             MMFilesDocumentDescriptor());

    // delete from indexes
    res = deleteSecondaryIndexes(trx, oldDocumentId, oldDoc,
                                 options.indexOperationMode);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldDocumentId, oldDoc,
                             Index::OperationMode::rollback);
      THROW_ARANGO_EXCEPTION(res);
    }

    res = deletePrimaryIndex(&trx, oldDocumentId, oldDoc, options);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldDocumentId, oldDoc,
                             Index::OperationMode::rollback);
      THROW_ARANGO_EXCEPTION(res);
    }

    operation.indexed();

    TRI_IF_FAILURE("RemoveDocumentNoOperation") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    try {
      removeLocalDocumentId(oldDocumentId, true);
    } catch (...) {
    }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res = static_cast<MMFilesTransactionState*>(trx.state())->addOperation(
      documentId, revisionId, operation, marker, options.waitForSync
    );

    if (res.ok() && callbackDuringLock != nullptr) {
      res = callbackDuringLock();
    }
  } catch (basics::Exception const& ex) {
    res = Result(ex.code(), ex.what());
  } catch (std::bad_alloc const&) {
    res = Result(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    res = Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    res = Result(TRI_ERROR_INTERNAL);
  }

  if (res.fail()) {
    operation.revert(&trx);
  } else {
    // store the tick that was used for removing the document
    resultMarkerTick = operation.tick();
  }
  return res;
}

/// @brief Defer a callback to be executed when the collection
///        can be dropped. The callback is supposed to drop
///        the collection and it is guaranteed that no one is using
///        it at that moment.
void MMFilesCollection::deferDropCollection(
    std::function<bool(LogicalCollection&)> const& callback
) {
  // add callback for dropping
  ditches()->createMMFilesDropCollectionDitch(
    _logicalCollection, callback, __FILE__, __LINE__
  );
}

/// @brief rolls back a document operation
Result MMFilesCollection::rollbackOperation(
    transaction::Methods& trx,
    TRI_voc_document_operation_e type,
    LocalDocumentId const& oldDocumentId,
    velocypack::Slice const& oldDoc,
    LocalDocumentId const& newDocumentId,
    velocypack::Slice const& newDoc
) {
  OperationOptions options;

  options.indexOperationMode=  Index::OperationMode::rollback;

  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(oldDocumentId.empty());
    TRI_ASSERT(oldDoc.isNone());
    TRI_ASSERT(!newDocumentId.empty());
    TRI_ASSERT(!newDoc.isNone());

    // ignore any errors we're getting from this
    deletePrimaryIndex(&trx, newDocumentId, newDoc, options);
    deleteSecondaryIndexes(
      trx, newDocumentId, newDoc, Index::OperationMode::rollback
    );

    return TRI_ERROR_NO_ERROR;
  }

  if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
      type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    TRI_ASSERT(!oldDocumentId.empty());
    TRI_ASSERT(!oldDoc.isNone());
    TRI_ASSERT(!newDocumentId.empty());
    TRI_ASSERT(!newDoc.isNone());

    // remove the current values from the indexes
    deleteSecondaryIndexes(trx, newDocumentId, newDoc,
                           Index::OperationMode::rollback);
    // re-insert old state
    return insertSecondaryIndexes(trx, oldDocumentId, oldDoc,
                                  Index::OperationMode::rollback);
  }

  if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    // re-insert old document
    TRI_ASSERT(!oldDocumentId.empty());
    TRI_ASSERT(!oldDoc.isNone());
    TRI_ASSERT(newDocumentId.empty());
    TRI_ASSERT(newDoc.isNone());

    auto res = insertPrimaryIndex(&trx, oldDocumentId, oldDoc, options);

    if (res.ok()) {
      res = insertSecondaryIndexes(
        trx, oldDocumentId, oldDoc, Index::OperationMode::rollback
      );
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
          << "error rolling back remove operation";
    }
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "logic error. invalid operation type on rollback";
#endif
  return TRI_ERROR_INTERNAL;
}

/// @brief removes a document or edge, fast path function for database documents
Result MMFilesCollection::removeFastPath(
    transaction::Methods& trx,
    TRI_voc_rid_t revisionId,
    LocalDocumentId const& oldDocumentId,
    velocypack::Slice const oldDoc,
    OperationOptions& options,
    LocalDocumentId const& documentId,
    velocypack::Slice const toRemove
) {
  TRI_IF_FAILURE("RemoveDocumentNoMarker") {
    // test what happens when no marker can be created
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("RemoveDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // create marker
  MMFilesCrudMarker removeMarker(
    TRI_DF_MARKER_VPACK_REMOVE,
    static_cast<MMFilesTransactionState*>(trx.state())->idForMarker(),
    documentId,
    toRemove
  );

  MMFilesWalMarker const* marker = &removeMarker;

  TRI_IF_FAILURE("RemoveDocumentNoLock") {
    // test what happens if no lock can be acquired
    return Result(TRI_ERROR_DEBUG);
  }

  VPackSlice key =
      arangodb::transaction::helpers::extractKeyFromDocument(oldDoc);
  TRI_ASSERT(!key.isNone());

  MMFilesDocumentOperation operation(
    &_logicalCollection, TRI_VOC_DOCUMENT_OPERATION_REMOVE
  );

  operation.setDocumentIds(MMFilesDocumentDescriptor(oldDocumentId, oldDoc.begin()),
                           MMFilesDocumentDescriptor());

  // delete from indexes
  Result res;
  try {
    res = deleteSecondaryIndexes(trx, oldDocumentId, oldDoc,
                                 options.indexOperationMode);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldDocumentId, oldDoc,
                             Index::OperationMode::rollback);
      THROW_ARANGO_EXCEPTION(res);
    }

    res = deletePrimaryIndex(&trx, oldDocumentId, oldDoc, options);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldDocumentId, oldDoc,
                             Index::OperationMode::rollback);
      THROW_ARANGO_EXCEPTION(res);
    }

    operation.indexed();

    try {
      removeLocalDocumentId(oldDocumentId, true);
    } catch (...) {
    }

    TRI_IF_FAILURE("RemoveDocumentNoOperation") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res = static_cast<MMFilesTransactionState*>(trx.state())->addOperation(
      documentId, revisionId, operation, marker, options.waitForSync
    );
  } catch (basics::Exception const& ex) {
    res = Result(ex.code());
  } catch (std::bad_alloc const&) {
    res = Result(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    res = Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    res = Result(TRI_ERROR_INTERNAL);
  }

  if (res.fail()) {
    operation.revert(&trx);
  }

  return res;
}

/// @brief looks up a document by key, low level worker
/// the caller must make sure the read lock on the collection is held
/// the key must be a string slice, no revision check is performed
Result MMFilesCollection::lookupDocument(transaction::Methods* trx,
                                         VPackSlice key,
                                         ManagedDocumentResult& result) {
  if (!key.isString()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }

  MMFilesSimpleIndexElement element =
      primaryIndex()->lookupKey(trx, key, result);
  if (element) {
    LocalDocumentId const documentId = element.localDocumentId();
    uint8_t const* vpack = lookupDocumentVPack(documentId);
    if (vpack != nullptr) {
      result.setUnmanaged(vpack, documentId);
    }
    return Result(TRI_ERROR_NO_ERROR);
  }

  return Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

/// @brief updates an existing document, low level worker
/// the caller must make sure the write lock on the collection is held
Result MMFilesCollection::updateDocument(
    transaction::Methods& trx,
    TRI_voc_rid_t revisionId,
    LocalDocumentId const& oldDocumentId,
    velocypack::Slice const& oldDoc,
    LocalDocumentId const& newDocumentId,
    velocypack::Slice const& newDoc,
    MMFilesDocumentOperation& operation,
    MMFilesWalMarker const* marker,
    OperationOptions& options,
    bool& waitForSync
) {
  // remove old document from secondary indexes
  // (it will stay in the primary index as the key won't change)
  Result res = deleteSecondaryIndexes(trx, oldDocumentId, oldDoc,
                                      options.indexOperationMode);

  if (res.fail()) {
    // re-insert the document in case of failure, ignore errors during rollback
    insertSecondaryIndexes(trx, oldDocumentId, oldDoc,
                           Index::OperationMode::rollback);
    return res;
  }

  // insert new document into secondary indexes
  res = insertSecondaryIndexes(trx, newDocumentId, newDoc,
                               options.indexOperationMode);

  if (res.fail()) {
    // rollback
    deleteSecondaryIndexes(trx, newDocumentId, newDoc,
                           Index::OperationMode::rollback);
    insertSecondaryIndexes(trx, oldDocumentId, oldDoc,
                           Index::OperationMode::rollback);
    return res;
  }

  // update the index element (primary index only - other indexes have been
  // adjusted)
  // TODO: pass key into this function so it does not have to be looked up again
  VPackSlice keySlice(transaction::helpers::extractKeyFromDocument(newDoc));
  auto* element = primaryIndex()->lookupKeyRef(&trx, keySlice);

  if (element != nullptr && element->isSet()) {
    element->updateLocalDocumentId(
        newDocumentId,
        static_cast<uint32_t>(keySlice.begin() - newDoc.begin()));
  }

  operation.indexed();

  try {
    removeLocalDocumentId(oldDocumentId, true);
  } catch (...) {
  }

  TRI_IF_FAILURE("UpdateDocumentNoOperation") {
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("UpdateDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return Result(
    static_cast<MMFilesTransactionState*>(trx.state())->addOperation(
      newDocumentId, revisionId, operation, marker, waitForSync
    )
  );
}

void MMFilesCollection::lockKey(KeyLockInfo& keyLockInfo, VPackSlice const& key) {
  TRI_ASSERT(keyLockInfo.key.empty());

  // copy out the key we need to lock
  TRI_ASSERT(key.isString());
  std::string k = key.copyString();

  MMFilesCollection::KeyLockShard& shard = getShardForKey(k); 

  // register key unlock function
  keyLockInfo.unlocker = [this](KeyLockInfo& keyLock) {
    unlockKey(keyLock);
  };

  do {
    {
      MUTEX_LOCKER(locker, shard._mutex);
      // if the insert fails because the key is already in the set,
      // we carry on trying until the previous value is gone from the set.
      // if the insert fails because of an out-of-memory exception,
      // we can let the exception escape from here. no harm will be done
      if (shard._keys.insert(k).second) {
        // if insertion into the lock set succeeded, we can
        // go on without the lock. otherwise we just need to carry on trying
        locker.unlock();

        // store key to unlock later. the move assignment will not fail
        keyLockInfo.key = std::move(k);
        return;
      }
    }
    std::this_thread::yield();
  } while (!application_features::ApplicationServer::isStopping());

  // we can only get here on shutdown
  THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
}

void MMFilesCollection::unlockKey(KeyLockInfo& keyLockInfo) noexcept {
  TRI_ASSERT(keyLockInfo.shouldLock);
  if (!keyLockInfo.key.empty()) {
    MMFilesCollection::KeyLockShard& shard = getShardForKey(keyLockInfo.key); 
    MUTEX_LOCKER(locker, shard._mutex);
    shard._keys.erase(keyLockInfo.key);
  }
}

MMFilesCollection::KeyLockShard& MMFilesCollection::getShardForKey(std::string const& key) noexcept {
  size_t hash = std::hash<std::string>()(key);
  return _keyLockShards[hash % numKeyLockShards];
}
