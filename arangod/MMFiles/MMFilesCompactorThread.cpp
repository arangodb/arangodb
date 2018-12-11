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

#include "MMFilesCompactionFeature.h"
#include "MMFilesCompactorThread.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesCompactionLocker.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesDatafileStatisticsContainer.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

static char const* ReasonCorrupted =
    "skipped compaction because collection has corrupted datafile(s)";
static char const* ReasonNoDatafiles =
    "skipped compaction because collection has no datafiles";
static char const* ReasonCompactionBlocked =
    "skipped compaction because existing compactor file is in the way and "
    "waits to be processed";
static char const* ReasonDatafileSmall =
    "compacting datafile because it's small and will be merged with next";
static char const* ReasonEmpty =
    "compacting datafile because collection is empty";
static char const* ReasonOnlyDeletions =
    "compacting datafile because it contains only deletion markers";
static char const* ReasonDeadSize =
    "compacting datafile because it contains much dead object space";
static char const* ReasonDeadSizeShare =
    "compacting datafile because it contains high share of dead objects";
static char const* ReasonDeadCount =
    "compacting datafile because it contains many dead objects";
static char const* ReasonNothingToCompact =
    "checked datafiles, but no compaction opportunity found";
  
/// @brief compaction state
namespace arangodb {
struct CompactionContext {
  transaction::Methods* _trx;
  LogicalCollection* _collection;
  MMFilesDatafile* _compactor;
  MMFilesDatafileStatisticsContainer _dfi;
  bool _keepDeletions;

  CompactionContext(CompactionContext const&) = delete;
  CompactionContext() : _trx(nullptr), _collection(nullptr), _compactor(nullptr), _dfi(), _keepDeletions(true) {}
};
}

/// @brief callback to drop a datafile
void MMFilesCompactorThread::DropDatafileCallback(MMFilesDatafile* df, LogicalCollection* collection) {
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  TRI_ASSERT(df != nullptr);

  std::unique_ptr<MMFilesDatafile> datafile(df);
  TRI_voc_fid_t fid = datafile->fid();
  
  std::string copy;
  std::string name("deleted-" + std::to_string(fid) + ".db");
  std::string filename = arangodb::basics::FileUtils::buildFilename(physical->path(), name);

  if (datafile->isPhysical()) {
    // copy the current filename
    copy = datafile->getName();

    int res = datafile->rename(filename);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::COMPACTOR)
          << "cannot rename obsolete datafile '" << copy << "' to '"
          << filename << "': " << TRI_errno_string(res);
    } else {
      LOG_TOPIC(DEBUG, Logger::COMPACTOR)
          << "renamed obsolete datafile '" << copy << "' to '"
          << filename << "': " << TRI_errno_string(res);
    }
  }

  LOG_TOPIC(DEBUG, Logger::COMPACTOR)
      << "finished compacting datafile '" << datafile->getName() << "'";

  int res = datafile->close();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, Logger::COMPACTOR)
        << "cannot close obsolete datafile '" << datafile->getName()
        << "': " << TRI_errno_string(res);
  } else if (datafile->isPhysical()) {
    LOG_TOPIC(DEBUG, Logger::COMPACTOR)
        << "wiping compacted datafile '" << datafile->getName()
        << "' from disk";

    res = TRI_UnlinkFile(filename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::COMPACTOR)
          << "cannot wipe obsolete datafile '" << datafile->getName()
          << "': " << TRI_errno_string(res);
    }

    // check for .dead files
    if (!copy.empty()) {
      // remove .dead file for datafile
      std::string deadfile = copy + ".dead";

      // check if .dead file exists, then remove it
      if (TRI_ExistsFile(deadfile.c_str())) {
        TRI_UnlinkFile(deadfile.c_str());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to rename a datafile
///
/// The datafile will be renamed to "temp-abc.db" (where "abc" is the fid of
/// the datafile) first. If this rename operation fails, there will be a
/// compactor file and a datafile. On startup, the datafile will be preferred
/// in this case.
/// If renaming succeeds, the compactor will be named to the original datafile.
/// If that does not succeed, there is a compactor file and a renamed datafile.
/// On startup, the compactor file will be used, and the renamed datafile
/// will be treated as a temporary file and dropped.
////////////////////////////////////////////////////////////////////////////////

void MMFilesCompactorThread::RenameDatafileCallback(MMFilesDatafile* datafile, 
                                                    MMFilesDatafile* compactor, 
                                                    LogicalCollection* collection) {
  TRI_ASSERT(datafile != nullptr);
  TRI_ASSERT(compactor != nullptr);
  TRI_ASSERT(collection != nullptr);
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  std::string compactorName = compactor->getName();

  bool ok = false;
  TRI_ASSERT(datafile->fid() == compactor->fid());

  if (datafile->isPhysical()) {
    // construct a suitable tempname
    std::string jname("temp-" + std::to_string(datafile->fid()) + ".db");
    std::string tempFilename = arangodb::basics::FileUtils::buildFilename(physical->path(), jname);
    std::string realName = datafile->getName();

    int res = datafile->rename(tempFilename);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::COMPACTOR) << "unable to rename datafile '" << datafile->getName() << "' to '" << tempFilename << "': " << TRI_errno_string(res);
    } else {
      LOG_TOPIC(DEBUG, arangodb::Logger::COMPACTOR)
        << "renamed datafile from '" << realName << "' to '"
        << tempFilename << "'";

      res = compactor->rename(realName);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(ERR, Logger::COMPACTOR) << "unable to rename compaction file '" << compactor->getName() << "' to '" << realName << "': " << TRI_errno_string(res);
      } else {
        LOG_TOPIC(DEBUG, arangodb::Logger::COMPACTOR)
          << "renamed datafile from '" << compactorName << "' to '"
          << tempFilename << "'";
      }
    }

    ok = (res == TRI_ERROR_NO_ERROR);
  } else {
    ok = true;
  }

  if (ok) {
    int res = static_cast<MMFilesCollection*>(collection->getPhysical())->replaceDatafileWithCompactor(datafile, compactor);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error: could not swap datafile and compactor files";
      return;
    }

    DropDatafileCallback(datafile, collection);
  }
}

