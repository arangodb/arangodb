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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "collection.h"

#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
#include "Basics/memory-map.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that an error code is set in all required places
////////////////////////////////////////////////////////////////////////////////

static void EnsureErrorCode(int code) {
  if (code == TRI_ERROR_NO_ERROR) {
    // must have an error code
    code = TRI_ERROR_INTERNAL;
  }

  TRI_set_errno(code);
  errno = code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the numeric part from a filename
/// the filename must look like this: /.*type-abc\.ending$/, where abc is
/// a number, and type and ending are arbitrary letters
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetNumericFilenamePart(char const* filename) {
  char const* pos1 = strrchr(filename, '.');

  if (pos1 == nullptr) {
    return 0;
  }

  char const* pos2 = strrchr(filename, '-');

  if (pos2 == nullptr || pos2 > pos1) {
    return 0;
  }

  return StringUtils::uint64(pos2 + 1, pos1 - pos2 - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the parameter info block
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_collection_t::updateCollectionInfo(TRI_vocbase_t* vocbase,
                                           VPackSlice const& slice, bool doSync) {
  WRITE_LOCKER(writeLocker, _infoLock);

  if (!slice.isNone()) {
    try {
      _info.update(slice, false, vocbase);
    } catch (...) {
      return TRI_ERROR_INTERNAL;
    }
  }

  return _info.saveToFile(_directory, doSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief seal a datafile
////////////////////////////////////////////////////////////////////////////////
    
int TRI_collection_t::sealDatafile(TRI_datafile_t* datafile, 
                                   bool isCompactor) { 
  int res = TRI_SealDatafile(datafile);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "failed to seal journal '" << datafile->getName(datafile)
             << "': " << TRI_last_error();
  } else if (!isCompactor && datafile->isPhysical(datafile)) {
    // rename the file
    std::string dname("datafile-" + std::to_string(datafile->_fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(_directory, dname);

    bool ok = TRI_RenameDatafile(datafile, filename.c_str());

    if (ok) {
      LOG(TRACE) << "closed file '" << datafile->getName(datafile) << "'";
    } else {
      LOG(ERR) << "failed to rename datafile '" << datafile->getName(datafile)
               << "' to '" << filename << "': " << TRI_last_error();
      res = TRI_ERROR_INTERNAL;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the active journal - will do nothing if there is no journal
////////////////////////////////////////////////////////////////////////////////

int TRI_collection_t::rotateActiveJournal() {
  WRITE_LOCKER(writeLocker, _filesLock);
  
  // note: only journals need to be handled here as the journal is the
  // only place that's ever written to. if a journal is full, it will have been
  // sealed and synced already
  if (_journals.empty()) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  TRI_datafile_t* datafile = _journals[0];
  TRI_ASSERT(datafile != nullptr);
  
  if (_state != TRI_COL_STATE_WRITE) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }
    
  // make sure we have enough room in the target vector before we go on
  _datafiles.reserve(_datafiles.size() + 1);
  
  int res = sealDatafile(datafile, false);
    
  TRI_ASSERT(!_journals.empty());
  _journals.erase(_journals.begin());
  TRI_ASSERT(_journals.empty());
  
  // shouldn't throw as we reserved enough space before
  _datafiles.emplace_back(datafile);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sync the active journal - will do nothing if there is no journal
/// or if the journal is volatile
////////////////////////////////////////////////////////////////////////////////

int TRI_collection_t::syncActiveJournal() {
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
        LOG_TOPIC(TRACE, Logger::COLLECTOR) << "msync succeeded " << (void*) synced << ", size "
                    << (written - synced);
        datafile->_synced = written;
      } else {
        res = TRI_errno();
        if (res == TRI_ERROR_NO_ERROR) {
          // oops, error code got lost
          res = TRI_ERROR_INTERNAL;
        }

        LOG_TOPIC(ERR, Logger::COLLECTOR) << "msync failed with: " << TRI_last_error();
        datafile->_state = TRI_DF_STATE_WRITE_ERROR;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve space in the current journal. if no create exists or the
/// current journal cannot provide enough space, close the old journal and 
/// create a new one
////////////////////////////////////////////////////////////////////////////////
 
int TRI_collection_t::reserveJournalSpace(TRI_voc_tick_t tick, 
                                          TRI_voc_size_t size,
                                          char*& resultPosition,
                                          TRI_datafile_t*& resultDatafile) { 
  // reset results
  resultPosition = nullptr;
  resultDatafile = nullptr;

  WRITE_LOCKER(writeLocker, _filesLock);
  
  // start with configured journal size
  TRI_voc_size_t targetSize = _info.maximalSize();

  // make sure that the document fits
  while (targetSize - 256 < size) {
    targetSize *= 2;
  }

  while (_state == TRI_COL_STATE_WRITE) {
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
      LOG_TOPIC(ERR, Logger::COLLECTOR) << "cannot select journal: '" << TRI_last_error() << "'";
      return res;
    }
  
    // TRI_ERROR_ARANGO_DATAFILE_FULL...   
    // journal is full, close it and sync
    LOG_TOPIC(DEBUG, Logger::COLLECTOR) << "closing full journal '" << datafile->getName(datafile) << "'";
 
    // make sure we have enough room in the target vector before we go on
    _datafiles.reserve(_datafiles.size() + 1);

    res = sealDatafile(datafile, false);

    // move journal into datafiles vector, regardless of whether an error occurred
    TRI_ASSERT(!_journals.empty());
    _journals.erase(_journals.begin());
    TRI_ASSERT(_journals.empty());
    // this shouldn't fail, as we have reserved space before already
    _datafiles.emplace_back(datafile);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred, we must stop here
      return res;
    }
  } // otherwise, next iteration!
    
  return TRI_ERROR_ARANGO_NO_JOURNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create compactor file
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_collection_t::createCompactor(TRI_voc_fid_t fid, 
                                                  TRI_voc_size_t maximalSize) {
  try {
    WRITE_LOCKER(writeLocker, _filesLock);
  
    TRI_ASSERT(_compactors.empty());
    // reserve enough space for the later addition
    _compactors.reserve(_compactors.size() + 1);

    TRI_datafile_t* compactor = createDatafile(fid, static_cast<TRI_voc_size_t>(maximalSize), true);

    if (compactor != nullptr) {
      // should not throw, as we've reserved enough space before
      _compactors.emplace_back(compactor);
    }
    return compactor;
  } catch (...) {
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close an existing compactor
////////////////////////////////////////////////////////////////////////////////

int TRI_collection_t::closeCompactor(TRI_datafile_t* datafile) {
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
  
////////////////////////////////////////////////////////////////////////////////
/// @brief replace a datafile with a compactor
////////////////////////////////////////////////////////////////////////////////

int TRI_collection_t::replaceDatafileWithCompactor(TRI_datafile_t* datafile, 
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

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a datafile
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_collection_t::createDatafile(TRI_voc_fid_t fid,
    TRI_voc_size_t journalSize, bool isCompactor) {
  TRI_ASSERT(fid > 0);
  TRI_document_collection_t* document = static_cast<TRI_document_collection_t*>(this);
  TRI_ASSERT(document != nullptr);

  // create an entry for the new datafile
  try {
    document->_datafileStatistics.create(fid);
  } catch (...) {
    EnsureErrorCode(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  TRI_datafile_t* datafile;

  if (document->_info.isVolatile()) {
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
    std::string filename = arangodb::basics::FileUtils::buildFilename(document->_directory, jname);

    TRI_IF_FAILURE("CreateJournalDocumentCollection") {
      // simulate disk full
      document->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);

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
      document->_lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);
    } else {
      document->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    }

    EnsureErrorCode(document->_lastError);

    return nullptr;
  }

  // datafile is there now
  TRI_ASSERT(datafile != nullptr);

  if (isCompactor) {
    LOG(TRACE) << "created new compactor '" << datafile->getName(datafile) << "'";
  } else {
    LOG(TRACE) << "created new journal '" << datafile->getName(datafile) << "'";
  }

  // create a collection header, still in the temporary file
  TRI_df_marker_t* position;
  int res = TRI_ReserveElementDatafile(datafile, sizeof(TRI_col_header_marker_t),
                                       &position, journalSize);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve1") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    document->_lastError = datafile->_lastError;
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
  DatafileHelper::InitMarker(reinterpret_cast<TRI_df_marker_t*>(&cm), TRI_DF_MARKER_COL_HEADER,
                         sizeof(TRI_col_header_marker_t), static_cast<TRI_voc_tick_t>(fid));
  cm._cid = document->_info.id();

  res = TRI_WriteCrcElementDatafile(datafile, position, &cm.base, false);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    document->_lastError = datafile->_lastError;
    LOG(ERR) << "cannot create collection header in file '"
             << datafile->getName(datafile) << "': " << TRI_last_error();

    // close the datafile and remove it
    TRI_CloseDatafile(datafile);
    TRI_UnlinkFile(datafile->getName(datafile));
    TRI_FreeDatafile(datafile);

    EnsureErrorCode(document->_lastError);

    return nullptr;
  }

  TRI_ASSERT(fid == datafile->_fid);

  // if a physical file, we can rename it from the temporary name to the correct
  // name
  if (!isCompactor && datafile->isPhysical(datafile)) {
    // and use the correct name
    std::string jname("journal-" + std::to_string(datafile->_fid) + ".db");
    std::string filename = arangodb::basics::FileUtils::buildFilename(document->_directory, jname);

    bool ok = TRI_RenameDatafile(datafile, filename.c_str());

    if (!ok) {
      LOG(ERR) << "failed to rename journal '" << datafile->getName(datafile)
                << "' to '" << filename << "': " << TRI_last_error();

      TRI_CloseDatafile(datafile);
      TRI_UnlinkFile(datafile->getName(datafile));
      TRI_FreeDatafile(datafile);

      EnsureErrorCode(document->_lastError);

      return nullptr;
    } else {
      LOG(TRACE) << "renamed journal from '" << datafile->getName(datafile)
                  << "' to '" << filename << "'";
    }
  }

  return datafile;
}
 
//////////////////////////////////////////////////////////////////////////////
/// @brief remove a compactor file from the list of compactors
//////////////////////////////////////////////////////////////////////////////

bool TRI_collection_t::removeCompactor(TRI_datafile_t* df) {
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

//////////////////////////////////////////////////////////////////////////////
/// @brief remove a datafile from the list of datafiles
//////////////////////////////////////////////////////////////////////////////

bool TRI_collection_t::removeDatafile(TRI_datafile_t* df) {
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

void TRI_collection_t::addIndexFile(std::string const& filename) {
  WRITE_LOCKER(readLocker, _filesLock);
  _indexFiles.emplace_back(filename);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file from the _indexFiles vector
////////////////////////////////////////////////////////////////////////////////

int TRI_collection_t::removeIndexFile(TRI_idx_iid_t id) {
  READ_LOCKER(readLocker, _filesLock);

  for (auto it = _indexFiles.begin(); it != _indexFiles.end(); ++it) {
    if (GetNumericFilenamePart((*it).c_str()) == id) {
      // found
      _indexFiles.erase(it);
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over all index files of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_collection_t::iterateIndexes(std::function<bool(std::string const&, void*)> const& callback, void* data) {
  // iterate over all index files
  for (auto const& filename : _indexFiles) {
    bool ok = callback(filename, data);

    if (!ok) {
      LOG(ERR) << "cannot load index '" << filename << "' for collection '"
               << _info.name() << "'";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two filenames, based on the numeric part contained in
/// the filename. this is used to sort datafile filenames on startup
////////////////////////////////////////////////////////////////////////////////

static bool FilenameComparator(std::string const& lhs, std::string const& rhs) {
  char const* l = lhs.c_str();
  char const* r = rhs.c_str();

  uint64_t const numLeft = GetNumericFilenamePart(l);
  uint64_t const numRight = GetNumericFilenamePart(r);

  if (numLeft != numRight) {
    return numLeft < numRight;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two datafiles, based on the numeric part contained in
/// the filename
////////////////////////////////////////////////////////////////////////////////

static bool DatafileComparator(TRI_datafile_t const* lhs, TRI_datafile_t const* rhs) {
  uint64_t const numLeft =
      (lhs->_filename != nullptr ? GetNumericFilenamePart(lhs->_filename) : 0);
  uint64_t const numRight =
      (rhs->_filename != nullptr ? GetNumericFilenamePart(rhs->_filename) : 0);

  if (numLeft != numRight) {
    return numLeft < numRight;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new collection
////////////////////////////////////////////////////////////////////////////////

static void InitCollection(TRI_vocbase_t* vocbase, TRI_collection_t* collection,
                           std::string const& directory,
                           VocbaseCollectionInfo const& info) {
  TRI_ASSERT(collection != nullptr);

  collection->_info.update(info);

  collection->_vocbase = vocbase;
  collection->_tickMax = 0;
  collection->_state = TRI_COL_STATE_WRITE;
  collection->_lastError = 0;
  collection->_directory = directory;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans a collection and locates all files
////////////////////////////////////////////////////////////////////////////////

static TRI_col_file_structure_t ScanCollectionDirectory(char const* path) {
  LOG_TOPIC(TRACE, Logger::DATAFILES) << "scanning collection directory '"
                                      << path << "'";

  TRI_col_file_structure_t structure;

  // check files within the directory
  std::vector<std::string> files = TRI_FilesDirectory(path);

  for (auto const& file : files) {
    std::vector<std::string> parts = StringUtils::split(file, '.');

    if (parts.size() < 2 || parts.size() > 3 || parts[0].empty()) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string filename = FileUtils::buildFilename(path, file);
    std::string extension = parts[1];
    std::string isDead = (parts.size() > 2) ? parts[2] : "";

    std::vector<std::string> next = StringUtils::split(parts[0], "-");

    if (next.size() < 2) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string filetype = next[0];
    next.erase(next.begin());
    std::string qualifier = StringUtils::join(next, '-');

    // .............................................................................
    // file is dead
    // .............................................................................

    if (!isDead.empty()) {
      if (isDead == "dead") {
        FileUtils::remove(filename);
      } else {
        LOG_TOPIC(DEBUG, Logger::DATAFILES)
            << "ignoring file '" << file
            << "' because it does not look like a datafile";
      }

      continue;
    }

    // .............................................................................
    // file is an index
    // .............................................................................

    if (filetype == "index" && extension == "json") {
      structure.indexes.emplace_back(filename);
      continue;
    }

    // .............................................................................
    // file is a journal or datafile
    // .............................................................................

    if (extension == "db") {
      // file is a journal
      if (filetype == "journal") {
        structure.journals.emplace_back(filename);
      }

      // file is a datafile
      else if (filetype == "datafile") {
        structure.datafiles.emplace_back(filename);
      }

      // file is a left-over compaction file. rename it back
      else if (filetype == "compaction") {
        std::string relName = "datafile-" + qualifier + "." + extension;
        std::string newName = FileUtils::buildFilename(path, relName);

        if (FileUtils::exists(newName)) {
          // we have a compaction-xxxx and a datafile-xxxx file. we'll keep
          // the datafile

          FileUtils::remove(filename);

          LOG_TOPIC(WARN, Logger::DATAFILES)
              << "removing left-over compaction file '" << filename << "'";

          continue;
        } else {
          // this should fail, but shouldn't do any harm either...
          FileUtils::remove(newName);

          // rename the compactor to a datafile
          int res = TRI_RenameFile(filename.c_str(), newName.c_str());

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_TOPIC(ERR, Logger::DATAFILES)
                << "unable to rename compaction file '" << filename << "'";
            continue;
          }
        }

        structure.datafiles.emplace_back(filename);
      }

      // temporary file, we can delete it!
      else if (filetype == "temp") {
        LOG_TOPIC(WARN, Logger::DATAFILES)
            << "found temporary file '" << filename
            << "', which is probably a left-over. deleting it";
        FileUtils::remove(filename);
      }

      // ups, what kind of file is that
      else {
        LOG_TOPIC(ERR, Logger::DATAFILES) << "unknown datafile type '"
                                          << file << "'";
      }
    }
  }

  // now sort the files in the structures that we created.
  // the sorting allows us to iterate the files in the correct order
  std::sort(structure.journals.begin(), structure.journals.end(), FilenameComparator);
  std::sort(structure.compactors.begin(), structure.compactors.end(), FilenameComparator);
  std::sort(structure.datafiles.begin(), structure.datafiles.end(), FilenameComparator);
  std::sort(structure.indexes.begin(), structure.indexes.end(), FilenameComparator);

  return structure;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a collection
////////////////////////////////////////////////////////////////////////////////

static bool CheckCollection(TRI_collection_t* collection, bool ignoreErrors) {
  LOG_TOPIC(TRACE, Logger::DATAFILES) << "check collection directory '"
                                      << collection->_directory << "'";

  std::vector<TRI_datafile_t*> all;
  std::vector<TRI_datafile_t*> compactors;
  std::vector<TRI_datafile_t*> datafiles;
  std::vector<TRI_datafile_t*> journals;
  std::vector<TRI_datafile_t*> sealed;
  bool stop = false;

  // check files within the directory
  std::vector<std::string> files = TRI_FilesDirectory(collection->_directory.c_str());

  for (auto const& file : files) {
    std::vector<std::string> parts = StringUtils::split(file, '.');

    if (parts.size() < 2 || parts.size() > 3 || parts[0].empty()) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string extension = parts[1];
    std::string isDead = (parts.size() > 2) ? parts[2] : "";

    std::vector<std::string> next = StringUtils::split(parts[0], "-");

    if (next.size() < 2) {
      LOG_TOPIC(DEBUG, Logger::DATAFILES)
          << "ignoring file '" << file
          << "' because it does not look like a datafile";
      continue;
    }

    std::string filename =
        FileUtils::buildFilename(collection->_directory, file);
    std::string filetype = next[0];
    next.erase(next.begin());
    std::string qualifier = StringUtils::join(next, '-');

    // .............................................................................
    // file is dead
    // .............................................................................

    if (!isDead.empty() || filetype == "temp") {
      if (isDead == "dead" || filetype == "temp") {
        LOG_TOPIC(TRACE, Logger::DATAFILES)
            << "found temporary file '" << filename
            << "', which is probably a left-over. deleting it";
        FileUtils::remove(filename);
        continue;
      } else {
        LOG_TOPIC(DEBUG, Logger::DATAFILES)
            << "ignoring file '" << file
            << "' because it does not look like a datafile";
        continue;
      }
    }

    // .............................................................................
    // file is an index, just store the filename
    // .............................................................................

    if (filetype == "index" && extension == "json") {
      collection->addIndexFile(filename);
      continue;
    }

    // .............................................................................
    // file is a journal or datafile, open the datafile
    // .............................................................................

    if (extension == "db") {
      // found a compaction file. now rename it back
      if (filetype == "compaction") {
        std::string relName = "datafile-" + qualifier + "." + extension;
        std::string newName =
            FileUtils::buildFilename(collection->_directory, relName);

        if (FileUtils::exists(newName)) {
          // we have a compaction-xxxx and a datafile-xxxx file. we'll keep
          // the datafile
          FileUtils::remove(filename);

          LOG_TOPIC(WARN, Logger::DATAFILES)
              << "removing unfinished compaction file '" << filename << "'";
          continue;
        } else {
          // this should fail, but shouldn't do any harm either...
          FileUtils::remove(newName);

          int res = TRI_RenameFile(filename.c_str(), newName.c_str());

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_TOPIC(ERR, Logger::DATAFILES)
                << "unable to rename compaction file '" << filename << "' to '"
                << newName << "'";
            stop = true;
            break;
          }
        }

        // reuse newName
        filename = newName;
      }

      TRI_datafile_t* datafile = TRI_OpenDatafile(filename.c_str(), ignoreErrors);

      if (datafile == nullptr) {
        collection->_lastError = TRI_errno();
        LOG_TOPIC(ERR, Logger::DATAFILES) << "cannot open datafile '"
                                          << filename
                                          << "': " << TRI_last_error();

        stop = true;
        break;
      }

      all.emplace_back(datafile);

      // check the document header
      char const* ptr = datafile->_data;

      // skip the datafile header
      ptr += DatafileHelper::AlignedSize<size_t>(sizeof(TRI_df_header_marker_t));
      TRI_col_header_marker_t const* cm = reinterpret_cast<TRI_col_header_marker_t const*>(ptr);

      if (cm->base.getType() != TRI_DF_MARKER_COL_HEADER) {
        LOG(ERR) << "collection header mismatch in file '" << filename
                 << "', expected TRI_DF_MARKER_COL_HEADER, found "
                 << cm->base.getType();

        stop = true;
        break;
      }

      if (cm->_cid != collection->_info.id()) {
        LOG(ERR) << "collection identifier mismatch, expected "
                 << collection->_info.id() << ", found " << cm->_cid;

        stop = true;
        break;
      }

      // file is a journal
      if (filetype == "journal") {
        if (datafile->_isSealed) {
          if (datafile->_state != TRI_DF_STATE_READ) {
            LOG_TOPIC(WARN, Logger::DATAFILES)
                << "strange, journal '" << filename
                << "' is already sealed; must be a left over; will use "
                   "it as datafile";
          }

          sealed.emplace_back(datafile);
        } else {
          journals.emplace_back(datafile);
        }
      }

      // file is a compactor
      else if (filetype == "compactor") {
        // ignore
      }

      // file is a datafile (or was a compaction file)
      else if (filetype == "datafile" || filetype == "compaction") {
        if (!datafile->_isSealed) {
          LOG_TOPIC(ERR, Logger::DATAFILES)
              << "datafile '" << filename
              << "' is not sealed, this should never happen";

          collection->_lastError =
              TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE);

          stop = true;
          break;
        } else {
          datafiles.emplace_back(datafile);
        }
      }

      else {
        LOG_TOPIC(ERR, Logger::DATAFILES) << "unknown datafile '" << file
                                          << "'";
      }
    } else {
      LOG_TOPIC(ERR, Logger::DATAFILES) << "unknown datafile '" << file << "'";
    }
  }

  // convert the sealed journals into datafiles
  if (!stop) {
    for (auto& datafile : sealed) {
      std::string dname("datafile-" + std::to_string(datafile->_fid) + ".db");
      std::string filename = arangodb::basics::FileUtils::buildFilename(collection->_directory, dname);

      bool ok = TRI_RenameDatafile(datafile, filename.c_str());

      if (ok) {
        datafiles.emplace_back(datafile);
        LOG(DEBUG) << "renamed sealed journal to '" << filename << "'";
      } else {
        collection->_lastError = datafile->_lastError;
        stop = true;
        LOG(ERR) << "cannot rename sealed log-file to " << filename
                 << ", this should not happen: " << TRI_last_error();
        break;
      }
    }
  }

  // stop if necessary
  if (stop) {
    for (auto& datafile : all) {
      LOG(TRACE) << "closing datafile '" << datafile->_filename << "'";

      TRI_CloseDatafile(datafile);
      TRI_FreeDatafile(datafile);
    }

    return false;
  }

  // sort the datafiles.
  // this allows us to iterate them in the correct order
  std::sort(datafiles.begin(), datafiles.end(), DatafileComparator);
  std::sort(journals.begin(), journals.end(), DatafileComparator);
  std::sort(compactors.begin(), compactors.end(), DatafileComparator);


  // add the datafiles and journals
  collection->_datafiles = datafiles;
  collection->_journals = journals;
  collection->_compactors = compactors;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all datafiles in a vector
////////////////////////////////////////////////////////////////////////////////

static bool IterateDatafilesVector(std::vector<TRI_datafile_t*> const& files,
                                   bool (*iterator)(TRI_df_marker_t const*,
                                                    void*, TRI_datafile_t*),
                                   void* data) {
  TRI_ASSERT(iterator != nullptr);

  for (auto const& datafile : files) {
    if (!TRI_IterateDatafile(datafile, iterator, data)) {
      return false;
    }

    if (datafile->isPhysical(datafile) && datafile->_isSealed) {
      TRI_MMFileAdvise(datafile->_data, datafile->_maximalSize,
                       TRI_MADVISE_RANDOM);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the datafiles passed in the vector
////////////////////////////////////////////////////////////////////////////////

static bool CloseDataFiles(std::vector<TRI_datafile_t*> const& files) {
  bool result = true;

  for (auto const& datafile : files) {
    TRI_ASSERT(datafile != nullptr);
    result &= TRI_CloseDatafile(datafile);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over a set of datafiles, identified by filenames
/// note: the files will be opened and closed
////////////////////////////////////////////////////////////////////////////////

static bool IterateFiles(std::vector<std::string> const& files,
                         bool (*iterator)(TRI_df_marker_t const*, void*,
                                          TRI_datafile_t*),
                         void* data) {
  TRI_ASSERT(iterator != nullptr);

  for (auto const& filename : files) {
    LOG(DEBUG) << "iterating over collection journal file '" << filename << "'";

    TRI_datafile_t* datafile = TRI_OpenDatafile(filename.c_str(), true);

    if (datafile != nullptr) {
      TRI_IterateDatafile(datafile, iterator, data);
      TRI_CloseDatafile(datafile);
      TRI_FreeDatafile(datafile);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the full directory name for a collection
////////////////////////////////////////////////////////////////////////////////

static std::string GetCollectionDirectory(char const* path, char const* name,
                                          TRI_voc_cid_t cid) {
  TRI_ASSERT(path != nullptr);
  TRI_ASSERT(name != nullptr);

  std::string filename("collection-");
  filename.append(std::to_string(cid));
  filename.push_back('-');
  filename.append(std::to_string(RandomGenerator::interval(UINT32_MAX)));

  return arangodb::basics::FileUtils::buildFilename(path, filename);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_CreateCollection(
    TRI_vocbase_t* vocbase, TRI_collection_t* collection, char const* path,
    arangodb::VocbaseCollectionInfo const& parameters) {
  // sanity check
  if (sizeof(TRI_df_header_marker_t) + sizeof(TRI_df_footer_marker_t) >
      parameters.maximalSize()) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATAFILE_FULL);

    LOG(ERR) << "cannot create datafile '" << parameters.name() << "' in '"
             << path << "', maximal size '"
             << (unsigned int)parameters.maximalSize() << "' is too small";

    return nullptr;
  }

  if (!TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_INVALID);

    LOG(ERR) << "cannot create collection '" << path
             << "', path is not a directory";

    return nullptr;
  }

  std::string const dirname =
      GetCollectionDirectory(path, parameters.namec_str(), parameters.id());

  // directory must not exist
  if (TRI_ExistsFile(dirname.c_str())) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS);

    LOG(ERR) << "cannot create collection '" << parameters.name()
             << "' in directory '" << dirname
             << "': directory already exists";

    return nullptr;
  }

  // use a temporary directory first. this saves us from leaving an empty
  // directory
  // behind, an the server refusing to start
  std::string const tmpname = dirname + ".tmp";

  // create directory
  std::string errorMessage;
  long systemError;
  int res = TRI_CreateDirectory(tmpname.c_str(), systemError, errorMessage);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot create collection '" << parameters.name()
             << "' in directory '" << path << "': " << TRI_errno_string(res)
             << " - " << systemError << " - " << errorMessage;

    return nullptr;
  }

  TRI_IF_FAILURE("CreateCollection::tempDirectory") { return nullptr; }

  // create a temporary file
  std::string const tmpfile(
      arangodb::basics::FileUtils::buildFilename(tmpname.c_str(), ".tmp"));
  res = TRI_WriteFile(tmpfile.c_str(), "", 0);

  TRI_IF_FAILURE("CreateCollection::tempFile") { return nullptr; }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot create collection '" << parameters.name()
             << "' in directory '" << path << "': " << TRI_errno_string(res)
             << " - " << systemError << " - " << errorMessage;
    TRI_RemoveDirectory(tmpname.c_str());

    return nullptr;
  }

  TRI_IF_FAILURE("CreateCollection::renameDirectory") { return nullptr; }

  res = TRI_RenameFile(tmpname.c_str(), dirname.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "cannot create collection '" << parameters.name()
             << "' in directory '" << path << "': " << TRI_errno_string(res)
             << " - " << systemError << " - " << errorMessage;
    TRI_RemoveDirectory(tmpname.c_str());

    return nullptr;
  }

  // now we have the collection directory in place with the correct name and a
  // .tmp file in it

  // create collection structure
  if (collection == nullptr) {
    try {
      TRI_collection_t* tmp = new TRI_collection_t(vocbase, parameters);
      collection = tmp;
    } catch (std::exception&) {
      collection = nullptr;
    }

    if (collection == nullptr) {
      LOG(ERR) << "cannot create collection '" << path << "': out of memory";

      return nullptr;
    }
  }

  InitCollection(vocbase, collection, dirname, parameters);

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCollection(TRI_collection_t* collection) {
  TRI_ASSERT(collection);
  collection->_info.clearKeyOptions();

  for (auto& it : collection->_datafiles) {
    TRI_FreeDatafile(it);
  }
  for (auto& it : collection->_journals) {
    TRI_FreeDatafile(it);
  }
  for (auto& it : collection->_compactors) {
    TRI_FreeDatafile(it);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollection(TRI_collection_t* collection) {
  TRI_ASSERT(collection);
  TRI_DestroyCollection(collection);
  delete collection;
}

VocbaseCollectionInfo::VocbaseCollectionInfo(CollectionInfo const& other)
    : _version(TRI_COL_VERSION),
      _type(other.type()),
      _revision(0),      // not known in the cluster case on the coordinator
      _cid(other.id()),  // this is on the coordinator and describes a
                         // cluster-wide collection, for safety reasons,
                         // we also set _cid
      _planId(other.id()),
      _maximalSize(other.journalSize()),
      _initialCount(-1),
      _indexBuckets(other.indexBuckets()),
      _keyOptions(nullptr),
      _isSystem(other.isSystem()),
      _deleted(other.deleted()),
      _doCompact(other.doCompact()),
      _isVolatile(other.isVolatile()),
      _waitForSync(other.waitForSync()) {
  std::string const name = other.name();
  memset(_name, 0, sizeof(_name));
  memcpy(_name, name.c_str(), name.size());
 
  VPackSlice keyOptionsSlice(other.keyOptions());

  if (!keyOptionsSlice.isNone()) {
    VPackBuilder builder;
    builder.add(keyOptionsSlice);
    _keyOptions = builder.steal();
  }
}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             char const* name,
                                             TRI_col_type_e type,
                                             TRI_voc_size_t maximalSize,
                                             VPackSlice const& keyOptions)
    : _version(TRI_COL_VERSION),
      _type(type),
      _revision(0),
      _cid(0),
      _planId(0),
      _maximalSize(vocbase->_settings.defaultMaximalSize),
      _initialCount(-1),
      _indexBuckets(TRI_DEFAULT_INDEX_BUCKETS),
      _keyOptions(nullptr),
      _isSystem(false),
      _deleted(false),
      _doCompact(true),
      _isVolatile(false),
      _waitForSync(vocbase->_settings.defaultWaitForSync) {
  _maximalSize =
      static_cast<TRI_voc_size_t>((maximalSize / PageSize) * PageSize);
  if (_maximalSize == 0 && maximalSize != 0) {
    _maximalSize = static_cast<TRI_voc_size_t>(PageSize);
  }
  memset(_name, 0, sizeof(_name));
  TRI_CopyString(_name, name, sizeof(_name) - 1);

  if (!keyOptions.isNone()) {
    VPackBuilder builder;
    builder.add(keyOptions);
    _keyOptions = builder.steal();
  }
}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             char const* name,
                                             VPackSlice const& options)
    : VocbaseCollectionInfo(vocbase, name, TRI_COL_TYPE_DOCUMENT, options) {}

VocbaseCollectionInfo::VocbaseCollectionInfo(TRI_vocbase_t* vocbase,
                                             char const* name,
                                             TRI_col_type_e type,
                                             VPackSlice const& options)
    : _version(TRI_COL_VERSION),
      _type(type),
      _revision(0),
      _cid(0),
      _planId(0),
      _maximalSize(vocbase->_settings.defaultMaximalSize),
      _initialCount(-1),
      _indexBuckets(TRI_DEFAULT_INDEX_BUCKETS),
      _keyOptions(nullptr),
      _isSystem(false),
      _deleted(false),
      _doCompact(true),
      _isVolatile(false),
      _waitForSync(vocbase->_settings.defaultWaitForSync) {
  memset(_name, 0, sizeof(_name));

  if (name != nullptr && *name != '\0') {
    TRI_CopyString(_name, name, sizeof(_name) - 1);
  }

  if (options.isObject()) {
    TRI_voc_size_t maximalSize;
    if (options.hasKey("journalSize")) {
      maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              options, "journalSize", vocbase->_settings.defaultMaximalSize);
    } else {
      maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              options, "maximalSize", vocbase->_settings.defaultMaximalSize);
    }

    _maximalSize =
        static_cast<TRI_voc_size_t>((maximalSize / PageSize) * PageSize);
    if (_maximalSize == 0 && maximalSize != 0) {
      _maximalSize = static_cast<TRI_voc_size_t>(PageSize);
    }
   
    if (options.hasKey("count")) { 
      _initialCount =
          arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
              options, "count", -1);
    }

    _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "doCompact", true);
    _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "waitForSync", vocbase->_settings.defaultWaitForSync);
    _isVolatile = arangodb::basics::VelocyPackHelper::getBooleanValue(
        options, "isVolatile", false);
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            options, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS);
    _type = static_cast<TRI_col_type_e>(
        arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
            options, "type", _type));

    std::string cname =
        arangodb::basics::VelocyPackHelper::getStringValue(options, "name", "");
    if (!cname.empty()) {
      TRI_CopyString(_name, cname.c_str(), sizeof(_name) - 1);
    }

    std::string cidString =
        arangodb::basics::VelocyPackHelper::getStringValue(options, "cid", "");
    if (!cidString.empty()) {
      // note: this may throw
      _cid = std::stoull(cidString);
    }

    if (options.hasKey("isSystem")) {
      VPackSlice isSystemSlice = options.get("isSystem");
      if (isSystemSlice.isBoolean()) {
        _isSystem = isSystemSlice.getBoolean();
      }
    } else {
      _isSystem = false;
    }

    if (options.hasKey("journalSize")) {
      VPackSlice maxSizeSlice = options.get("journalSize");
      TRI_voc_size_t maximalSize =
          maxSizeSlice.getNumericValue<TRI_voc_size_t>();
      if (maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "journalSize is too small");
      }
    }

    VPackSlice const planIdSlice = options.get("planId");
    TRI_voc_cid_t planId = 0;
    if (planIdSlice.isNumber()) {
      planId = planIdSlice.getNumericValue<TRI_voc_cid_t>();
    } else if (planIdSlice.isString()) {
      std::string tmp = planIdSlice.copyString();
      planId = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
    }

    if (planId > 0) {
      _planId = planId;
    }

    VPackSlice const cidSlice = options.get("id");
    if (cidSlice.isNumber()) {
      _cid = cidSlice.getNumericValue<TRI_voc_cid_t>();
    } else if (cidSlice.isString()) {
      std::string tmp = cidSlice.copyString();
      _cid = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
    }

    if (options.hasKey("keyOptions")) {
      VPackSlice const slice = options.get("keyOptions");
      VPackBuilder builder;
      builder.add(slice);
      // Copy the ownership of the options over
      _keyOptions = builder.steal();
    }

    if (options.hasKey("deleted")) {
      VPackSlice const slice = options.get("deleted");
      if (slice.isBoolean()) {
        _deleted = slice.getBoolean();
      }
    }
  }

#ifndef TRI_HAVE_ANONYMOUS_MMAP
  if (_isVolatile) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections are not supported on this platform");
  }
#endif

  if (_isVolatile && _waitForSync) {
    // the combination of waitForSync and isVolatile makes no sense
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "volatile collections do not support the waitForSync option");
  }

  if (_indexBuckets < 1 || _indexBuckets > 1024) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "indexBuckets must be a two-power between 1 and 1024");
  }

  if (!TRI_IsAllowedNameCollection(_isSystem, _name)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // fix _isSystem value if mis-specified by user
  _isSystem = (*_name == '_');
}

VocbaseCollectionInfo VocbaseCollectionInfo::fromFile(
    char const* path, TRI_vocbase_t* vocbase, char const* collectionName,
    bool versionWarning) {
  // find parameter file
  std::string const filename = arangodb::basics::FileUtils::buildFilename(path, TRI_VOC_PARAMETER_FILE);

  if (!TRI_ExistsFile(filename.c_str())) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  std::shared_ptr<VPackBuilder> content =
      arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
  VPackSlice slice = content->slice();
  if (!slice.isObject()) {
    LOG(ERR) << "cannot open '" << filename
             << "', collection parameters are not readable";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE);
  }

  // fiddle "isSystem" value, which is not contained in the JSON file
  bool isSystemValue = false;
  if (slice.hasKey("name")) {
    auto name = slice.get("name").copyString();
    if (!name.empty()) {
      isSystemValue = name[0] == '_';
    }
  }

  VPackBuilder bx;
  bx.openObject();
  bx.add("isSystem", VPackValue(isSystemValue));
  bx.close();
  VPackSlice isSystem = bx.slice();
  VPackBuilder b2 = VPackCollection::merge(slice, isSystem, false);
  slice = b2.slice();

  VocbaseCollectionInfo info(vocbase, collectionName, slice);

  // warn about wrong version of the collection
  if (versionWarning && info.version() < TRI_COL_VERSION_20) {
    if (info.name()[0] != '\0') {
      // only warn if the collection version is older than expected, and if it's
      // not a shape collection
      LOG(WARN) << "collection '" << info.name()
                << "' has an old version and needs to be upgraded.";
    }
  }
  return info;
}

// collection version
TRI_col_version_t VocbaseCollectionInfo::version() const { return _version; }

// collection type
TRI_col_type_e VocbaseCollectionInfo::type() const { return _type; }

// local collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::id() const { return _cid; }

// cluster-wide collection identifier
TRI_voc_cid_t VocbaseCollectionInfo::planId() const { return _planId; }

// last revision id written
TRI_voc_rid_t VocbaseCollectionInfo::revision() const { return _revision; }

// maximal size of memory mapped file
TRI_voc_size_t VocbaseCollectionInfo::maximalSize() const {
  return _maximalSize;
}

// initial count, used when loading a collection
int64_t VocbaseCollectionInfo::initialCount() const { return _initialCount; }

// number of buckets used in hash tables for indexes
uint32_t VocbaseCollectionInfo::indexBuckets() const { return _indexBuckets; }

// name of the collection
std::string VocbaseCollectionInfo::name() const { return std::string(_name); }

// name of the collection as c string
char const* VocbaseCollectionInfo::namec_str() const { return _name; }

// options for key creation
std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
VocbaseCollectionInfo::keyOptions() const {
  return _keyOptions;
}

// If true, collection has been deleted
bool VocbaseCollectionInfo::deleted() const { return _deleted; }

// If true, collection will be compacted
bool VocbaseCollectionInfo::doCompact() const { return _doCompact; }

// If true, collection is a system collection
bool VocbaseCollectionInfo::isSystem() const { return _isSystem; }

// If true, collection is memory-only
bool VocbaseCollectionInfo::isVolatile() const { return _isVolatile; }

// If true waits for mysnc
bool VocbaseCollectionInfo::waitForSync() const { return _waitForSync; }

void VocbaseCollectionInfo::setVersion(TRI_col_version_t version) {
  _version = version;
}

void VocbaseCollectionInfo::rename(char const* name) {
  TRI_CopyString(_name, name, sizeof(_name) - 1);
}

void VocbaseCollectionInfo::setRevision(TRI_voc_rid_t rid, bool force) {
  if (force || rid > _revision) {
    _revision = rid;
  }
}

void VocbaseCollectionInfo::setCollectionId(TRI_voc_cid_t cid) { _cid = cid; }

void VocbaseCollectionInfo::updateCount(size_t size) { _initialCount = size; }

void VocbaseCollectionInfo::setPlanId(TRI_voc_cid_t planId) {
  _planId = planId;
}

void VocbaseCollectionInfo::setDeleted(bool deleted) { _deleted = deleted; }

void VocbaseCollectionInfo::clearKeyOptions() { _keyOptions.reset(); }

int VocbaseCollectionInfo::saveToFile(std::string const& path, bool forceSync) const {
  std::string filename = basics::FileUtils::buildFilename(path, TRI_VOC_PARAMETER_FILE);

  VPackBuilder builder;
  builder.openObject();
  TRI_CreateVelocyPackCollectionInfo(*this, builder);
  builder.close();

  bool ok = VelocyPackHelper::velocyPackToFile(filename.c_str(), builder.slice(), forceSync);

  if (!ok) {
    int res = TRI_errno();
    LOG(ERR) << "cannot save collection properties file '" << filename
             << "': " << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

void VocbaseCollectionInfo::update(VPackSlice const& slice, bool preferDefaults,
                                   TRI_vocbase_t const* vocbase) {
  // the following collection properties are intentionally not updated as
  // updating
  // them would be very complicated:
  // - _cid
  // - _name
  // - _type
  // - _isSystem
  // - _isVolatile
  // ... probably a few others missing here ...

  if (preferDefaults) {
    if (vocbase != nullptr) {
      _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "doCompact", true);
      _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync", vocbase->_settings.defaultWaitForSync);
      if (slice.hasKey("journalSize")) {
        _maximalSize = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            slice, "journalSize", vocbase->_settings.defaultMaximalSize);
      } else {
        _maximalSize = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            slice, "maximalSize", vocbase->_settings.defaultMaximalSize);
      }
      _indexBuckets =
          arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
              slice, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS);
    } else {
      _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "doCompact", true);
      _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "waitForSync", false);
      if (slice.hasKey("journalSize")) {
        _maximalSize =
            arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
                slice, "journalSize", TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE);
      } else {
        _maximalSize =
            arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
                slice, "maximalSize", TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE);
      }
      _indexBuckets =
          arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
              slice, "indexBuckets", TRI_DEFAULT_INDEX_BUCKETS);
    }
  } else {
    _doCompact = arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "doCompact", _doCompact);
    _waitForSync = arangodb::basics::VelocyPackHelper::getBooleanValue(
        slice, "waitForSync", _waitForSync);
    if (slice.hasKey("journalSize")) {
      _maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              slice, "journalSize", _maximalSize);
    } else {
      _maximalSize =
          arangodb::basics::VelocyPackHelper::getNumericValue<TRI_voc_size_t>(
              slice, "maximalSize", _maximalSize);
    }
    _indexBuckets =
        arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
            slice, "indexBuckets", _indexBuckets);
    
    _initialCount =
        arangodb::basics::VelocyPackHelper::getNumericValue<int64_t>(
            slice, "count", _initialCount);
  }
}

void VocbaseCollectionInfo::update(VocbaseCollectionInfo const& other) {
  _version = other.version();
  _type = other.type();
  _cid = other.id();
  _planId = other.planId();
  _revision = other.revision();
  _maximalSize = other.maximalSize();
  _initialCount = other.initialCount();
  _indexBuckets = other.indexBuckets();

  TRI_CopyString(_name, other.namec_str(), sizeof(_name) - 1);

  _keyOptions = other.keyOptions();

  _deleted = other.deleted();
  _doCompact = other.doCompact();
  _isSystem = other.isSystem();
  _isVolatile = other.isVolatile();
  _waitForSync = other.waitForSync();
}

std::shared_ptr<VPackBuilder> TRI_CreateVelocyPackCollectionInfo(
    arangodb::VocbaseCollectionInfo const& info) {
  // This function might throw
  auto builder = std::make_shared<VPackBuilder>();
  builder->openObject();
  TRI_CreateVelocyPackCollectionInfo(info, *builder);
  builder->close();
  return builder;
}

void TRI_CreateVelocyPackCollectionInfo(
    arangodb::VocbaseCollectionInfo const& info, VPackBuilder& builder) {
  // This function might throw
  //
  TRI_ASSERT(!builder.isClosed());

  std::string cidString = std::to_string(info.id());
  std::string planIdString = std::to_string(info.planId());

  builder.add("version", VPackValue(info.version()));
  builder.add("type", VPackValue(info.type()));
  builder.add("cid", VPackValue(cidString));

  if (info.planId() > 0) {
    builder.add("planId", VPackValue(planIdString));
  }

  if (info.initialCount() >= 0) {
    builder.add("count", VPackValue(info.initialCount()));
  }
  builder.add("indexBuckets", VPackValue(info.indexBuckets()));
  builder.add("deleted", VPackValue(info.deleted()));
  builder.add("doCompact", VPackValue(info.doCompact()));
  builder.add("maximalSize", VPackValue(info.maximalSize()));
  builder.add("name", VPackValue(info.name()));
  builder.add("isVolatile", VPackValue(info.isVolatile()));
  builder.add("waitForSync", VPackValue(info.waitForSync()));

  auto opts = info.keyOptions();
  if (opts.get() != nullptr) {
    VPackSlice const slice(opts->data());
    builder.add("keyOptions", slice);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// You must hold the @ref TRI_WRITE_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollection(TRI_collection_t* collection, char const* name) {
  // Save name for rollback
  std::string oldName = collection->_info.name();
  collection->_info.rename(name);

  int res = collection->_info.saveToFile(collection->_directory, true);
  if (res != TRI_ERROR_NO_ERROR) {
    // Rollback
    collection->_info.rename(oldName.c_str());
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateCollection(TRI_collection_t* collection,
                           bool (*iterator)(TRI_df_marker_t const*, void*,
                                            TRI_datafile_t*),
                           void* data) {
  TRI_ASSERT(iterator != nullptr);

  bool result;
  if (!IterateDatafilesVector(collection->_datafiles, iterator, data) ||
      !IterateDatafilesVector(collection->_compactors, iterator, data) ||
      !IterateDatafilesVector(collection->_journals, iterator, data)) {
    result = false;
  } else {
    result = true;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_OpenCollection(TRI_vocbase_t* vocbase,
                                     TRI_collection_t* collection,
                                     char const* path, bool ignoreErrors) {
  TRI_ASSERT(collection != nullptr);

  if (!TRI_IsDirectory(path)) {
    TRI_set_errno(TRI_ERROR_ARANGO_DATADIR_INVALID);

    LOG(ERR) << "cannot open '" << path << "', not a directory or not found";

    return nullptr;
  }

  try {
    // read parameters, no need to lock as we are opening the collection
    VocbaseCollectionInfo info =
        VocbaseCollectionInfo::fromFile(path, vocbase,
                                        "",  // Name will be set later on
                                        true);
    InitCollection(vocbase, collection, std::string(path), info);

    double start = TRI_microtime();

    LOG_TOPIC(TRACE, Logger::PERFORMANCE)
        << "open-collection { collection: " << vocbase->_name << "/"
        << collection->_info.name();

    // check for journals and datafiles
    bool ok = CheckCollection(collection, ignoreErrors);

    if (!ok) {
      LOG(DEBUG) << "cannot open '" << collection->_directory
                 << "', check failed";
      return nullptr;
    }

    LOG_TOPIC(TRACE, Logger::PERFORMANCE)
        << "[timer] " << Logger::FIXED(TRI_microtime() - start)
        << " s, open-collection { collection: " << vocbase->_name << "/"
        << collection->_info.name() << " }";

    return collection;
  } catch (...) {
    LOG(ERR) << "cannot load collection parameter file '" << path
             << "': " << TRI_last_error();
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseCollection(TRI_collection_t* collection) {
  // close compactor files
  CloseDataFiles(collection->_compactors);

  // close journal files
  CloseDataFiles(collection->_journals);

  // close datafiles
  CloseDataFiles(collection->_datafiles);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection files
////////////////////////////////////////////////////////////////////////////////

TRI_col_file_structure_t TRI_FileStructureCollectionDirectory(
    char const* path) {
  return ScanCollectionDirectory(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over the markers in a collection's datafiles
///
/// this function may be called on server startup for all collections, in order
/// to get the last tick value used
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateTicksCollection(char const* const path,
                                bool (*iterator)(TRI_df_marker_t const*, void*,
                                                 TRI_datafile_t*),
                                void* data) {
  TRI_ASSERT(iterator != nullptr);

  TRI_col_file_structure_t structure = ScanCollectionDirectory(path);
  LOG(TRACE) << "iterating ticks of journal '" << path << "'";

  if (structure.journals.empty()) {
    // no journal found for collection. should not happen normally, but if
    // it does, we need to grab the ticks from the datafiles, too
    return IterateFiles(structure.datafiles, iterator, data);
  } 
    
  // compactor files don't need to be iterated... they just contain data
  // copied
  // from other files, so their tick values will never be any higher
  return IterateFiles(structure.journals, iterator, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection name is a system collection name
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemNameCollection(char const* name) {
  if (name == nullptr) {
    return false;
  }

  return *name == '_';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a collection name is allowed
///
/// Returns true if the name is allowed and false otherwise
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameCollection(bool allowSystem, char const* name) {
  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (ptr = name; *ptr; ++ptr) {
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') ||
             ('A' <= *ptr && *ptr <= 'Z');
      } else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}
