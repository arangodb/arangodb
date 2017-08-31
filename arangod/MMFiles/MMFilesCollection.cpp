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
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/encoding.h"
#include "Basics/process-utils.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/CollectionLockState.h"
#include "Indexes/IndexIterator.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollectionReadLocker.h"
#include "MMFiles/MMFilesCollectionWriteLocker.h"
#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesDocumentOperation.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "MMFiles/MMFilesToken.h"
#include "MMFiles/MMFilesTransactionState.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

namespace {

/// @brief helper class for filling indexes
class MMFilesIndexFillerTask : public basics::LocalTask {
 public:
  MMFilesIndexFillerTask(
      std::shared_ptr<basics::LocalTaskQueue> queue, transaction::Methods* trx, Index* idx,
      std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents)
      : LocalTask(queue), _trx(trx), _idx(idx), _documents(documents) {}

  void run() {
    TRI_ASSERT(_idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

    try {
      _idx->batchInsert(_trx, _documents, _queue);
    } catch (std::exception const&) {
      _queue->setStatus(TRI_ERROR_INTERNAL);
    }

    _queue->join();
  }

 private:
  transaction::Methods* _trx;
  Index* _idx;
  std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& _documents;
};

/// @brief find a statistics container for a given file id
static MMFilesDatafileStatisticsContainer* FindDatafileStats(
    MMFilesCollection::OpenIteratorState* state, TRI_voc_fid_t fid) {
  auto it = state->_stats.find(fid);

  if (it != state->_stats.end()) {
    return (*it).second;
  }

  auto stats = std::make_unique<MMFilesDatafileStatisticsContainer>();
  state->_stats.emplace(fid, stats.get());
  return stats.release();
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
          slice, "waitForSync", _logicalCollection->waitForSync())) {
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
    TRI_voc_size_t toUpdate = journalSlice.getNumericValue<TRI_voc_size_t>();
    if (toUpdate < TRI_JOURNAL_MINIMAL_SIZE) {
      return {TRI_ERROR_BAD_PARAMETER, "<properties>.journalSize too small"};
    }
  }

  _indexBuckets = Helper::getNumericValue<uint32_t>(slice, "indexBuckets",
                                                    _indexBuckets);  // MMFiles

