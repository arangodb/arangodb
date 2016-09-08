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
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/WriteLocker.h"
#include "Indexes/PrimaryIndex.h"
#include "Logger/Logger.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/datafile.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;

namespace {

/// @brief state during opening of a collection
struct OpenIteratorState {
  LogicalCollection* _collection;
  TRI_voc_tid_t _tid;
  TRI_voc_fid_t _fid;
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer*> _stats;
  DatafileStatisticsContainer* _dfi;
  arangodb::Transaction* _trx;
  uint64_t _deletions;
  uint64_t _documents;
  int64_t _initialCount;

  explicit OpenIteratorState(LogicalCollection* collection) 
      : _collection(collection),
        _tid(0),
        _fid(0),
        _stats(),
        _dfi(nullptr),
        _trx(nullptr),
        _deletions(0),
        _documents(0),
        _initialCount(-1) {
    TRI_ASSERT(collection != nullptr);
  }

  ~OpenIteratorState() {
    for (auto& it : _stats) {
      delete it.second;
    }
  }
};

/// @brief find a statistics container for a given file id
static DatafileStatisticsContainer* FindDatafileStats(
    OpenIteratorState* state, TRI_voc_fid_t fid) {
  auto it = state->_stats.find(fid);

  if (it != state->_stats.end()) {
    return (*it).second;
  }

  auto stats = std::make_unique<DatafileStatisticsContainer>();
  state->_stats.emplace(fid, stats.get());
  return stats.release();
}

/// @brief process a document (or edge) marker when opening a collection
static int OpenIteratorHandleDocumentMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            OpenIteratorState* state) {
  auto const fid = datafile->fid();
  LogicalCollection* collection = state->_collection;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  collection->setRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  collection->keyGenerator()->track(p, length);

  ++state->_documents;
 
  if (state->_fid != fid) {
    // update the state
    state->_fid = fid; // when we're here, we're looking at a datafile
    state->_dfi = FindDatafileStats(state, fid);
  }

  auto primaryIndex = collection->primaryIndex();

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto found = primaryIndex->lookupKey(trx, keySlice);

  // it is a new entry
  if (found == nullptr) {
    TRI_doc_mptr_t* header = collection->requestMasterpointer();

    if (header == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    header->setFid(fid, false);
    header->setHash(primaryIndex->calculateHash(trx, keySlice));
    header->setVPackFromMarker(marker);  

    // insert into primary index
    void const* result = nullptr;
    int res = primaryIndex->insertKey(trx, header, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      collection->releaseMasterpointer(header);
      LOG(ERR) << "inserting document into primary index failed with error: " << TRI_errno_string(res);

      return res;
    }

    collection->incNumberDocuments();

    // update the datafile info
    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += DatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  // it is an update, but only if found has a smaller revision identifier
  else {
    // save the old data
    TRI_doc_mptr_t oldData = *found;

    // update the header info
    found->setFid(fid, false); // when we're here, we're looking at a datafile
    found->setVPackFromMarker(marker);

    // update the datafile info
    DatafileStatisticsContainer* dfi;
    if (oldData.getFid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, oldData.getFid());
    }

    if (oldData.vpack() != nullptr) { 
      int64_t size = static_cast<int64_t>(oldData.markerSize());

      dfi->numberAlive--;
      dfi->sizeAlive -= DatafileHelper::AlignedSize<int64_t>(size);
      dfi->numberDead++;
      dfi->sizeDead += DatafileHelper::AlignedSize<int64_t>(size);
    }

    state->_dfi->numberAlive++;
    state->_dfi->sizeAlive += DatafileHelper::AlignedMarkerSize<int64_t>(marker);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief process a deletion marker when opening a collection
static int OpenIteratorHandleDeletionMarker(TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            OpenIteratorState* state) {
  LogicalCollection* collection = state->_collection;
  arangodb::Transaction* trx = state->_trx;

  VPackSlice const slice(reinterpret_cast<char const*>(marker) + DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_REMOVE));
  
  VPackSlice keySlice;
  TRI_voc_rid_t revisionId;

  Transaction::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
 
  collection->setRevision(revisionId, false);
  VPackValueLength length;
  char const* p = keySlice.getString(length);
  collection->keyGenerator()->track(p, length);

  ++state->_deletions;

  if (state->_fid != datafile->fid()) {
    // update the state
    state->_fid = datafile->fid();
    state->_dfi = FindDatafileStats(state, datafile->fid());
  }

  // no primary index lock required here because we are the only ones reading
  // from the index ATM
  auto primaryIndex = collection->primaryIndex();
  TRI_doc_mptr_t* found = primaryIndex->lookupKey(trx, keySlice);

  // it is a new entry, so we missed the create
  if (found == nullptr) {
    // update the datafile info
    state->_dfi->numberDeletions++;
  }

  // it is a real delete
  else {
    // update the datafile info
    DatafileStatisticsContainer* dfi;

    if (found->getFid() == state->_fid) {
      dfi = state->_dfi;
    } else {
      dfi = FindDatafileStats(state, found->getFid());
    }

    TRI_ASSERT(found->vpack() != nullptr);

    int64_t size = DatafileHelper::AlignedSize<int64_t>(found->markerSize());

    dfi->numberAlive--;
    dfi->sizeAlive -= DatafileHelper::AlignedSize<int64_t>(size);
    dfi->numberDead++;
    dfi->sizeDead += DatafileHelper::AlignedSize<int64_t>(size);
    state->_dfi->numberDeletions++;

    collection->deletePrimaryIndex(trx, found);
    collection->decNumberDocuments();

    // free the header
    collection->releaseMasterpointer(found);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief iterator for open
static bool OpenIterator(TRI_df_marker_t const* marker, OpenIteratorState* data,
                         TRI_datafile_t* datafile) {
  TRI_voc_tick_t const tick = marker->getTick();
  TRI_df_marker_type_t const type = marker->getType();

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

    LOG(TRACE) << "skipping marker type " << TRI_NameMarkerDatafile(marker);
    res = TRI_ERROR_NO_ERROR;
  }

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }

  if (tick > datafile->_tickMax) {
    datafile->_tickMax = tick;
  }

  if (tick > data->_collection->maxTick()) {
    if (type != TRI_DF_MARKER_HEADER &&
        type != TRI_DF_MARKER_FOOTER &&
        type != TRI_DF_MARKER_COL_HEADER &&
        type != TRI_DF_MARKER_PROLOGUE) {
      data->_collection->maxTick(tick);
    }
  }

  return (res == TRI_ERROR_NO_ERROR);
}

} // namespace

MMFilesCollection::MMFilesCollection(LogicalCollection* collection)
    : PhysicalCollection(collection), _ditches(collection), _initialCount(0), _revision(0) {}

MMFilesCollection::~MMFilesCollection() { 
  try {
    close(); 
  } catch (...) {
    // dtor must not propagate exceptions
  }
}

TRI_voc_rid_t MMFilesCollection::revision() const { 
  return _revision; 
}

void MMFilesCollection::setRevision(TRI_voc_rid_t revision, bool force) {
  if (force || revision > _revision) {
    _revision = revision;
  }
}

int64_t MMFilesCollection::initialCount() const { 
  return _initialCount;
}

void MMFilesCollection::updateCount(int64_t count) {
  _initialCount = count;
}

/// @brief closes an open collection
int MMFilesCollection::close() {
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

  _revision = 0;

  return TRI_ERROR_NO_ERROR;
}

/// @brief seal a datafile
int MMFilesCollection::sealDatafile(TRI_datafile_t* datafile, bool isCompactor) {
  int res = datafile->seal();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to seal journal '" << datafile->getName()
             << "': " << TRI_errno_string(res);
    return res;
  }

  if (!isCompactor && datafile->isPhysical()) {
    // rename the file
    std::string dname("datafile-" + std::to_string(datafile->fid()) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(_logicalCollection->path(), dname);

    res = datafile->rename(filename);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG(TRACE) << "closed file '" << datafile->getName() << "'";
    } else {
      LOG(ERR) << "failed to rename datafile '" << datafile->getName()
               << "' to '" << filename << "': " << TRI_errno_string(res);
    }
  }

  return res;
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

  TRI_datafile_t* datafile = _journals[0];
  TRI_ASSERT(datafile != nullptr);

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
  _journals.erase(_journals.begin());
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

  TRI_datafile_t* datafile = _journals[0];
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

        LOG_TOPIC(ERR, Logger::COLLECTOR)
            << "msync failed with: " << TRI_last_error();
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
                                           TRI_datafile_t*& resultDatafile) {
  // reset results
  resultPosition = nullptr;
  resultDatafile = nullptr;

  WRITE_LOCKER(writeLocker, _filesLock);

  // start with configured journal size
  TRI_voc_size_t targetSize = static_cast<TRI_voc_size_t>(_logicalCollection->journalSize());

  // make sure that the document fits
  while (targetSize - 256 < size) {
    targetSize *= 2;
  }

  while (true) {
    TRI_datafile_t* datafile = nullptr;

    if (_journals.empty()) {
      // create enough room in the journals vector
      _journals.reserve(_journals.size() + 1);

      try {
        std::unique_ptr<TRI_datafile_t> df(createDatafile(tick, targetSize, false));

        // shouldn't throw as we reserved enough space before
        _journals.emplace_back(df.get());
        df.release();
      } catch (basics::Exception const& ex) {
        LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: " << ex.what();
        return ex.code();
      } catch (std::exception const& ex) {
        LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: " << ex.what();
        return TRI_ERROR_INTERNAL;
      } catch (...) {
        LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: unknown exception";
        return TRI_ERROR_INTERNAL;
      }
    } 
      
    // select datafile
    TRI_ASSERT(!_journals.empty());
    datafile = _journals[0];

    TRI_ASSERT(datafile != nullptr);

    // try to reserve space in the datafile

    TRI_df_marker_t* position = nullptr;
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
    _journals.erase(_journals.begin());
    TRI_ASSERT(_journals.empty());

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred, we must stop here
      return res;
    }
  }  // otherwise, next iteration!

  return TRI_ERROR_ARANGO_NO_JOURNAL;
}

