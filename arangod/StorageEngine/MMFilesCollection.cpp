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
#include "Basics/WriteLocker.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/collection.h"
#include "VocBase/datafile.h"

using namespace arangodb;

namespace {

/// @brief ensures that an error code is set in all required places
static void EnsureErrorCode(int code) {
  if (code == TRI_ERROR_NO_ERROR) {
    // must have an error code
    code = TRI_ERROR_INTERNAL;
  }

  TRI_set_errno(code);
  errno = code;
}

}


MMFilesCollection::MMFilesCollection(LogicalCollection* collection) 
    : PhysicalCollection(collection) {}

MMFilesCollection::~MMFilesCollection() {
  close();

  for (auto& it : _datafiles) {
    TRI_FreeDatafile(it);
  }
  for (auto& it : _journals) {
    TRI_FreeDatafile(it);
  }
  for (auto& it : _compactors) {
    TRI_FreeDatafile(it);
  }
}
  
TRI_voc_rid_t MMFilesCollection::revision() const { 
  return 0; 
}

void MMFilesCollection::setRevision(TRI_voc_rid_t revision, bool force) {
}

int64_t MMFilesCollection::initialCount() const { 
  return 0; 
}

/// @brief closes an open collection
int MMFilesCollection::close() {
  // close compactor files
  closeDatafiles(_compactors);
  _compactors.clear();

  // close journal files
  closeDatafiles(_journals);
  _journals.clear();

  // close datafiles
  closeDatafiles(_datafiles);
  _datafiles.clear();

  return TRI_ERROR_NO_ERROR;
}