  if (slice.hasKey("journalSize")) {
    _journalSize = Helper::getNumericValue<TRI_voc_size_t>(slice, "journalSize",
                                                           _journalSize);
  } else {
    _journalSize = Helper::getNumericValue<TRI_voc_size_t>(slice, "maximalSize",
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
    VPackBuilder infoBuilder = _logicalCollection->toVelocyPackIgnore(
        {"path", "statusString"}, true, true);
    MMFilesCollectionMarker marker(TRI_DF_MARKER_VPACK_CHANGE_COLLECTION,
                                   _logicalCollection->vocbase()->id(),
                                   _logicalCollection->cid(),
                                   infoBuilder.slice());
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

PhysicalCollection* MMFilesCollection::clone(LogicalCollection* logical) {
  return new MMFilesCollection(logical, this);
}

/// @brief process a document (or edge) marker when opening a collection
int MMFilesCollection::OpenIteratorHandleDocumentMarker(
    MMFilesMarker const* marker, MMFilesDatafile* datafile,
    MMFilesCollection::OpenIteratorState* state) {
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

  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice,
                                                     revisionId);

  physical->setRevision(revisionId, false);

  if (state->_trackKeys) {
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
      state->_primaryIndex->lookupKeyRef(trx, keySlice, state->_mmdr);

  // it is a new entry
  if (found == nullptr || found->revisionId() == 0) {
    physical->insertRevision(revisionId, vpack, fid, false, false);

    // insert into primary index
    Result res = state->_primaryIndex->insertKey(trx, revisionId,
                                                VPackSlice(vpack),
                                                state->_mmdr);

    if (res.errorNumber() != TRI_ERROR_NO_ERROR) {
      physical->removeRevision(revisionId, false);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
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
    TRI_voc_rid_t const oldRevisionId = found->revisionId();
    // update the revision id in primary index
    found->updateRevisionId(revisionId,
                            static_cast<uint32_t>(keySlice.begin() - vpack));

    MMFilesDocumentPosition const old = physical->lookupRevision(oldRevisionId);

    // remove old revision
    physical->removeRevision(oldRevisionId, false);

    // insert new revision
    physical->insertRevision(revisionId, vpack, fid, false, false);

    // update the datafile info
    MMFilesDatafileStatisticsContainer* dfi;
    if (old.fid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, old.fid());
    }

    if (old.dataptr() != nullptr) {
      uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
      int64_t size = static_cast<int64_t>(
          MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT) +
          VPackSlice(vpack).byteSize());

      dfi->numberAlive--;
      dfi->sizeAlive -= encoding::alignedSize<int64_t>(size);
      dfi->numberDead++;
      dfi->sizeDead += encoding::alignedSize<int64_t>(size);
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
    MMFilesCollection::OpenIteratorState* state) {
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

  transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice,
                                                     revisionId);

  physical->setRevision(revisionId, false);
  if (state->_trackKeys) {
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
      state->_primaryIndex->lookupKey(trx, keySlice, state->_mmdr);

  // it is a new entry, so we missed the create
  if (!found) {
    // update the datafile info
    state->_dfi->numberDeletions++;
  }

  // it is a real delete
  else {
    TRI_voc_rid_t oldRevisionId = found.revisionId();

    MMFilesDocumentPosition const old = physical->lookupRevision(oldRevisionId);

    // update the datafile info
    MMFilesDatafileStatisticsContainer* dfi;

    if (old.fid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, old.fid());
    }

    TRI_ASSERT(old.dataptr() != nullptr);

    uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
    int64_t size = encoding::alignedSize<int64_t>(
        MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT) +
        VPackSlice(vpack).byteSize());

    dfi->numberAlive--;
    dfi->sizeAlive -= encoding::alignedSize<int64_t>(size);
    dfi->numberDead++;
    dfi->sizeDead += encoding::alignedSize<int64_t>(size);
    state->_dfi->numberDeletions++;

    state->_primaryIndex->removeKey(trx, oldRevisionId, VPackSlice(vpack),
                                    state->_mmdr);

    physical->removeRevision(oldRevisionId, true);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief iterator for open
bool MMFilesCollection::OpenIterator(MMFilesMarker const* marker,
                                     MMFilesCollection::OpenIteratorState* data,
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

    if (++data->_operations % 1024 == 0) {
      data->_mmdr.reset();
    }
  } else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile, data);
    if (++data->_operations % 1024 == 0) {
      data->_mmdr.reset();
    }
  } else {
    if (type == TRI_DF_MARKER_HEADER) {
      // ensure there is a datafile info entry for each datafile of the
      // collection
      FindDatafileStats(data, datafile->fid());
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "skipping marker type "
                                              << TRI_NameMarkerDatafile(marker);
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

MMFilesCollection::MMFilesCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info),
      _ditches(collection),
      _initialCount(0),
      _revisionError(false),
      _lastRevision(0),
      _uncollectedLogfileEntries(0),
      _nextCompactionStartIndex(0),
      _lastCompactionStatus(nullptr),
      _lastCompactionStamp(0.0),
      _journalSize(Helper::readNumericValue<TRI_voc_size_t>(
          info, "maximalSize",  // Backwards compatibility. Agency uses
                                // journalSize. paramters.json uses maximalSize
          Helper::readNumericValue<TRI_voc_size_t>(info, "journalSize",
                                                   TRI_JOURNAL_DEFAULT_SIZE))),
      _isVolatile(arangodb::basics::VelocyPackHelper::readBooleanValue(
          info, "isVolatile", false)),
      _persistentIndexes(0),
      _indexBuckets(Helper::readNumericValue<uint32_t>(
          info, "indexBuckets", defaultIndexBuckets)),
      _useSecondaryIndexes(true),
      _doCompact(Helper::readBooleanValue(info, "doCompact", true)),
      _maxTick(0) {
  if (_isVolatile && _logicalCollection->waitForSync()) {
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

MMFilesCollection::MMFilesCollection(LogicalCollection* logical,
                                     PhysicalCollection* physical)
    : PhysicalCollection(logical, VPackSlice::emptyObjectSlice()),
      _ditches(logical),
      _isVolatile(static_cast<MMFilesCollection*>(physical)->isVolatile()) {
  MMFilesCollection& mmfiles = *static_cast<MMFilesCollection*>(physical);
  _persistentIndexes = mmfiles._persistentIndexes;
  _useSecondaryIndexes = mmfiles._useSecondaryIndexes;
  _initialCount = mmfiles._initialCount;
  _revisionError = mmfiles._revisionError;
  _lastRevision = mmfiles._lastRevision;
  _nextCompactionStartIndex = mmfiles._nextCompactionStartIndex;
  _lastCompactionStatus = mmfiles._lastCompactionStatus;
  _lastCompactionStamp = mmfiles._lastCompactionStamp;
  _journalSize = mmfiles._journalSize;
  _indexBuckets = mmfiles._indexBuckets;
  _path = mmfiles._path;
  _doCompact = mmfiles._doCompact;
  _maxTick = mmfiles._maxTick;

  // Copy over index definitions
  _indexes.reserve(mmfiles._indexes.size());
  for (auto const& idx : mmfiles._indexes) {
    _indexes.emplace_back(idx);
  }

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
  if (!_logicalCollection->deleted() &&
      !_logicalCollection->vocbase()->isDropped()) {
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
      engine->changeCollection(_logicalCollection->vocbase(),
                               _logicalCollection->cid(), _logicalCollection,
                               doSync);
    }
  }

  {
    // We also have to unload the indexes.
    WRITE_LOCKER(writeLocker, _dataLock);
    for (auto& idx : _indexes) {
      idx->unload();
    }
  }

  // wait until ditches have been processed fully
  while (_ditches.contains(MMFilesDitch::TRI_DITCH_DATAFILE_DROP) ||
         _ditches.contains(MMFilesDitch::TRI_DITCH_DATAFILE_RENAME) ||
         _ditches.contains(MMFilesDitch::TRI_DITCH_COMPACTION)) {
    usleep(20000);
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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to seal journal '"
                                            << datafile->getName()
                                            << "': " << TRI_errno_string(res);
    return res;
  }

  if (!isCompactor && datafile->isPhysical()) {
    // rename the file
    std::string dname("datafile-" + std::to_string(datafile->fid()) + ".db");
    std::string filename =
        arangodb::basics::FileUtils::buildFilename(path(), dname);

    res = datafile->rename(filename);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "closed file '"
                                                << datafile->getName() << "'";
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
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

  int res = TRI_ERROR_NO_ERROR;

  // we only need to care about physical datafiles
  // anonymous regions do not need to be synced
  if (datafile->isPhysical()) {
    char const* synced = datafile->_synced;
    char* written = datafile->_written;

    if (synced < written) {
      bool ok = datafile->sync(synced, written);

      if (ok) {
        LOG_TOPIC(TRACE, Logger::COLLECTOR) << "msync succeeded "
                                            << (void*)synced << ", size "
                                            << (written - synced);
        datafile->_synced = written;
      } else {
        res = TRI_errno();
        if (res == TRI_ERROR_NO_ERROR) {
          // oops, error code got lost
          res = TRI_ERROR_INTERNAL;
        }

        LOG_TOPIC(ERR, Logger::COLLECTOR) << "msync failed with: "
                                          << TRI_last_error();
        datafile->setState(TRI_DF_STATE_WRITE_ERROR);
      }
    }
  }

  return res;
}

/// @brief reserve space in the current journal. if no create exists or the
/// current journal cannot provide enough space, close the old journal and
/// create a new one
int MMFilesCollection::reserveJournalSpace(TRI_voc_tick_t tick,
                                           TRI_voc_size_t size,
                                           char*& resultPosition,
                                           MMFilesDatafile*& resultDatafile) {
  // reset results
  resultPosition = nullptr;
  resultDatafile = nullptr;

  // start with configured journal size
  TRI_voc_size_t targetSize = static_cast<TRI_voc_size_t>(_journalSize);

  // make sure that the document fits
  while (targetSize - 256 < size) {
    targetSize *= 2;
  }

  WRITE_LOCKER(writeLocker, _filesLock);
  TRI_ASSERT(_journals.size() <= 1);

  while (true) {
    // no need to go on if the collection is already deleted
    if (_logicalCollection->status() == TRI_VOC_COL_STATUS_DELETED) {
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
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
      datafile->_written = ((char*)position) + size;
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
    TRI_voc_fid_t fid, TRI_voc_size_t maximalSize) {
  WRITE_LOCKER(writeLocker, _filesLock);

  TRI_ASSERT(_compactors.empty());
  // reserve enough space for the later addition
  _compactors.reserve(_compactors.size() + 1);

  std::unique_ptr<MMFilesDatafile> compactor(
      createDatafile(fid, static_cast<TRI_voc_size_t>(maximalSize), true));

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
                                                   TRI_voc_size_t journalSize,
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
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "created new compactor '"
                                              << datafile->getName() << "'";
  } else {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "created new journal '"
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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
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
  cm._cid = _logicalCollection->cid();

  res = datafile->writeCrcElement(position, &cm.base, false);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    int res = datafile->_lastError;
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
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
    std::string jname("journal-" + std::to_string(datafile->fid()) + ".db");
    std::string filename =
        arangodb::basics::FileUtils::buildFilename(path(), jname);

    int res = datafile->rename(filename);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "failed to rename journal '" << datafile->getName() << "' to '"
          << filename << "': " << TRI_errno_string(res);

      std::string temp(datafile->getName());
      datafile.reset();
      TRI_UnlinkFile(temp.c_str());

      THROW_ARANGO_EXCEPTION(res);
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "renamed journal from '"
                                              << datafile->getName() << "' to '"
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

void MMFilesCollection::getPropertiesVPackCoordinator(
    velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add("doCompact", VPackValue(_doCompact));
  result.add("indexBuckets", VPackValue(_indexBuckets));
  result.add("journalSize", VPackValue(_journalSize));
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

  builder->add("compactionStatus", VPackValue(VPackValueType::Object));
  builder->add("message", VPackValue(lastCompactionStatus));
  builder->add("time", VPackValue(&lastCompactionStampString[0]));
  builder->close();  // compactionStatus

  builder->add("documentReferences",
               VPackValue(_ditches.numMMFilesDocumentMMFilesDitches()));

  char const* waitingForDitch = _ditches.head();
  builder->add("waitingFor",
               VPackValue(waitingForDitch == nullptr ? "-" : waitingForDitch));

  // add datafile statistics
  MMFilesDatafileStatisticsContainer dfi = _datafileStatistics.all();

  builder->add("alive", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(dfi.numberAlive));
  builder->add("size", VPackValue(dfi.sizeAlive));
  builder->close();  // alive

  builder->add("dead", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(dfi.numberDead));
  builder->add("size", VPackValue(dfi.sizeDead));
  builder->add("deletion", VPackValue(dfi.numberDeletions));
  builder->close();  // dead

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
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
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
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "getting datafiles in data range " << dataMin << " - " << dataMax;

  std::vector<DatafileDescription> datafiles =
      datafilesInRange(dataMin, dataMax);
  // now we have a list of datafiles...

  size_t const n = datafiles.size();

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
bool MMFilesCollection::openIndex(VPackSlice const& description,
                                  transaction::Methods* trx) {
  // VelocyPack must be an index description
  if (!description.isObject()) {
    return false;
  }

  bool unused = false;
  auto idx = _logicalCollection->createIndex(trx, description, unused);

  if (idx == nullptr) {
    // error was already printed if we get here
    return false;
  }

  return true;
}

/// @brief initializes an index with a set of existing documents
void MMFilesCollection::fillIndex(
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue, transaction::Methods* trx,
    arangodb::Index* idx,
    std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents,
    bool skipPersistent) {
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

// @brief return the primary index
// WARNING: Make sure that this LogicalCollection Instance
// is somehow protected. If it goes out of all scopes
// or it's indexes are freed the pointer returned will get invalidated.
arangodb::MMFilesPrimaryIndex* MMFilesCollection::primaryIndex() const {
  // The primary index always has iid 0
  auto primary = _logicalCollection->lookupIndex(0);
  TRI_ASSERT(primary != nullptr);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (primary->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "got invalid indexes for collection '" << _logicalCollection->name()
        << "'";
    for (auto const& it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
  }
#endif
  TRI_ASSERT(primary->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  // the primary index must be the index at position #0
  return static_cast<arangodb::MMFilesPrimaryIndex*>(primary.get());
}

int MMFilesCollection::fillAllIndexes(transaction::Methods* trx) {
  return fillIndexes(trx, _indexes);
}

/// @brief Fill the given list of Indexes
int MMFilesCollection::fillIndexes(
    transaction::Methods* trx,
    std::vector<std::shared_ptr<arangodb::Index>> const& indexes,
    bool skipPersistent) {
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
      _logicalCollection->vocbase()->name() + "/" + _logicalCollection->name() +
      " }, indexes: " + std::to_string(n - 1));

  auto queue = std::make_shared<arangodb::basics::LocalTaskQueue>();

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

    std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> documents;
    documents.reserve(blockSize);

    auto insertInAllIndexes = [&]() -> void {
      for (size_t i = 0; i < n; ++i) {
        auto idx = indexes[i];
        if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
          continue;
        }
        fillIndex(queue, trx, idx.get(), documents, skipPersistent);
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
        MMFilesSimpleIndexElement element =
            primaryIdx->lookupSequential(trx, position, total);

        if (!element) {
          break;
        }

        TRI_voc_rid_t revisionId = element.revisionId();

        uint8_t const* vpack = lookupRevisionVPack(revisionId);
        if (vpack != nullptr) {
          documents.emplace_back(std::make_pair(revisionId, VPackSlice(vpack)));

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
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "caught exception while filling indexes: " << ex.what();
  } catch (std::bad_alloc const&) {
    queue->setStatus(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "caught exception while filling indexes: " << ex.what();
    queue->setStatus(TRI_ERROR_INTERNAL);
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
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
  auto vocbase = _logicalCollection->vocbase();
  PerformanceLogScope logScope(std::string("open-collection { collection: ") +
                               vocbase->name() + "/" +
                               _logicalCollection->name() + " }");

  try {
    // check for journals and datafiles
    MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
    int res = engine->openCollection(vocbase, _logicalCollection, ignoreErrors);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "cannot open '" << path()
                                                << "', check failed";
      return res;
    }

    return TRI_ERROR_NO_ERROR;
  } catch (basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot load collection parameter file '" << path()
        << "': " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot load collection parameter file '" << path()
        << "': " << ex.what();
    return TRI_ERROR_INTERNAL;
  }
}

void MMFilesCollection::open(bool ignoreErrors) {
  VPackBuilder builder;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto vocbase = _logicalCollection->vocbase();
  auto cid = _logicalCollection->cid();
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
      std::string("open-document-collection { collection: ") + vocbase->name() +
      "/" + _logicalCollection->name() + " }");

  int res = openWorker(ignoreErrors);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res,
        std::string("cannot open document collection from path '") + path() +
            "': " + TRI_errno_string(res));
  }

  arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase), cid,
      AccessMode::Type::READ);
  // the underlying collections must not be locked here because the "load"
  // routine can be invoked from any other place, e.g. from an AQL query
  trx.addHint(transaction::Hints::Hint::LOCK_NEVER);

  {
    PerformanceLogScope logScope(std::string("iterate-markers { collection: ") +
                                 vocbase->name() + "/" +
                                 _logicalCollection->name() + " }");
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
      detectIndexes(&trx);
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

  if (!engine->inRecovery()) {
    // build the index structures, and fill the indexes
    fillIndexes(&trx, _indexes);
  }

  // successfully opened collection. now adjust version number
  if (_logicalCollection->version() != LogicalCollection::VERSION_31 &&
      !_revisionError &&
      application_features::ApplicationServer::server
          ->getFeature<DatabaseFeature>("Database")
          ->check30Revisions()) {
    _logicalCollection->setVersion(LogicalCollection::VERSION_31);
    bool const doSync =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database")
            ->forceSyncProperties();
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    engine->changeCollection(_logicalCollection->vocbase(),
                             _logicalCollection->cid(), _logicalCollection,
                             doSync);
  }
}

/// @brief iterate all markers of the collection
int MMFilesCollection::iterateMarkersOnLoad(transaction::Methods* trx) {
  // initialize state for iteration
  OpenIteratorState openState(_logicalCollection, trx);

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

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "found " << openState._documents << " document markers, "
      << openState._deletions << " deletion markers for collection '"
      << _logicalCollection->name() << "'";

  if (_logicalCollection->version() <= LogicalCollection::VERSION_30 &&
      _lastRevision >= static_cast<TRI_voc_rid_t>(2016ULL - 1970ULL) * 1000ULL *
                           60ULL * 60ULL * 24ULL * 365ULL &&
      application_features::ApplicationServer::server
          ->getFeature<DatabaseFeature>("Database")
          ->check30Revisions()) {
    // a collection from 3.0 or earlier with a _rev value that is higher than we
    // can handle safely
    setRevisionError();

    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "collection '" << _logicalCollection->name()
        << "' contains _rev values that are higher than expected for an "
           "ArangoDB 3.1 database. If this collection was created or used with "
           "a pre-release or development version of ArangoDB 3.1, please "
           "restart the server with option '--database.check-30-revisions "
           "false' to suppress this warning. If this collection was created "
           "with an ArangoDB 3.0, please dump the 3.0 database with arangodump "
           "and restore it in 3.1 with arangorestore.";
    if (application_features::ApplicationServer::server
            ->getFeature<DatabaseFeature>("Database")
            ->fail30Revisions()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_ARANGO_CORRUPTED_DATAFILE,
          std::string("collection '") + _logicalCollection->name() +
              "' contains _rev values from 3.0 and needs to be migrated using "
              "dump/restore");
    }
  }

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

DocumentIdentifierToken MMFilesCollection::lookupKey(transaction::Methods *trx,
                                                     VPackSlice const& key) {
  MMFilesPrimaryIndex *index = primaryIndex();
  MMFilesSimpleIndexElement element = index->lookupKey(trx, key);
  return element ? MMFilesToken(element.revisionId()) : MMFilesToken();
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
  MMFilesCollectionReadLocker collectionLocker(this, useDeadlockDetector, lock);

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
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  auto tkn = static_cast<MMFilesToken const*>(&token);
  TRI_voc_rid_t revisionId = tkn->revisionId();
  uint8_t const* vpack = lookupRevisionVPack(revisionId);
  if (vpack != nullptr) {
    result.setUnmanaged(vpack, revisionId);
    return true;
  }
  return false;
}

bool MMFilesCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                 DocumentIdentifierToken const& token,
                                                 IndexIterator::DocumentCallback const& cb) {
  auto tkn = static_cast<MMFilesToken const*>(&token);
  TRI_voc_rid_t revisionId = tkn->revisionId();
  uint8_t const* vpack = lookupRevisionVPack(revisionId);
  if (vpack != nullptr) {
    cb(token, VPackSlice(vpack));
    return true;
  }
  return false;
}

bool MMFilesCollection::readDocumentConditional(
    transaction::Methods* trx, DocumentIdentifierToken const& token,
    TRI_voc_tick_t maxTick, ManagedDocumentResult& result) {
  auto tkn = static_cast<MMFilesToken const*>(&token);
  TRI_voc_rid_t revisionId = tkn->revisionId();
  TRI_ASSERT(revisionId != 0);
  uint8_t const* vpack =
      lookupRevisionVPackConditional(revisionId, maxTick, true);
  if (vpack != nullptr) {
    result.setUnmanaged(vpack, revisionId);
    return true;
  }
  return false;
}

void MMFilesCollection::prepareIndexes(VPackSlice indexesSlice) {
  TRI_ASSERT(indexesSlice.isArray());
  if (indexesSlice.length() == 0) {
    createInitialIndexes();
  }

  bool foundPrimary = false;
  bool foundEdge = false;

  for (auto const& it : VPackArrayIterator(indexesSlice)) {
    auto const& s = it.get("type");
    if (s.isString()) {
      std::string const type = s.copyString();
      if (type == "primary") {
        foundPrimary = true;
      } else if (type == "edge") {
        foundEdge = true;
      }
    }
  }
  for (auto const& idx : _indexes) {
    if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
      foundPrimary = true;
    } else if (_logicalCollection->type() == TRI_COL_TYPE_EDGE &&
               idx->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      foundEdge = true;
    }
  }

  if (!foundPrimary ||
      (!foundEdge && _logicalCollection->type() == TRI_COL_TYPE_EDGE)) {
    // we still do not have any of the default indexes, so create them now
    createInitialIndexes();
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory();
  TRI_ASSERT(idxFactory != nullptr);

  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error",
                                                            false)) {
      // We have an error here.
      // Do not add index.
      // TODO Handle Properly
      continue;
    }

    auto idx =
        idxFactory->prepareIndexFromSlice(v, false, _logicalCollection, true);

    if (ServerState::instance()->isRunningInCluster()) {
      addIndexCoordinator(idx);
    } else {
      addIndexLocal(idx);
    }
  }

  TRI_ASSERT(!_indexes.empty());

  if (_indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE &&
       (_indexes.size() < 2 || _indexes[1]->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX))) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (auto const& it : _indexes) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "- " << it.get();
    }
#endif
    std::string errorMsg("got invalid indexes for collection '");
    errorMsg.append(_logicalCollection->name());
    errorMsg.push_back('\'');
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    bool foundPrimary = false;
    for (auto const& it : _indexes) {
      if (it->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
        if (foundPrimary) {
          std::string errorMsg("found multiple primary indexes for collection '");
          errorMsg.append(_logicalCollection->name());
          errorMsg.push_back('\'');
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
        }
        foundPrimary = true;
      }
    }
  }
#endif
}

/// @brief creates the initial indexes for the collection
void MMFilesCollection::createInitialIndexes() {
  if (!_indexes.empty()) {
    return;
  }

  std::vector<std::shared_ptr<arangodb::Index>> systemIndexes;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory();
  TRI_ASSERT(idxFactory != nullptr);

  idxFactory->fillSystemIndexes(_logicalCollection, systemIndexes);
  for (auto const& it : systemIndexes) {
    addIndex(it);
  }
}

std::shared_ptr<Index> MMFilesCollection::lookupIndex(
    VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid index definition");
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  for (auto const& idx : _indexes) {
    if (idx->type() == type) {
      // Only check relevant indices
      if (idx->matchesDefinition(info)) {
        // We found an index for this definition.
        return idx;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<Index> MMFilesCollection::createIndex(transaction::Methods* trx,
                                                      VPackSlice const& info,
                                                      bool& created) {
  // TODO Get LOCK for the vocbase
  auto idx = lookupIndex(info);
  if (idx != nullptr) {
    created = false;
    // We already have this index.
    return idx;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory();
  TRI_ASSERT(idxFactory != nullptr);

  // We are sure that we do not have an index of this type.
  // We also hold the lock.
  // Create it

  idx =
      idxFactory->prepareIndexFromSlice(info, true, _logicalCollection, false);
  TRI_ASSERT(idx != nullptr);
  if (ServerState::instance()->isCoordinator()) {
    // In the coordinator case we do not fill the index
    // We only inform the others.
    addIndexCoordinator(idx);
    created = true;
    return idx;
  }

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
  {
    bool const doSync =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database")
            ->forceSyncProperties();
    VPackBuilder builder =
        _logicalCollection->toVelocyPackIgnore({"path", "statusString"}, true, true);
    _logicalCollection->updateProperties(builder.slice(), doSync);
  }
  created = true;
  return idx;
}

/// @brief Persist an index information to file
int MMFilesCollection::saveIndex(transaction::Methods* trx,
                                 std::shared_ptr<arangodb::Index> idx) {
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
    builder = idx->toVelocyPack(false, true);
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot save index definition";
    return TRI_ERROR_INTERNAL;
  }
  if (builder == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot save index definition";
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  auto vocbase = _logicalCollection->vocbase();
  auto collectionId = _logicalCollection->cid();
  VPackSlice data = builder->slice();

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->createIndex(vocbase, collectionId, idx->id(), data);

  if (!engine->inRecovery()) {
    // We need to write an index marker
    try {
      MMFilesCollectionMarker marker(TRI_DF_MARKER_VPACK_CREATE_INDEX,
                                     vocbase->id(), collectionId, data);

      MMFilesWalSlotInfoCopy slotInfo =
          MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);
      res = slotInfo.errorCode;
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }
  }
  return res;
}

bool MMFilesCollection::addIndex(std::shared_ptr<arangodb::Index> idx) {
  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return false;
    }
  }

  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

  _indexes.emplace_back(idx);
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

void MMFilesCollection::addIndexCoordinator(
    std::shared_ptr<arangodb::Index> idx) {
  addIndex(idx);
}

int MMFilesCollection::restoreIndex(transaction::Methods* trx,
                                    VPackSlice const& info,
                                    std::shared_ptr<arangodb::Index>& idx) {
  // The coordinator can never get into this state!
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  idx.reset();  // Clear it to make sure.
  if (!info.isObject()) {
    return TRI_ERROR_INTERNAL;
  }

  // We create a new Index object to make sure that the index
  // is not handed out except for a successful case.
  std::shared_ptr<Index> newIdx;
  try {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    IndexFactory const* idxFactory = engine->indexFactory();
    TRI_ASSERT(idxFactory != nullptr);
    newIdx = idxFactory->prepareIndexFromSlice(info, false, _logicalCollection,
                                               false);
  } catch (arangodb::basics::Exception const& e) {
    // Something with index creation went wrong.
    // Just report.
    return e.code();
  }
  TRI_ASSERT(newIdx != nullptr);

  TRI_UpdateTickServer(newIdx->id());

  auto const id = newIdx->id();
  for (auto& it : _indexes) {
    if (it->id() == id) {
      // index already exists
      idx = it;
      return TRI_ERROR_NO_ERROR;
    }
  }

  TRI_ASSERT(newIdx.get()->type() !=
             Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  std::vector<std::shared_ptr<arangodb::Index>> indexListLocal;
  indexListLocal.emplace_back(newIdx);

  int res = fillIndexes(trx, indexListLocal, false);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  addIndexLocal(newIdx);
  idx = newIdx;
  return TRI_ERROR_NO_ERROR;
}

bool MMFilesCollection::dropIndex(TRI_idx_iid_t iid) {
  if (iid == 0) {
    // invalid index id or primary index
    events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
    return true;
  }
  auto vocbase = _logicalCollection->vocbase();

  if (!removeIndex(iid)) {
    // We tried to remove an index that does not exist
    events::DropIndex("", std::to_string(iid),
                      TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    return false;
  }

  auto cid = _logicalCollection->cid();
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  engine->dropIndex(vocbase, cid, iid);
  {
    bool const doSync =
        application_features::ApplicationServer::getFeature<DatabaseFeature>(
            "Database")
            ->forceSyncProperties();
    VPackBuilder builder =
        _logicalCollection->toVelocyPackIgnore({"path", "statusString"}, true, true);
    _logicalCollection->updateProperties(builder.slice(), doSync);
  }

  if (!engine->inRecovery()) {
    int res = TRI_ERROR_NO_ERROR;

    VPackBuilder markerBuilder;
    markerBuilder.openObject();
    markerBuilder.add("id", VPackValue(std::to_string(iid)));
    markerBuilder.close();
    engine->dropIndexWalMarker(vocbase, cid, markerBuilder.slice(), true, res);

    if (res == TRI_ERROR_NO_ERROR) {
      events::DropIndex("", std::to_string(iid), TRI_ERROR_NO_ERROR);
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "could not save index drop marker in log: "
          << TRI_errno_string(res);
      events::DropIndex("", std::to_string(iid), res);
    }
  }
  return true;
}

/// @brief removes an index by id
bool MMFilesCollection::removeIndex(TRI_idx_iid_t iid) {
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

std::unique_ptr<IndexIterator> MMFilesCollection::getAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) const {
  return std::unique_ptr<IndexIterator>(
      primaryIndex()->allIterator(trx, mdr, reverse));
}

std::unique_ptr<IndexIterator> MMFilesCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) const {
  return std::unique_ptr<IndexIterator>(primaryIndex()->anyIterator(trx, mdr));
}

void MMFilesCollection::invokeOnAllElements(
    transaction::Methods* trx,
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  primaryIndex()->invokeOnAllElements(callback);
}

/// @brief read locks a collection, with a timeout (in µseconds)
int MMFilesCollection::lockRead(bool useDeadlockDetector, double timeout) {
  if (CollectionLockState::_noLockHeaders != nullptr) {
    auto it =
        CollectionLockState::_noLockHeaders->find(_logicalCollection->name());
    if (it != CollectionLockState::_noLockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginReadTimed blocked: " << _name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

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
        _logicalCollection->vocbase()->_deadlockDetector.addReader(
            _logicalCollection, wasBlocked);
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
          if (_logicalCollection->vocbase()->_deadlockDetector.setReaderBlocked(
                  _logicalCollection) == TRI_ERROR_DEADLOCK) {
            // deadlock
            LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
                << "deadlock detected while trying to acquire read-lock "
                   "on collection '"
                << _logicalCollection->name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
          LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
              << "waiting for read-lock on collection '"
              << _logicalCollection->name() << "'";
          // fall-through intentional
        } else if (++iterations >= 5) {
          // periodically check for deadlocks
          TRI_ASSERT(wasBlocked);
          iterations = 0;
          if (_logicalCollection->vocbase()->_deadlockDetector.detectDeadlock(
                  _logicalCollection, false) == TRI_ERROR_DEADLOCK) {
            // deadlock
            _logicalCollection->vocbase()->_deadlockDetector.unsetReaderBlocked(
                _logicalCollection);
            LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
                << "deadlock detected while trying to acquire read-lock "
                   "on collection '"
                << _logicalCollection->name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
        }
      } catch (...) {
        // clean up!
        if (wasBlocked) {
          _logicalCollection->vocbase()->_deadlockDetector.unsetReaderBlocked(
              _logicalCollection);
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
        _logicalCollection->vocbase()->_deadlockDetector.unsetReaderBlocked(
            _logicalCollection);
      }
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "timed out after " << timeout
          << " s waiting for read-lock on collection '"
          << _logicalCollection->name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }

    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      usleep(static_cast<TRI_usleep_t>(waitTime));
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief write locks a collection, with a timeout
int MMFilesCollection::lockWrite(bool useDeadlockDetector, double timeout) {
  if (CollectionLockState::_noLockHeaders != nullptr) {
    auto it =
        CollectionLockState::_noLockHeaders->find(_logicalCollection->name());
    if (it != CollectionLockState::_noLockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginWriteTimed blocked: " << _name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

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
        _logicalCollection->vocbase()->_deadlockDetector.addWriter(
            _logicalCollection, wasBlocked);
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
          if (_logicalCollection->vocbase()->_deadlockDetector.setWriterBlocked(
                  _logicalCollection) == TRI_ERROR_DEADLOCK) {
            // deadlock
            LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
                << "deadlock detected while trying to acquire "
                   "write-lock on collection '"
                << _logicalCollection->name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
          LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
              << "waiting for write-lock on collection '"
              << _logicalCollection->name() << "'";
        } else if (++iterations >= 5) {
          // periodically check for deadlocks
          TRI_ASSERT(wasBlocked);
          iterations = 0;
          if (_logicalCollection->vocbase()->_deadlockDetector.detectDeadlock(
                  _logicalCollection, true) == TRI_ERROR_DEADLOCK) {
            // deadlock
            _logicalCollection->vocbase()->_deadlockDetector.unsetWriterBlocked(
                _logicalCollection);
            LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
                << "deadlock detected while trying to acquire "
                   "write-lock on collection '"
                << _logicalCollection->name() << "'";
            return TRI_ERROR_DEADLOCK;
          }
        }
      } catch (...) {
        // clean up!
        if (wasBlocked) {
          _logicalCollection->vocbase()->_deadlockDetector.unsetWriterBlocked(
              _logicalCollection);
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
        _logicalCollection->vocbase()->_deadlockDetector.unsetWriterBlocked(
            _logicalCollection);
      }
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "timed out after " << timeout
          << " s waiting for write-lock on collection '"
          << _logicalCollection->name() << "'";
      return TRI_ERROR_LOCK_TIMEOUT;
    }

    if (now - startTime < 0.001) {
      std::this_thread::yield();
    } else {
      usleep(static_cast<TRI_usleep_t>(waitTime));
      if (waitTime < 32) {
        waitTime *= 2;
      }
    }
  }
}

/// @brief read unlocks a collection
int MMFilesCollection::unlockRead(bool useDeadlockDetector) {
  if (CollectionLockState::_noLockHeaders != nullptr) {
    auto it =
        CollectionLockState::_noLockHeaders->find(_logicalCollection->name());
    if (it != CollectionLockState::_noLockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndRead blocked: " << _name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  if (useDeadlockDetector) {
    // unregister reader
    try {
      _logicalCollection->vocbase()->_deadlockDetector.unsetReader(
          _logicalCollection);
    } catch (...) {
    }
  }

  // LOCKING-DEBUG
  // std::cout << "EndRead: " << _name << std::endl;
  _dataLock.unlockRead();

  return TRI_ERROR_NO_ERROR;
}

/// @brief write unlocks a collection
int MMFilesCollection::unlockWrite(bool useDeadlockDetector) {
  if (CollectionLockState::_noLockHeaders != nullptr) {
    auto it =
        CollectionLockState::_noLockHeaders->find(_logicalCollection->name());
    if (it != CollectionLockState::_noLockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndWrite blocked: " << _name <<
      // std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  if (useDeadlockDetector) {
    // unregister writer
    try {
      _logicalCollection->vocbase()->_deadlockDetector.unsetWriter(
          _logicalCollection);
    } catch (...) {
      // must go on here to unlock the lock
    }
  }

  // LOCKING-DEBUG
  // std::cout << "EndWrite: " << _name << std::endl;
  _dataLock.unlockWrite();

  return TRI_ERROR_NO_ERROR;
}

void MMFilesCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  auto primaryIdx = primaryIndex();

  options.ignoreRevs = true;

  // create remove marker
  transaction::BuilderLeaser builder(trx);

  auto callback = [&](MMFilesSimpleIndexElement const& element) {
    TRI_voc_rid_t oldRevisionId = element.revisionId();
    uint8_t const* vpack = lookupRevisionVPack(oldRevisionId);
    if (vpack != nullptr) {
      builder->clear();
      VPackSlice oldDoc(vpack);
      newObjectForRemove(trx, oldDoc, TRI_RidToString(oldRevisionId),
                         *builder.get());
      TRI_voc_rid_t revisionId = TRI_HybridLogicalClock();

      Result res = removeFastPath(trx, oldRevisionId, VPackSlice(vpack),
                                  options, revisionId, builder->slice());

      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res.errorNumber());
      }
    }

    return true;
  };
  primaryIdx->invokeOnAllElementsForRemoval(callback);
}

Result MMFilesCollection::insert(transaction::Methods* trx,
                                 VPackSlice const slice,
                                 ManagedDocumentResult& result,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick, bool lock) {
  VPackSlice fromSlice;
  VPackSlice toSlice;

  bool const isEdgeCollection =
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE);

  if (isEdgeCollection) {
    // _from:
    fromSlice = slice.get(StaticStrings::FromString);
    if (!fromSlice.isString()) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
    VPackValueLength len;
    char const* docId = fromSlice.getString(len);
    size_t split;
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len),
                                            &split)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
    // _to:
    toSlice = slice.get(StaticStrings::ToString);
    if (!toSlice.isString()) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
    docId = toSlice.getString(len);
    if (!TRI_ValidateDocumentIdKeyGenerator(docId, static_cast<size_t>(len),
                                            &split)) {
      return Result(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
    }
  }

  transaction::BuilderLeaser builder(trx);
  VPackSlice newSlice;
  Result res(TRI_ERROR_NO_ERROR);
  if (options.recoveryData == nullptr) {
    res = newObjectForInsert(trx, slice, fromSlice, toSlice, isEdgeCollection,
                             *builder.get(), options.isRestore);
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
      _logicalCollection->keyGenerator()->track(p, l);
    }
  }

  // create marker
  MMFilesCrudMarker insertMarker(
      TRI_DF_MARKER_VPACK_DOCUMENT,
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
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
  MMFilesDocumentOperation operation(_logicalCollection,
                                     TRI_VOC_DOCUMENT_OPERATION_INSERT);

  TRI_IF_FAILURE("InsertDocumentNoHeader") {
    // test what happens if no header can be acquired
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("InsertDocumentNoHeaderExcept") {
    // test what happens if no header can be acquired
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  TRI_voc_rid_t revisionId =
      transaction::helpers::extractRevFromDocument(newSlice);
  VPackSlice doc(marker->vpack());
  operation.setRevisions(DocumentDescriptor(),
                         DocumentDescriptor(revisionId, doc.begin()));

  MMFilesDocumentPosition old;

  try {
    old = insertRevision(revisionId, marker->vpack(), 0, true, true);
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

  res = Result(TRI_ERROR_NO_ERROR);
  {
    // use lock?
    bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
    try {
      arangodb::MMFilesCollectionWriteLocker collectionLocker(
          this, useDeadlockDetector, lock);

      try {
        // insert into indexes
        res = insertDocument(trx, revisionId, doc, operation, marker,
                             options.waitForSync);
      } catch (basics::Exception const& ex) {
        res = Result(ex.code());
      } catch (std::bad_alloc const&) {
        res = Result(TRI_ERROR_OUT_OF_MEMORY);
      } catch (std::exception const& ex) {
        res = Result(TRI_ERROR_INTERNAL, ex.what());
      } catch (...) {
        res = Result(TRI_ERROR_INTERNAL);
      }
    } catch (...) {
      // the collectionLocker may have thrown in its constructor...
      // if it did, then we need to manually remove the revision id
      // from the list of revisions
      try {
        removeRevision(revisionId, false);
        if (old) {
          insertRevision(old, true);
        }
      } catch (...) {
      }
      throw;
    }

    if (res.fail()) {
      operation.revert(trx);

      if (old) {
        insertRevision(old, true);
      }
    }
  }

  if (res.ok()) {
    uint8_t const* vpack = lookupRevisionVPack(revisionId);
    if (vpack != nullptr) {
      result.setUnmanaged(vpack, revisionId);
    }

    // store the tick that was used for writing the document
    resultMarkerTick = operation.tick();
  }
  return res;
}

bool MMFilesCollection::isFullyCollected() const {
  int64_t uncollected = _uncollectedLogfileEntries.load();
  return (uncollected == 0);
}

MMFilesDocumentPosition MMFilesCollection::lookupRevision(
    TRI_voc_rid_t revisionId) const {
  TRI_ASSERT(revisionId != 0);
  MMFilesDocumentPosition const old = _revisionsCache.lookup(revisionId);
  if (old) {
    return old;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "got invalid revision value on lookup");
}

uint8_t const* MMFilesCollection::lookupRevisionVPack(
    TRI_voc_rid_t revisionId) const {
  TRI_ASSERT(revisionId != 0);

  MMFilesDocumentPosition const old = _revisionsCache.lookup(revisionId);
  if (old) {
    uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
    TRI_ASSERT(VPackSlice(vpack).isObject());
    return vpack;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "got invalid vpack value on lookup");
}

uint8_t const* MMFilesCollection::lookupRevisionVPackConditional(
    TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal) const {
  TRI_ASSERT(revisionId != 0);

  MMFilesDocumentPosition const old = _revisionsCache.lookup(revisionId);
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

MMFilesDocumentPosition MMFilesCollection::insertRevision(
    TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid,
    bool isInWal, bool shouldLock) {
  TRI_ASSERT(revisionId != 0);
  TRI_ASSERT(dataptr != nullptr);
  return _revisionsCache.insert(revisionId, dataptr, fid, isInWal, shouldLock);
}

void MMFilesCollection::insertRevision(MMFilesDocumentPosition const& position,
                                       bool shouldLock) {
  return _revisionsCache.insert(position, shouldLock);
}

void MMFilesCollection::updateRevision(TRI_voc_rid_t revisionId,
                                       uint8_t const* dataptr,
                                       TRI_voc_fid_t fid, bool isInWal) {
  TRI_ASSERT(revisionId != 0);
  TRI_ASSERT(dataptr != nullptr);
  _revisionsCache.update(revisionId, dataptr, fid, isInWal);
}

bool MMFilesCollection::updateRevisionConditional(
    TRI_voc_rid_t revisionId, MMFilesMarker const* oldPosition,
    MMFilesMarker const* newPosition, TRI_voc_fid_t newFid, bool isInWal) {
  TRI_ASSERT(revisionId != 0);
  TRI_ASSERT(newPosition != nullptr);
  return _revisionsCache.updateConditional(revisionId, oldPosition, newPosition,
                                           newFid, isInWal);
}

void MMFilesCollection::removeRevision(TRI_voc_rid_t revisionId,
                                       bool updateStats) {
  TRI_ASSERT(revisionId != 0);
  if (updateStats) {
    MMFilesDocumentPosition const old =
        _revisionsCache.fetchAndRemove(revisionId);
    if (old && !old.pointsToWal() && old.fid() != 0) {
      TRI_ASSERT(old.dataptr() != nullptr);
      uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
      int64_t size = encoding::alignedSize<int64_t>(
          MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT) +
          VPackSlice(vpack).byteSize());
      _datafileStatistics.increaseDead(old.fid(), 1, size);
    }
  } else {
    _revisionsCache.remove(revisionId);
  }
}

/// @brief creates a new entry in the primary index
Result MMFilesCollection::insertPrimaryIndex(transaction::Methods* trx,
                                             TRI_voc_rid_t revisionId,
                                             VPackSlice const& doc) {
  TRI_IF_FAILURE("InsertPrimaryIndex") { return Result(TRI_ERROR_DEBUG); }

  // insert into primary index
  return primaryIndex()->insertKey(trx, revisionId, doc);
}

/// @brief deletes an entry from the primary index
Result MMFilesCollection::deletePrimaryIndex(
    arangodb::transaction::Methods* trx, TRI_voc_rid_t revisionId,
    VPackSlice const& doc) {
  TRI_IF_FAILURE("DeletePrimaryIndex") { return Result(TRI_ERROR_DEBUG); }

  return primaryIndex()->removeKey(trx, revisionId, doc);
}

/// @brief creates a new entry in the secondary indexes
Result MMFilesCollection::insertSecondaryIndexes(
    arangodb::transaction::Methods* trx, TRI_voc_rid_t revisionId,
    VPackSlice const& doc, bool isRollback) {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_IF_FAILURE("InsertSecondaryIndexes") { return Result(TRI_ERROR_DEBUG); }

  bool const useSecondary = useSecondaryIndexes();
  if (!useSecondary && _persistentIndexes == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  Result result = Result(TRI_ERROR_NO_ERROR);

  auto indexes = _indexes;
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

    if (!useSecondary && !idx->isPersistent()) {
      continue;
    }

    Result res = idx->insert(trx, revisionId, doc, isRollback);

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
    arangodb::transaction::Methods* trx, TRI_voc_rid_t revisionId,
    VPackSlice const& doc, bool isRollback) {
  // Coordintor doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  bool const useSecondary = useSecondaryIndexes();
  if (!useSecondary && _persistentIndexes == 0) {
    return Result(TRI_ERROR_NO_ERROR);
  }

  TRI_IF_FAILURE("DeleteSecondaryIndexes") { return Result(TRI_ERROR_DEBUG); }

  Result result = Result(TRI_ERROR_NO_ERROR);

  // TODO FIXME
  auto indexes = _logicalCollection->getIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    TRI_ASSERT(idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);

    if (!useSecondary && !idx->isPersistent()) {
      continue;
    }

    Result res = idx->remove(trx, revisionId, doc, isRollback);

    if (res.fail()) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

/// @brief enumerate all indexes of the collection, but don't fill them yet
int MMFilesCollection::detectIndexes(transaction::Methods* trx) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  VPackBuilder builder;
  engine->getCollectionInfo(_logicalCollection->vocbase(),
                            _logicalCollection->cid(), builder, true,
                            UINT64_MAX);

  // iterate over all index files
  for (auto const& it : VPackArrayIterator(builder.slice().get("indexes"))) {
    bool ok = openIndex(it, trx);

    if (!ok) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "cannot load index for collection '" << _logicalCollection->name()
          << "'";
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief insert a document into all indexes known to
///        this collection.
///        This function guarantees all or nothing,
///        If it returns NO_ERROR all indexes are filled.
///        If it returns an error no documents are inserted
Result MMFilesCollection::insertIndexes(arangodb::transaction::Methods* trx,
                                        TRI_voc_rid_t revisionId,
                                        VPackSlice const& doc) {
  // insert into primary index first
  Result res = insertPrimaryIndex(trx, revisionId, doc);

  if (res.fail()) {
    // insert has failed
    return res;
  }

  // insert into secondary indexes
  res = insertSecondaryIndexes(trx, revisionId, doc, false);

  if (res.fail()) {
    deleteSecondaryIndexes(trx, revisionId, doc, true);
    deletePrimaryIndex(trx, revisionId, doc);
  }
  return res;
}

/// @brief insert a document, low level worker
/// the caller must make sure the write lock on the collection is held
Result MMFilesCollection::insertDocument(arangodb::transaction::Methods* trx,
                                         TRI_voc_rid_t revisionId,
                                         VPackSlice const& doc,
                                         MMFilesDocumentOperation& operation,
                                         MMFilesWalMarker const* marker,
                                         bool& waitForSync) {
  Result res = insertIndexes(trx, revisionId, doc);
  if (res.fail()) {
    return res;
  }
  operation.indexed();

  TRI_IF_FAILURE("InsertDocumentNoOperation") { return Result(TRI_ERROR_DEBUG); }

  TRI_IF_FAILURE("InsertDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return Result(static_cast<MMFilesTransactionState*>(trx->state())
      ->addOperation(revisionId, operation, marker, waitForSync));
}

Result MMFilesCollection::update(
    arangodb::transaction::Methods* trx, VPackSlice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous, TRI_voc_rid_t const& revisionId,
    VPackSlice const key) {
  bool const isEdgeCollection =
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE);
  TRI_IF_FAILURE("UpdateDocumentNoLock") { return Result(TRI_ERROR_DEBUG); }

  bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
  arangodb::MMFilesCollectionWriteLocker collectionLocker(
      this, useDeadlockDetector, lock);

  // get the previous revision
  Result res = lookupDocument(trx, key, previous);

  if (res.fail()) {
    return res;
  }

  uint8_t const* vpack = previous.vpack();
  VPackSlice oldDoc(vpack);
  TRI_voc_rid_t oldRevisionId =
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
      return res;
    }
  }

  if (newSlice.length() <= 1) {
    // no need to do anything
    result = std::move(previous);
    if (_logicalCollection->waitForSync()) {
      options.waitForSync = true;
    }
    return Result(TRI_ERROR_NO_ERROR);
  }

  // merge old and new values
  transaction::BuilderLeaser builder(trx);
  if (options.recoveryData == nullptr) {
    mergeObjectsForUpdate(trx, oldDoc, newSlice, isEdgeCollection,
                          TRI_RidToString(revisionId), options.mergeObjects,
                          options.keepNull, *builder.get());

    if (trx->state()->isDBServer()) {
      // Need to check that no sharding keys have changed:
      if (arangodb::shardKeysChanged(_logicalCollection->dbName(),
                                     trx->resolver()->getCollectionNameCluster(
                                         _logicalCollection->planId()),
                                     oldDoc, builder->slice(), false)) {
        return Result(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
      }
    }
  }

  // create marker
  MMFilesCrudMarker updateMarker(
      TRI_DF_MARKER_VPACK_DOCUMENT,
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
      builder->slice());

  MMFilesWalMarker const* marker;
  if (options.recoveryData == nullptr) {
    marker = &updateMarker;
  } else {
    marker = static_cast<MMFilesWalMarker*>(options.recoveryData);
  }

  VPackSlice const newDoc(marker->vpack());

  MMFilesDocumentOperation operation(_logicalCollection,
                                     TRI_VOC_DOCUMENT_OPERATION_UPDATE);

  try {
    insertRevision(revisionId, marker->vpack(), 0, true, true);

    operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()),
                           DocumentDescriptor(revisionId, newDoc.begin()));

    if (oldRevisionId == revisionId) {
      // update with same revision id => can happen if isRestore = true
      result.reset();
    }

    res = updateDocument(trx, oldRevisionId, oldDoc, revisionId, newDoc,
                         operation, marker, options.waitForSync);
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
    uint8_t const* vpack = lookupRevisionVPack(revisionId);
    if (vpack != nullptr) {
      result.setUnmanaged(vpack, revisionId);
    }
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
    ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId,
    VPackSlice const fromSlice, VPackSlice const toSlice) {
  bool const isEdgeCollection =
      (_logicalCollection->type() == TRI_COL_TYPE_EDGE);
  TRI_IF_FAILURE("ReplaceDocumentNoLock") { return Result(TRI_ERROR_DEBUG); }

  // get the previous revision
  VPackSlice key = newSlice.get(StaticStrings::KeyString);
  if (key.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
  arangodb::MMFilesCollectionWriteLocker collectionLocker(
      this, useDeadlockDetector, lock);

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
  transaction::BuilderLeaser builder(trx);
  newObjectForReplace(trx, oldDoc, newSlice, fromSlice, toSlice,
                      isEdgeCollection, TRI_RidToString(revisionId),
                      *builder.get());

  if (trx->state()->isDBServer()) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection->dbName(),
                                   trx->resolver()->getCollectionNameCluster(
                                       _logicalCollection->planId()),
                                   oldDoc, builder->slice(), false)) {
      return Result(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
  }

  // create marker
  MMFilesCrudMarker replaceMarker(
      TRI_DF_MARKER_VPACK_DOCUMENT,
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
      builder->slice());

  MMFilesWalMarker const* marker;
  if (options.recoveryData == nullptr) {
    marker = &replaceMarker;
  } else {
    marker = static_cast<MMFilesWalMarker*>(options.recoveryData);
  }

  VPackSlice const newDoc(marker->vpack());

  MMFilesDocumentOperation operation(_logicalCollection,
                                     TRI_VOC_DOCUMENT_OPERATION_REPLACE);

  try {
    insertRevision(revisionId, marker->vpack(), 0, true, true);

    operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()),
                           DocumentDescriptor(revisionId, newDoc.begin()));

    if (oldRevisionId == revisionId) {
      // update with same revision id => can happen if isRestore = true
      result.reset();
    }

    res = updateDocument(trx, oldRevisionId, oldDoc, revisionId, newDoc,
                         operation, marker, options.waitForSync);
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
    if (oldRevisionId == revisionId) {
      // update with same revision id => can happen if isRestore = true
      result.reset();
    }
    uint8_t const* vpack = lookupRevisionVPack(revisionId);
    if (vpack != nullptr) {
      result.setUnmanaged(vpack, revisionId);
    }

    if (options.waitForSync) {
      // store the tick that was used for writing the new document
      resultMarkerTick = operation.tick();
    }
  }

  return res;
}

Result MMFilesCollection::remove(arangodb::transaction::Methods* trx,
                                 VPackSlice const slice,
                                 ManagedDocumentResult& previous,
                                 OperationOptions& options,
                                 TRI_voc_tick_t& resultMarkerTick, bool lock,
                                 TRI_voc_rid_t const& revisionId,
                                 TRI_voc_rid_t& prevRev) {
  prevRev = 0;

  transaction::BuilderLeaser builder(trx);
  newObjectForRemove(trx, slice, TRI_RidToString(revisionId), *builder.get());

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
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
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

  MMFilesDocumentOperation operation(_logicalCollection,
                                     TRI_VOC_DOCUMENT_OPERATION_REMOVE);

  bool const useDeadlockDetector =
      (lock && !trx->isSingleOperationTransaction() && !trx->state()->hasHint(transaction::Hints::Hint::NO_DLD));
  arangodb::MMFilesCollectionWriteLocker collectionLocker(
      this, useDeadlockDetector, lock);

  // get the previous revision
  Result res = lookupDocument(trx, key, previous);

  if (res.fail()) {
    return res;
  }

  uint8_t const* vpack = previous.vpack();
  VPackSlice oldDoc(vpack);
  TRI_voc_rid_t oldRevisionId =
      arangodb::transaction::helpers::extractRevFromDocument(oldDoc);
  prevRev = oldRevisionId;

  // Check old revision:
  if (!options.ignoreRevs && slice.isObject()) {
    TRI_voc_rid_t expectedRevisionId = TRI_ExtractRevisionId(slice);
    res = checkRevision(trx, expectedRevisionId, oldRevisionId);

    if (res.fail()) {
      return res;
    }
  }

  // we found a document to remove
  try {
    operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()),
                           DocumentDescriptor());

    // delete from indexes
    res = deleteSecondaryIndexes(trx, oldRevisionId, oldDoc, false);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res);
    }

    res = deletePrimaryIndex(trx, oldRevisionId, oldDoc);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res);
    }

    operation.indexed();

    TRI_IF_FAILURE("RemoveDocumentNoOperation") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    try {
      removeRevision(oldRevisionId, true);
    } catch (...) {
    }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res =
        static_cast<MMFilesTransactionState*>(trx->state())
            ->addOperation(revisionId, operation, marker, options.waitForSync);
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
    std::function<bool(LogicalCollection*)> callback) {
  // add callback for dropping
  ditches()->createMMFilesDropCollectionDitch(_logicalCollection, callback,
                                              __FILE__, __LINE__);
}

/// @brief rolls back a document operation
Result MMFilesCollection::rollbackOperation(transaction::Methods* trx,
                                            TRI_voc_document_operation_e type,
                                            TRI_voc_rid_t oldRevisionId,
                                            VPackSlice const& oldDoc,
                                            TRI_voc_rid_t newRevisionId,
                                            VPackSlice const& newDoc) {
  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(oldRevisionId == 0);
    TRI_ASSERT(oldDoc.isNone());
    TRI_ASSERT(newRevisionId != 0);
    TRI_ASSERT(!newDoc.isNone());

    // ignore any errors we're getting from this
    deletePrimaryIndex(trx, newRevisionId, newDoc);
    deleteSecondaryIndexes(trx, newRevisionId, newDoc, true);
    return TRI_ERROR_NO_ERROR;
  }

  if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
      type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    TRI_ASSERT(oldRevisionId != 0);
    TRI_ASSERT(!oldDoc.isNone());
    TRI_ASSERT(newRevisionId != 0);
    TRI_ASSERT(!newDoc.isNone());

    // remove the current values from the indexes
    deleteSecondaryIndexes(trx, newRevisionId, newDoc, true);
    // re-insert old state
    return insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
  }