/// @brief remove an empty compactor file
int MMFilesCompactorThread::removeCompactor(LogicalCollection* collection,
                                            MMFilesDatafile* compactor) {
  LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "removing empty compaction file '" << compactor->getName() << "'";

  // remove the compactor from the list of compactors
  bool ok = static_cast<MMFilesCollection*>(collection->getPhysical())->removeCompactor(compactor);

  if (!ok) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error: could not locate compactor";

    return TRI_ERROR_INTERNAL;
  }

  // close the file & remove it
  if (compactor->isPhysical()) {
    std::string filename = compactor->getName();
    delete compactor;
    TRI_UnlinkFile(filename.c_str());
  } else {
    delete compactor;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief remove an empty datafile
int MMFilesCompactorThread::removeDatafile(LogicalCollection* collection,
                                           MMFilesDatafile* df) {
  LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "removing empty datafile '" << df->getName() << "'";

  bool ok = static_cast<MMFilesCollection*>(collection->getPhysical())->removeDatafile(df);

  if (!ok) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error: could not locate datafile";

    return TRI_ERROR_INTERNAL;
  }

  // update dfi
  static_cast<MMFilesCollection*>(collection->getPhysical())->_datafileStatistics.remove(df->fid());

  return TRI_ERROR_NO_ERROR;
}