/// @brief seal a datafile
int MMFilesCollection::sealDatafile(TRI_datafile_t* datafile, bool isCompactor) {
  int res = TRI_SealDatafile(datafile);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to seal journal '" << datafile->getName(datafile)
             << "': " << TRI_errno_string(res);
    return res;
  }

  if (!isCompactor && datafile->isPhysical(datafile)) {
    // rename the file
    std::string dname("datafile-" + std::to_string(datafile->_fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(_logicalCollection->path(), dname);

    res = TRI_RenameDatafile(datafile, filename.c_str());

    if (res == TRI_ERROR_NO_ERROR) {
      LOG(TRACE) << "closed file '" << datafile->getName(datafile) << "'";
    } else {
      LOG(ERR) << "failed to rename datafile '" << datafile->getName(datafile)
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
  if (datafile->isPhysical(datafile)) {
    char const* synced = datafile->_synced;
    char* written = datafile->_written;

    if (synced < written) {
      bool ok = datafile->sync(datafile, synced, written);

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
        datafile->_state = TRI_DF_STATE_WRITE_ERROR;
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

      datafile = createDatafile(tick, targetSize, false);

      if (datafile == nullptr) {
        int res = TRI_errno();
        // could not create a datafile, this is a serious error

        if (res == TRI_ERROR_NO_ERROR) {
          // oops, error code got lost
          res = TRI_ERROR_INTERNAL;
        }

        return res;
      }

      // shouldn't throw as we reserved enough space before
      _journals.emplace_back(datafile);
    } else {
      // select datafile
      datafile = _journals[0];
    }

    TRI_ASSERT(datafile != nullptr);

    // try to reserve space in the datafile

    TRI_df_marker_t* position = nullptr;
    int res = TRI_ReserveElementDatafile(datafile, size, &position, targetSize);

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
                                        << datafile->getName(datafile) << "'";

    // make sure we have enough room in the target vector before we go on
    _datafiles.reserve(_datafiles.size() + 1);

    res = sealDatafile(datafile, false);

    // move journal into datafiles vector, regardless of whether an error
    // occurred
    TRI_ASSERT(!_journals.empty());
    _journals.erase(_journals.begin());
    TRI_ASSERT(_journals.empty());
    // this shouldn't fail, as we have reserved space before already
    _datafiles.emplace_back(datafile);

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
  try {
    WRITE_LOCKER(writeLocker, _filesLock);

    TRI_ASSERT(_compactors.empty());
    // reserve enough space for the later addition
    _compactors.reserve(_compactors.size() + 1);

    TRI_datafile_t* compactor =
        createDatafile(fid, static_cast<TRI_voc_size_t>(maximalSize), true);

    if (compactor != nullptr) {
      // should not throw, as we've reserved enough space before
      _compactors.emplace_back(compactor);
    }
    return compactor;
  } catch (...) {
    return nullptr;
  }
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
    if (_datafiles[i]->_fid == datafile->_fid) {
      // found!
      // now put the compactor in place of the datafile
      _datafiles[i] = compactor;

      // remove the compactor file from the list of compactors
      TRI_ASSERT(_compactors[0] != nullptr);
      TRI_ASSERT(_compactors[0]->_fid == compactor->_fid);

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
    _logicalCollection->_collection->_datafileStatistics.create(fid);
  } catch (...) {
    EnsureErrorCode(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  TRI_datafile_t* datafile;

  if (_logicalCollection->isVolatile()) {
    // in-memory collection
    datafile = TRI_CreateDatafile(nullptr, fid, journalSize, true);
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
      EnsureErrorCode(TRI_ERROR_ARANGO_FILESYSTEM_FULL);

      return nullptr;
    }

    // remove an existing temporary file first
    if (TRI_ExistsFile(filename.c_str())) {
      // remove an existing file first
      TRI_UnlinkFile(filename.c_str());
    }

    datafile = TRI_CreateDatafile(filename.c_str(), fid, journalSize, true);
  }

  if (datafile == nullptr) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      EnsureErrorCode(TRI_ERROR_OUT_OF_MEMORY_MMAP);
    } else {
      EnsureErrorCode(TRI_ERROR_ARANGO_NO_JOURNAL);
    }

    return nullptr;
  }

  // datafile is there now
  TRI_ASSERT(datafile != nullptr);

  if (isCompactor) {
    LOG(TRACE) << "created new compactor '" << datafile->getName(datafile)
               << "'";
  } else {
    LOG(TRACE) << "created new journal '" << datafile->getName(datafile) << "'";
  }

  // create a collection header, still in the temporary file
  TRI_df_marker_t* position;
  int res = TRI_ReserveElementDatafile(
      datafile, sizeof(TRI_col_header_marker_t), &position, journalSize);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve1") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot create collection header in file '"
             << datafile->getName(datafile) << "': " << TRI_errno_string(res);

    // close the journal and remove it
    TRI_CloseDatafile(datafile);
    TRI_UnlinkFile(datafile->getName(datafile));
    TRI_FreeDatafile(datafile);

    EnsureErrorCode(res);

    return nullptr;
  }

  TRI_col_header_marker_t cm;
  DatafileHelper::InitMarker(
      reinterpret_cast<TRI_df_marker_t*>(&cm), TRI_DF_MARKER_COL_HEADER,
      sizeof(TRI_col_header_marker_t), static_cast<TRI_voc_tick_t>(fid));
  cm._cid = _logicalCollection->cid();

  res = TRI_WriteCrcElementDatafile(datafile, position, &cm.base, false);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    int res = datafile->_lastError;
    LOG(ERR) << "cannot create collection header in file '"
             << datafile->getName(datafile) << "': " << TRI_last_error();

    // close the datafile and remove it
    TRI_CloseDatafile(datafile);
    TRI_UnlinkFile(datafile->getName(datafile));
    TRI_FreeDatafile(datafile);

    EnsureErrorCode(res);

    return nullptr;
  }

  TRI_ASSERT(fid == datafile->_fid);

  // if a physical file, we can rename it from the temporary name to the correct
  // name
  if (!isCompactor && datafile->isPhysical(datafile)) {
    // and use the correct name
    std::string jname("journal-" + std::to_string(datafile->_fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(_logicalCollection->path(), jname);

    int res = TRI_RenameDatafile(datafile, filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "failed to rename journal '" << datafile->getName(datafile)
               << "' to '" << filename << "': " << TRI_errno_string(res);

      TRI_CloseDatafile(datafile);
      TRI_UnlinkFile(datafile->getName(datafile));
      TRI_FreeDatafile(datafile);

      EnsureErrorCode(res);

      return nullptr;
    } 
      
    LOG(TRACE) << "renamed journal from '" << datafile->getName(datafile)
               << "' to '" << filename << "'";
  }

  return datafile;
}

/// @brief remove a compactor file from the list of compactors
bool MMFilesCollection::removeCompactor(TRI_datafile_t* df) {
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

    if (datafile->isPhysical(datafile) && datafile->_isSealed) {
      TRI_MMFileAdvise(datafile->_data, datafile->_maximalSize,
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
    if (datafile->_state == TRI_DF_STATE_CLOSED) {
      continue;
    }
    
    int res = TRI_CloseDatafile(datafile);
    
    if (res != TRI_ERROR_NO_ERROR) {
      result = false;
    }
  }
  
  return result;
}
  
void MMFilesCollection::figures(std::shared_ptr<arangodb::velocypack::Builder>& builder) {
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
std::vector<DatafileDescription> MMFilesCollection::datafilesInRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax) {
  std::vector<DatafileDescription> result;

  auto apply = [&dataMin, &dataMax, &result](TRI_datafile_t const* datafile, bool isJournal) {
    DatafileDescription entry = {datafile, datafile->_dataMin, datafile->_dataMax, datafile->_tickMax, isJournal};
    LOG(TRACE) << "checking datafile " << datafile->_fid << " with data range " << datafile->_dataMin << " - " << datafile->_dataMax << ", tick max: " << datafile->_tickMax;

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

