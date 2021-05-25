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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBShaCalculator.h"

#include <algorithm>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabasePathFeature.h"

namespace arangodb {

// must call Thread::shutdown() in order to properly shutdown
RocksDBShaCalculatorThread::~RocksDBShaCalculatorThread() { shutdown(); }

void RocksDBShaCalculatorThread::run() {
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

            case actionNeeded_t::DELETE_ACTION:
              deleteFile(next._path);
              break;

            default:
              LOG_TOPIC("7c75c", ERR, arangodb::Logger::ENGINES)
                  << "RocksDBShaCalculatorThread::run encountered unknown "
                     "action";
              TRI_ASSERT(false);
              break;
          }
          mutexLock.lock();
        }
      }

      // we could find files that subsequently post to _pendingQueue ... no worries.
      checkMissingShaFiles(getRocksDBPath(), 5 * 60);
      // need not to be written to in the past 5 minutes!

      // no need for fast retry, hotbackups do not happen often
      {
        CONDITION_LOCKER(condLock, _loopingCondvar);
        if (!isStopping()) {
          condLock.wait(std::chrono::minutes(5));
        }
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("a27a1", ERR, arangodb::Logger::ENGINES)
          << "RocksDBShaCalculatorThread::run caught exception: " << ex.what();
    } catch (...) {
      LOG_TOPIC("66a10", ERR, arangodb::Logger::ENGINES)
          << "RocksDBShaCalculatorThread::run caught an exception";
    }
  }
}

void RocksDBShaCalculatorThread::queueShaCalcFile(std::string const& pathName) {
  {
    MUTEX_LOCKER(lock, _pendingMutex);
    _pendingQueue.emplace(actionNeeded_t{actionNeeded_t::CALC_SHA, pathName});
  }
  signalLoop();
}

void RocksDBShaCalculatorThread::queueDeleteFile(std::string const& pathName) {
  {
    MUTEX_LOCKER(lock, _pendingMutex);
    _pendingQueue.emplace(actionNeeded_t{actionNeeded_t::DELETE_ACTION, pathName});
  }

  signalLoop();
}

void RocksDBShaCalculatorThread::signalLoop() {
  {
    CONDITION_LOCKER(lock, _loopingCondvar);
    lock.signal();
  }
}

bool RocksDBShaCalculatorThread::shaCalcFile(std::string const& filename) {
  bool good = false;

  if (4 < filename.size() && 0 == filename.substr(filename.size() - 4).compare(".sst")) {
    TRI_SHA256Functor sha;
    LOG_TOPIC("af088", DEBUG, arangodb::Logger::ENGINES)
        << "shaCalcFile: computing " << filename;
    good = TRI_ProcessFile(filename.c_str(), std::ref(sha));

    if (good) {
      std::string newfile = filename.substr(0, filename.size() - 4);
      newfile += ".sha.";
      newfile += sha.finalize();
      newfile += ".hash";
      LOG_TOPIC("80257", DEBUG, arangodb::Logger::ENGINES)
          << "shaCalcFile: done " << filename << " result: " << newfile;
      auto ret_val = TRI_WriteFile(newfile.c_str(), "", 0);
      if (TRI_ERROR_NO_ERROR != ret_val) {
        good = false;
        LOG_TOPIC("8f7ef", DEBUG, arangodb::Logger::ENGINES)
            << "shaCalcFile: TRI_WriteFile failed with " << ret_val << " for " << newfile;
      }
    } else {
      LOG_TOPIC("7f3fd", DEBUG, arangodb::Logger::ENGINES)
          << "shaCalcFile:  TRI_ProcessFile failed for " << filename;
    }
  }

  return good;
}

bool RocksDBShaCalculatorThread::deleteFile(std::string const& filename) {
  bool good = false, found = false;

  // need filename without ".sst" for matching to ".sha." file
  std::string basename = TRI_Basename(filename.c_str());
  if (4 < basename.size() && 0 == basename.substr(basename.size() - 4).compare(".sst")) {
    basename = basename.substr(0, basename.size() - 4);
  } else {
    // abort looking
    found = true;
  }

  if (!found) {
    std::string dirname = TRI_Dirname(filename);
    std::vector<std::string> filelist = TRI_FilesDirectory(dirname.c_str());

    // future thought: are there faster ways to find matching .sha. file?
    for (auto const& it : filelist) {
      // sha256 is 64 characters long.  ".sha." is added 5.  So 69 characters is minimum length
      if (69 < it.size() && 0 == it.compare(0, basename.size(), basename) &&
          0 == it.substr(basename.size(), 5).compare(".sha.")) {
        std::string deletefile = basics::FileUtils::buildFilename(dirname, it);
        auto ret_val = TRI_UnlinkFile(deletefile.c_str());
        good = (TRI_ERROR_NO_ERROR == ret_val);
        if (!good) {
          LOG_TOPIC("acb34", DEBUG, arangodb::Logger::ENGINES)
              << "deleteCalcFile:  TRI_UnlinkFile failed with " << ret_val
              << " for " << deletefile;
        } else {
          LOG_TOPIC("e0a0d", DEBUG, arangodb::Logger::ENGINES)
              << "deleteCalcFile:  TRI_UnlinkFile succeeded for " << deletefile;
        }
        break;
      }
    }
  }

  return good;
}