/// @brief calculate the target size for the compactor to be created
MMFilesCompactorThread::CompactionInitialContext MMFilesCompactorThread::getCompactionContext(
    transaction::Methods* trx, LogicalCollection* collection,
    std::vector<CompactionInfo> const& toCompact) {
  CompactionInitialContext context(trx, collection);

  // this is the minimum required size
  context._targetSize =
      sizeof(MMFilesDatafileHeaderMarker) + sizeof(MMFilesCollectionHeaderMarker) +
      sizeof(MMFilesDatafileFooterMarker) + 256;  // allow for some overhead

  size_t const n = toCompact.size();

  for (size_t i = 0; i < n; ++i) {
    auto compaction = toCompact[i];
    MMFilesDatafile* df = compaction._datafile;

    // We will sequentially scan the logfile for collection:
    if (df->isPhysical()) {
      df->sequentialAccess();
      df->willNeed();
    }

    if (i == 0) {
      // extract and store fid
      context._fid = compaction._datafile->fid();
    }

    context._keepDeletions = compaction._keepDeletions;

    /// @brief datafile iterator, calculates necessary total size
    auto calculateSize = [&context](MMFilesMarker const* marker, MMFilesDatafile* datafile) -> bool {
      LogicalCollection* collection = context._collection;
      TRI_ASSERT(collection != nullptr);
      auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
      TRI_ASSERT(physical != nullptr);
      MMFilesMarkerType const type = marker->getType();

      // new or updated document
      if (type == TRI_DF_MARKER_VPACK_DOCUMENT) {
        VPackSlice const slice(reinterpret_cast<char const*>(marker) + MMFilesDatafileHelper::VPackOffset(type));
        TRI_ASSERT(slice.isObject());

        VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(slice);

        // check if the document is still active
        auto primaryIndex = physical->primaryIndex();
        MMFilesMarker const* markerPtr = nullptr;
        MMFilesSimpleIndexElement element = primaryIndex->lookupKey(context._trx, keySlice);
        if (element) {
          MMFilesDocumentPosition const old =
              physical->lookupDocument(element.localDocumentId());
          markerPtr = reinterpret_cast<MMFilesMarker const*>(
              static_cast<uint8_t const*>(old.dataptr()) -
              MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
        }

        bool deleted = (markerPtr == nullptr || marker != markerPtr);

        if (deleted) {
          return true;
        }

        context._keepDeletions = true;
        context._targetSize += MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(marker);
      }

      // deletions
      else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
        if (context._keepDeletions) {
          context._targetSize += MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(marker);
        }
      }

      return true;
    };

    bool ok;
    {
      auto physical = static_cast<MMFilesCollection*>(context._collection->getPhysical());
      TRI_ASSERT(physical != nullptr);
      bool const useDeadlockDetector = false;
      int res = physical->lockRead(useDeadlockDetector, trx->state(), 86400.0);

      if (res != TRI_ERROR_NO_ERROR) {
        ok = false;
      } else {
        // got read lock
        try {
          ok = TRI_IterateDatafile(df, calculateSize);
        } catch (...) {
          ok = false;
        }
        physical->unlockRead(useDeadlockDetector, trx->state());
      }
    }

    if (df->isPhysical()) {
      df->randomAccess();
    }

    if (!ok) {
      context._failed = true;
      break;
    }
  }

  return context;
}

