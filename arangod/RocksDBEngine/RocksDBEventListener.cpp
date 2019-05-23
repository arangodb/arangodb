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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEventListener.h"

#include <algorithm>

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "RestServer/DatabasePathFeature.h"

namespace arangodb {

// must call Thread::shutdown() in order to properly shutdown
RocksDBEventListenerThread::~RocksDBEventListenerThread() { shutdown(); }

void RocksDBEventListenerThread::run() {

  while (!isStopping()) {
    try {
      {
        MUTEX_LOCKER(mutexLock, _pendingMutex);

        while (!_pendingQueue.empty()) {
          actionNeeded_t next(_pendingQueue.front());
          _pendingQueue.pop();

          mutexLock.unlock();
          switch (next._action) {
            case actionNeeded_t::CALC_SHA:
              shaCalcFile(next._path);
              break;

            case actionNeeded_t::DELETE:
              deleteFile(next._path);
              break;

            default:
              LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
                << "RocksDBEventListenerThread::run encountered unknown _action";
              TRI_ASSERT(false);
              break;
          } // switch
          mutexLock.lock();
        } // while
      }

      // we could find files that subsequently post to _pendingQueue ... no worries.
      checkMissingShaFiles(getRocksDBPath());

      // no need for fast retry, hotbackups do not happen often
      {
        CONDITION_LOCKER(condLock, _loopingCondvar);
        if (!isStopping()) {
          condLock.wait(std::chrono::minutes(5));
        } // if
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBEventListenerThread::run caught exception: " << ex.what();
    } catch (...) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "RocksDBEventListenerThread::run caught an exception";
    } // catch
  } // while

} // RocksDBEventListenerThread::run()


void RocksDBEventListenerThread::queueShaCalcFile(std::string const& pathName) {

  {
    MUTEX_LOCKER(lock, _pendingMutex);
    _pendingQueue.emplace(actionNeeded_t{actionNeeded_t::CALC_SHA, pathName});
  }
  {
    CONDITION_LOCKER(lock, _loopingCondvar);
    lock.signal();
  }

} // RocksDBEventListenerThread::queueShaCalcFile


void RocksDBEventListenerThread::queueDeleteFile(std::string const & pathName) {

  {
    MUTEX_LOCKER(lock, _pendingMutex);
    _pendingQueue.emplace(actionNeeded_t{actionNeeded_t::DELETE, pathName});
  }

  signalLoop();
} // RocksDBEventListenerThread::queueDeleteFile

void RocksDBEventListenerThread::signalLoop() {

  {
    CONDITION_LOCKER(lock, _loopingCondvar);
    lock.signal();
  }

} // RocksDBEventListenerThread::signalLoop


bool RocksDBEventListenerThread::shaCalcFile(std::string const& filename) {
  bool good = false;

  if (4 < filename.size() && 0 == filename.substr(filename.size() - 4).compare(".sst")) {
    TRI_SHA256Functor sha;
    good = TRI_ProcessFile(filename.c_str(), std::ref(sha));

    if (good) {
      std::string newfile = filename.substr(0, filename.size() - 4);
      newfile += ".sha.";
      newfile += sha.final();
      newfile += ".hash";
      int ret_val = TRI_WriteFile(newfile.c_str(), "", 0);
      if (TRI_ERROR_NO_ERROR != ret_val) {
        good = false;
        LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
          << "shaCalcFile: TRI_WriteFile failed with " << ret_val
          << " for " << newfile.c_str();
      } // if
    } else {
      LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
        << "shaCalcFile:  TRI_ProcessFile failed for " << filename.c_str();
    } // else
  } // if

  return good;

} // RocksDBEventListenerThread::shaCalcFile


bool RocksDBEventListenerThread::deleteFile(std::string const& filename) {
  bool good = false, found = false;

  // need filename without ".sst" for matching to ".sha." file
  std::string basename = TRI_Basename(filename.c_str());
  if (4 < basename.size()
      && 0 == basename.substr(basename.size() - 4).compare(".sst")) {
    basename = basename.substr(0, basename.size() - 4);
  } else {
    // abort looking
    found = true;
  } // else

  if (!found) {
    std::string dirname = TRI_Dirname(filename.c_str());
    std::vector<std::string> filelist = TRI_FilesDirectory(dirname.c_str());

    // future thought: are there faster ways to find matching .sha. file?
    for (auto iter = filelist.begin(); !found && filelist.end() != iter; ++iter) {
      // sha256 is 64 characters long.  ".sha." is added 5.  So 69 characters is minimum length
      if (69 < iter->size() && 0 == iter->substr(0, basename.size()).compare(basename)
          && 0 == iter->substr(basename.size(), 5).compare(".sha.")) {
        found = true;
        std::string deletefile = dirname;
        deletefile += TRI_DIR_SEPARATOR_CHAR;
        deletefile += *iter;
        int ret_val = TRI_UnlinkFile(deletefile.c_str());
        good = (TRI_ERROR_NO_ERROR == ret_val);
        if (!good) {
          LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
            << "deleteCalcFile:  TRI_UnlinkFile failed with " << ret_val
            << " for " << deletefile.c_str();
        } // if
      } // if
    } // for
  } // if

  return good;

} // RocksDBEventListenerThread::shaCalcFile


//
// @brief Wrapper for getFeature<DatabasePathFeature> to simplify
//        unit testing
//
std::string RocksDBEventListenerThread::getRocksDBPath() {
  // get base path from DatabaseServerFeature
  auto databasePathFeature =
      application_features::ApplicationServer::getFeature<DatabasePathFeature>(
          "DatabasePath");
  std::string rockspath = databasePathFeature->directory();
  rockspath += TRI_DIR_SEPARATOR_CHAR;
  rockspath += "engine-rocksdb";

  return rockspath;

} // RocksDBEventListenerThread::getRocksDBPath

///
/// @brief Double check the active directory to see that all .sst files
///        have a matching .sha. (and delete any none matched .sha. files)
void RocksDBEventListenerThread::checkMissingShaFiles(std::string const& pathname) {
  std::string temppath, tempname;
  std::vector<std::string> filelist = TRI_FilesDirectory(pathname.c_str());

  // sorting will put xxxxxx.sha.yyy just before xxxxxx.sst
  std::sort(filelist.begin(), filelist.end());

  for (auto iter = filelist.begin(); filelist.end() != iter; ++iter) {
    std::string::size_type shaIdx;
    shaIdx = iter->find(".sha.");
    if (std::string::npos != shaIdx) {
      // two cases: 1. its .sst follows so skip both, 2. no matching .sst, so delete
      bool match = false;
      auto next = iter + 1;
      if (filelist.end() != next) {
        tempname = iter->substr(0, shaIdx);
        tempname += ".sst";
        if (0 == next->compare(tempname)) {
          match = true;
          iter = next;
        } // if
      } // if

      if (!match) {
        temppath = pathname;
        temppath += TRI_DIR_SEPARATOR_CHAR;
        temppath += *iter;
        TRI_UnlinkFile(temppath.c_str());
      } // if
    } else if (0 == iter->substr(iter->size() - 4).compare(".sst")) {
      // reaching this point means no .sha. preceeded, now check the
      // modification time, if younger than 5 mins, just leave it, otherwise,
      // we create a sha file. This is to ensure that sha files are eventually
      // generated if somebody switches on backup after the fact. However,
      // normally, the shas should only be computed when the sst file has
      // been fully written, which can only be guaranteed if we got a
      // creation event.
      int64_t now = ::time(nullptr);
      int64_t modTime;
      int r = TRI_MTimeFile(iter->c_str(), &modTime);
      if (r == 0 && (now - modTime) > 5 * 60) {  // 5 mins
        temppath = pathname;
        temppath += TRI_DIR_SEPARATOR_CHAR;
        temppath += *iter;
        shaCalcFile(temppath);
      }
    } // else
  } // for

  return;

} // RocksDBEventListenerThread::checkMissingShaFiles


//
// Setup the object, clearing variables, but do no real work
//
RocksDBEventListener::RocksDBEventListener()
  : _shaThread("Sha256Thread") {
  _shaThread.start(&_threadDone);
} // RocksDBEventListener::RocksDBEventListener

//
// Shutdown the background thread only if it was ever started
//
RocksDBEventListener::~RocksDBEventListener() {

  _shaThread.signalLoop();
  CONDITION_LOCKER(locker, _threadDone);
  if (_shaThread.isRunning()) {
    _threadDone.wait();
  }

} // RocksDBEventListener::~RocksDBEventListener


///
/// @brief
///
///
void RocksDBEventListener::OnFlushCompleted(rocksdb::DB* db,
                                       const rocksdb::FlushJobInfo& flush_job_info) {
  _shaThread.queueShaCalcFile(flush_job_info.file_path);
}  // RocksDBEventListener::OnFlushCompleted

void RocksDBEventListener::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo& info) {
  _shaThread.queueDeleteFile(info.file_path);

} // RocksDBEventListener::OnTableFileDeleted

void RocksDBEventListener::OnCompactionCompleted(rocksdb::DB* db,
                                            const rocksdb::CompactionJobInfo& ci) {
  for (auto filename : ci.output_files ) {
    _shaThread.queueShaCalcFile(filename);
  } // for
}  // RocksDBEventListener::OnCompactionCompleted




}  // namespace arangodb