/// @brief Wrapper for getFeature<DatabasePathFeature> to simplify
///        unit testing
std::string RocksDBShaCalculatorThread::getRocksDBPath() {
  // get base path from DatabaseServerFeature
  auto& databasePathFeature = _server.getFeature<DatabasePathFeature>();
  return databasePathFeature.subdirectoryName("engine-rocksdb");
}

/// @brief Double check the active directory to see that all .sst files
///        have a matching .sha. (and delete any none matched .sha. files)
///        Will only consider .sst files which have not been written to for
///        `requireAge` seconds.
void RocksDBShaCalculatorThread::checkMissingShaFiles(std::string const& pathname,
                                                      int64_t requireAge) {
  std::vector<std::string> filelist = TRI_FilesDirectory(pathname.c_str());

  // sorting will put xxxxxx.sha.yyy just before xxxxxx.sst
  std::sort(filelist.begin(), filelist.end());

  std::string temppath, tempname;
  for (auto iter = filelist.begin(); filelist.end() != iter; ++iter) {
    // cppcheck-suppress derefInvalidIteratorRedundantCheck
    std::string::size_type shaIdx = iter->find(".sha.");
    if (std::string::npos != shaIdx) {
      // two cases: 1. its .sst follows so skip both, 2. no matching .sst, so delete
      bool match = false;
      // cppcheck-suppress derefInvalidIteratorRedundantCheck
      auto next = iter + 1;
      // cppcheck-suppress derefInvalidIteratorRedundantCheck
      if (filelist.end() != next) {
        tempname = iter->substr(0, shaIdx);
        tempname += ".sst";
        if (0 == next->compare(tempname)) {
          match = true;
          iter = next;
        }
      }

      if (!match) {
        temppath = basics::FileUtils::buildFilename(pathname, *iter);
        LOG_TOPIC("4eac9", DEBUG, arangodb::Logger::ENGINES)
            << "checkMissingShaFiles:"
               " Deleting file "
            << temppath;
        TRI_UnlinkFile(temppath.c_str());
      }
    } else if (0 == iter->substr(iter->size() - 4).compare(".sst")) {
      // reaching this point means no .sha. preceeded, now check the
      // modification time, if younger than 5 mins, just leave it, otherwise,
      // we create a sha file. This is to ensure that sha files are eventually
      // generated if somebody switches on backup after the fact. However,
      // normally, the shas should only be computed when the sst file has
      // been fully written, which can only be guaranteed if we got a
      // creation event.
      temppath = basics::FileUtils::buildFilename(pathname, *iter);
      int64_t now = ::time(nullptr);
      int64_t modTime;
      auto r = TRI_MTimeFile(temppath.c_str(), &modTime);
      if (r == TRI_ERROR_NO_ERROR && (now - modTime) >= requireAge) {
        LOG_TOPIC("d6c86", DEBUG, arangodb::Logger::ENGINES)
            << "checkMissingShaFiles:"
               " Computing checksum for "
            << temppath;
        shaCalcFile(temppath);
      } else {
        LOG_TOPIC("7f70f", DEBUG, arangodb::Logger::ENGINES)
            << "checkMissingShaFiles:"
               " Not computing checksum for "
            << temppath << " since it is too young";
      }
    }
  }
}

/// @brief Setup the object, clearing variables, but do no real work
RocksDBShaCalculator::RocksDBShaCalculator(application_features::ApplicationServer& server)
    : _shaThread(server, "Sha256Thread") {
  _shaThread.start(&_threadDone);
}

/// @brief Shutdown the background thread only if it was ever started
RocksDBShaCalculator::~RocksDBShaCalculator() {
  _shaThread.signalLoop();
  CONDITION_LOCKER(locker, _threadDone);
  if (_shaThread.isRunning()) {
    _threadDone.wait();
  }
}

void RocksDBShaCalculator::OnFlushCompleted(rocksdb::DB* db,
                                            const rocksdb::FlushJobInfo& flush_job_info) {
  _shaThread.queueShaCalcFile(flush_job_info.file_path);
}

void RocksDBShaCalculator::OnTableFileDeleted(const rocksdb::TableFileDeletionInfo& info) {
  _shaThread.queueDeleteFile(info.file_path);
}

void RocksDBShaCalculator::OnCompactionCompleted(rocksdb::DB* db,
                                                 const rocksdb::CompactionJobInfo& ci) {
  for (auto const& filename : ci.output_files) {
    _shaThread.queueShaCalcFile(filename);
  }
}

}  // namespace arangodb