  if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    // re-insert old revision
    TRI_ASSERT(oldRevisionId != 0);
    TRI_ASSERT(!oldDoc.isNone());
    TRI_ASSERT(newRevisionId == 0);
    TRI_ASSERT(newDoc.isNone());

    Result res = insertPrimaryIndex(trx, oldRevisionId, oldDoc);

    if (res.ok()) {
      res = insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "error rolling back remove operation";
    }
    return res;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "logic error. invalid operation type on rollback";
#endif
  return TRI_ERROR_INTERNAL;
}

/// @brief removes a document or edge, fast path function for database documents
Result MMFilesCollection::removeFastPath(arangodb::transaction::Methods* trx,
                                         TRI_voc_rid_t oldRevisionId,
                                         VPackSlice const oldDoc,
                                         OperationOptions& options,
                                         TRI_voc_rid_t const& revisionId,
                                         VPackSlice const toRemove) {
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
      static_cast<MMFilesTransactionState*>(trx->state())->idForMarker(),
      toRemove);

  MMFilesWalMarker const* marker = &removeMarker;

  TRI_IF_FAILURE("RemoveDocumentNoLock") {
    // test what happens if no lock can be acquired
    return Result(TRI_ERROR_DEBUG);
  }

  VPackSlice key =
      arangodb::transaction::helpers::extractKeyFromDocument(oldDoc);
  TRI_ASSERT(!key.isNone());

  MMFilesDocumentOperation operation(_logicalCollection,
                                     TRI_VOC_DOCUMENT_OPERATION_REMOVE);

  operation.setRevisions(DocumentDescriptor(oldRevisionId, oldDoc.begin()),
                         DocumentDescriptor());

  // delete from indexes
  Result res;
  try {
    res = deleteSecondaryIndexes(trx, oldRevisionId, oldDoc, false);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res.errorNumber());
    }

    res = deletePrimaryIndex(trx, oldRevisionId, oldDoc);

    if (res.fail()) {
      insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
      THROW_ARANGO_EXCEPTION(res.errorNumber());
    }

    operation.indexed();

    try {
      removeRevision(oldRevisionId, true);
    } catch (...) {
    }

    TRI_IF_FAILURE("RemoveDocumentNoOperation") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res =
        static_cast<MMFilesTransactionState*>(trx->state())
            ->addOperation(revisionId, operation, marker, options.waitForSync);
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
    TRI_voc_rid_t revisionId = element.revisionId();
    uint8_t const* vpack = lookupRevisionVPack(revisionId);
    if (vpack != nullptr) {
      result.setUnmanaged(vpack, revisionId);
    }
    return Result(TRI_ERROR_NO_ERROR);
  }

  return Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