/// @brief create compactor file
TRI_datafile_t* MMFilesCollection::createCompactor(TRI_voc_fid_t fid,
                                                   TRI_voc_size_t maximalSize) {
  WRITE_LOCKER(writeLocker, _filesLock);

  TRI_ASSERT(_compactors.empty());
  // reserve enough space for the later addition
  _compactors.reserve(_compactors.size() + 1);

  std::unique_ptr<TRI_datafile_t> compactor(createDatafile(fid, static_cast<TRI_voc_size_t>(maximalSize), true));

  // should not throw, as we've reserved enough space before
  _compactors.emplace_back(compactor.get());
  return compactor.release();
}

/// @brief close an existing compactor
int MMFilesCollection::closeCompactor(TRI_datafile_t* datafile) {
  WRITE_LOCKER(writeLocker, _filesLock);

  if (_compactors.size() != 1) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  TRI_datafile_t* compactor = _compactors[0];

  if (datafile != compactor) {
    // wrong compactor file specified... should not happen
    return TRI_ERROR_INTERNAL;
  }

  return sealDatafile(datafile, true);
}

/// @brief replace a datafile with a compactor
int MMFilesCollection::replaceDatafileWithCompactor(TRI_datafile_t* datafile,
                                                    TRI_datafile_t* compactor) {
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
TRI_datafile_t* MMFilesCollection::createDatafile(TRI_voc_fid_t fid,
                                                  TRI_voc_size_t journalSize,
                                                  bool isCompactor) {
  TRI_ASSERT(fid > 0);

  // create an entry for the new datafile
  try {
    _datafileStatistics.create(fid);
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::unique_ptr<TRI_datafile_t> datafile;

  if (_logicalCollection->isVolatile()) {
    // in-memory collection
    datafile.reset(TRI_datafile_t::create(StaticStrings::Empty, fid, journalSize, true));
  } else {
    // construct a suitable filename (which may be temporary at the beginning)
    std::string jname;
    if (isCompactor) {
      jname = "compaction-";
    } else {
      jname = "temp-";
    }

    jname.append(std::to_string(fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(_logicalCollection->path(), jname);

    TRI_IF_FAILURE("CreateJournalDocumentCollection") {
      // simulate disk full
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
    }

    // remove an existing temporary file first
    if (TRI_ExistsFile(filename.c_str())) {
      // remove an existing file first
      TRI_UnlinkFile(filename.c_str());
    }

    datafile.reset(TRI_datafile_t::create(filename, fid, journalSize, true));
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
    LOG(TRACE) << "created new compactor '" << datafile->getName()
               << "'";
  } else {
    LOG(TRACE) << "created new journal '" << datafile->getName() << "'";
  }

  // create a collection header, still in the temporary file
  TRI_df_marker_t* position;
  int res = datafile->reserveElement(sizeof(TRI_col_header_marker_t), &position, journalSize);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve1") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot create collection header in file '"
             << datafile->getName() << "': " << TRI_errno_string(res);

    // close the journal and remove it
    std::string temp(datafile->getName());
    datafile.reset();
    TRI_UnlinkFile(temp.c_str());

    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_col_header_marker_t cm;
  DatafileHelper::InitMarker(
      reinterpret_cast<TRI_df_marker_t*>(&cm), TRI_DF_MARKER_COL_HEADER,
      sizeof(TRI_col_header_marker_t), static_cast<TRI_voc_tick_t>(fid));
  cm._cid = _logicalCollection->cid();

  res = datafile->writeCrcElement(position, &cm.base, false);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    int res = datafile->_lastError;
    LOG(ERR) << "cannot create collection header in file '"
             << datafile->getName() << "': " << TRI_last_error();

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
    std::string filename = arangodb::basics::FileUtils::buildFilename(_logicalCollection->path(), jname);

    int res = datafile->rename(filename);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "failed to rename journal '" << datafile->getName()
               << "' to '" << filename << "': " << TRI_errno_string(res);

      std::string temp(datafile->getName());
      datafile.reset();
      TRI_UnlinkFile(temp.c_str());

      THROW_ARANGO_EXCEPTION(res);
    } 
      
    LOG(TRACE) << "renamed journal from '" << datafile->getName()
               << "' to '" << filename << "'";
  }

  return datafile.release();
}

/// @brief remove a compactor file from the list of compactors
bool MMFilesCollection::removeCompactor(TRI_datafile_t* df) {
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
bool MMFilesCollection::removeDatafile(TRI_datafile_t* df) {
  TRI_ASSERT(df != nullptr);

  WRITE_LOCKER(writeLocker, _filesLock);

  for (auto it = _datafiles.begin(); it != _datafiles.end(); ++it) {
    if ((*it) == df) {
      // and finally remove the file from the _compactors vector
      _datafiles.erase(it);
      return true;
    }
  }

  // not found
  return false;
}

/// @brief iterates over a collection
bool MMFilesCollection::iterateDatafiles(std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb) {
  if (!iterateDatafilesVector(_datafiles, cb) ||
      !iterateDatafilesVector(_compactors, cb) ||
      !iterateDatafilesVector(_journals, cb)) {
    return false;
  }
  return true;
}

/// @brief iterate over all datafiles in a vector
bool MMFilesCollection::iterateDatafilesVector(std::vector<TRI_datafile_t*> const& files,
                                               std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb) {
  for (auto const& datafile : files) {
    if (!TRI_IterateDatafile(datafile, cb)) {
      return false;
    }

    if (datafile->isPhysical() && datafile->_isSealed) {
      TRI_MMFileAdvise(datafile->_data, datafile->maximalSize(),
                       TRI_MADVISE_RANDOM);
    }
  }

  return true;
}

/// @brief closes the datafiles passed in the vector
bool MMFilesCollection::closeDatafiles(std::vector<TRI_datafile_t*> const& files) {
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
  
void MMFilesCollection::figures(std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  builder->add("documentReferences", VPackValue(_ditches.numDocumentDitches()));
  
  char const* waitingForDitch = _ditches.head();
  builder->add("waitingFor", VPackValue(waitingForDitch == nullptr ? "-" : waitingForDitch));
  
  // add datafile statistics
  DatafileStatisticsContainer dfi = _datafileStatistics.all();

  builder->add("alive", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(dfi.numberAlive));
  builder->add("size", VPackValue(dfi.sizeAlive));
  builder->close(); // alive
  
  builder->add("dead", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(dfi.numberDead));
  builder->add("size", VPackValue(dfi.sizeDead));
  builder->add("deletion", VPackValue(dfi.numberDeletions));
  builder->close(); // dead

  // add file statistics
  READ_LOCKER(readLocker, _filesLock); 
  
  size_t sizeDatafiles = 0;
  builder->add("datafiles", VPackValue(VPackValueType::Object));
  for (auto const& it : _datafiles) {
    sizeDatafiles += it->_initSize;
  }

  builder->add("count", VPackValue(_datafiles.size()));
  builder->add("fileSize", VPackValue(sizeDatafiles));
  builder->close(); // datafiles
  
  size_t sizeJournals = 0;
  for (auto const& it : _journals) {
    sizeJournals += it->_initSize;
  }
  builder->add("journals", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(_journals.size()));
  builder->add("fileSize", VPackValue(sizeJournals));
  builder->close(); // journals
  
  size_t sizeCompactors = 0;
  for (auto const& it : _compactors) {
    sizeCompactors += it->_initSize;
  }
  builder->add("compactors", VPackValue(VPackValueType::Object));
  builder->add("count", VPackValue(_compactors.size()));
  builder->add("fileSize", VPackValue(sizeCompactors));
  builder->close(); // compactors
}

/// @brief iterate over a vector of datafiles and pick those with a specific
/// data range
std::vector<MMFilesCollection::DatafileDescription> MMFilesCollection::datafilesInRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax) {
  std::vector<DatafileDescription> result;

  auto apply = [&dataMin, &dataMax, &result](TRI_datafile_t const* datafile, bool isJournal) {
    DatafileDescription entry = {datafile, datafile->_dataMin, datafile->_dataMax, datafile->_tickMax, isJournal};
    LOG(TRACE) << "checking datafile " << datafile->fid() << " with data range " << datafile->_dataMin << " - " << datafile->_dataMax << ", tick max: " << datafile->_tickMax;

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

int MMFilesCollection::applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                        std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) {
  LOG(TRACE) << "getting datafiles in data range " << dataMin << " - " << dataMax;

  std::vector<DatafileDescription> datafiles = datafilesInRange(dataMin, dataMax);
  // now we have a list of datafiles...
 
  size_t const n = datafiles.size();

  for (size_t i = 0; i < n; ++i) {
    auto const& e = datafiles[i];
    TRI_datafile_t const* datafile = e._data;

    // we are reading from a journal that might be modified in parallel
    // so we must read-lock it
    CONDITIONAL_READ_LOCKER(readLocker, _filesLock, e._isJournal); 
    
    if (!e._isJournal) {
      TRI_ASSERT(datafile->_isSealed);
    }
    
    char const* ptr = datafile->_data;
    char const* end = ptr + datafile->_currentSize;

    while (ptr < end) {
      auto const* marker = reinterpret_cast<TRI_df_marker_t const*>(ptr);

      if (marker->getSize() == 0) {
        // end of datafile
        break;
      }
      
      TRI_df_marker_type_t type = marker->getType();
        
      if (type <= TRI_DF_MARKER_MIN) {
        break;
      }

      ptr += DatafileHelper::AlignedMarkerSize<size_t>(marker);

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
        return false; // hasMore = false 
      }

      if (type != TRI_DF_MARKER_VPACK_DOCUMENT &&
          type != TRI_DF_MARKER_VPACK_REMOVE) {
        // found a non-data marker...

        // check if we can abort searching
        if (foundTick >= dataMax || (foundTick > e._tickMax && i == (n - 1))) {
          // fetched the last available marker
          return false; // hasMore = false
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
        return false; // hasMore = false
      }

      if (doAbort) {
        return true; // hasMore = true
      }
    } // next marker in datafile
  } // next datafile

  return false; // hasMore = false
}
  
/// @brief order a new master pointer
TRI_doc_mptr_t* MMFilesCollection::requestMasterpointer() {
  return _masterPointers.request();
}
        
/// @brief release an existing master pointer
void MMFilesCollection::releaseMasterpointer(TRI_doc_mptr_t* mptr) {
  TRI_ASSERT(mptr != nullptr);
  _masterPointers.release(mptr);
}
 
/// @brief report extra memory used by indexes etc.
size_t MMFilesCollection::memory() const {
  return _masterPointers.memory();
}

/// @brief disallow compaction of the collection 
void MMFilesCollection::preventCompaction() {
  _compactionLock.readLock();
}
  
/// @brief try disallowing compaction of the collection 
bool MMFilesCollection::tryPreventCompaction() {
  return _compactionLock.tryReadLock();
}

/// @brief re-allow compaction of the collection 
void MMFilesCollection::allowCompaction() {
  _compactionLock.unlock();
}
  
/// @brief exclusively lock the collection for compaction
void MMFilesCollection::lockForCompaction() {
  _compactionLock.writeLock();
}
  
/// @brief try to exclusively lock the collection for compaction
bool MMFilesCollection::tryLockForCompaction() {
  return _compactionLock.tryWriteLock();
}

/// @brief signal that compaction is finished
void MMFilesCollection::finishCompaction() {
  _compactionLock.unlock();
}

/// @brief iterate all markers of the collection
int MMFilesCollection::iterateMarkersOnLoad(arangodb::Transaction* trx) {
  // initialize state for iteration
  OpenIteratorState openState(_logicalCollection);

  if (_initialCount != -1) {
    auto primaryIndex = _logicalCollection->primaryIndex();

    int res = primaryIndex->resize(
        trx, static_cast<size_t>(_initialCount * 1.1));

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    openState._initialCount = _initialCount;
  }

  // read all documents and fill primary index
  auto cb = [&openState](TRI_df_marker_t const* marker, TRI_datafile_t* datafile) -> bool {
    return OpenIterator(marker, &openState, datafile); 
  };

  iterateDatafiles(cb);

  LOG(TRACE) << "found " << openState._documents << " document markers, " 
             << openState._deletions << " deletion markers for collection '" << _logicalCollection->name() << "'";
  
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