/// @brief compact the specified datafiles
void MMFilesCompactorThread::compactDatafiles(LogicalCollection* collection,
    std::vector<CompactionInfo> const& toCompact) {
  TRI_ASSERT(collection != nullptr);
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  size_t const n = toCompact.size();
  TRI_ASSERT(n > 0);
  
  auto context = std::make_unique<CompactionContext>();

  /// @brief datafile iterator, copies "live" data from datafile into compactor
  /// this function is called for all markers in the collected datafiles. Its
  /// purpose is to find the still-alive markers and copy them into the compactor
  /// file.
  /// IMPORTANT: if the logic inside this function is adjusted, the total size
  /// calculated by function CalculateSize might need adjustment, too!!
  auto compactifier = [&context, &physical, this](MMFilesMarker const* marker, MMFilesDatafile* datafile) -> bool {
    TRI_voc_fid_t const targetFid = context->_compactor->fid();

    MMFilesMarkerType const type = marker->getType();

    // new or updated document
    if (type == TRI_DF_MARKER_VPACK_DOCUMENT) {
      VPackSlice const slice(reinterpret_cast<char const*>(marker) + MMFilesDatafileHelper::VPackOffset(type));
      TRI_ASSERT(slice.isObject());

      VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(slice);

      // check if the document is still active
      auto primaryIndex = physical->primaryIndex();
      MMFilesMarker const* markerPtr = nullptr;
      MMFilesSimpleIndexElement element = primaryIndex->lookupKey(context->_trx, keySlice);
      if (element) {
        MMFilesDocumentPosition const old = physical->lookupDocument(element.localDocumentId());
        markerPtr = reinterpret_cast<MMFilesMarker const*>(static_cast<uint8_t const*>(old.dataptr()) - MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
      }
        
      bool deleted = (markerPtr == nullptr || marker != markerPtr);

      if (deleted) {
        // found a dead document
        return true;
      }

      context->_keepDeletions = true;

      // write to compactor files
      MMFilesMarker* result;
      int res = copyMarker(context->_compactor, marker, &result);

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("cannot write document marker into compactor file: ") + TRI_errno_string(res)); 
      }

      // let marker point to the new position
      uint8_t const* dataptr = reinterpret_cast<uint8_t const*>(result) + MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT);
      physical->updateLocalDocumentId(element.localDocumentId(), dataptr, targetFid, false);

      context->_dfi.numberAlive++;
      context->_dfi.sizeAlive += MMFilesDatafileHelper::AlignedMarkerSize<int64_t>(marker);
    }

    // deletions
    else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
      if (context->_keepDeletions) {
        // write to compactor files
        MMFilesMarker* result;
        int res = copyMarker(context->_compactor, marker, &result);

        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("cannot write remove marker into compactor file: ") + TRI_errno_string(res));
        }

        // update datafile info
        context->_dfi.numberDeletions++;
      }
    }

    return true;
  };

  arangodb::SingleCollectionTransaction trx(
    arangodb::transaction::StandaloneContext::Create(collection->vocbase()),
    *collection,
    AccessMode::Type::WRITE
  );

  trx.addHint(transaction::Hints::Hint::NO_BEGIN_MARKER);
  trx.addHint(transaction::Hints::Hint::NO_ABORT_MARKER);
  trx.addHint(transaction::Hints::Hint::NO_COMPACTION_LOCK);
  trx.addHint(transaction::Hints::Hint::NO_THROTTLING);
  // when we get into this function, the caller has already acquired the
  // collection's status lock - so we better do not lock it again
  trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);

  CompactionInitialContext initial = getCompactionContext(&trx, collection, toCompact);

  if (initial._failed) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "could not create initialize compaction";

    return;
  }

  LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "compaction writes to be executed for collection '" << collection->id() << "', number of source datafiles: " << n << ", target datafile size: " << initial._targetSize;

  // now create a new compactor file
  // we are re-using the _fid of the first original datafile!
  MMFilesDatafile* compactor = nullptr;
  try {
    compactor = physical->createCompactor(initial._fid, static_cast<uint32_t>(initial._targetSize));
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "could not create compactor file: " << ex.what();
    return;
  } catch (...) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "could not create compactor file: unknown exception";
    return;
  }

  TRI_ASSERT(compactor != nullptr);

  LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "created new compactor file '" << compactor->getName() << "', size: " << compactor->maximalSize();

  // these attributes remain the same for all datafiles we collect
  context->_collection = collection;
  context->_compactor = compactor;
  context->_trx = &trx;

  Result res = trx.begin();

  if (!res.ok()) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "error during compaction: " << res.errorMessage();
    return;
  }

  // now compact all datafiles
  uint64_t nrCombined = 0;
  uint64_t compactionBytesRead = 0;
  for (size_t i = 0; i < n; ++i) {
    auto compaction = toCompact[i];
    MMFilesDatafile* df = compaction._datafile;

    compactionBytesRead += df->currentSize();
    LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "compacting datafile '" << df->getName() << "' into '" << compactor->getName() << "', number: " << i << ", keep deletions: " << compaction._keepDeletions;

    // if this is the first datafile in the list of datafiles, we can also
    // collect deletion markers
    context->_keepDeletions = compaction._keepDeletions;

    // run the actual compaction of a single datafile
    bool ok;
    try {
      ok = TRI_IterateDatafile(df, compactifier);
    } catch (std::exception const& ex) {
      LOG_TOPIC(WARN, Logger::COMPACTOR) << "failed to compact datafile '" << df->getName() << "': " << ex.what();
      throw;
    }

    if (!ok) {
      LOG_TOPIC(WARN, Logger::COMPACTOR) << "failed to compact datafile '" << df->getName() << "'";
      // compactor file does not need to be removed now. will be removed on next
      // startup
      return;
    }

    ++nrCombined;
  }  // next file

  TRI_ASSERT(context->_dfi.numberDead == 0);
  TRI_ASSERT(context->_dfi.sizeDead == 0);

  physical->_datafileStatistics.compactionRun(nrCombined, compactionBytesRead, context->_dfi.sizeAlive);
  try {
    physical->_datafileStatistics.replace(compactor->fid(), context->_dfi, true);
  } catch (...) {
  }

  trx.commit();

  // remove all datafile statistics that we don't need anymore
  for (size_t i = 1; i < n; ++i) {
    auto compaction = toCompact[i];
    physical->_datafileStatistics.remove(compaction._datafile->fid());
  }

  if (physical->closeCompactor(compactor) != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "could not close compactor file";
    // TODO: how do we recover from this state?
    return;
  }

  if (context->_dfi.numberAlive == 0 && context->_dfi.numberDead == 0 &&
      context->_dfi.numberDeletions == 0) {
    // everything is empty after compaction
    if (n > 1) {
      // create .dead files for all collected files
      for (size_t i = 0; i < n; ++i) {
        auto compaction = toCompact[i];
        MMFilesDatafile* datafile = compaction._datafile;

        if (datafile->isPhysical()) {
          std::string filename(datafile->getName());
          filename.append(".dead");

          TRI_WriteFile(filename.c_str(), "", 0);
        }
      }
    }

    // compactor is fully empty. remove it
    removeCompactor(collection, compactor);

    for (size_t i = 0; i < n; ++i) {
      auto compaction = toCompact[i];

      // datafile is also empty after compaction and thus useless
      removeDatafile(collection, compaction._datafile);

      // add a deletion ditch to the collection
      auto b = arangodb::MMFilesCollection::toMMFilesCollection(collection)
                   ->ditches()
                   ->createMMFilesDropDatafileDitch(compaction._datafile, collection,
                                             DropDatafileCallback, __FILE__,
                                             __LINE__);

      if (b == nullptr) {
        LOG_TOPIC(ERR, Logger::COMPACTOR) << "out of memory when creating datafile-drop ditch";
      }
    }
  } else {
    if (n > 1) {
      // create .dead files for all collected files but the first
      for (size_t i = 1; i < n; ++i) {
        auto compaction = toCompact[i];
        MMFilesDatafile* datafile = compaction._datafile;

        if (datafile->isPhysical()) {
          std::string filename(datafile->getName());
          filename.append(".dead");

          TRI_WriteFile(filename.c_str(), "", 0);
        }
      }
    }

    for (size_t i = 0; i < n; ++i) {
      auto compaction = toCompact[i];

      if (i == 0) {
        // add a rename marker
        auto b = arangodb::MMFilesCollection::toMMFilesCollection(collection)
                     ->ditches()
                     ->createMMFilesRenameDatafileDitch(
                         compaction._datafile, context->_compactor,
                         context->_collection, RenameDatafileCallback, __FILE__,
                         __LINE__);

        if (b == nullptr) {
          LOG_TOPIC(ERR, Logger::COMPACTOR) << "out of memory when creating datafile-rename ditch";
        } else {
          _vocbase.signalCleanup();
        }
      } else {
        // datafile is empty after compaction and thus useless
        removeDatafile(collection, compaction._datafile);

        // add a drop datafile marker
        auto b = arangodb::MMFilesCollection::toMMFilesCollection(collection)
                     ->ditches()
                     ->createMMFilesDropDatafileDitch(compaction._datafile, collection,
                                               DropDatafileCallback, __FILE__,
                                               __LINE__);

        if (b == nullptr) {
          LOG_TOPIC(ERR, Logger::COMPACTOR) << "out of memory when creating datafile-drop ditch";
        } else {
          _vocbase.signalCleanup();
        }
      }
    }
  }
}

