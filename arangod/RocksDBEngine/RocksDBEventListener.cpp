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

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/files.h"
#include "Logger/Logger.h"

namespace arangodb {

void RocksDBEventListenerThread::run() {

  while(isRunning()) {
    try {
      {
        MUTEX_LOCKER(mutexLock, _pendingMutex);

        while(0 != _pendingQueue.size() ) {
          actionNeeded_t next(_pendingQueue.front());
          _pendingQueue.pop();

          mutexLock.unlock();
          switch(next._action) {
            case actionNeeded_t::CALC_SHA:

              break;

            case actionNeeded_t::DELETE:
              break;

            default:
              break;
          } // switch
          mutexLock.lock();
        } // while
      }

      /// ??? manual scan of directory for missing sha files?

      // no need for fast retry, hotbackups do not happen often
      {
        CONDITION_LOCKER(condLock, _loopingCondvar);
        condLock.wait(std::chrono::minutes(5));
      }
    } catch(...) {
      /// log something but keep on looping
    }
  } // while

} // RocksDBEventListenerThread::run()

void RocksDBEventListenerThread::queueShaCalcFile(std::string const & pathName) {

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
  {
    CONDITION_LOCKER(lock, _loopingCondvar);
    lock.signal();
  }

} // RocksDBEventListenerThread::queueDeleteFile

void RocksDBEventListenerThread::signalLoop() {

  {
    CONDITION_LOCKER(lock, _loopingCondvar);
    lock.signal();
  }

} // RocksDBEventListenerThread::signalLoop


bool RocksDBEventListenerThread::shaCalcFile(std::string const & filename) {
  TRI_SHA256Functor sha;
  bool good={false};

  if (4<filename.size() && 0 == filename.substr(filename.size() - 4).compare(".sst")) {
    good = TRI_ProcessFile(filename.c_str(), sha);

    if (good) {
      std::string newfile = filename.substr(0, filename.size() - 4);
      newfile += ".sha.";
      newfile += sha.final();
      int ret_val = TRI_WriteFile(newfile.c_str(), "", 0);
      if (TRI_ERROR_NO_ERROR != ret_val) {
        good = false;
        LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
          << "shaCalcFile: TRI_WriteFile failed with " << ret_val
          << " for " << newfile.c_str();
      } // if
    } else {
      good = false;
      LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
        << "shaCalcFile:  TRI_ProcessFile failed for " << filename.c_str();
    } // else
  } // if

  return good;

} // RocksDBEventListenerThread::shaCalcFile


bool RocksDBEventListenerThread::deleteFile(std::string const & filename) {
  bool good = {false};
  bool found = {false};

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
      // sha265 is 64 characters long.  ".sha." is added 5.  So 69 characters is minimum length
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

  if (_shaThread.isRunning()) {
    _shaThread.beginShutdown();
    _shaThread.signalLoop();
    _threadDone.wait();
  } // if
}


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