/// @brief updates an existing document, low level worker
/// the caller must make sure the write lock on the collection is held
Result MMFilesCollection::updateDocument(
    transaction::Methods* trx, TRI_voc_rid_t oldRevisionId,
    VPackSlice const& oldDoc, TRI_voc_rid_t newRevisionId,
    VPackSlice const& newDoc, MMFilesDocumentOperation& operation,
    MMFilesWalMarker const* marker, bool& waitForSync) {
  // remove old document from secondary indexes
  // (it will stay in the primary index as the key won't change)
  Result res = deleteSecondaryIndexes(trx, oldRevisionId, oldDoc, false);

  if (res.fail()) {
    // re-enter the document in case of failure, ignore errors during rollback
    insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
    return res;
  }

  // insert new document into secondary indexes
  res = insertSecondaryIndexes(trx, newRevisionId, newDoc, false);

  if (res.fail()) {
    // rollback
    deleteSecondaryIndexes(trx, newRevisionId, newDoc, true);
    insertSecondaryIndexes(trx, oldRevisionId, oldDoc, true);
    return res;
  }

  // update the index element (primary index only - other index have been
  // adjusted)
  VPackSlice keySlice(transaction::helpers::extractKeyFromDocument(newDoc));
  MMFilesSimpleIndexElement* element =
      primaryIndex()->lookupKeyRef(trx, keySlice);
  if (element != nullptr && element->revisionId() != 0) {
    element->updateRevisionId(
        newRevisionId,
        static_cast<uint32_t>(keySlice.begin() - newDoc.begin()));
  }

  operation.indexed();

  if (oldRevisionId != newRevisionId) {
    try {
      removeRevision(oldRevisionId, true);
    } catch (...) {
    }
  }

  TRI_IF_FAILURE("UpdateDocumentNoOperation") {
    return Result(TRI_ERROR_DEBUG);
  }

  TRI_IF_FAILURE("UpdateDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return Result(static_cast<MMFilesTransactionState*>(trx->state())
      ->addOperation(newRevisionId, operation, marker, waitForSync));
}