/// @brief checks all datafiles of a collection
bool MMFilesCompactorThread::compactCollection(LogicalCollection* collection, bool& wasBlocked) {
  // we can hopefully get away without the lock here...
  //  if (! document->isFullyCollected()) {
  //    return false;
  //  }

  wasBlocked = false;

  // if we cannot acquire the read lock instantly, we will exit directly.
  // otherwise we'll risk a multi-thread deadlock between synchronizer,
  // compactor and data-modification threads (e.g. POST /_api/document)
  MMFilesCollection* physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  TRY_READ_LOCKER(readLocker, physical->_filesLock);

  if (!readLocker.isLocked()) {
    // unable to acquire the lock at the moment
    wasBlocked = true;
    return false;
  }
  
  // check if there is already a compactor file
  if (!physical->_compactors.empty()) {
    // we already have created a compactor file in progress.
    // if this happens, then a previous compaction attempt for this collection
    // failed or is not finished yet
    physical->setCompactionStatus(ReasonCompactionBlocked);
    wasBlocked = true;
    return false;
  }

  // copy datafiles vector
  std::vector<MMFilesDatafile*> datafiles = physical->_datafiles;

  if (datafiles.empty()) {
    // collection has no datafiles
    physical->setCompactionStatus(ReasonNoDatafiles);
    return false;
  }
  
  std::vector<CompactionInfo> toCompact;
  toCompact.reserve(MMFilesCompactionFeature::COMPACTOR->maxFiles());

  // now we have datafiles that we can process 
  size_t const n = datafiles.size();
  LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "inspecting datafiles of collection '" << collection->name() << "' for compaction opportunities";

  size_t start = physical->getNextCompactionStartIndex();

  // get number of documents from collection
  uint64_t const numDocuments = getNumberOfDocuments(*collection);

  // get maximum size of result file
  uint64_t maxSize = MMFilesCompactionFeature::COMPACTOR->maxSizeFactor() * static_cast<MMFilesCollection*>(collection->getPhysical())->journalSize();
  if (maxSize < 8 * 1024 * 1024) {
    maxSize = 8 * 1024 * 1024;
  }
  if (maxSize >= MMFilesCompactionFeature::COMPACTOR->maxResultFilesize()) {
    maxSize = MMFilesCompactionFeature::COMPACTOR->maxResultFilesize();
  }

  if (start >= n || numDocuments == 0) {
    start = 0;
  }

  int64_t numAlive = 0;
  if (start > 0) {
    // we don't know for sure if there are alive documents in the first
    // datafile,
    // so let's assume there are some
    numAlive = 16384;
  }

  bool doCompact = false;
  uint64_t totalSize = 0;
  char const* reason = nullptr;
  char const* firstReason = nullptr;
  
  for (size_t i = start; i < n; ++i) {
    MMFilesDatafile* df = datafiles[i];
    if (df->state() == TRI_DF_STATE_OPEN_ERROR || df->state() == TRI_DF_STATE_WRITE_ERROR) {
      LOG_TOPIC(WARN, Logger::COMPACTOR) << "cannot compact datafile " << df->fid() << " of collection '" << collection->name() << "' because it has errors";
      physical->setCompactionStatus(ReasonCorrupted);
      return false;
    }
  }

  for (size_t i = start; i < n; ++i) {
    MMFilesDatafile* df = datafiles[i];
    TRI_ASSERT(df != nullptr);

    MMFilesDatafileStatisticsContainer dfi = static_cast<MMFilesCollection*>(collection->getPhysical())->_datafileStatistics.get(df->fid());

    if (dfi.numberUncollected > 0) {
      LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "cannot compact datafile " << df->fid() << " of collection '" << collection->name() << "' because it still has uncollected entries";
      start = i + 1;
      break;
    }

    if (!doCompact &&
        df->maximalSize() < MMFilesCompactionFeature::COMPACTOR->smallDatafileSize() &&
        (i < n - 1)) {
      // very small datafile and not the last one. let's compact it so it's
      // merged with others
      doCompact = true;
      reason = ReasonDatafileSmall;
    } else if (numDocuments == 0 &&
               (dfi.numberAlive > 0 || dfi.numberDead > 0 ||
                dfi.numberDeletions > 0)) {
      // collection is empty, but datafile statistics indicate there is
      // something in this datafile
      doCompact = true;
      reason = ReasonEmpty;
    } else if (numAlive == 0 && dfi.numberAlive == 0 &&
               dfi.numberDeletions > 0) {
      // compact first datafile(s) if they contain only deletions
      doCompact = true;
      reason = ReasonOnlyDeletions;
    } else if (dfi.sizeDead >= MMFilesCompactionFeature::COMPACTOR->deadSizeThreshold()) {
      // the size of dead objects is above some threshold
      doCompact = true;
      reason = ReasonDeadSize;
    } else if (dfi.sizeDead > 0 &&
               (((double)dfi.sizeDead /
                     ((double)dfi.sizeDead + (double)dfi.sizeAlive) >= MMFilesCompactionFeature::COMPACTOR->deadShare()) ||
                ((double)dfi.sizeDead / (double)df->maximalSize() >= MMFilesCompactionFeature::COMPACTOR->deadShare()))) {
      // the size of dead objects is above some share
      doCompact = true;
      reason = ReasonDeadSizeShare;
    } else if (dfi.numberDead >= MMFilesCompactionFeature::COMPACTOR->deadNumberThreshold()) {
      // the number of dead objects is above some threshold
      doCompact = true;
      reason = ReasonDeadCount;
    }

    if (!doCompact) {
      numAlive += static_cast<int64_t>(dfi.numberAlive);
      continue;
    }
    

    TRI_ASSERT(doCompact);

    if (firstReason == nullptr) {
      firstReason = reason;
    }


    // remember for next compaction
    start = i + 1;

    // if we got only deletions then it's safe to continue compaction, regardless of
    // the size of the resulting file. this is because deletions will reduce the
    // size of the resulting file
    if (reason != ReasonOnlyDeletions) {
      if (!toCompact.empty() && 
          totalSize + (uint64_t)df->maximalSize() >= maxSize &&
          (toCompact.size() != 1 || reason != ReasonDatafileSmall)) {
        // found enough files to compact (in terms of cumulated size)
        // there's one exception to this: if we're merging multiple datafiles, 
        // then we don't stop at the first one even if the merge of file #1 and #2
        // would be too big. if we wouldn't stop in this case, then file #1 would
        // be selected for compaction over and over
        // normally this case won't happen at all, it can occur however if one
        // decreases the journalSize configuration for the collection afterwards, and
        // there are already datafiles which are more than 3 times bigger than the
        // new (smaller) journalSize value
        break;
      }
    }

    TRI_ASSERT(reason != nullptr);

    LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "found datafile #" << i << " eligible for compaction. fid: " << df->fid() << ", size: " << df->maximalSize() << ", reason: " << reason << ", numberDead: " << dfi.numberDead << ", numberAlive: " << dfi.numberAlive << ", numberDeletions: " << dfi.numberDeletions << ", numberUncollected: " << dfi.numberUncollected << ", sizeDead: " << dfi.sizeDead << ", sizeAlive: " << dfi.sizeAlive;
    totalSize += static_cast<uint64_t>(df->maximalSize());

    CompactionInfo compaction;
    compaction._datafile = df;
    compaction._keepDeletions = (numAlive > 0 && i > 0);
    // TODO: verify that keepDeletions actually works with wrong numAlive stats

    try {
      toCompact.push_back(compaction);
    } catch (...) {
      // silently fail. either we had found something to compact or not
      // if not, then we can try again next time. if yes, then we'll simply
      // forget
      // about it and also try again next time
      break;
    }

    // we stop at the first few datafiles.
    // this is better than going over all datafiles in a collection in one go
    // because the compactor is single-threaded, and collecting all datafiles
    // might take a long time (it might even be that there is a request to
    // delete the collection in the middle of compaction, but the compactor
    // will not pick this up as it is read-locking the collection status)

    if (totalSize >= maxSize) {
      // result file will be big enough
      break;
    }

    if (totalSize >= MMFilesCompactionFeature::COMPACTOR->smallDatafileSize() &&
        toCompact.size() >= MMFilesCompactionFeature::COMPACTOR->maxFiles()) {
      // found enough files to compact
      break;
    }

    numAlive += static_cast<int64_t>(dfi.numberAlive);
  }

  // we can now continue without the lock
  readLocker.unlock();

  if (toCompact.empty()) {
    // nothing to compact. now reset start index
    physical->setNextCompactionStartIndex(0);
    
    // cleanup local variables
    physical->setCompactionStatus(ReasonNothingToCompact);
    LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "inspecting datafiles of collection yielded: " << ReasonNothingToCompact;
    return false;
  }
    
  // handle datafiles with dead objects
  TRI_ASSERT(toCompact.size() >= 1);
  TRI_ASSERT(reason != nullptr);
  physical->setCompactionStatus(reason);
  physical->setNextCompactionStartIndex(start);
  compactDatafiles(collection, toCompact);

  return true;
}

MMFilesCompactorThread::MMFilesCompactorThread(TRI_vocbase_t& vocbase)
    : Thread("MMFilesCompactor"), _vocbase(vocbase) {}

MMFilesCompactorThread::~MMFilesCompactorThread() { shutdown(); }

void MMFilesCompactorThread::signal() {
  CONDITION_LOCKER(locker, _condition);
  locker.signal();
}

void MMFilesCompactorThread::run() {
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections;
  int numCompacted = 0;
  while (true) {
    // keep initial _state value as vocbase->_state might change during
    // compaction loop
    TRI_vocbase_t::State state = _vocbase.state();

    try {
      engine->tryPreventCompaction(
        &_vocbase,
        [this, &numCompacted, &collections, &engine](TRI_vocbase_t* vocbase) {
        // compaction is currently allowed
        numCompacted = 0;

        try {
          // copy all collections
          collections = _vocbase.collections(false);
        } catch (...) {
          collections.clear();
        }
  
        for (auto& collection : collections) {
          bool worked = false;
            
          if (engine->isCompactionDisabled()) {
            continue;
          }

          auto callback = [this, &collection, &worked, &engine]() -> void {
            if (collection->status() != TRI_VOC_COL_STATUS_LOADED &&
                collection->status() != TRI_VOC_COL_STATUS_UNLOADING) {
              return;
            }

            bool doCompact = static_cast<MMFilesCollection*>(collection->getPhysical())->doCompact();

            if (engine->isCompactionDisabled()) {
              doCompact = false;
            }

            // for document collection, compactify datafiles
            if (collection->status() == TRI_VOC_COL_STATUS_LOADED && doCompact) {
              // check whether someone else holds a read-lock on the compaction
              // lock
              
              auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
              TRI_ASSERT(physical != nullptr);

              MMFilesTryCompactionLocker compactionLocker(physical);

              if (!compactionLocker.isLocked()) {
                // someone else is holding the compactor lock, we'll not compact
                return;
              }

              try {
                double const now = TRI_microtime();
                if (physical->lastCompactionStamp() + MMFilesCompactionFeature::COMPACTOR->compactionCollectionInterval() <= now) {
                  auto ce = arangodb::MMFilesCollection::toMMFilesCollection(collection.get())
                                ->ditches()
                                ->createMMFilesCompactionDitch(__FILE__, __LINE__);

                  if (ce == nullptr) {
                    // out of memory
                    LOG_TOPIC(WARN, Logger::COMPACTOR) << "out of memory when trying to create compaction ditch";
                  } else {
                    try {
                      bool wasBlocked = false;
                      worked = compactCollection(collection.get(), wasBlocked);

                      if (!worked && !wasBlocked) {
                        // set compaction stamp
                        physical->lastCompactionStamp(now);
                      }
                      // if we worked or were blocked, then we don't set the compaction stamp to
                      // force another round of compaction
                    } catch (std::exception const& ex) {
                      LOG_TOPIC(ERR, Logger::COMPACTOR) << "caught exception during compaction: " << ex.what();
                    } catch (...) {
                      LOG_TOPIC(ERR, Logger::COMPACTOR) << "an unknown exception occurred during compaction";
                      // in case an error occurs, we must still free this ditch
                    }

                    arangodb::MMFilesCollection::toMMFilesCollection(collection.get())
                        ->ditches()
                        ->freeDitch(ce);
                  }
                }
              } catch (std::exception const& ex) {
                LOG_TOPIC(ERR, Logger::COMPACTOR) << "caught exception during compaction: " << ex.what();
              } catch (...) {
                // in case an error occurs, we must still relase the lock
                LOG_TOPIC(ERR, Logger::COMPACTOR) << "an unknown exception occurred during compaction";
              }
            }
          };

          if (!collection->tryExecuteWhileStatusLocked(callback)) {
            continue;
          }

          if (worked) {
            ++numCompacted;

            // signal the cleanup thread that we worked and that it can now wake
            // up
            CONDITION_LOCKER(locker, _condition);
            locker.signal();
          }
        }
      }, true);

      if (numCompacted > 0) {
        // no need to sleep long or go into wait state if we worked.
        // maybe there's still work left
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
      } else if (state != TRI_vocbase_t::State::SHUTDOWN_COMPACTOR
                 && _vocbase.state() == TRI_vocbase_t::State::NORMAL) {
        // only sleep while server is still running
        CONDITION_LOCKER(locker, _condition);
        _condition.wait(MMFilesCompactionFeature::COMPACTOR->compactionSleepTime());
      }

      if (state == TRI_vocbase_t::State::SHUTDOWN_COMPACTOR || isStopping()) {
        // server shutdown or database has been removed
        break;
      }
    } catch (...) {
      // caught an error during compaction. simply ignore it and go on
    }
  }

  LOG_TOPIC(TRACE, Logger::COMPACTOR) << "shutting down compactor thread";
}

/// @brief determine the number of documents in the collection
uint64_t MMFilesCompactorThread::getNumberOfDocuments(
    LogicalCollection& collection
) {
  SingleCollectionTransaction trx(
    transaction::StandaloneContext::Create(_vocbase),
    collection,
    AccessMode::Type::READ
  );

  // only try to acquire the lock here
  // if lock acquisition fails, we go on and report an (arbitrary) positive number
  trx.addHint(transaction::Hints::Hint::TRY_LOCK); 
  trx.addHint(transaction::Hints::Hint::NO_THROTTLING);
  // when we get into this function, the caller has already acquired the
  // collection's status lock - so we better do not lock it again
  trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);

  Result res = trx.begin();

  if (!res.ok()) {
    return 16384; // assume some positive value 
  }

  return collection.numberDocuments(&trx, transaction::CountType::Normal);
}

/// @brief write a copy of the marker into the datafile
int MMFilesCompactorThread::copyMarker(MMFilesDatafile* compactor, MMFilesMarker const* marker,
                                       MMFilesMarker** result) {
  int res = compactor->reserveElement(marker->getSize(), result, 0);

  if (res != TRI_ERROR_NO_ERROR) {
    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  return compactor->writeElement(*result, marker);
}
